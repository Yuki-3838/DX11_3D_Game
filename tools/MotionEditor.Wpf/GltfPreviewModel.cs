using System.Buffers.Binary;
using System.IO;
using System.Numerics;
using System.Text;
using System.Text.Json;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using System.Windows.Media.Imaging;
using NumericsQuaternion = System.Numerics.Quaternion;

namespace DX11MotionEditor;

/// <summary>
/// Minimal GLB reader used by the editor preview. It intentionally supports the
/// subset used by the project's player model: node TRS, one skin, indexed
/// primitives, positions/normals/UVs, JOINTS_0 and WEIGHTS_0.
/// </summary>
public sealed class GltfPreviewModel
{
    private readonly List<GltfNode> _nodes = new();
    private readonly List<GltfPrimitive> _primitives = new();
    private readonly List<int> _skinJoints = new();
    private readonly List<Matrix4x4> _inverseBindMatrices = new();
    private Matrix4x4[] _restWorldMatrices = Array.Empty<Matrix4x4>();
    private byte[] _binary = Array.Empty<byte>();
    private ImageSource? _diffuseImage;
    private int _sceneRoot = -1;

    public string FilePath { get; private set; } = string.Empty;
    public bool IsLoaded => _nodes.Count > 0 && _primitives.Count > 0;
    public string ErrorText { get; private set; } = string.Empty;

