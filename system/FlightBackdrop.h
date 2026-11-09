#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <vector>
#include <wrl/client.h>

#include "CIndexBuffer.h"
#include "CMaterial.h"
#include "CShader.h"
#include "CVertexBuffer.h"
#include "CommonTypes.h"
#include "renderer.h"

/**
 * @class FlightBackdrop
 * @brief ドラゴンが闘技場へ飛んでくる場面の背景。夕焼けの空と、後ろへ流れていく雲。
 *
 * @details
 * 新しい画像は使わず、EnemyMotionEffects と同じ「中心が濃く縁が透明な円盤」を重ねて雲を作る。
 * 空はカメラを中心にした球で、頂点の色だけでグラデーションを付ける。
 *
 * ドラゴンは動かさず、雲の方を後ろへ流す。遠くまで飛ばすと座標が大きくなるうえ、
 * 雲を置き直すだけで「ずっと飛び続けている」ように見せられるためである。
 *
 * 使い方:
 *   Initialize()                            … シーンの初期化時に1度
 *   DrawSky(cameraPosition)                 … 描画の最初に(闘技場の場面でも使う)
 *   Update(dt, speed)                       … 飛んでいる間、毎フレーム
 *   PrepareClouds(view, cameraPosition, rise) … 飛んでいる間、雲を描く前に(頂点を作る)
 *   DrawClouds()                            … ドラゴンより後に
 */
class FlightBackdrop
{
public:
	void Initialize()
	{
		m_vertices.assign(MAX_QUADS * 4, VERTEX_3D{});
		for (auto& v : m_vertices)
		{
			v.Normal = Vector3(0.0f, 1.0f, 0.0f);
			v.TexCoord = Vector2(0.0f, 0.0f);
			v.Diffuse = Color(1.0f, 1.0f, 1.0f, 0.0f);
		}
		m_vertexBuffer.Create(m_vertices);

		std::vector<uint32_t> indices;
		indices.reserve(MAX_QUADS * 6);
		for (std::size_t i = 0; i < MAX_QUADS; ++i)
		{
			const uint32_t base = static_cast<uint32_t>(i * 4);
			indices.push_back(base + 0);
			indices.push_back(base + 1);
			indices.push_back(base + 2);
			indices.push_back(base + 2);
			indices.push_back(base + 1);
			indices.push_back(base + 3);
		}
		m_indexBuffer.Create(indices);

		MATERIAL material{};
		material.Ambient = Color(0.0f, 0.0f, 0.0f, 0.0f);
		material.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
		material.Emission = Color(0.0f, 0.0f, 0.0f, 0.0f);
		material.Specular = Color(0.0f, 0.0f, 0.0f, 0.0f);
		material.Shiness = 0.0f;
		material.TextureEnable = FALSE;
		m_material.Create(material);

		m_shader.Create("shader/unlitTextureVS.hlsl", "shader/unlitTexturePS.hlsl");

		// 雲は「比べるが書き込まない」。ドラゴンの向こう側の雲は体に隠れ、手前の雲は体に掛かる。
		D3D11_DEPTH_STENCIL_DESC depthDesc{};
		depthDesc.DepthEnable = TRUE;
		depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		depthDesc.StencilEnable = FALSE;
		m_depthTestNoWrite.Reset();
		Renderer::GetDevice()->CreateDepthStencilState(&depthDesc, m_depthTestNoWrite.GetAddressOf());

		// 空と太陽は動かないので最初に1度だけ作り、描くときにカメラの位置へ動かす。
		m_skyQuads = PushSun(PushSky(0));
		m_vertexBuffer.Modify(m_vertices);

		SpawnClouds();
		m_initialized = true;
	}

	/// @param speed ドラゴンが飛ぶ速さ。雲はこの速さで後ろ(-Z)へ流れる。
	void Update(float deltaSeconds, float speed)
	{
		m_scroll += speed * std::max(deltaSeconds, 0.0f);
	}

