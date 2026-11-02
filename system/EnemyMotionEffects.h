#pragma once

#include <algorithm>
#include <cmath>
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
 * @class EnemyMotionEffects
 * @brief 敵の体の動きに付ける演出。土煙・地面を走る衝撃の輪・咆哮の衝撃波をまとめて扱う。
 *
 * @details
 * どれも「重いものが動いた」ことを見せるためのもの。いつ・どこに出すかはシーン側が
 * 敵の骨の位置と状態から決め、このクラスは出す・進める・描くだけを受け持つ。
 *
 * 見た目は EnemyFireball と同じく、テクスチャを使わず中心が濃く縁が透明な円盤や輪を重ねて作る。
 * ただし炎と違って土煙は自分で光らないので、加算合成ではなく**半透明合成**で描く
 * (加算にすると土煙が光って見える)。
 *
 * 深度は2通りに分ける。
 * - 土煙・地面の輪 … 比べるが書き込まない。敵の体の向こう側の土煙は体に隠れる。
 * - 咆哮の衝撃波   … 比べない。口元から広がる大きな輪なので、比べると体や床で途中が欠ける。
 *
 * 使い方:
 *   Initialize()                       … シーンの初期化時に1度
 *   SpawnDust(...) / SpawnGroundRing(...) / SpawnRoarRing(...) … 出したい瞬間に
 *   Update(dt)                         … 毎フレーム
 *   Draw(viewMatrix)                   … 描画時(キャラクターの後)
 */
