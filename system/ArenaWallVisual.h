#pragma once

#include <algorithm>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <wrl/client.h>

#include "CIndexBuffer.h"
#include "CMaterial.h"
#include "CShader.h"
#include "CStaticMesh.h"
#include "CVertexBuffer.h"
#include "CommonTypes.h"
#include "renderer.h"
#include "stb_image.h"

/**
 * @class ArenaWallVisual
 * @brief 闘技場を囲む石の城壁の見た目。胸壁の付いた石壁のモデルを、壁の多角形に沿って16枚並べる。
 *
 * @details
 * モデルは Poly Haven「Modular Fort 01」(CC0) の厚い石壁(wall_thick_straight_01)。
 * tools/blender_make_arena_wall.py で1枚だけ取り出し、OBJ にしてある。
 *
 * - 衝突は今までどおり GameScene の m_walls(厚さ8の板)が受け持つ。ここは見た目だけ。
 *   壁の内側の面を m_walls の内側の面にそろえ、厚みは外側へ伸ばす(闘技場は狭くならない)。
 * - 16枚を起動時に1つの頂点バッファへ焼き込み、マテリアル(石積み・縁石)ごとに1回ずつ描く。
 * - 照明はシェーダーではなく頂点の色で付ける。ゲームの照明シェーダーだと、光の反対を向いた半周の壁が
 *   ほぼ真っ黒になる(暗い側は明るさ0.15しか残らない)。空の夕日(FlightBackdrop)の向きから光が来るとして、
 *   日の当たる面を明るく、足元を暗くする。影(キャラクターが落とす影)は無照明シェーダーでも受ける。
 * - 遠くの壁で石の目地がちらつかないよう、写真は縮小版(ミップマップ)まで作って使う。
 */