	/**
	 * @brief 雲の頂点を作って送る。
	 * @param rise 雲を持ち上げる量。下へ突っ込む場面で、雲海が下から迫ってくるのに使う。
	 */
	void PrepareClouds(const Matrix4x4& viewMatrix, const Vector3& cameraPosition, float rise)
	{
		if (!m_initialized)
			return;
		const std::size_t quad = PushClouds(m_skyQuads, viewMatrix, cameraPosition, rise);
		m_cloudQuads = quad - m_skyQuads;
		for (std::size_t i = quad; i < m_preparedQuads; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				m_vertices[i * 4 + k].Position = Vector3(0.0f, 0.0f, 0.0f);
				m_vertices[i * 4 + k].Diffuse = Color(0.0f, 0.0f, 0.0f, 0.0f);
			}
		}
		m_preparedQuads = quad;
		m_vertexBuffer.Modify(m_vertices);
	}

	void DrawSky(const Vector3& cameraPosition)
	{
		if (!m_initialized || m_skyQuads == 0)
			return;
		BeginDraw(Matrix4x4::CreateTranslation(cameraPosition));
		Renderer::SetDepthEnable(false);
		Renderer::GetDeviceContext()->DrawIndexed(static_cast<unsigned int>(m_skyQuads * 6), 0, 0);
		Renderer::SetDepthEnable(true);
		Renderer::DisableCulling(true);
	}

	void DrawClouds()
	{
		if (!m_initialized || m_cloudQuads == 0)
			return;
		BeginDraw(Matrix4x4::Identity);
		ID3D11DeviceContext* context = Renderer::GetDeviceContext();
		if (m_depthTestNoWrite)
			context->OMSetDepthStencilState(m_depthTestNoWrite.Get(), 0);
		else
			Renderer::SetDepthEnable(false);
		context->DrawIndexed(
			static_cast<unsigned int>(m_cloudQuads * 6),
			static_cast<unsigned int>(m_skyQuads * 6), 0);
		Renderer::SetDepthEnable(true);
		Renderer::DisableCulling(true);
	}

