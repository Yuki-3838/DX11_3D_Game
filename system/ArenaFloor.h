#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <wrl/client.h>

#include "CIndexBuffer.h"
#include "CMaterial.h"
#include "CShader.h"
#include "CVertexBuffer.h"
#include "CommonTypes.h"
#include "renderer.h"
#include "stb_image.h"

/**
 * @class ArenaFloor
 * @brief 闘技場の床。踏み固めた砂の地面に、距離を数えるための目盛りの線を薄く重ねる。
 *
 * @details
 * 一色の床では、敵との距離や回避で動いた量が画面から読めない(試遊での指摘)。
 * そのため床へ目盛り(マス目)を付けている。
 *
 * - 1マスは`TILE_SIZE`(10単位。身長約18単位のキャラクターで約1m)。
 * - `TILES_PER_MAJOR`マスごとに太い線を引く(約4m)。
 *   回避(34単位)はおよそ3.4マス、と数えられる。
 *
 * 見た目は3通りから選べる(デバッグ表示の「撮影・調整用」、または dev_settings.ini の floor=)。
 * - SandWithLines … 砂の写真(Poly Haven「Sand 01」、CC0)に、目盛りの線を薄く重ねる(既定)。
 * - Sand          … 砂の写真だけ。
 * - Grid          … 以前のマス目の床(市松模様と線。色は Palette で選ぶ)。
 *
 * 砂と線は、繰り返す長さが違う(砂は SAND_WORLD_SIZE、線は TILES_PER_MAJOR マス)ので、
 * 同じ形の床を2回描く。1回目に砂、2回目に線だけの半透明の画像を重ねる(深さが同じなので上に乗る)。
 * マス目と線の画像はファイルを使わず、起動時にピクセルを計算して作る。
 * シェーダーは照明付きのものをそのまま使う(キャラクターの影も今までどおり床に落ちる)。
 */
class ArenaFloor
{
public:
	// 色の組み合わせ。照明が掛かる前の色。
	struct Palette
	{
		Color tileA;     // 市松の明るい方
		Color tileB;     // 市松の暗い方
		Color minorLine; // 1マスごとの線
		Color majorLine; // TILES_PER_MAJORマスごとの太い線
	};

	enum class Surface
	{
		SandWithLines,
		Sand,
		Grid,
	};

	// 闘技場の石の色に合わせた控えめな目盛り。
	static Palette StonePalette()
	{
		return {
			Color(0.25f, 0.21f, 0.17f, 1.0f),
			Color(0.21f, 0.175f, 0.14f, 1.0f),
			Color(0.31f, 0.26f, 0.21f, 1.0f),
			Color(0.40f, 0.34f, 0.27f, 1.0f) };
	}

	// ストリートファイター6のトレーニングステージ風。灰色の床に明るい線。
	static Palette TrainingPalette()
	{
		return {
			Color(0.42f, 0.43f, 0.46f, 1.0f),
			Color(0.36f, 0.37f, 0.40f, 1.0f),
			Color(0.52f, 0.53f, 0.57f, 1.0f),
			Color(0.70f, 0.71f, 0.76f, 1.0f) };
	}

