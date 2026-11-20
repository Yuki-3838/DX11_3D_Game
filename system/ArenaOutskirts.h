#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "CIndexBuffer.h"
#include "CMaterial.h"
#include "CShader.h"
#include "CVertexBuffer.h"
#include "CommonTypes.h"
#include "renderer.h"

/**
 * @class ArenaOutskirts
 * @brief 闘技場の外に広がる地面。遠くほど夕焼けの空の色へ溶けていく、模様の無い大きな円盤。
 *
 * @details
 * 以前は一辺2000の正方形に草の画像を10回くり返して貼っていた(field)。次の2つの問題があった。
 * - 闘技場の床(-0.29)と草の地面(-0.3)の高さの差が0.01しかなく、遠くでは深度の精度が足りずに
 *   草が床を突き抜けてちらついた。
 * - 敵が飛んでくる場面ではカメラが壁(高さ120)より上へ出るので、明るい緑の草と、
 *   その向こうの地面の端(空との境目)がそのまま見えていた。
 *
 * ここでは画像を使わず、頂点の色だけで「近くは土の色、遠くは空の地平線の色」に変える。
 * 端の色を空(FlightBackdrop)の地平線の色とそろえるので、地面の終わりが見えない。
 * 高さは床より十分下(OUTSKIRTS_Y)に置き、闘技場の内側では床に隠れる。
 */
class ArenaOutskirts
{
public:
	void Initialize()
	{
		// 輪の半径。近くは細かく、遠くは粗く分ける(色の変わり方は近くの方が大きい)。
		static const float ringRadii[] = {
			0.0f, 380.0f, 520.0f, 700.0f, 950.0f, 1300.0f, 1800.0f, 2500.0f, 3400.0f, 4500.0f, OUTER_RADIUS };
		constexpr std::size_t RING_COUNT = sizeof(ringRadii) / sizeof(ringRadii[0]);

		std::vector<VERTEX_3D> vertices;
		vertices.reserve(RING_COUNT * SEGMENTS);
		for (std::size_t ring = 0; ring < RING_COUNT; ++ring)
		{
			const float radius = ringRadii[ring];
			for (std::size_t s = 0; s < SEGMENTS; ++s)
			{
				const float angle = TWO_PI * static_cast<float>(s) / static_cast<float>(SEGMENTS);
				VERTEX_3D v{};
				v.Position = Vector3(std::sin(angle) * radius, OUTSKIRTS_Y, std::cos(angle) * radius);
				v.Normal = Vector3(0.0f, 1.0f, 0.0f);
				v.TexCoord = Vector2(0.0f, 0.0f);
				v.Diffuse = GroundColor(radius, ring, s);
				vertices.push_back(v);
			}
		}
		m_vertexBuffer.Create(vertices);

		// 上から見て時計回り(このプロジェクトの表面の向き)。角度は+Zから+Xへ増えるので、
		// 内側の輪の s → s+1 → 外側の輪の s の順が時計回りになる。
		std::vector<unsigned int> indices;
		indices.reserve((RING_COUNT - 1) * SEGMENTS * 6);
		for (std::size_t ring = 0; ring + 1 < RING_COUNT; ++ring)
		{
			for (std::size_t s = 0; s < SEGMENTS; ++s)
			{
				const std::size_t next = (s + 1) % SEGMENTS;
				const unsigned int in0 = static_cast<unsigned int>(ring * SEGMENTS + s);
				const unsigned int in1 = static_cast<unsigned int>(ring * SEGMENTS + next);
				const unsigned int out0 = static_cast<unsigned int>((ring + 1) * SEGMENTS + s);
				const unsigned int out1 = static_cast<unsigned int>((ring + 1) * SEGMENTS + next);
				indices.push_back(in0);
				indices.push_back(in1);
				indices.push_back(out0);
				indices.push_back(out0);
				indices.push_back(in1);
				indices.push_back(out1);
			}
		}
		m_indexCount = static_cast<unsigned int>(indices.size());
		m_indexBuffer.Create(indices);

		MATERIAL material{};
		material.Ambient = Color(0.0f, 0.0f, 0.0f, 0.0f);
		material.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
		material.Emission = Color(0.0f, 0.0f, 0.0f, 0.0f);
		material.Specular = Color(0.0f, 0.0f, 0.0f, 0.0f);
		material.Shiness = 0.0f;
		material.TextureEnable = FALSE;
		m_material.Create(material);

		// 照明は付けない(空と同じく夕焼けの色そのままを見せる)。影だけは無照明シェーダーでも受ける。
		m_shader.Create("shader/unlitTextureVS.hlsl", "shader/unlitTexturePS.hlsl");
		m_initialized = true;
	}

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
		m_vertexBuffer.SetGPU();
		m_indexBuffer.SetGPU();
		context->DrawIndexed(m_indexCount, 0, 0);
	}

	/// 床より十分低くする。床(-0.29)との差が小さいと、遠くで深度の精度が足りずにちらつく。
	static constexpr float OUTSKIRTS_Y = -4.0f;

private:
	// 近くの土の色から、遠くの空の地平線の色へ。
	static Color GroundColor(float radius, std::size_t ring, std::size_t segment)
	{
		// 壁(0.38, 0.27, 0.19)より少し暗く落ち着いた土の色。
		const Color nearColor(0.30f, 0.23f, 0.18f, 1.0f);
		// FlightBackdrop の空の地平線のすぐ下の色。カメラが壁の上へ出ても、地面の端が空に溶ける。
		const Color horizonColor(0.97f, 0.76f, 0.52f, 1.0f);
		// 遠くの空気の厚みで色が空へ近づく(霞)。距離に対して指数的に近づける。
		float haze = 1.0f - std::exp(-std::max(radius - 350.0f, 0.0f) / 1500.0f);
		if (radius >= OUTER_RADIUS)
			haze = 1.0f;

		// 一色だと板に見えるので、近くだけ明るさを少し揺らす(遠くは霞で消える)。
		uint32_t h = static_cast<uint32_t>(ring * 73856093u) ^ static_cast<uint32_t>(segment * 19349663u);
		h ^= h >> 13;
		h *= 0x5bd1e995u;
		h ^= h >> 15;
		const float noise = (static_cast<float>(h & 0xFFFF) / 65535.0f - 0.5f) * 0.10f * (1.0f - haze);

		const auto mix = [&](float a, float b) { return std::clamp(a + (b - a) * haze + noise, 0.0f, 1.0f); };
		return Color(
			mix(nearColor.x, horizonColor.x),
			mix(nearColor.y, horizonColor.y),
			mix(nearColor.z, horizonColor.z),
			1.0f);
	}

	static constexpr float TWO_PI = 6.2831853f;
	// 空の球(半径6000)より少し内側。カメラは闘技場の近くにしか来ないので、端は常に地平線の近くに見える。
	static constexpr float OUTER_RADIUS = 5600.0f;
	static constexpr std::size_t SEGMENTS = 64;

	CVertexBuffer<VERTEX_3D> m_vertexBuffer;
	CIndexBuffer m_indexBuffer;
	CMaterial m_material;
	CShader m_shader;
	unsigned int m_indexCount = 0;
	bool m_initialized = false;
};
