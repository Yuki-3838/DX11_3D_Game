#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>
#include <wrl/client.h>

#include "CIndexBuffer.h"
#include "CMaterial.h"
#include "CShader.h"
#include "CVertexBuffer.h"
#include "CommonTypes.h"
#include "renderer.h"

/**
 * @class ArenaFloor
 * @brief 闘技場の床。マス目(市松模様と目盛りの線)を描き、距離感をつかみやすくする。
 *
 * @details
 * 一色の床では、敵との距離や回避で動いた量が画面から読めない(試遊での指摘)。
 * ストリートファイター6のトレーニングステージのように床へ目盛りを付ける。
 *
 * - 1マスは`TILE_SIZE`(10単位。身長約18単位のキャラクターで約1m)。
 * - `TILES_PER_MAJOR`マスごとに太い線を引く(約4m)。
 *   回避(34単位)はおよそ3.4マス、と数えられる。
 *
 * 模様の画像はファイルを使わず、起動時にピクセルを計算して作る。
 * 色を変えるときに画像を描き直さずに済むため。
 * シェーダーは既存の照明付きシェーダーをそのまま使う(キャラクターの影も今までどおり床に落ちる)。
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
	 * @param size 床の一辺の長さ(正方形。中心は原点)。
	 * @param surfaceY 床の上面の高さ。
	 */
	void Initialize(float size, float surfaceY, const Palette& palette)
	{
		const float half = size * 0.5f;
		// テクスチャ1枚がTILES_PER_MAJORマス分。UVはワールド座標から決め、マス目を原点にそろえる
		// (闘技場の中心がマスの角になり、中心からの距離を数えやすい)。
		const float uvScale = 1.0f / (TILE_SIZE * static_cast<float>(TILES_PER_MAJOR));
		std::vector<VERTEX_3D> vertices(4);
		const Vector3 corners[4] = {
			Vector3(-half, surfaceY,  half),
			Vector3( half, surfaceY,  half),
			Vector3(-half, surfaceY, -half),
			Vector3( half, surfaceY, -half) };
		for (int i = 0; i < 4; ++i)
		{
			vertices[i].Position = corners[i];
			vertices[i].Normal = Vector3(0.0f, 1.0f, 0.0f);
			vertices[i].Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
			vertices[i].TexCoord = Vector2(corners[i].x * uvScale, -corners[i].z * uvScale);
		}
		m_vertexBuffer.Create(vertices);
		// 上から見て時計回り(このプロジェクトの表面の向き)。
		m_indexBuffer.Create({ 0, 1, 2, 2, 1, 3 });

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

		SetPalette(palette);
		m_initialized = true;
	}

	// 模様の色を変える(テクスチャを作り直す)。
	void SetPalette(const Palette& palette)
	{
		m_palette = palette;
		CreatePatternTexture();
	}

	const Palette& GetPalette() const { return m_palette; }

	void Draw()
	{
		if (!m_initialized || !m_textureView)
			return;

		Matrix4x4 world = Matrix4x4::Identity;
		Renderer::SetWorldMatrix(&world);

		ID3D11DeviceContext* context = Renderer::GetDeviceContext();
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_shader.SetGPU();
		m_material.SetGPU();
		m_vertexBuffer.SetGPU();
		m_indexBuffer.SetGPU();
		context->PSSetShaderResources(0, 1, m_textureView.GetAddressOf());
		context->DrawIndexed(6, 0, 0);
	}

	static constexpr float TILE_SIZE = 10.0f;
	static constexpr int TILES_PER_MAJOR = 4;

private:
	// 1マスのピクセル数。線の太さはこれに対する割合で決める。
	static constexpr int TILE_PIXELS = 128;
	static constexpr int MINOR_LINE_PIXELS = 3;
	static constexpr int MAJOR_LINE_PIXELS = 6;

	void CreatePatternTexture()
	{
		constexpr int size = TILE_PIXELS * TILES_PER_MAJOR;
		std::vector<uint32_t> pixels(static_cast<std::size_t>(size) * size);
		const auto pack = [](const Color& c) {
			const auto channel = [](float v) {
				return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
			};
			return channel(c.x) | (channel(c.y) << 8) | (channel(c.z) << 16) | (channel(c.w) << 24);
		};
		const uint32_t tileA = pack(m_palette.tileA);
		const uint32_t tileB = pack(m_palette.tileB);
		const uint32_t minorLine = pack(m_palette.minorLine);
		const uint32_t majorLine = pack(m_palette.majorLine);

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

		// 遠くの床で線がちらつかないよう、縮小版(ミップマップ)まで作る。
		// GenerateMipsを使うため、描画先としても使える指定にする。
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = size;
		desc.Height = size;
		desc.MipLevels = 0;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		m_textureView.Reset();
		if (FAILED(Renderer::GetDevice()->CreateTexture2D(&desc, nullptr, texture.GetAddressOf())))
			return;
		ID3D11DeviceContext* context = Renderer::GetDeviceContext();
		context->UpdateSubresource(texture.Get(), 0, nullptr, pixels.data(), size * 4, 0);
		if (FAILED(Renderer::GetDevice()->CreateShaderResourceView(texture.Get(), nullptr, m_textureView.GetAddressOf())))
			return;
		context->GenerateMips(m_textureView.Get());
	}

	CVertexBuffer<VERTEX_3D> m_vertexBuffer;
	CIndexBuffer m_indexBuffer;
	CMaterial m_material;
	CShader m_shader;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureView;
	Palette m_palette{};
	bool m_initialized = false;
};