class EnemyMotionEffects
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

		// 板の向きで明暗が変わると土煙がまだらに見えるので、無照明で描く(色は頂点色で決める)。
		m_shader.Create(
			"shader/unlitTextureVS.hlsl",
			"shader/unlitTexturePS.hlsl");

		// 深度は「比べるが書き込まない」(WeaponTrail と同じ理由)。
		D3D11_DEPTH_STENCIL_DESC depthDesc{};
		depthDesc.DepthEnable = TRUE;
		depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		depthDesc.StencilEnable = FALSE;
		m_depthTestNoWrite.Reset();
		Renderer::GetDevice()->CreateDepthStencilState(&depthDesc, m_depthTestNoWrite.GetAddressOf());
		m_initialized = true;
	}

	void Clear()
	{
		m_puffs.clear();
		m_groundRings.clear();
		m_roarRings.clear();
	}

	bool IsEmpty() const
	{
		return m_puffs.empty() && m_groundRings.empty() && m_roarRings.empty();
	}

	// 撮影のログ用。いま出ている土煙の粒・地面の輪・咆哮の輪の数。
	std::size_t GetPuffCount() const { return m_puffs.size(); }
	std::size_t GetRingCount() const { return m_groundRings.size() + m_roarRings.size(); }

	/**
	 * @brief 土煙を舞い上げる。
	 * @param position 出す場所(地面の高さ)。
	 * @param push     土煙全体を流す速さと向き(足を蹴り出した向き・滑っている向きなど)。
	 * @param count    粒の数。
	 * @param spread   横へ散る速さ。強く踏むほど大きくする。
	 * @param size     粒の大きさ(広がり切ったときの半径)。
	 * @param lifetime 消えるまでの時間。
	 * @param alpha    濃さ。
	 */
	void SpawnDust(
		const Vector3& position,
		const Vector3& push,
		int count,
		float spread,
		float size,
		float lifetime,
		float alpha)
	{
		for (int i = 0; i < count; ++i)
		{
			if (m_puffs.size() >= MAX_PUFFS)
				m_puffs.erase(m_puffs.begin());
			const float angle = Random01() * TWO_PI;
			const float speed = spread * (0.35f + 0.65f * Random01());
			Puff puff;
			// 中心を少し持ち上げる。地面の高さに置くと、板の下半分が床に刺さって切れて見える。
			puff.position = position +
				Vector3(std::cos(angle), 0.0f, std::sin(angle)) * (size * 0.25f * Random01()) +
				Vector3(0.0f, size * 0.35f, 0.0f);
			puff.velocity = push +
				Vector3(std::cos(angle) * speed, spread * (0.15f + 0.25f * Random01()), std::sin(angle) * speed);
			puff.size = size * (0.75f + 0.5f * Random01());
			puff.lifetime = lifetime * (0.8f + 0.4f * Random01());
			puff.alpha = alpha;
			// 少しずつ色を変えて、1色の塊に見えないようにする。
			puff.shade = 0.88f + 0.12f * Random01();
			m_puffs.push_back(puff);
		}
	}

	/**
	 * @brief 地面を走る衝撃の輪を出す。叩き付けや着地など、地面を強く打った瞬間に使う。
	 * @param center 輪の中心(地面の高さ)。
	 * @param radius 広がり切ったときの半径。
	 */
	void SpawnGroundRing(const Vector3& center, float radius, float lifetime, float alpha)
	{
		if (m_groundRings.size() >= MAX_RINGS)
			m_groundRings.erase(m_groundRings.begin());
		m_groundRings.push_back({ center, 0.0f, lifetime, radius, alpha });
	}

	/**
	 * @brief 咆哮の衝撃波を1つ出す。口元から画面に向いた輪が広がって消える。
	 * 咆哮の間、一定の間隔で出し続けると「声の圧」が波になって見える。
	 */
	void SpawnRoarRing(const Vector3& center, float radius, float lifetime, float alpha)
	{
		if (m_roarRings.size() >= MAX_RINGS)
			m_roarRings.erase(m_roarRings.begin());
		m_roarRings.push_back({ center, 0.0f, lifetime, radius, alpha });
	}

	void Update(float deltaSeconds)
	{
		if (deltaSeconds <= 0.0f)
			return;
		// 空気の抵抗で速さを落とす。蹴り上げた直後だけ勢いよく広がり、すぐ漂う動きになる。
		const float drag = std::exp(-DUST_DRAG * deltaSeconds);
		for (Puff& puff : m_puffs)
		{
			puff.age += deltaSeconds;
			puff.velocity = puff.velocity * drag;
			// 細かい砂は舞い上がったまま、ゆっくり昇っていく。
			puff.velocity.y += DUST_RISE * deltaSeconds;
			puff.position += puff.velocity * deltaSeconds;
		}
		m_puffs.erase(
			std::remove_if(m_puffs.begin(), m_puffs.end(),
				[](const Puff& p) { return p.age >= p.lifetime; }),
			m_puffs.end());

		const auto ageRings = [deltaSeconds](std::vector<Ring>& rings)
		{
			for (Ring& ring : rings)
				ring.age += deltaSeconds;
			rings.erase(
				std::remove_if(rings.begin(), rings.end(),
					[](const Ring& r) { return r.age >= r.lifetime; }),
				rings.end());
		};
		ageRings(m_groundRings);
		ageRings(m_roarRings);
	}

	void Draw(const Matrix4x4& viewMatrix)
	{
		if (!m_initialized || IsEmpty())
			return;

		const Vector3 cameraRight(viewMatrix._11, viewMatrix._21, viewMatrix._31);
		const Vector3 cameraUp(viewMatrix._12, viewMatrix._22, viewMatrix._32);
		std::size_t quad = 0;

		// --- 地面の輪 ---(深度を比べる組)
		// 地面に寝かせた輪が外へ広がる。内側と外側の縁を透明にし、真ん中の帯だけを見せる。
		for (const Ring& ring : m_groundRings)
		{
			const float t = std::clamp(ring.age / ring.lifetime, 0.0f, 1.0f);
			const float grow = 1.0f - (1.0f - t) * (1.0f - t);
			const float radius = ring.radius * (0.15f + 0.85f * grow);
			const float width = ring.radius * (0.10f + 0.22f * t);
			const float fade = (1.0f - t) * (1.0f - t);
			quad = PushRing(quad, ring.center + Vector3(0.0f, GROUND_RING_LIFT, 0.0f),
				radius, width, WithAlpha(DustColor(), ring.alpha * DUST_DENSITY * fade),
				Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f));
		}

		// --- 土煙 ---(深度を比べる組)
		// 出た直後に一気に濃くなり、広がりながら薄れて消える。
		for (const Puff& puff : m_puffs)
		{
			const float t = std::clamp(puff.age / puff.lifetime, 0.0f, 1.0f);
			const float grow = 1.0f - (1.0f - t) * (1.0f - t);
			const float radius = puff.size * (0.35f + 0.65f * grow);
			const float fadeIn = std::min(1.0f, t * 8.0f);
			const float fadeOut = (1.0f - t) * std::sqrt(1.0f - t);
			Color color = DustColor();
			color.x *= puff.shade;
			color.y *= puff.shade;
			color.z *= puff.shade;
			quad = PushDisc(quad, puff.position, radius,
				WithAlpha(color, puff.alpha * DUST_DENSITY * fadeIn * fadeOut), cameraRight, cameraUp);
		}
		const std::size_t depthTestedQuads = quad;

		// --- 咆哮の衝撃波 ---(深度を比べない組)
		// 広がるほど細く薄くなる。白い輪にすると光って見えるので、空気のゆがみに近い淡い色にする。
		for (const Ring& ring : m_roarRings)
		{
			const float t = std::clamp(ring.age / ring.lifetime, 0.0f, 1.0f);
			const float grow = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
			const float radius = ring.radius * (0.08f + 0.92f * grow);
			const float width = ring.radius * (0.16f - 0.10f * t);
			const float fade = (1.0f - t) * (1.0f - t);
			quad = PushRing(quad, ring.center, radius, width,
				WithAlpha(RoarColor(), ring.alpha * fade), cameraRight, cameraUp);
		}

		if (quad == 0)
			return;
		for (std::size_t i = quad; i < m_drawnQuads; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				m_vertices[i * 4 + k].Position = Vector3(0.0f, 0.0f, 0.0f);
				m_vertices[i * 4 + k].Diffuse = Color(0.0f, 0.0f, 0.0f, 0.0f);
			}
		}
		m_drawnQuads = quad;
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
		Renderer::SetBlendState(BS_ALPHABLEND);

		if (depthTestedQuads > 0)
		{
			if (m_depthTestNoWrite)
				context->OMSetDepthStencilState(m_depthTestNoWrite.Get(), 0);
			else
				Renderer::SetDepthEnable(false);
			context->DrawIndexed(static_cast<unsigned int>(depthTestedQuads * 6), 0, 0);
		}
		if (quad > depthTestedQuads)
		{
			Renderer::SetDepthEnable(false);
			context->DrawIndexed(
				static_cast<unsigned int>((quad - depthTestedQuads) * 6),
				static_cast<unsigned int>(depthTestedQuads * 6), 0);
		}

		Renderer::SetDepthEnable(true);
		Renderer::DisableCulling(true);
	}

