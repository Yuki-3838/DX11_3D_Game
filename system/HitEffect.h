#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "CIndexBuffer.h"
#include "CMaterial.h"
#include "CShader.h"
#include "CVertexBuffer.h"
#include "CommonTypes.h"
#include "renderer.h"

/**
 * @class HitEffect
 * @brief 命中した点から飛ぶ火花と、衝撃の閃光を描く。
 *
 * @details
 * **特定の武器やキャラクターに依存しない。**
 * 必要なのは「当たったワールド座標」と「弾く向き」だけなので、
 * プレイヤーの剣・敵の攻撃・別のモデルへ差し替えた武器、どれにも同じものを使える。
 *
 * テクスチャを使わずに火花らしく見せるため、次の3つを組み合わせている。
 *  1. 火花1本を「白熱した細い芯」と「その外側の広がった輝き」の2枚重ねで描く。
 *     単色の板1枚だとのっぺりして安っぽく見えるため。
 *  2. 色を寿命で変える(白熱 → 橙 → 赤)。温度が下がっていくように見せる。
 *  3. 先端を太く根元を細くした涙型にする。等幅の帯だと棒に見える。
 *
 * さらに、命中の瞬間だけ大きな閃光を1枚重ねて衝撃を強調する。
 *
 * 使い方:
 *   Initialize()               … シーンの初期化時に1度
 *   Spawn(position, direction) … 命中したフレームに1度
 *   Update(deltaSeconds)       … 毎フレーム
 *   Draw(viewMatrix)           … 描画時
 */
class HitEffect
{
public:
	void Initialize(std::size_t maxSparks = 96)
	{
		m_maxSparks = std::max<std::size_t>(maxSparks, 1);
		m_sparks.clear();
		m_sparks.reserve(m_maxSparks);

		// 火花1本につき2枚(芯と輝き)、加えて閃光の光条ぶんを確保する。
		m_maxQuads = m_maxSparks * 2 + FLASH_RAY_COUNT;
		m_vertices.assign(m_maxQuads * 4, VERTEX_3D{});
		for (auto& v : m_vertices)
		{
			v.Normal = Vector3(0.0f, 1.0f, 0.0f);
			v.TexCoord = Vector2(0.0f, 0.0f);
			v.Diffuse = Color(1.0f, 1.0f, 1.0f, 0.0f);
		}
		m_vertexBuffer.Create(m_vertices);

		std::vector<uint32_t> indices;
		indices.reserve(m_maxQuads * 6);
		for (std::size_t i = 0; i < m_maxQuads; ++i)
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

		// 面の向きで明暗が変わると火花が途中で暗くなるため無照明で描く。
		m_shader.Create(
			"shader/unlitTextureVS.hlsl",
			"shader/unlitTexturePS.hlsl");

		m_initialized = true;
	}

	void Clear()
	{
		m_sparks.clear();
		m_flashAge = m_flashLifetime;
	}