class ArenaWallVisual
{
public:
	/**
	 * @param radius 中心から壁(m_walls のパネルの中心)までの距離。
	 * @param segments 壁の枚数。パネル i は角度 2πi/segments に置く(m_walls と同じ並び)。
	 * @param height 壁の高さ(胸壁の上端まで)。
	 * @param collisionDepth 衝突用の壁の厚み。見た目の内側の面をその内側の面にそろえる。
	 */
	bool Initialize(float radius, int segments, float height, float collisionDepth)
	{
		m_ready = false;
		CStaticMesh mesh;
		mesh.Load(MODEL_PATH, TEXTURE_DIRECTORY, true);
		const std::vector<VERTEX_3D>& sourceVertices = mesh.GetVertices();
		const std::vector<unsigned int>& sourceIndices = mesh.GetIndices();
		if (sourceVertices.empty() || sourceIndices.empty())
		{
			std::cout << "[ArenaWall] model load failed: " << MODEL_PATH << std::endl;
			return false;
		}

		// モデルの大きさ(メートル)。内側の面が z=0、厚みは +z へ伸びる向きに書き出してある。
		Vector3 minimum(1e9f, 1e9f, 1e9f), maximum(-1e9f, -1e9f, -1e9f);
		for (const VERTEX_3D& v : sourceVertices)
		{
			minimum = Vector3::Min(minimum, v.Position);
			maximum = Vector3::Max(maximum, v.Position);
		}
		const Vector3 size = maximum - minimum;
		// 書き出し方を変えて厚みが -z へ伸びていたら、向きを裏返して使う。
		const bool mirrorZ = std::fabs(minimum.z) > std::fabs(maximum.z);
		if (size.y <= 0.0f || size.x <= 0.0f)
			return false;

		const float step = 6.2831853f / static_cast<float>(segments);
		const float innerRadius = radius - collisionDepth * 0.5f;
		// 壁の根元は、外の地面(ArenaOutskirts::OUTSKIRTS_Y = -4)より下から始める。
		// 床(-0.29)から立ち上げると、壁の外側の根元と外の地面との間に隙間が見える。
		const float baseY = -5.0f;
		// 高さを壁の高さに合わせる倍率。厚みにも同じ倍率を使い、石の形を崩さない。
		const float scaleY = (height - baseY) / size.y;
		const float depth = size.z * scaleY;
		// 横は、外側の面の角どうしがつながる幅まで伸ばす(角に隙間ができると、上から見たときに外が見える)。
		// 内側では隣の壁へ少し食い込むが、凸多角形の内側から見ると食い込んだ部分は隣の壁の裏に隠れる。
		const float outerChord = 2.0f * (innerRadius + depth) * std::tan(step * 0.5f);
		const float scaleX = outerChord / size.x;
		m_thickness = depth;

		// 夕日の方向(FlightBackdrop の太陽と同じ。カメラは -Z 側を見ていて、日は -Z 側の低い空にある)。
		Vector3 toSun(0.16f, 0.30f, -0.94f);
		toSun.Normalize();

		std::vector<VERTEX_3D> vertices;
		std::vector<unsigned int> indices;
		vertices.reserve(sourceVertices.size() * segments);
		indices.reserve(sourceIndices.size() * segments);
		m_batches.clear();

		// マテリアル(サブセット)ごとに、16枚分のインデックスを続けて並べる。1回の描画で16枚を描ける。
		const std::vector<SUBSET>& subsets = mesh.GetSubsets();
		std::vector<std::pair<unsigned int, unsigned int>> ranges;   // (元のインデックス開始, 数)と VertexBase
		std::vector<unsigned int> subsetVertexBase;
		if (subsets.empty())
		{
			ranges.push_back({ 0u, static_cast<unsigned int>(sourceIndices.size()) });
			subsetVertexBase.push_back(0u);
		}
		for (const SUBSET& subset : subsets)
		{
			ranges.push_back({ subset.IndexBase, subset.IndexNum });
			subsetVertexBase.push_back(subset.VertexBase);
		}

		// まず全部の頂点を16枚分作る。
		const unsigned int vertexCount = static_cast<unsigned int>(sourceVertices.size());
		for (int i = 0; i < segments; ++i)
		{
			const float angle = step * static_cast<float>(i);
			const Matrix4x4 rotation = Matrix4x4::CreateRotationY(angle);
			const Vector3 inner(std::sin(angle) * innerRadius, baseY, std::cos(angle) * innerRadius);
			for (const VERTEX_3D& source : sourceVertices)
			{
				Vector3 local(
					source.Position.x * scaleX,
					(source.Position.y - minimum.y) * scaleY,
					(mirrorZ ? -source.Position.z : source.Position.z) * scaleY);
				Vector3 normal(
					source.Normal.x / scaleX,
					source.Normal.y / scaleY,
					(mirrorZ ? -source.Normal.z : source.Normal.z) / scaleY);
				if (normal.LengthSquared() > 0.0f)
					normal.Normalize();
				VERTEX_3D v = source;
				v.Position = Vector3::Transform(local, rotation) + inner;
				v.Normal = Vector3::TransformNormal(normal, rotation);
				v.Diffuse = Shade(v.Normal, local.y / (height - baseY), toSun);
				vertices.push_back(v);
			}
		}
		// 次にマテリアルごとに、16枚分のインデックスを並べる。
		for (std::size_t s = 0; s < ranges.size(); ++s)
		{
			Batch batch;
			batch.indexStart = static_cast<unsigned int>(indices.size());
			for (int i = 0; i < segments; ++i)
			{
				const unsigned int base = vertexCount * static_cast<unsigned int>(i) + subsetVertexBase[s];
				for (unsigned int k = 0; k + 2 < ranges[s].second; k += 3)
				{
					const unsigned int a = sourceIndices[ranges[s].first + k];
					const unsigned int b = sourceIndices[ranges[s].first + k + 1];
					const unsigned int c = sourceIndices[ranges[s].first + k + 2];
					// 裏返したときは三角形の回り方も逆にする(表の面が見えるように)。
					indices.push_back(base + a);
					indices.push_back(base + (mirrorZ ? c : b));
					indices.push_back(base + (mirrorZ ? b : c));
				}
			}
			batch.indexCount = static_cast<unsigned int>(indices.size()) - batch.indexStart;
			const std::string materialName = s < subsets.size() ? subsets[s].MtrlName : std::string("wall");
			batch.texture = TextureFor(materialName);
			if (batch.indexCount > 0)
				m_batches.push_back(batch);
		}

		m_vertexBuffer.Create(vertices);
		m_indexBuffer.Create(indices);

		MATERIAL material{};
		material.Ambient = Color(0.0f, 0.0f, 0.0f, 0.0f);
		material.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
		material.Emission = Color(0.0f, 0.0f, 0.0f, 0.0f);
		material.Specular = Color(0.0f, 0.0f, 0.0f, 0.0f);
		material.Shiness = 0.0f;
		material.TextureEnable = TRUE;
		m_material.Create(material);
		m_shader.Create("shader/unlitTextureVS.hlsl", "shader/unlitTexturePS.hlsl");

		m_ready = !m_batches.empty();
		for (const Batch& batch : m_batches)
			m_ready = m_ready && batch.texture != nullptr;
		std::cout << "[ArenaWall] model=" << MODEL_PATH << " panels=" << segments
			<< " tris=" << indices.size() / 3 << " thickness=" << depth << std::endl;
		return m_ready;
	}

