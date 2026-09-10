using System.IO;
using System.Globalization;
using System.Numerics;
using System.Text;

namespace DX11MotionEditor;

public sealed class MotionKey
{
    public float Time { get; set; }
    public Vector3 Rotation { get; set; }
    public Vector3 Position { get; set; }
    public Vector3 Scale { get; set; } = Vector3.One;

    public MotionKey Clone() => new()
    {
        Time = Time,
        Rotation = Rotation,
        Position = Position,
        Scale = Scale
    };
}

public sealed class MotionSnapshot
{
    public float Duration { get; init; }
    public Dictionary<string, List<MotionKey>> Bones { get; init; } = new(StringComparer.Ordinal);
    public string Signature { get; init; } = string.Empty;
}

public sealed class MotionDocument
{
    private readonly Dictionary<string, List<MotionKey>> _bones = new(StringComparer.Ordinal);

    public event EventHandler? Changed;
    public string FilePath { get; private set; } = string.Empty;
    public float Duration { get; set; } = 1.0f;
    public float CurrentTime { get; set; }
    public IReadOnlyDictionary<string, List<MotionKey>> Bones => _bones;
    public IReadOnlyList<string> BoneNames => _bones.Keys.OrderBy(x => x, StringComparer.Ordinal).ToList();

    public void Load(string path)
    {
        var loadedBones = new Dictionary<string, List<MotionKey>>(StringComparer.Ordinal);
        var duration = 1.0f;
        string? currentBone = null;

        foreach (var rawLine in File.ReadLines(path))
        {
            var line = rawLine.Trim();
            if (line.Length == 0 || line.StartsWith("#", StringComparison.Ordinal))
                continue;

            if (line.StartsWith("duration ", StringComparison.Ordinal))
            {
                if (float.TryParse(line[9..], NumberStyles.Float, CultureInfo.InvariantCulture, out var parsed))
                    duration = Math.Max(parsed, 0.01f);
                continue;
            }

            if (line.StartsWith("bone ", StringComparison.Ordinal))
            {
                var firstQuote = line.IndexOf('"');
                var lastQuote = line.LastIndexOf('"');
                if (firstQuote >= 0 && lastQuote > firstQuote)
                {
                    currentBone = line[(firstQuote + 1)..lastQuote];
                    currentBone = currentBone.Replace("\\\"", "\"");
                    currentBone = currentBone.Replace("\\\\", "\\");
                    loadedBones.TryAdd(currentBone, new List<MotionKey>());
                }
                continue;
            }

            if (line.StartsWith("endbone", StringComparison.Ordinal))
            {
                currentBone = null;
                continue;
            }

            if (currentBone is null || !line.StartsWith("key ", StringComparison.Ordinal))
                continue;

            var values = line[4..].Split(' ', StringSplitOptions.RemoveEmptyEntries);
            if (values.Length < 10)
                continue;
            var numbers = new float[10];
            var valid = true;
            for (var i = 0; i < numbers.Length; i++)
            {
                if (!float.TryParse(values[i], NumberStyles.Float, CultureInfo.InvariantCulture, out numbers[i]) ||
                    !float.IsFinite(numbers[i]))
                {
                    valid = false;
                    break;
                }
            }
            if (!valid)
                continue;

            loadedBones[currentBone].Add(new MotionKey
            {
                Time = numbers[0],
                Rotation = new Vector3(numbers[1], numbers[2], numbers[3]),
                Position = new Vector3(numbers[4], numbers[5], numbers[6]),
                Scale = new Vector3(numbers[7], numbers[8], numbers[9])
            });
        }

        foreach (var keys in loadedBones.Values)
            keys.Sort((a, b) => a.Time.CompareTo(b.Time));

        _bones.Clear();
        foreach (var pair in loadedBones)
            _bones[pair.Key] = pair.Value;
        Duration = duration;
        CurrentTime = 0.0f;
        FilePath = Path.GetFullPath(path);
        Touch();
    }

    public MotionSnapshot Capture()
    {
        var bones = new Dictionary<string, List<MotionKey>>(StringComparer.Ordinal);
        foreach (var pair in _bones)
            bones[pair.Key] = pair.Value.Select(key => key.Clone()).ToList();
        return new MotionSnapshot
        {
            Duration = Duration,
            Bones = bones,
            Signature = BuildSignature(Duration, bones)
        };
    }

    public void Restore(MotionSnapshot snapshot)
    {
        _bones.Clear();
        foreach (var pair in snapshot.Bones)
            _bones[pair.Key] = pair.Value.Select(key => key.Clone()).ToList();
        Duration = Math.Max(snapshot.Duration, 0.01f);
        CurrentTime = Math.Clamp(CurrentTime, 0.0f, Duration);
        Touch();
    }

    public MotionKey? FindKey(string boneName, float time, float tolerance = 0.035f)
    {
        if (!_bones.TryGetValue(boneName, out var keys))
            return null;
        return keys.OrderBy(key => Math.Abs(key.Time - time)).FirstOrDefault(key => Math.Abs(key.Time - time) <= tolerance);
    }

    public MotionKey? FirstKey(string boneName)
    {
        return _bones.TryGetValue(boneName, out var keys) && keys.Count > 0
            ? keys[0].Clone()
            : null;
    }