	/**
	 * @brief 命中点から火花と閃光を出す。
	 * @param position 当たったワールド座標。
	 * @param direction 主に飛ばしたい向き(刃の進行方向など)。正規化不要。
	 * @param count 火花の本数。
	 * @param scale 大きさの倍率。強い攻撃ほど大きくする。
	 */
	void Spawn(
		const Vector3& position,
		const Vector3& direction,
		int count = 34,
		float scale = 1.0f)
	{
		if (!m_initialized)
			return;

		Vector3 mainDir = direction;
		if (mainDir.LengthSquared() < 0.0001f)
			mainDir = Vector3(0.0f, 1.0f, 0.0f);
		mainDir.Normalize();

		// 命中の瞬間の閃光。
		m_flashPosition = position;
		m_flashAge = 0.0f;
		m_flashScale = scale;

		for (int i = 0; i < count; ++i)
		{
			if (m_sparks.size() >= m_maxSparks)
				break;

			// 乱数生成器を持ち込まず、通し番号から散らす。
			m_spawnCounter = (m_spawnCounter + 7919u) % 65521u;
			const float a = static_cast<float>((m_spawnCounter * 31u) % 628u) * 0.01f;
			const float b = static_cast<float>((m_spawnCounter * 17u) % 628u) * 0.01f;
			const float r0 = static_cast<float>((m_spawnCounter * 13u) % 1000u) * 0.001f;
			const float r1 = static_cast<float>((m_spawnCounter * 29u) % 1000u) * 0.001f;

			// 主方向へ寄せつつ、全方向へも散らす。
			// 一方向へ揃えすぎると噴射のように見えるため、球状の散らしを混ぜる。
			const Vector3 spread(
				std::sinf(a) * std::cosf(b),
				std::cosf(a),
				std::sinf(a) * std::sinf(b));
			Vector3 velocity = mainDir * (0.9f + r0 * 0.8f) + spread * 1.15f;
			if (velocity.LengthSquared() < 0.0001f)
				velocity = mainDir;
			velocity.Normalize();

			// 速さと寿命に大きな差をつける。揃っていると花火の球のように見え、
			// 火花らしいばらつきが出ない。
			const float speedRatio = 0.35f + r0 * r0 * 1.65f;

			Spark spark;
			spark.position = position;
			spark.velocity = velocity * (BURST_SPEED * speedRatio * scale);
			spark.age = 0.0f;
			spark.lifetime = 0.18f + r1 * 0.30f;
			// 速い火花ほど長く伸ばす。実際の火花も速いものほど尾が長い。
			spark.length = (1.6f + speedRatio * 4.5f) * scale;
			spark.width = (0.045f + r1 * 0.075f) * scale;
			m_sparks.push_back(spark);
		}
	}

	void Update(float deltaSeconds)
	{
		if (deltaSeconds <= 0.0f)
			return;

		m_flashAge += deltaSeconds;

		for (auto& spark : m_sparks)
		{
			spark.age += deltaSeconds;
			spark.position += spark.velocity * deltaSeconds;
			// 空気抵抗で減速させる。等速で飛ぶと直線的で作り物に見える。
			spark.velocity *= std::max(0.0f, 1.0f - DRAG * deltaSeconds);
			spark.velocity.y -= GRAVITY * deltaSeconds;
		}
		m_sparks.erase(
			std::remove_if(
				m_sparks.begin(),
				m_sparks.end(),
				[](const Spark& s) { return s.age >= s.lifetime; }),
			m_sparks.end());
	}

	bool IsEmpty() const
	{
		return m_sparks.empty() && m_flashAge >= m_flashLifetime;
	}