private:
	struct Cloud
	{
		Vector3 position{};
		float radius = 100.0f;
		float alpha = 0.5f;
		float light = 1.0f;   ///< 1で日の当たる明るい色、0で影の暗い色
		bool flat = false;    ///< 地面と平行に寝かせる(雲海)か、カメラへ向ける(もくもくした雲)か
	};

	void BeginDraw(Matrix4x4 world)
	{
		m_material.SetDiffuse(Color(1.0f, 1.0f, 1.0f, 1.0f));
		m_material.Update();
		m_shader.SetGPU();
		m_material.SetGPU();
		m_vertexBuffer.SetGPU();
		m_indexBuffer.SetGPU();
		Renderer::SetWorldMatrix(&world);
		Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Renderer::DisableCulling(false);
		Renderer::SetBlendState(BS_ALPHABLEND);
	}

	// 毎回同じ並びになる簡単な乱数(EnemyMotionEffects と同じ)。撮影で同じ絵を再現できるようにする。
	float Random01()
	{
		m_random ^= m_random << 13;
		m_random ^= m_random >> 17;
		m_random ^= m_random << 5;
		return static_cast<float>(m_random & 0xFFFFFF) / static_cast<float>(0x1000000);
	}
	float RandomRange(float low, float high) { return low + (high - low) * Random01(); }

	void SpawnClouds()
	{
		m_clouds.clear();
		m_clouds.reserve(MAX_CLOUDS);
		// 雲海。低いところに敷き詰める。高度と速さの感じはここで決まる。
		for (int i = 0; i < 90; ++i)
		{
			Cloud cloud;
			cloud.position = Vector3(RandomRange(-1700.0f, 1700.0f), RandomRange(-320.0f, -280.0f), RandomRange(0.0f, CLOUD_LOOP_LENGTH));
			cloud.radius = RandomRange(170.0f, 360.0f);
			cloud.alpha = RandomRange(0.55f, 0.85f);
			cloud.light = RandomRange(0.55f, 0.95f);
			cloud.flat = true;
			m_clouds.push_back(cloud);
		}
		// 雲海から盛り上がる雲。下の方を暗く、上の方を明るくして立体に見せる。
		for (int i = 0; i < 28; ++i)
		{
			const Vector3 base(RandomRange(-1300.0f, 1300.0f), -270.0f, RandomRange(0.0f, CLOUD_LOOP_LENGTH));
			for (int k = 0; k < 5; ++k)
			{
				const float height = static_cast<float>(k) / 4.0f;
				Cloud cloud;
				cloud.position = base + Vector3(
					RandomRange(-90.0f, 90.0f),
					height * RandomRange(80.0f, 170.0f),
					RandomRange(-70.0f, 70.0f));
				cloud.radius = RandomRange(70.0f, 150.0f) * (1.0f - 0.35f * height);
				cloud.alpha = RandomRange(0.65f, 0.9f);
				cloud.light = 0.35f + 0.65f * height;
				m_clouds.push_back(cloud);
			}
		}
		// ドラゴンのすぐ近くを流れる薄い雲。近くのものが速く流れることで、速さが伝わる。
		for (int i = 0; i < 22; ++i)
		{
			Cloud cloud;
			float x = RandomRange(-360.0f, 360.0f);
			// 体に重なって隠さないよう、ドラゴンのすぐ横には置かない。
			x += x >= 0.0f ? 70.0f : -70.0f;
			cloud.position = Vector3(x, RandomRange(-110.0f, 90.0f), RandomRange(0.0f, CLOUD_LOOP_LENGTH));
			cloud.radius = RandomRange(35.0f, 80.0f);
			cloud.alpha = RandomRange(0.22f, 0.42f);
			cloud.light = RandomRange(0.75f, 1.0f);
			m_clouds.push_back(cloud);
		}
	}

	// 緯度ごとの色を決めた球を描く(内側から見る)。
	std::size_t PushSky(std::size_t quad)
	{
		struct Band { float elevation; Color color; };
		// 地平線が一番明るい。下は雲海の色へ、上は夜に近い青へつなぐ。
		// 闘技場の壁が茶色なので、青空より夕焼けの方が次の場面へなじむ。
		static const Band bands[] = {
			{ -1.57f, Color(0.42f, 0.34f, 0.40f, 1.0f) },
			{ -0.35f, Color(0.70f, 0.54f, 0.50f, 1.0f) },
			{ -0.03f, Color(1.00f, 0.78f, 0.52f, 1.0f) },
			{  0.14f, Color(0.98f, 0.66f, 0.46f, 1.0f) },
			{  0.40f, Color(0.66f, 0.48f, 0.56f, 1.0f) },
			{  0.85f, Color(0.27f, 0.28f, 0.48f, 1.0f) },
			{  1.57f, Color(0.11f, 0.14f, 0.32f, 1.0f) },
		};
		constexpr std::size_t BAND_COUNT = sizeof(bands) / sizeof(bands[0]);
		const auto pointOnSky = [&](float azimuth, float elevation)
		{
			const float flat = std::cos(elevation);
			return Vector3(
				std::cos(azimuth) * flat, std::sin(elevation), std::sin(azimuth) * flat) * SKY_RADIUS;
		};
		for (std::size_t band = 0; band + 1 < BAND_COUNT; ++band)
		{
			for (std::size_t s = 0; s < SKY_SEGMENTS; ++s)
			{
				const float a0 = TWO_PI * static_cast<float>(s) / static_cast<float>(SKY_SEGMENTS);
				const float a1 = TWO_PI * static_cast<float>(s + 1) / static_cast<float>(SKY_SEGMENTS);
				const std::size_t v = quad * 4;
				m_vertices[v + 0].Position = pointOnSky(a0, bands[band].elevation);
				m_vertices[v + 0].Diffuse = bands[band].color;
				m_vertices[v + 1].Position = pointOnSky(a1, bands[band].elevation);
				m_vertices[v + 1].Diffuse = bands[band].color;
				m_vertices[v + 2].Position = pointOnSky(a0, bands[band + 1].elevation);
				m_vertices[v + 2].Diffuse = bands[band + 1].color;
				m_vertices[v + 3].Position = pointOnSky(a1, bands[band + 1].elevation);
				m_vertices[v + 3].Diffuse = bands[band + 1].color;
				++quad;
			}
		}
		return quad;
	}

	std::size_t PushSun(std::size_t quad)
	{
		// ドラゴンは+Zへ飛び、カメラはその進む先から-Z側を見ている。
		// カメラの見る方向の地平線の少し上に日を置き、夕日を背に飛んでくる絵にする。
		// 闘技場の登場演出のカメラも同じ-Z側を見ているので、次の場面でも空の向きがそろう。
		Vector3 direction(0.16f, 0.05f, -0.98f);
		direction.Normalize();
		Vector3 right = Vector3(0.0f, 1.0f, 0.0f).Cross(direction);
		right.Normalize();
		const Vector3 up = direction.Cross(right);
		const Vector3 center = direction * (SKY_RADIUS * 0.95f);
		quad = PushDisc(quad, center, 2600.0f, Color(1.0f, 0.72f, 0.42f, 0.55f), right, up);
		quad = PushDisc(quad, center, 420.0f, Color(1.0f, 0.95f, 0.78f, 0.95f), right, up);
		return quad;
	}

	std::size_t PushClouds(
		std::size_t quad, const Matrix4x4& viewMatrix, const Vector3& cameraPosition, float rise)
	{
		const Vector3 right(viewMatrix._11, viewMatrix._21, viewMatrix._31);
		const Vector3 up(viewMatrix._12, viewMatrix._22, viewMatrix._32);

		// 雲は半透明なので、遠いものから順に描く。
		m_frameClouds.clear();
		for (const Cloud& cloud : m_clouds)
		{
			float z = std::fmod(cloud.position.z - m_scroll, CLOUD_LOOP_LENGTH);
			if (z < 0.0f)
				z += CLOUD_LOOP_LENGTH;
			z += CLOUD_LOOP_START;
			const Vector3 position(cloud.position.x, cloud.position.y + rise, z);
			FrameCloud frame;
			frame.cloud = &cloud;
			frame.position = position;
			frame.distanceSquared = (position - cameraPosition).LengthSquared();
			// 列の端で置き直す瞬間が見えないよう、両端で薄くする。
			const float fromStart = (z - CLOUD_LOOP_START) / 900.0f;
			const float fromEnd = (CLOUD_LOOP_START + CLOUD_LOOP_LENGTH - z) / 200.0f;
			frame.fade = std::clamp(std::min(fromStart, fromEnd), 0.0f, 1.0f);
			m_frameClouds.push_back(frame);
		}
		std::sort(m_frameClouds.begin(), m_frameClouds.end(),
			[](const FrameCloud& a, const FrameCloud& b) { return a.distanceSquared > b.distanceSquared; });

		const Vector3 groundAxisA(1.0f, 0.0f, 0.0f);
		const Vector3 groundAxisB(0.0f, 0.0f, 1.0f);
		for (const FrameCloud& frame : m_frameClouds)
		{
			const Cloud& cloud = *frame.cloud;
			const float distance = std::sqrt(frame.distanceSquared);
			// カメラのすぐ前で巨大な円盤が突然現れて切れないよう、近づいたら薄くする。
			const float nearFade = std::clamp((distance - cloud.radius * 0.6f) / 90.0f, 0.0f, 1.0f);
			const float alpha = cloud.alpha * frame.fade * nearFade;
			const float light = cloud.light;
			const Color color(
				0.56f + (1.00f - 0.56f) * light,
				0.42f + (0.86f - 0.42f) * light,
				0.48f + (0.72f - 0.48f) * light,
				alpha);
			quad = PushDisc(quad, frame.position, cloud.radius, color,
				cloud.flat ? groundAxisA : right, cloud.flat ? groundAxisB : up);
		}
		return quad;
	}

	std::size_t PushDisc(
		std::size_t quad,
		const Vector3& center,
		float radius,
		const Color& centerColor,
		const Vector3& axisA,
		const Vector3& axisB)
	{
		if (quad + DISC_SEGMENTS > MAX_QUADS || centerColor.w <= 0.002f)
			return quad;
		Color edgeColor = centerColor;
		edgeColor.w = 0.0f;
		for (std::size_t s = 0; s < DISC_SEGMENTS; ++s)
		{
			const float a0 = TWO_PI * static_cast<float>(s) / static_cast<float>(DISC_SEGMENTS);
			const float a1 = TWO_PI * static_cast<float>(s + 1) / static_cast<float>(DISC_SEGMENTS);
			const std::size_t v = quad * 4;
			m_vertices[v + 0].Position = center;
			m_vertices[v + 0].Diffuse = centerColor;
			m_vertices[v + 1].Position = center;
			m_vertices[v + 1].Diffuse = centerColor;
			m_vertices[v + 2].Position = center + (axisA * std::cos(a0) + axisB * std::sin(a0)) * radius;
			m_vertices[v + 2].Diffuse = edgeColor;
			m_vertices[v + 3].Position = center + (axisA * std::cos(a1) + axisB * std::sin(a1)) * radius;
			m_vertices[v + 3].Diffuse = edgeColor;
			++quad;
		}
		return quad;
	}

	struct FrameCloud
	{
		const Cloud* cloud = nullptr;
		Vector3 position{};
		float distanceSquared = 0.0f;
		float fade = 1.0f;
	};

	static constexpr float TWO_PI = 6.2831853f;
	static constexpr float SKY_RADIUS = 6000.0f;
	static constexpr std::size_t SKY_SEGMENTS = 24;
	static constexpr std::size_t DISC_SEGMENTS = 10;
	static constexpr std::size_t MAX_CLOUDS = 90 + 28 * 5 + 22;
	static constexpr std::size_t MAX_QUADS =
		SKY_SEGMENTS * 6 + DISC_SEGMENTS * (MAX_CLOUDS + 2);
	// 雲を流す範囲(Z)。カメラの少し後ろ(+Z側)で生まれ、ドラゴンの向こうのはるか遠くへ流れていく。
	static constexpr float CLOUD_LOOP_START = -3000.0f;
	static constexpr float CLOUD_LOOP_LENGTH = 4200.0f;

	std::vector<Cloud> m_clouds;
	std::vector<FrameCloud> m_frameClouds;
	std::vector<VERTEX_3D> m_vertices;
	CVertexBuffer<VERTEX_3D> m_vertexBuffer;
	CIndexBuffer m_indexBuffer;
	CMaterial m_material;
	CShader m_shader;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthTestNoWrite;
	std::size_t m_skyQuads = 0;
	std::size_t m_cloudQuads = 0;
	std::size_t m_preparedQuads = 0;
	float m_scroll = 0.0f;
	uint32_t m_random = 0x51ED27F3u;
	bool m_initialized = false;
};