	void Draw()
	{
		if (!m_ready)
			return;
		Matrix4x4 world = Matrix4x4::Identity;
		Renderer::SetWorldMatrix(&world);
		ID3D11DeviceContext* context = Renderer::GetDeviceContext();
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_shader.SetGPU();
		m_material.SetGPU();
		m_vertexBuffer.SetGPU();
		m_indexBuffer.SetGPU();
		for (const Batch& batch : m_batches)
		{
			context->PSSetShaderResources(0, 1, &batch.texture);
			context->DrawIndexed(batch.indexCount, batch.indexStart, 0);
		}
	}

	bool IsReady() const { return m_ready; }
	/// 見た目の壁の厚み(内側の面から外へ)。
	float GetThickness() const { return m_thickness; }

private:
	static constexpr const char* MODEL_PATH = "assets/model/ModularFort/arena_wall_thick.obj";
	static constexpr const char* TEXTURE_DIRECTORY = "assets/model/ModularFort/textures";

	struct Batch
	{
		unsigned int indexStart = 0;
		unsigned int indexCount = 0;
		ID3D11ShaderResourceView* texture = nullptr;
	};

	// 頂点の明るさ。日の当たり方・空からの光・足元の暗さを足し合わせる。
	static Color Shade(const Vector3& normal, float heightRatio, const Vector3& toSun)
	{
		const float sun = std::max(normal.Dot(toSun), 0.0f);
		const float sky = 0.5f + 0.5f * std::clamp(normal.y, -1.0f, 1.0f);
		// 地面に近いほど暗い(地面との接地の影)。
		const float ground = 0.62f + 0.38f * std::sqrt(std::clamp(heightRatio, 0.0f, 1.0f));
		const float light = (0.58f + 0.30f * sky + 0.55f * sun) * ground;
		// 夕日の色をわずかに乗せる。
		return Color(light * 1.10f, light * 1.00f, light * 0.90f, 1.0f);
	}

	// マテリアル名(modular_fort_01_wall_1 など)から、色の画像を選ぶ。
	ID3D11ShaderResourceView* TextureFor(const std::string& materialName)
	{
		const char* key = "wall";
		if (materialName.find("trim") != std::string::npos)
			key = "trim";
		else if (materialName.find("plaster") != std::string::npos)
			key = "plaster";
		for (auto& entry : m_textures)
		{
			if (entry.first == key)
				return entry.second.Get();
		}
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
		const std::string path = std::string(TEXTURE_DIRECTORY) + "/modular_fort_01_" + key + "_diff_2k.jpg";
		if (!LoadTextureWithMips(path, view))
			return nullptr;
		m_textures.push_back({ key, view });
		return view.Get();
	}

	static bool LoadTextureWithMips(
		const std::string& path, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& view)
	{
		int width = 0, height = 0, channels = 0;
		unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
		if (pixels == nullptr)
		{
			std::cout << "[ArenaWall] texture load failed: " << path << std::endl;
			return false;
		}
		// GenerateMips を使うため、描画先としても使える指定にする。
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = static_cast<UINT>(width);
		desc.Height = static_cast<UINT>(height);
		desc.MipLevels = 0;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		bool ok = SUCCEEDED(Renderer::GetDevice()->CreateTexture2D(&desc, nullptr, texture.GetAddressOf()));
		if (ok)
		{
			ID3D11DeviceContext* context = Renderer::GetDeviceContext();
			context->UpdateSubresource(texture.Get(), 0, nullptr, pixels, static_cast<UINT>(width) * 4, 0);
			ok = SUCCEEDED(Renderer::GetDevice()->CreateShaderResourceView(
				texture.Get(), nullptr, view.ReleaseAndGetAddressOf()));
			if (ok)
				context->GenerateMips(view.Get());
		}
		stbi_image_free(pixels);
		return ok;
	}

	CVertexBuffer<VERTEX_3D> m_vertexBuffer;
	CIndexBuffer m_indexBuffer;
	CMaterial m_material;
	CShader m_shader;
	std::vector<Batch> m_batches;
	std::vector<std::pair<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>> m_textures;
	float m_thickness = 0.0f;
	bool m_ready = false;
};