	/**
	 * @brief 火花と閃光を描画する。
	 * @param viewMatrix カメラのビュー行列。板をカメラへ正対させるために使う。
	 */
	void Draw(const Matrix4x4& viewMatrix)
	{
		if (!m_initialized)
			return;
		if (m_sparks.empty() && m_flashAge >= m_flashLifetime)
			return;

		// ビュー行列からカメラの基底を取り出す。行ベクトル規約なので列が基底になる。
		const Vector3 cameraRight(viewMatrix._11, viewMatrix._21, viewMatrix._31);
		const Vector3 cameraUp(viewMatrix._12, viewMatrix._22, viewMatrix._32);
		const Vector3 cameraForward(viewMatrix._13, viewMatrix._23, viewMatrix._33);

		std::size_t quad = 0;

		// --- 命中の閃光 ---
		// 一瞬だけ光らせ、火花が広がる前に衝撃を伝える。
		//
		// 四角い板1枚で光らせると、テクスチャが無いため輪郭がそのまま箱に見えて
		// かえって安っぽくなる(実際に試して失敗した)。
		// 代わりに中心から放射する光条を並べ、星状の閃光として描く。
		// 中心を明るく、先端を透明にすることで、板の輪郭が見えなくなる。
		if (m_flashAge < m_flashLifetime)
		{
			const float t = std::clamp(m_flashAge / m_flashLifetime, 0.0f, 1.0f);
			const float alpha = (1.0f - t) * (1.0f - t);
			for (std::size_t i = 0; i < FLASH_RAY_COUNT; ++i)
			{
				if (quad >= m_maxQuads)
					break;

				const float angle =
					(6.2831853f / static_cast<float>(FLASH_RAY_COUNT)) *
					static_cast<float>(i);
				// 光条ごとに長さを変える。すべて同じだと歯車のように見える。
				const float lengthScale = (i % 2 == 0) ? 1.0f : 0.55f;
				const float rayLength =
					(1.8f + t * 5.0f) * lengthScale * m_flashScale;
				const float rayWidth = (0.55f * (1.0f - t) + 0.12f) * m_flashScale;

				// カメラ平面上で放射方向と、それに垂直な幅方向を作る。
				const Vector3 rayDir =
					cameraRight * std::cosf(angle) + cameraUp * std::sinf(angle);
				const Vector3 raySide =
					(cameraRight * -std::sinf(angle) + cameraUp * std::cosf(angle)) *
					rayWidth;

				const Vector3 tip = m_flashPosition + rayDir * rayLength;
				const Color center(1.0f, 0.96f, 0.82f, alpha);
				const Color edge(1.0f, 0.72f, 0.30f, 0.0f);
				PushQuad(
					quad++,
					tip - raySide * 0.15f, tip + raySide * 0.15f,
					m_flashPosition - raySide, m_flashPosition + raySide,
					edge, edge, center, center);
			}
		}

		// --- 火花本体 ---
		for (const Spark& spark : m_sparks)
		{
			if (quad + 2 > m_maxQuads)
				break;

			const float ageRatio = std::clamp(spark.age / spark.lifetime, 0.0f, 1.0f);
			const float alpha = (1.0f - ageRatio) * (1.0f - ageRatio);

			Vector3 dir = spark.velocity;
			if (dir.LengthSquared() < 0.0001f)
				dir = Vector3(0.0f, 1.0f, 0.0f);
			dir.Normalize();

			Vector3 side = dir.Cross(cameraForward);
			if (side.LengthSquared() < 0.0001f)
				side = cameraRight;
			side.Normalize();

			const Vector3 head = spark.position;
			const Vector3 tail = spark.position - dir * spark.length;

			// 温度が下がるように色を変える。白熱 → 橙 → 赤。
			const Color hot(1.0f, 0.98f, 0.90f, 1.0f);
			const Color warm(1.0f, 0.62f, 0.18f, 1.0f);
			const Color cool(0.85f, 0.16f, 0.05f, 1.0f);
			const Color tint = ageRatio < 0.5f
				? LerpColor(hot, warm, ageRatio * 2.0f)
				: LerpColor(warm, cool, (ageRatio - 0.5f) * 2.0f);

			// 形は「先端が点、尾へ向かって広がって消える」彗星型にする。
			//
			// 最初は先端を幅広にしていたが、先端が平らな辺になるため
			// 板や木片のように見えて安っぽかった(実際に試して失敗した)。
			// 先端を点に近づけると、同じ四角形1枚でも線状の火花に見える。

			// 外側の輝き。芯より太く薄い板で、まわりの光を表す。
			{
				const Vector3 wide = side * (spark.width * 2.4f);
				Color glowHead = tint;
				glowHead.w = alpha * 0.35f;
				Color glowTail = tint;
				glowTail.w = 0.0f;
				PushQuad(
					quad++,
					tail - wide, tail + wide,
					head - wide * 0.18f, head + wide * 0.18f,
					glowTail, glowTail, glowHead, glowHead);
			}

			// 白熱した芯。細く明るくして、火花の「線」を作る。
			{
				const Vector3 thin = side * spark.width;
				Color coreHead = LerpColor(tint, Color(1.0f, 1.0f, 1.0f, 1.0f), 0.65f);
				coreHead.w = alpha;
				Color coreTail = tint;
				coreTail.w = 0.0f;
				PushQuad(
					quad++,
					tail - thin, tail + thin,
					head - thin * 0.12f, head + thin * 0.12f,
					coreTail, coreTail, coreHead, coreHead);
			}
		}

		// 余った分は完全に透明へ潰す。
		for (std::size_t i = quad; i < m_maxQuads; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				m_vertices[i * 4 + k].Position = Vector3(0.0f, 0.0f, 0.0f);
				m_vertices[i * 4 + k].Diffuse = Color(0.0f, 0.0f, 0.0f, 0.0f);
			}
		}
		m_vertexBuffer.Modify(m_vertices);