    public MotionKey? SampleKey(string boneName, float time)
    {
        if (!_bones.TryGetValue(boneName, out var keys) || keys.Count == 0)
            return null;
        if (keys.Count == 1 || time <= keys[0].Time)
            return keys[0].Clone();
        if (time >= keys[^1].Time)
            return keys[^1].Clone();

        for (var i = 1; i < keys.Count; i++)
        {
            var next = keys[i];
            if (time > next.Time)
                continue;
            var previous = keys[i - 1];
            var span = Math.Max(next.Time - previous.Time, 0.0001f);
            var amount = Math.Clamp((time - previous.Time) / span, 0.0f, 1.0f);
            return new MotionKey
            {
                Time = time,
                Rotation = Vector3.Lerp(previous.Rotation, next.Rotation, amount),
                Position = Vector3.Lerp(previous.Position, next.Position, amount),
                Scale = Vector3.Lerp(previous.Scale, next.Scale, amount)
            };
        }
        return keys[^1].Clone();
    }

    public bool UpsertKey(string boneName, MotionKey key)
    {
        if (string.IsNullOrWhiteSpace(boneName))
            return false;
        if (!_bones.TryGetValue(boneName, out var keys))
        {
            keys = new List<MotionKey>();
            _bones[boneName] = keys;
        }
        var existing = keys.FirstOrDefault(item => Math.Abs(item.Time - key.Time) < 0.001f);
        if (existing is not null)
        {
            existing.Rotation = key.Rotation;
            existing.Position = key.Position;
            existing.Scale = key.Scale;
        }
        else
            keys.Add(key.Clone());
        keys.Sort((a, b) => a.Time.CompareTo(b.Time));
        return true;
    }

    public bool MoveKey(string boneName, float fromTime, float toTime)
    {
        var key = FindKey(boneName, fromTime);
        if (key is null)
            return false;
        key.Time = Math.Clamp(toTime, 0.0f, Duration);
        _bones[boneName].Sort((a, b) => a.Time.CompareTo(b.Time));
        CurrentTime = key.Time;
        return true;
    }

    public bool DeleteKey(string boneName, float time)
    {
        if (!_bones.TryGetValue(boneName, out var keys))
            return false;
        var removed = keys.RemoveAll(key => Math.Abs(key.Time - time) <= 0.035f);
        return removed > 0;
    }

    public bool DuplicateKey(string boneName, float time)
    {
        var source = FindKey(boneName, time);
        if (source is null)
            return false;
        var copy = source.Clone();
        copy.Time = Math.Clamp(source.Time + 1.0f / 30.0f, 0.0f, Duration);
        return UpsertKey(boneName, copy);
    }

    public void Save(string path)
    {
        var fullPath = Path.GetFullPath(path);
        Directory.CreateDirectory(Path.GetDirectoryName(fullPath)!);
        File.WriteAllText(fullPath, Serialize(), new UTF8Encoding(false));
        FilePath = fullPath;
    }

    public string Serialize()
    {
        var builder = new StringBuilder();
        builder.AppendLine("DX11_MOTION 1");
        builder.Append("duration ").AppendLine(Duration.ToString("0.#########", CultureInfo.InvariantCulture));
        foreach (var pair in _bones.OrderBy(pair => pair.Key, StringComparer.Ordinal))
        {
            var escapedName = pair.Key.Replace("\\", "\\\\").Replace("\"", "\\\"");
            builder.Append("bone \"").Append(escapedName).AppendLine("\"");
            foreach (var key in pair.Value.OrderBy(key => key.Time))
            {
                builder.Append("key ")
                    .Append(key.Time.ToString("0.#########", CultureInfo.InvariantCulture)).Append(' ')
                    .Append(key.Rotation.X.ToString("0.#########", CultureInfo.InvariantCulture)).Append(' ')
                    .Append(key.Rotation.Y.ToString("0.#########", CultureInfo.InvariantCulture)).Append(' ')
                    .Append(key.Rotation.Z.ToString("0.#########", CultureInfo.InvariantCulture)).Append(' ')
                    .Append(key.Position.X.ToString("0.#########", CultureInfo.InvariantCulture)).Append(' ')
                    .Append(key.Position.Y.ToString("0.#########", CultureInfo.InvariantCulture)).Append(' ')
                    .Append(key.Position.Z.ToString("0.#########", CultureInfo.InvariantCulture)).Append(' ')
                    .Append(key.Scale.X.ToString("0.#########", CultureInfo.InvariantCulture)).Append(' ')
                    .Append(key.Scale.Y.ToString("0.#########", CultureInfo.InvariantCulture)).Append(' ')
                    .Append(key.Scale.Z.ToString("0.#########", CultureInfo.InvariantCulture)).AppendLine();
            }
            builder.AppendLine("endbone");
        }
        return builder.ToString();
    }

    public void Touch() => Changed?.Invoke(this, EventArgs.Empty);

    private static string BuildSignature(float duration, Dictionary<string, List<MotionKey>> bones)
    {
        var builder = new StringBuilder(duration.ToString("R", CultureInfo.InvariantCulture));
        foreach (var pair in bones.OrderBy(pair => pair.Key, StringComparer.Ordinal))
        {
            builder.Append('|').Append(pair.Key);
            foreach (var key in pair.Value.OrderBy(key => key.Time))
                builder.Append('|').Append(key.Time.ToString("R", CultureInfo.InvariantCulture))
                    .Append('|').Append(key.Rotation).Append('|').Append(key.Position).Append('|').Append(key.Scale);
        }
        return builder.ToString();
    }
}