    public bool Load(string path)
    {
        Clear();
        try
        {
            var bytes = File.ReadAllBytes(path);
            if (bytes.Length < 20 || Encoding.ASCII.GetString(bytes, 0, 4) != "glTF")
                throw new InvalidDataException("GLBヘッダーが見つかりません。");

            var jsonLength = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(12, 4));
            var jsonStart = 20;
            if (jsonStart + jsonLength > bytes.Length)
                throw new InvalidDataException("GLB JSONチャンクが壊れています。");
            var json = Encoding.UTF8.GetString(bytes, jsonStart, checked((int)jsonLength)).Trim();
            using var document = JsonDocument.Parse(json);
            var root = document.RootElement;
            _jsonRoot = root.Clone();

            var binStart = jsonStart + checked((int)jsonLength);
            while (binStart + 8 <= bytes.Length)
            {
                var chunkLength = checked((int)BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(binStart, 4)));
                var chunkType = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(binStart + 4, 4));
                var dataStart = binStart + 8;
                if (dataStart + chunkLength > bytes.Length)
                    throw new InvalidDataException("GLBバイナリチャンクが壊れています。");
                if (chunkType == 0x004E4942)
                {
                    _binary = bytes[dataStart..(dataStart + chunkLength)];
                    break;
                }
                binStart = dataStart + chunkLength;
            }
            if (_binary.Length == 0)
                throw new InvalidDataException("GLBバイナリチャンクが見つかりません。");

            ParseNodes(root);
            _sceneRoot = GetSceneRoot(root);
            ParseSkin(root);
            ParseDiffuseTexture(root);
            ParseMeshes(root);
            if (_primitives.Count == 0)
                throw new InvalidDataException("GLBに表示可能なメッシュがありません。");

            FilePath = Path.GetFullPath(path);
            ErrorText = string.Empty;
            return true;
        }
        catch (Exception ex)
        {
            Clear();
            ErrorText = ex.Message;
            return false;
        }
    }

    public Model3DGroup BuildPose(
        float time,
        Func<string, float, MotionKey?> sampleKey,
        Func<string, MotionKey?>? firstKey = null,
        Func<string, MotionKey?>? guardKey = null,
        bool rebaseImportedMotion = false)
    {
        var group = new Model3DGroup();
        if (!IsLoaded)
            return group;

        var worldMatrices = new Matrix4x4[_nodes.Count];
        var visited = new bool[_nodes.Count];
        if (_sceneRoot >= 0)
            BuildWorldMatrices(_sceneRoot, Matrix4x4.Identity, time, sampleKey, firstKey, guardKey, rebaseImportedMotion, worldMatrices, visited);
        for (var i = 0; i < _nodes.Count; i++)
        {
            if (!visited[i])
                BuildWorldMatrices(i, Matrix4x4.Identity, time, sampleKey, firstKey, guardKey, rebaseImportedMotion, worldMatrices, visited);
        }

        foreach (var primitive in _primitives)
        {
            // This asset's two accessory mesh labels are swapped: the mesh named
            // "Helmet" is the large shield-shaped accessory, while the mesh named
            // "Shield" is the helmet shell around the head.  Hide the former so
            // the editor does not show the detached shield during attacks, but
            // keep the actual helmet mesh visible.
            if (primitive.Name.Contains("Helmet", StringComparison.OrdinalIgnoreCase))
                continue;

            var mesh = BuildPrimitiveMesh(primitive, worldMatrices);
            var color = PrimitiveColor(primitive.Name);
            var material = CreateMaterial(color);
            group.Children.Add(new GeometryModel3D(mesh, material) { BackMaterial = material });
        }
        return group;
    }

    private void BuildWorldMatrices(
        int nodeIndex,
        Matrix4x4 parentWorld,
        float time,
        Func<string, float, MotionKey?> sampleKey,
        Func<string, MotionKey?>? firstKey,
        Func<string, MotionKey?>? guardKey,
        bool rebaseImportedMotion,
        Matrix4x4[] worldMatrices,
        bool[] visited)
    {
        if (nodeIndex < 0 || nodeIndex >= _nodes.Count || visited[nodeIndex])
            return;
        var node = _nodes[nodeIndex];
        var key = string.IsNullOrEmpty(node.Name) ? null : sampleKey(node.Name, time);
        var motion = BuildMotionMatrix(node.Name, key, firstKey, guardKey, rebaseImportedMotion);
        var local = motion * node.RestLocal;
        worldMatrices[nodeIndex] = local * parentWorld;
        visited[nodeIndex] = true;
        foreach (var child in node.Children)
            BuildWorldMatrices(child, worldMatrices[nodeIndex], time, sampleKey, firstKey, guardKey, rebaseImportedMotion, worldMatrices, visited);
    }

    private static Matrix4x4 BuildMotionMatrix(
        string boneName,
        MotionKey? key,
        Func<string, MotionKey?>? firstKey,
        Func<string, MotionKey?>? guardKey,
        bool rebaseImportedMotion)
    {
        if (key is null)
            return Matrix4x4.Identity;
        var current = MotionMatrix(key);
        if (!rebaseImportedMotion || firstKey is null)
            return current;

        var basePose = guardKey?.Invoke(boneName);
        var baseMatrix = basePose is null ? Matrix4x4.Identity : MotionMatrix(basePose);
        if (!IsImportedDriverBone(boneName))
            return baseMatrix;

        var sourceStart = firstKey(boneName);
        if (sourceStart is null || !Matrix4x4.Invert(MotionMatrix(sourceStart), out var sourceStartInverse))
            return baseMatrix;
        var sourceDelta = current * sourceStartInverse;
        return sourceDelta * baseMatrix;
    }

    private static bool IsImportedDriverBone(string name)
    {
        var normalized = name.ToLowerInvariant()
            .Replace("mixamorig:", string.Empty, StringComparison.Ordinal)
            .Replace("_", string.Empty, StringComparison.Ordinal)
            .Replace("-", string.Empty, StringComparison.Ordinal)
            .Replace(" ", string.Empty, StringComparison.Ordinal);
        return normalized is "spine" or "spine1" or "spine2" or
            "leftarm" or "rightarm" or
            "leftforearm" or "rightforearm" or
            "lefthand" or "righthand";
    }

    private MeshGeometry3D BuildPrimitiveMesh(GltfPrimitive primitive, Matrix4x4[] worldMatrices)
    {
        var mesh = new MeshGeometry3D();
        var meshWorld = primitive.NodeIndex >= 0 && primitive.NodeIndex < worldMatrices.Length
            ? worldMatrices[primitive.NodeIndex]
            : Matrix4x4.Identity;
        Matrix4x4.Invert(meshWorld, out var inverseMeshWorld);

        for (var vertex = 0; vertex < primitive.Positions.Length; vertex++)
        {
            var position = SkinPosition(primitive, vertex, worldMatrices);
            var normal = SkinNormal(primitive, vertex, worldMatrices);
            if (!primitive.HasSkin)
            {
                position = Vector3.Transform(position, meshWorld);
                normal = Vector3.TransformNormal(normal, inverseMeshWorld);
            }

            mesh.Positions.Add(new Point3D(position.X, position.Y, position.Z));
            mesh.Normals.Add(new Vector3D(normal.X, normal.Y, normal.Z));
            if (primitive.Texcoords is not null)
            {
                var uv = primitive.Texcoords[vertex];
                // The accessory mesh carrying the helmet was exported with the
                // opposite V convention from the body mesh.  Correct only this
                // mesh; flipping every primitive moves the body onto unrelated
                // regions of the atlas.
                if (primitive.Name.Contains("Shield", StringComparison.OrdinalIgnoreCase))
                    uv.Y = 1.0f - uv.Y;
                mesh.TextureCoordinates.Add(new System.Windows.Point(uv.X, uv.Y));
            }
        }
        foreach (var index in primitive.Indices)
            mesh.TriangleIndices.Add(index);
        return mesh;
    }

    private Vector3 SkinPosition(GltfPrimitive primitive, int vertex, Matrix4x4[] worldMatrices)
    {
        if (!primitive.HasSkin || primitive.Joints is null || primitive.Weights is null)
            return primitive.Positions[vertex];
        var result = Vector3.Zero;
        var joints = primitive.Joints[vertex];
        var weights = primitive.Weights[vertex];
        for (var i = 0; i < 4; i++)
        {
            var weight = GetComponent(weights, i);
            if (weight <= 0.00001f)
                continue;
            var jointSlot = (int)GetComponent(joints, i);
            if (jointSlot < 0 || jointSlot >= _skinJoints.Count)
                continue;
            var jointNode = _skinJoints[jointSlot];
            var inverseBind = jointSlot < _inverseBindMatrices.Count
                ? _inverseBindMatrices[jointSlot]
                : Matrix4x4.Identity;
            var skinMatrix = inverseBind * worldMatrices[jointNode];
            result += Vector3.Transform(primitive.Positions[vertex], skinMatrix) * weight;
        }
        return result;
    }

    private Vector3 SkinNormal(GltfPrimitive primitive, int vertex, Matrix4x4[] worldMatrices)
    {
        var source = primitive.Normals is not null && vertex < primitive.Normals.Length
            ? primitive.Normals[vertex]
            : Vector3.UnitY;
        if (!primitive.HasSkin || primitive.Joints is null || primitive.Weights is null)
            return Vector3.Normalize(source);
        var result = Vector3.Zero;
        var joints = primitive.Joints[vertex];
        var weights = primitive.Weights[vertex];
        for (var i = 0; i < 4; i++)
        {
            var weight = GetComponent(weights, i);
            if (weight <= 0.00001f)
                continue;
            var jointSlot = (int)GetComponent(joints, i);
            if (jointSlot < 0 || jointSlot >= _skinJoints.Count)
                continue;
            var jointNode = _skinJoints[jointSlot];
            var inverseBind = jointSlot < _inverseBindMatrices.Count
                ? _inverseBindMatrices[jointSlot]
                : Matrix4x4.Identity;
            result += Vector3.TransformNormal(source, inverseBind * worldMatrices[jointNode]) * weight;
        }
        return result.LengthSquared() > 0.00001f ? Vector3.Normalize(result) : Vector3.UnitY;
    }

    private void ParseNodes(JsonElement root)
    {
        if (!root.TryGetProperty("nodes", out var nodes))
            throw new InvalidDataException("GLBにノードがありません。");
        foreach (var element in nodes.EnumerateArray())
        {
            var node = new GltfNode
            {
                Name = GetString(element, "name"),
                MeshIndex = GetInt(element, "mesh", -1),
                SkinIndex = GetInt(element, "skin", -1),
                RestLocal = ReadNodeTransform(element)
            };
            if (element.TryGetProperty("children", out var children))
                node.Children.AddRange(children.EnumerateArray().Select(item => item.GetInt32()));
            _nodes.Add(node);
        }
    }

    private void ParseSkin(JsonElement root)
    {
        if (!root.TryGetProperty("skins", out var skins) || skins.GetArrayLength() == 0)
            return;
        var skin = skins[0];
        if (skin.TryGetProperty("joints", out var joints))
            _skinJoints.AddRange(joints.EnumerateArray().Select(item => item.GetInt32()));
        if (skin.TryGetProperty("inverseBindMatrices", out var inverseBindAccessor))
        {
            _inverseBindMatrices.AddRange(ReadAccessorMatrices(inverseBindAccessor.GetInt32()));
        }
        if (_inverseBindMatrices.Count != _skinJoints.Count)
            RebuildInverseBindMatrices();
    }

    private void RebuildInverseBindMatrices()
    {
        _restWorldMatrices = new Matrix4x4[_nodes.Count];
        var visited = new bool[_nodes.Count];
        if (_sceneRoot >= 0)
            BuildRestWorldMatrices(_sceneRoot, Matrix4x4.Identity, visited);
        for (var i = 0; i < _nodes.Count; i++)
        {
            if (!visited[i])
                BuildRestWorldMatrices(i, Matrix4x4.Identity, visited);
        }

        _inverseBindMatrices.Clear();
        foreach (var jointNode in _skinJoints)
        {
            if (jointNode >= 0 && jointNode < _restWorldMatrices.Length &&
                Matrix4x4.Invert(_restWorldMatrices[jointNode], out var inverse))
                _inverseBindMatrices.Add(inverse);
            else
                _inverseBindMatrices.Add(Matrix4x4.Identity);
        }
    }

    private void BuildRestWorldMatrices(int nodeIndex, Matrix4x4 parentWorld, bool[] visited)
    {
        if (nodeIndex < 0 || nodeIndex >= _nodes.Count || visited[nodeIndex])
            return;
        var node = _nodes[nodeIndex];
        _restWorldMatrices[nodeIndex] = node.RestLocal * parentWorld;
        visited[nodeIndex] = true;
        foreach (var child in node.Children)
            BuildRestWorldMatrices(child, _restWorldMatrices[nodeIndex], visited);
    }

    private void ParseDiffuseTexture(JsonElement root)
    {
        try
        {
            var material = root.GetProperty("materials")[0];
            var pbr = material.GetProperty("pbrMetallicRoughness");
            var textureIndex = GetInt(pbr.GetProperty("baseColorTexture"), "index", -1);
            if (textureIndex < 0)
                return;
            var texture = root.GetProperty("textures")[textureIndex];
            var imageIndex = GetInt(texture, "source", -1);
            if (imageIndex < 0)
                return;
            var image = root.GetProperty("images")[imageIndex];
            var bufferView = GetInt(image, "bufferView", -1);
            if (bufferView < 0)
                return;
            var bytes = ReadBufferViewBytes(bufferView);
            using var stream = new MemoryStream(bytes, writable: false);
            var bitmap = new BitmapImage();
            bitmap.BeginInit();
            bitmap.CacheOption = BitmapCacheOption.OnLoad;
            bitmap.StreamSource = stream;
            bitmap.EndInit();
            bitmap.Freeze();
            _diffuseImage = bitmap;
        }
        catch
        {
            _diffuseImage = null;
        }
    }

    private void ParseMeshes(JsonElement root)
    {
        if (!root.TryGetProperty("meshes", out var meshes))
            return;
        var meshNodes = _nodes
            .Select((node, index) => (node, index))
            .Where(pair => pair.node.MeshIndex >= 0)
            .ToDictionary(pair => pair.node.MeshIndex, pair => pair.index);
        var meshIndex = 0;
        foreach (var mesh in meshes.EnumerateArray())
        {
            var meshName = GetString(mesh, "name");
            if (!mesh.TryGetProperty("primitives", out var primitives))
            {
                meshIndex++;
                continue;
            }
            foreach (var primitive in primitives.EnumerateArray())
            {
                if (!primitive.TryGetProperty("attributes", out var attributes) ||
                    !attributes.TryGetProperty("POSITION", out var positionAccessor))
                    continue;
                var positions = ReadAccessorVector3(positionAccessor.GetInt32());
                var normals = attributes.TryGetProperty("NORMAL", out var normalAccessor)
                    ? ReadAccessorVector3(normalAccessor.GetInt32())
                    : null;
                var texcoords = attributes.TryGetProperty("TEXCOORD_0", out var uvAccessor)
                    ? ReadAccessorVector2(uvAccessor.GetInt32())
                    : null;
                var joints = attributes.TryGetProperty("JOINTS_0", out var jointAccessor)
                    ? ReadAccessorVector4(jointAccessor.GetInt32())
                    : null;
                var weights = attributes.TryGetProperty("WEIGHTS_0", out var weightAccessor)
                    ? ReadAccessorVector4(weightAccessor.GetInt32())
                    : null;
                var indices = primitive.TryGetProperty("indices", out var indexAccessor)
                    ? ReadAccessorIndices(indexAccessor.GetInt32())
                    : Enumerable.Range(0, positions.Length).ToArray();
                _primitives.Add(new GltfPrimitive
                {
                    Name = string.IsNullOrEmpty(meshName) ? $"mesh_{meshIndex}" : meshName,
                    NodeIndex = meshNodes.TryGetValue(meshIndex, out var nodeIndex) ? nodeIndex : -1,
                    Positions = positions,
                    Normals = normals,
                    Texcoords = texcoords,
                    Joints = joints,
                    Weights = weights,
                    Indices = indices,
                    HasSkin = joints is not null && weights is not null && _skinJoints.Count > 0
                });
            }
            meshIndex++;
        }
    }

    private int GetSceneRoot(JsonElement root)
    {
        var sceneIndex = GetInt(root, "scene", 0);
        if (!root.TryGetProperty("scenes", out var scenes) || sceneIndex < 0 || sceneIndex >= scenes.GetArrayLength())
            return -1;
        var scene = scenes[sceneIndex];
        if (!scene.TryGetProperty("nodes", out var roots) || roots.GetArrayLength() == 0)
            return -1;
        return roots[0].GetInt32();
    }

    private Vector3[] ReadAccessorVector3(int accessor) => ReadAccessor(accessor, 3).Select(values => new Vector3(values[0], values[1], values[2])).ToArray();
    private Vector2[] ReadAccessorVector2(int accessor) => ReadAccessor(accessor, 2).Select(values => new Vector2(values[0], values[1])).ToArray();
    private Vector4[] ReadAccessorVector4(int accessor) => ReadAccessor(accessor, 4).Select(values => new Vector4(values[0], values[1], values[2], values[3])).ToArray();
    private Matrix4x4[] ReadAccessorMatrices(int accessor) => ReadAccessor(accessor, 16).Select(ReadMatrix).ToArray();

    private int[] ReadAccessorIndices(int accessor)
    {
        var definition = ReadAccessorDefinition(accessor);
        var view = ReadBufferView(definition.BufferView);
        var componentSize = ComponentSize(definition.ComponentType);
        var stride = view.Stride > 0 ? view.Stride : componentSize;
        var values = new int[definition.Count];
        for (var i = 0; i < values.Length; i++)
            values[i] = (int)ReadComponent(view.Offset + definition.ByteOffset + i * stride, definition.ComponentType);
        return values;
    }

    private IEnumerable<float[]> ReadAccessor(int accessor, int components)
    {
        var definition = ReadAccessorDefinition(accessor);
        var view = ReadBufferView(definition.BufferView);
        var componentSize = ComponentSize(definition.ComponentType);
        var stride = view.Stride > 0 ? view.Stride : componentSize * components;
        for (var i = 0; i < definition.Count; i++)
        {
            var values = new float[components];
            for (var component = 0; component < components; component++)
            {
                var offset = view.Offset + definition.ByteOffset + i * stride + component * componentSize;
                values[component] = ReadComponent(offset, definition.ComponentType, definition.Normalized);
            }
            yield return values;
        }
    }

    private AccessorDefinition ReadAccessorDefinition(int accessor)
    {
        var root = _jsonRoot ?? throw new InvalidOperationException("GLB JSONが未初期化です。");
        var accessors = root.GetProperty("accessors");
        var element = accessors[accessor];
        return new AccessorDefinition(
            GetInt(element, "bufferView", -1),
            GetInt(element, "componentType", 5126),
            GetInt(element, "count", 0),
            GetString(element, "type"),
            GetInt(element, "byteOffset", 0),
            GetBool(element, "normalized", false));
    }

    private BufferViewDefinition ReadBufferView(int bufferView)
    {
        var root = _jsonRoot ?? throw new InvalidOperationException("GLB JSONが未初期化です。");
        var views = root.GetProperty("bufferViews");
        var element = views[bufferView];
        return new BufferViewDefinition(
            GetInt(element, "byteOffset", 0),
            GetInt(element, "byteLength", 0),
            GetInt(element, "byteStride", 0));
    }

    private byte[] ReadBufferViewBytes(int bufferView)
    {
        var view = ReadBufferView(bufferView);
        if (view.Offset < 0 || view.Length <= 0 || view.Offset + view.Length > _binary.Length)
            return Array.Empty<byte>();
        return _binary[view.Offset..(view.Offset + view.Length)];
    }

    private float ReadComponent(int offset, int componentType, bool normalized = false)
    {
        if (offset < 0 || offset >= _binary.Length)
            return 0.0f;
        var value = componentType switch
        {
            5120 => (sbyte)_binary[offset],
            5121 => _binary[offset],
            5122 => BinaryPrimitives.ReadInt16LittleEndian(_binary.AsSpan(offset, 2)),
            5123 => BinaryPrimitives.ReadUInt16LittleEndian(_binary.AsSpan(offset, 2)),
            5125 => BinaryPrimitives.ReadUInt32LittleEndian(_binary.AsSpan(offset, 4)),
            5126 => BitConverter.Int32BitsToSingle(BinaryPrimitives.ReadInt32LittleEndian(_binary.AsSpan(offset, 4))),
            _ => 0.0f
        };
        if (!normalized)
            return value;
        return componentType switch
        {
            5120 => Math.Max(value / 127.0f, -1.0f),
            5121 => value / 255.0f,
            5122 => Math.Max(value / 32767.0f, -1.0f),
            5123 => value / 65535.0f,
            _ => value
        };
    }

    private static int ComponentSize(int componentType) => componentType switch
    {
        5120 or 5121 => 1,
        5122 or 5123 => 2,
        5125 or 5126 => 4,
        _ => 4
    };

    private JsonElement? _jsonRoot;

    private void Clear()
    {
        _nodes.Clear();
        _primitives.Clear();
        _skinJoints.Clear();
        _inverseBindMatrices.Clear();
        _restWorldMatrices = Array.Empty<Matrix4x4>();
        _binary = Array.Empty<byte>();
        _diffuseImage = null;
        _jsonRoot = null;
        _sceneRoot = -1;
        FilePath = string.Empty;
    }

    private static Matrix4x4 MotionMatrix(MotionKey key) =>
        Matrix4x4.CreateScale(ToFiniteScale(key.Scale)) *
        Matrix4x4.CreateRotationX(key.Rotation.X) *
        Matrix4x4.CreateRotationY(key.Rotation.Y) *
        Matrix4x4.CreateRotationZ(key.Rotation.Z) *
        Matrix4x4.CreateTranslation(key.Position);

    private static Vector3 ToFiniteScale(Vector3 value) => new(
        float.IsFinite(value.X) ? value.X : 1.0f,
        float.IsFinite(value.Y) ? value.Y : 1.0f,
        float.IsFinite(value.Z) ? value.Z : 1.0f);

    private static Matrix4x4 ReadMatrix(float[] values) => new(
        values[0], values[1], values[2], values[3],
        values[4], values[5], values[6], values[7],
        values[8], values[9], values[10], values[11],
        values[12], values[13], values[14], values[15]);

    private static Matrix4x4 ReadNodeTransform(JsonElement element)
    {
        if (element.TryGetProperty("matrix", out var matrix))
            return ReadMatrix(matrix.EnumerateArray().Select(item => item.GetSingle()).ToArray());
        var translation = ReadVector3(element, "translation", Vector3.Zero);
        var rotation = ReadQuaternion(element, "rotation", NumericsQuaternion.Identity);
        var scale = ReadVector3(element, "scale", Vector3.One);
        return Matrix4x4.CreateScale(scale) * Matrix4x4.CreateFromQuaternion(rotation) * Matrix4x4.CreateTranslation(translation);
    }

    private Material CreateMaterial(Color color)
    {
        var group = new MaterialGroup();
        if (_diffuseImage is not null)
        {
            group.Children.Add(new DiffuseMaterial(new ImageBrush(_diffuseImage) { Stretch = Stretch.Fill }));
        }
        else
        {
            group.Children.Add(new DiffuseMaterial(new SolidColorBrush(color)));
            group.Children.Add(new EmissiveMaterial(new SolidColorBrush(Color.FromArgb(18, color.R, color.G, color.B))));
        }
        return group;
    }

    private static Color PrimitiveColor(string name)
    {
        if (name.Contains("Helmet", StringComparison.OrdinalIgnoreCase))
            return Color.FromRgb(52, 60, 76);
        if (name.Contains("Shield", StringComparison.OrdinalIgnoreCase))
            return Color.FromRgb(125, 48, 52);
        if (name.Contains("Sword", StringComparison.OrdinalIgnoreCase))
            return Color.FromRgb(190, 204, 218);
        return Color.FromRgb(74, 82, 98);
    }

    private static string GetString(JsonElement element, string property) =>
        element.TryGetProperty(property, out var value) && value.ValueKind == JsonValueKind.String
            ? value.GetString() ?? string.Empty
            : string.Empty;

    private static int GetInt(JsonElement element, string property, int fallback) =>
        element.TryGetProperty(property, out var value) && value.ValueKind == JsonValueKind.Number
            ? value.GetInt32()
            : fallback;

    private static bool GetBool(JsonElement element, string property, bool fallback)
    {
        if (!element.TryGetProperty(property, out var value))
            return fallback;
        return value.ValueKind switch
        {
            JsonValueKind.True => true,
            JsonValueKind.False => false,
            _ => fallback
        };
    }

    private static Vector3 ReadVector3(JsonElement element, string property, Vector3 fallback)
    {
        if (!element.TryGetProperty(property, out var value) || value.ValueKind != JsonValueKind.Array || value.GetArrayLength() < 3)
            return fallback;
        return new Vector3(value[0].GetSingle(), value[1].GetSingle(), value[2].GetSingle());
    }

    private static NumericsQuaternion ReadQuaternion(JsonElement element, string property, NumericsQuaternion fallback)
    {
        if (!element.TryGetProperty(property, out var value) || value.ValueKind != JsonValueKind.Array || value.GetArrayLength() < 4)
            return fallback;
        return NumericsQuaternion.Normalize(new NumericsQuaternion(value[0].GetSingle(), value[1].GetSingle(), value[2].GetSingle(), value[3].GetSingle()));
    }

    private static float GetComponent(Vector4 value, int index) => index switch
    {
        0 => value.X,
        1 => value.Y,
        2 => value.Z,
        _ => value.W
    };

    private sealed class GltfNode
    {
        public string Name { get; init; } = string.Empty;
        public int MeshIndex { get; init; } = -1;
        public int SkinIndex { get; init; } = -1;
        public Matrix4x4 RestLocal { get; init; }
        public List<int> Children { get; } = new();
    }

    private sealed class GltfPrimitive
    {
        public string Name { get; init; } = string.Empty;
        public int NodeIndex { get; init; }
        public Vector3[] Positions { get; init; } = Array.Empty<Vector3>();
        public Vector3[]? Normals { get; init; }
        public Vector2[]? Texcoords { get; init; }
        public Vector4[]? Joints { get; init; }
        public Vector4[]? Weights { get; init; }
        public int[] Indices { get; init; } = Array.Empty<int>();
        public bool HasSkin { get; init; }
    }

    private readonly record struct AccessorDefinition(int BufferView, int ComponentType, int Count, string Type, int ByteOffset, bool Normalized);
    private readonly record struct BufferViewDefinition(int Offset, int Length, int Stride);
}