		m_material.SetDiffuse(Color(1.0f, 1.0f, 1.0f, 1.0f));
		m_material.Update();
		m_shader.SetGPU();
		m_material.SetGPU();
		m_vertexBuffer.SetGPU();
		m_indexBuffer.SetGPU();

		Matrix4x4 world = Matrix4x4::Identity;
		Renderer::SetWorldMatrix(&world);

		ID3D11DeviceContext* context = Renderer::GetDeviceContext();
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		Renderer::DisableCulling(false);
		// 加算合成にする。重なった火花ほど明るくなり、光っているように見える。
		// 半透明合成だと重なっても明るくならず、紙を貼ったような見た目になる。
		Renderer::SetBlendState(BS_ADDITIVE);
		Renderer::SetDepthEnable(false);

		context->DrawIndexed(static_cast<unsigned int>(quad * 6), 0, 0);

		// 以降の描画へ影響しないよう戻す。
		Renderer::SetDepthEnable(true);
		Renderer::SetBlendState(BS_ALPHABLEND);
		Renderer::DisableCulling(true);
	}

private:
	struct Spark
	{
		Vector3 position{};
		Vector3 velocity{};
		float age = 0.0f;
		float lifetime = 0.3f;
		float length = 6.0f;
		float width = 1.0f;
	};

	static Color LerpColor(const Color& a, const Color& b, float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return Color(
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
			a.w + (b.w - a.w) * t);
	}

	void PushQuad(
		std::size_t index,
		const Vector3& p0, const Vector3& p1,
		const Vector3& p2, const Vector3& p3,
		const Color& c0, const Color& c1,
		const Color& c2, const Color& c3)
	{
		const std::size_t v = index * 4;
		m_vertices[v + 0].Position = p0;
		m_vertices[v + 0].Diffuse = c0;
		m_vertices[v + 1].Position = p1;
		m_vertices[v + 1].Diffuse = c1;
		m_vertices[v + 2].Position = p2;
		m_vertices[v + 2].Diffuse = c2;
		m_vertices[v + 3].Position = p3;
		m_vertices[v + 3].Diffuse = c3;
	}

	// 閃光を構成する光条の本数。
	static constexpr std::size_t FLASH_RAY_COUNT = 10;
	static constexpr float BURST_SPEED = 46.0f;
	static constexpr float GRAVITY = 220.0f;
	static constexpr float DRAG = 3.2f;

	std::vector<Spark> m_sparks;
	std::vector<VERTEX_3D> m_vertices;
	CVertexBuffer<VERTEX_3D> m_vertexBuffer;
	CIndexBuffer m_indexBuffer;
	CMaterial m_material;
	CShader m_shader;
	std::size_t m_maxSparks = 96;
	std::size_t m_maxQuads = 0;
	unsigned int m_spawnCounter = 1u;
	Vector3 m_flashPosition{};
	float m_flashAge = 1.0f;
	float m_flashScale = 1.0f;
	static constexpr float m_flashLifetime = 0.12f;
	bool m_initialized = false;
};