private:
	struct Puff
	{
		Vector3 position{};
		Vector3 velocity{};
		float age = 0.0f;
		float lifetime = 1.0f;
		float size = 1.0f;
		float alpha = 0.5f;
		float shade = 1.0f;
	};
	struct Ring
	{
		Vector3 center{};
		float age = 0.0f;
		float lifetime = 0.5f;
		float radius = 10.0f;
		float alpha = 0.5f;
	};

	static Color WithAlpha(Color color, float alpha)
	{
		color.w = std::clamp(alpha, 0.0f, 1.0f);
		return color;
	}

	// 毎回同じ並びになる簡単な乱数。撮影で同じ場面を何度でも再現できるよう、時刻や乱数の種を使わない。
	float Random01()
	{
		m_random ^= m_random << 13;
		m_random ^= m_random >> 17;
		m_random ^= m_random << 5;
		return static_cast<float>(m_random & 0xFFFFFF) / static_cast<float>(0x1000000);
	}

	/**
	 * @brief 中心が濃く縁が透明な円盤を1枚積む(EnemyFireball::PushDisc と同じ作り)。
	 * 四角形の2点を中心に重ね、残りの1枚の三角形=中心・縁・縁 として使う。
	 */
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

	/**
	 * @brief 輪を1つ積む。半径 radius の円を真ん中に、幅 width の帯を張る。
	 * 帯の内側と外側の縁は透明にし、真ん中だけ color の濃さにする(1区切りあたり四角形2枚)。
	 * axisA/axisB に地面の2軸を渡せば地面に寝た輪、カメラの右・上を渡せば画面に向いた輪になる。
	 */
	std::size_t PushRing(
		std::size_t quad,
		const Vector3& center,
		float radius,
		float width,
		const Color& color,
		const Vector3& axisA,
		const Vector3& axisB)
	{
		if (quad + RING_SEGMENTS * 2 > MAX_QUADS || color.w <= 0.002f)
			return quad;
		Color clear = color;
		clear.w = 0.0f;
		const float inner = std::max(0.0f, radius - width * 0.5f);
		const float outer = radius + width * 0.5f;
		for (std::size_t s = 0; s < RING_SEGMENTS; ++s)
		{
			const float a0 = TWO_PI * static_cast<float>(s) / static_cast<float>(RING_SEGMENTS);
			const float a1 = TWO_PI * static_cast<float>(s + 1) / static_cast<float>(RING_SEGMENTS);
			const Vector3 d0 = axisA * std::cos(a0) + axisB * std::sin(a0);
			const Vector3 d1 = axisA * std::cos(a1) + axisB * std::sin(a1);
			// 内側の縁 → 真ん中
			std::size_t v = quad * 4;
			m_vertices[v + 0].Position = center + d0 * inner;
			m_vertices[v + 0].Diffuse = clear;
			m_vertices[v + 1].Position = center + d0 * radius;
			m_vertices[v + 1].Diffuse = color;
			m_vertices[v + 2].Position = center + d1 * inner;
			m_vertices[v + 2].Diffuse = clear;
			m_vertices[v + 3].Position = center + d1 * radius;
			m_vertices[v + 3].Diffuse = color;
			++quad;
			// 真ん中 → 外側の縁
			v = quad * 4;
			m_vertices[v + 0].Position = center + d0 * radius;
			m_vertices[v + 0].Diffuse = color;
			m_vertices[v + 1].Position = center + d0 * outer;
			m_vertices[v + 1].Diffuse = clear;
			m_vertices[v + 2].Position = center + d1 * radius;
			m_vertices[v + 2].Diffuse = color;
			m_vertices[v + 3].Position = center + d1 * outer;
			m_vertices[v + 3].Diffuse = clear;
			++quad;
		}
		return quad;
	}

	static constexpr float TWO_PI = 6.2831853f;
	// 土煙はぼんやりした塊なので、炎(12)より少ない区切りでも丸く見える。
	static constexpr std::size_t DISC_SEGMENTS = 8;
	static constexpr std::size_t RING_SEGMENTS = 32;
	static constexpr std::size_t MAX_PUFFS = 220;
	static constexpr std::size_t MAX_RINGS = 8;
	static constexpr std::size_t MAX_QUADS =
		DISC_SEGMENTS * MAX_PUFFS + RING_SEGMENTS * 2 * MAX_RINGS * 2;
	// 速さが1秒で約 e^-3.5 ≒ 3% まで落ちる。
	static constexpr float DUST_DRAG = 3.5f;
	static constexpr float DUST_RISE = 6.0f;
	// 床と同じ高さに置くと床とちらつくので、少しだけ浮かせる。
	static constexpr float GROUND_RING_LIFT = 0.4f;
	// 土の色。明るい灰色のマス目の床の上で見えるよう、床より暗く茶色に寄せる
	// (初めは床に近い明るい灰色(0.66, 0.61, 0.53)にしていて、撮影で床に溶けて見えなかった)。
	static Color DustColor() { return Color(0.50f, 0.42f, 0.32f, 1.0f); }
	// 土煙と地面の輪の濃さの倍率。出す側が渡す濃さはそのままに、全体の見え方をここで合わせる。
	static constexpr float DUST_DENSITY = 1.6f;
	static Color RoarColor() { return Color(0.90f, 0.90f, 0.86f, 1.0f); }

	std::vector<Puff> m_puffs;
	std::vector<Ring> m_groundRings;
	std::vector<Ring> m_roarRings;
	std::vector<VERTEX_3D> m_vertices;
	CVertexBuffer<VERTEX_3D> m_vertexBuffer;
	CIndexBuffer m_indexBuffer;
	CMaterial m_material;
	CShader m_shader;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthTestNoWrite;
	std::size_t m_drawnQuads = 0;
	uint32_t m_random = 0x9E3779B9u;
	bool m_initialized = false;
};