	/**
	 * @param wallRadius 壁(中心から壁パネルの中心まで)の距離。床は壁の多角形に合わせて切る。
	 * @param wallSegments 壁の枚数(闘技場の多角形の角の数)。
	 * @param wallDepth 壁の厚み。床は壁の下まで敷き、壁の外側へははみ出さない。
	 * @param surfaceY 床の上面の高さ。
	 *
	 * 以前は一辺が直径の正方形で、四隅が壁の外へはみ出していた。
	 * カメラが壁より上へ出る場面(敵の登場)で、壁の外にマス目の角が見えていたので、壁の多角形に合わせる。
	 */
	void Initialize(float wallRadius, int wallSegments, float wallDepth, float surfaceY, const Palette& palette)
	{
		// マス目の画像1枚がTILES_PER_MAJORマス分。UVはワールド座標から決め、マス目を原点にそろえる
		// (闘技場の中心がマスの角になり、中心からの距離を数えやすい)。
		const float gridUvScale = 1.0f / (TILE_SIZE * static_cast<float>(TILES_PER_MAJOR));
		const float sandUvScale = 1.0f / SAND_WORLD_SIZE;
		const int segments = std::max(wallSegments, 3);
		const float step = 6.2831853f / static_cast<float>(segments);
		// 壁パネルの中心は角度 i*step にあるので、多角形の角は (i+0.5)*step。
		// 壁の外側の面までの距離を角の位置へ直す。
		const float cornerRadius = (wallRadius + wallDepth * 0.5f) / std::cos(step * 0.5f);

		std::vector<Vector3> positions;
		positions.reserve(static_cast<std::size_t>(segments) + 1);
		positions.push_back(Vector3(0.0f, surfaceY, 0.0f));
		for (int i = 0; i < segments; ++i)
		{
			const float angle = step * (static_cast<float>(i) + 0.5f);
			positions.push_back(Vector3(
				std::sin(angle) * cornerRadius, surfaceY, std::cos(angle) * cornerRadius));
		}
		const auto makeVertices = [&](float uvScale) {
			std::vector<VERTEX_3D> vertices;
			vertices.reserve(positions.size());
			for (const Vector3& position : positions)
			{
				VERTEX_3D v{};
				v.Position = position;
				v.Normal = Vector3(0.0f, 1.0f, 0.0f);
				v.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
				v.TexCoord = Vector2(position.x * uvScale, -position.z * uvScale);
				vertices.push_back(v);
			}
			return vertices;
		};
		m_gridVertexBuffer.Create(makeVertices(gridUvScale));
		m_sandVertexBuffer.Create(makeVertices(sandUvScale));

		// 中心から扇形に分ける。角度は+Zから+Xへ増えるので、中心 → i → i+1 が上から見て時計回り
		// (このプロジェクトの表面の向き)。
		std::vector<unsigned int> indices;
		indices.reserve(static_cast<std::size_t>(segments) * 3);
		for (int i = 0; i < segments; ++i)
		{
			indices.push_back(0);
			indices.push_back(static_cast<unsigned int>(1 + i));
			indices.push_back(static_cast<unsigned int>(1 + (i + 1) % segments));
		}
		m_indexCount = static_cast<unsigned int>(indices.size());
		m_indexBuffer.Create(indices);

		MATERIAL material{};
		material.Ambient = Color(0.0f, 0.0f, 0.0f, 0.0f);
		material.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
		material.Emission = Color(0.0f, 0.0f, 0.0f, 0.0f);
		material.Specular = Color(0.0f, 0.0f, 0.0f, 0.0f);
		material.Shiness = 0.0f;
		material.TextureEnable = TRUE;
		m_material.Create(material);

		// 以前の床(BoxDrawer)と同じ照明付きシェーダー。明るさの付き方を変えないため。
		m_shader.Create("shader/vertexLightingVS.hlsl", "shader/vertexLightingPS.hlsl");

		// 砂の上に重ねる線。濃い土の色を半透明で引く(白い線だと明るい砂の上で見えにくい)。
		// 線の無いところは同じ色の透明にして、縮小版を作ったときに線の縁が白っぽくにじまないようにする。
		const Color lineColor(0.16f, 0.11f, 0.07f, 0.0f);
		CreatePatternTexture({
			lineColor,
			lineColor,
			Color(lineColor.x, lineColor.y, lineColor.z, 0.22f),
			Color(lineColor.x, lineColor.y, lineColor.z, 0.40f) }, m_lineTexture);
		LoadTextureWithMips(SAND_TEXTURE_PATH, m_sandTexture);
		if (!m_sandTexture)
			m_surface = Surface::Grid;

		SetPalette(palette);
		m_initialized = true;
	}

	/// マス目の床(Surface::Grid)の色を変える(テクスチャを作り直す)。見た目の切り替えは SetSurface で行う。
	void SetPalette(const Palette& palette)
	{
		m_palette = palette;
		CreatePatternTexture(m_palette, m_gridTexture);
	}

	const Palette& GetPalette() const { return m_palette; }

	/// 床の見た目を切り替える。砂の写真が読めなかったときは、マス目の床のままにする。
	void SetSurface(Surface surface)
	{
		m_surface = (surface != Surface::Grid && !m_sandTexture) ? Surface::Grid : surface;
	}

	Surface GetSurface() const { return m_surface; }

	void Draw()
	{
		if (!m_initialized)
			return;

		Matrix4x4 world = Matrix4x4::Identity;
		Renderer::SetWorldMatrix(&world);

		ID3D11DeviceContext* context = Renderer::GetDeviceContext();
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_shader.SetGPU();
		m_material.SetGPU();
		m_indexBuffer.SetGPU();

		if (m_surface == Surface::Grid)
		{
			if (!m_gridTexture)
				return;
			m_gridVertexBuffer.SetGPU();
			context->PSSetShaderResources(0, 1, m_gridTexture.GetAddressOf());
			context->DrawIndexed(m_indexCount, 0, 0);
			return;
		}

		m_sandVertexBuffer.SetGPU();
		context->PSSetShaderResources(0, 1, m_sandTexture.GetAddressOf());
		context->DrawIndexed(m_indexCount, 0, 0);

		if (m_surface == Surface::SandWithLines && m_lineTexture)
		{
			// 同じ形をもう1回描いて、線だけを半透明で重ねる。深さの比較は「以下」なので、同じ深さの面は上に描ける。
			Renderer::SetBlendState(BS_ALPHABLEND);
			m_gridVertexBuffer.SetGPU();
			context->PSSetShaderResources(0, 1, m_lineTexture.GetAddressOf());
			context->DrawIndexed(m_indexCount, 0, 0);
		}
	}

	static constexpr float TILE_SIZE = 10.0f;
	static constexpr int TILES_PER_MAJOR = 4;

private:
	static constexpr const char* SAND_TEXTURE_PATH = "assets/texture/sand_01_diff_2k.jpg";
	// 砂の写真1枚が表すワールドの長さ。元の写真は約1.5m四方(約15単位)だが、そのままでは
	// 同じ模様が細かくくり返して目立つので、粒を少し大きくして広く貼る。
	static constexpr float SAND_WORLD_SIZE = 45.0f;

	// 1マスのピクセル数。線の太さはこれに対する割合で決める。
	static constexpr int TILE_PIXELS = 128;
	static constexpr int MINOR_LINE_PIXELS = 3;
	static constexpr int MAJOR_LINE_PIXELS = 6;

	static void CreatePatternTexture(
		const Palette& palette, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& view)
	{
		constexpr int size = TILE_PIXELS * TILES_PER_MAJOR;
		std::vector<uint32_t> pixels(static_cast<std::size_t>(size) * size);
		const auto pack = [](const Color& c) {
			const auto channel = [](float v) {
				return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
			};
			return channel(c.x) | (channel(c.y) << 8) | (channel(c.z) << 16) | (channel(c.w) << 24);
		};
		const uint32_t tileA = pack(palette.tileA);
		const uint32_t tileB = pack(palette.tileB);
		const uint32_t minorLine = pack(palette.minorLine);
		const uint32_t majorLine = pack(palette.majorLine);

		// 線はマスの境目をまたいで両側へ半分ずつ引く。
		// テクスチャの端(=太い線)も、くり返したときに反対側の端とつながって同じ太さになる。
		const auto distanceToEdge = [](int p, int period) {
			const int m = p % period;
			return std::min(m, period - 1 - m);
		};
		for (int y = 0; y < size; ++y)
		{
			for (int x = 0; x < size; ++x)
			{
				uint32_t color = ((x / TILE_PIXELS + y / TILE_PIXELS) % 2 == 0) ? tileA : tileB;
				const int toMinor = std::min(distanceToEdge(x, TILE_PIXELS), distanceToEdge(y, TILE_PIXELS));
				const int toMajor = std::min(distanceToEdge(x, size), distanceToEdge(y, size));
				if (toMajor < MAJOR_LINE_PIXELS / 2)
					color = majorLine;
				else if (toMinor < MINOR_LINE_PIXELS / 2 + 1)
					color = minorLine;
				pixels[static_cast<std::size_t>(y) * size + x] = color;
			}
		}
		CreateTextureWithMips(pixels.data(), size, size, view);
	}

	static bool LoadTextureWithMips(
		const std::string& path, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& view)
	{
		int width = 0, height = 0, channels = 0;
		unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
		if (pixels == nullptr)
		{
			std::cout << "[ArenaFloor] texture load failed: " << path << std::endl;
			return false;
		}
		const bool ok = CreateTextureWithMips(pixels, width, height, view);
		stbi_image_free(pixels);
		return ok;
	}

	// 遠くの床で模様がちらつかないよう、縮小版(ミップマップ)まで作る。
	// GenerateMipsを使うため、描画先としても使える指定にする。
	static bool CreateTextureWithMips(
		const void* rgbaPixels, int width, int height, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& view)
	{
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
		view.Reset();
		if (FAILED(Renderer::GetDevice()->CreateTexture2D(&desc, nullptr, texture.GetAddressOf())))
			return false;
		ID3D11DeviceContext* context = Renderer::GetDeviceContext();
		context->UpdateSubresource(texture.Get(), 0, nullptr, rgbaPixels, static_cast<UINT>(width) * 4, 0);
		if (FAILED(Renderer::GetDevice()->CreateShaderResourceView(texture.Get(), nullptr, view.GetAddressOf())))
			return false;
		context->GenerateMips(view.Get());
		return true;
	}

	CVertexBuffer<VERTEX_3D> m_gridVertexBuffer;   // UV: TILES_PER_MAJOR マスで1枚(マス目・線)
	CVertexBuffer<VERTEX_3D> m_sandVertexBuffer;   // UV: SAND_WORLD_SIZE で1枚(砂)
	CIndexBuffer m_indexBuffer;
	CMaterial m_material;
	CShader m_shader;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_gridTexture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_lineTexture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_sandTexture;
	Palette m_palette{};
	Surface m_surface = Surface::SandWithLines;
	unsigned int m_indexCount = 0;
	bool m_initialized = false;
};
