#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "CIndexBuffer.h"
#include "CMaterial.h"
#include "CShader.h"
#include "CVertexBuffer.h"
#include "CombatAttackTable.h"
#include "CommonTypes.h"
#include "renderer.h"

/**
 * @class EnemyFireball
 * @brief 敵が吐く火球。飛んでいく弾・着弾の爆発・吐く前に口に溜まる火をまとめて扱う。
 *
 * @details
 * 火球は敵の体から離れて飛ぶので、当たり判定を敵の攻撃の段階(OneVsOneCombat)には乗せられない
 * (吐いた後、敵が隙に入っても火球は飛び続ける)。そこで弾そのものに当たり判定を持たせ、
 * 着弾したかどうかをシーンへ返す。ダメージを入れる・画面を揺らすのはシーン側の仕事にする。
 *
 * 狙いは**吐いた瞬間のプレイヤーの足元**で、飛んでいる間は追いかけない。
 * 「敵がプレイヤーを見続ける」のではなく「狙った場所へ撃つ」ので、吐く瞬間に横へずれれば外れる。
 *
 * 見た目はテクスチャを使わず、中心が明るく縁が透明な円盤(扇形の三角形)を重ねて作る。
 * HitEffect と同じく加算合成で描くので、重なったところほど明るく燃えて見える。
 *
 * 使い方:
 *   Initialize()                    … シーンの初期化時に1度
 *   Launch(mouth, target)           … 吐いた瞬間に1度
 *   SetMouthCharge(mouth, charge)   … 予兆の間、毎フレーム(0で消える)
 *   Update(dt, player, ...)         … 毎フレーム。着弾したら events に積む
 *   Draw(viewMatrix)                … 描画時
 */
class EnemyFireball
{
public:
	/** 着弾したときの知らせ。シーンが火花・揺れ・ダメージを出すのに使う。 */
	struct Impact
	{
		Vector3 position{};   ///< 爆発した位置
		Vector3 direction{};  ///< 火球が飛んでいた向き(プレイヤーを飛ばす向きに使う)
		bool hitPlayer = false; ///< 爆発にプレイヤーが巻き込まれたか
	};

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

		// 炎は自分で光っているので、面の向きで暗くならないよう無照明で描く。
		m_shader.Create(
			"shader/unlitTextureVS.hlsl",
			"shader/unlitTexturePS.hlsl");
		m_initialized = true;
	}

	void Clear()
	{
		m_balls.clear();
		m_puffs.clear();
		m_blasts.clear();
		m_mouthCharge = 0.0f;
	}

	/**
	 * @brief 火球を吐く。
	 * @param mouth  口の位置(ワールド)。
	 * @param target 狙う地面の位置(ワールド)。吐いた瞬間のプレイヤーの足元を渡す。
	 */
	void Launch(const Vector3& mouth, const Vector3& target)
	{
		Vector3 direction = target - mouth;
		if (direction.LengthSquared() < 0.0001f)
			direction = Vector3(0.0f, -1.0f, 0.0f);
		direction.Normalize();

		Ball ball;
		ball.position = mouth;
		ball.velocity = direction * Combat::Tuning::ENEMY_FIREBALL_SPEED;
		m_balls.push_back(ball);
		// 口の火は吐いたら消える。
		m_mouthCharge = 0.0f;
	}

	/**
	 * @brief 吐く前に口に溜まる火。予兆の間だけ出す。
	 * @param charge 溜まり具合(0→1)。0なら出さない。
	 */
	void SetMouthCharge(const Vector3& mouth, float charge)
	{
		m_mouthPosition = mouth;
		m_mouthCharge = std::clamp(charge, 0.0f, 1.0f);
	}

	/**
	 * @brief 火球を進め、着弾したものを events に積む。
	 * @param playerCenter     プレイヤーの体の中心(足元ではない)。
	 * @param playerRadius     プレイヤーの体の太さ。
	 * @param playerInvincible 無敵(前転・吹き飛ばし中)なら、巻き込まれても当たりにしない。
	 * @param groundY          地面の高さ。ここまで落ちたら爆発する。
	 */
	void Update(
		float deltaSeconds,
		const Vector3& playerCenter,
		float playerRadius,
		bool playerInvincible,
		float groundY,
		std::vector<Impact>& events)
	{
		if (deltaSeconds <= 0.0f)
			return;
		m_time += deltaSeconds;

		for (auto& ball : m_balls)
		{
			ball.age += deltaSeconds;
			ball.position += ball.velocity * deltaSeconds;

			// 尾を引く炎。一定の間隔で、通った場所へ消えていく炎の塊を置いていく。
			ball.puffTimer += deltaSeconds;
			while (ball.puffTimer >= PUFF_INTERVAL)
			{
				ball.puffTimer -= PUFF_INTERVAL;
				Puff puff;
				puff.position = ball.position;
				// 少しだけ上へ昇らせる。熱い煙は上へ流れる。
				puff.velocity = Vector3(0.0f, 6.0f, 0.0f);
				m_puffs.push_back(puff);
			}

			// 着弾の条件: プレイヤーに触れた / 地面に落ちた / 飛びすぎた。
			const float touchDistance = Combat::Tuning::ENEMY_FIREBALL_RADIUS + playerRadius;
			const bool touchedPlayer =
				(ball.position - playerCenter).LengthSquared() <= touchDistance * touchDistance;
			const bool touchedGround =
				ball.position.y <= groundY + Combat::Tuning::ENEMY_FIREBALL_RADIUS * 0.5f;
			if (!touchedPlayer && !touchedGround &&
				ball.age < Combat::Tuning::ENEMY_FIREBALL_LIFETIME)
			{
				continue;
			}

			ball.exploded = true;
			Impact impact;
			impact.position = ball.position;
			if (touchedGround)
				impact.position.y = groundY;
			impact.direction = ball.velocity;
			impact.direction.y = 0.0f;
			// 爆発に巻き込まれたか。地面で爆発しても、近くにいれば当たる。
			// 高さは体の中心から測るので、足元の爆発でも届くよう半径に体の太さを足す。
			const float blastReach =
				Combat::Tuning::ENEMY_FIREBALL_EXPLOSION_RADIUS + playerRadius;
			impact.hitPlayer = !playerInvincible &&
				(touchedPlayer ||
				 (impact.position - playerCenter).LengthSquared() <= blastReach * blastReach);
			// 寿命切れ(壁の外へ外れたもの)は爆発を見せずに消す。
			if (touchedPlayer || touchedGround)
			{
				Blast blast;
				blast.position = impact.position;
				m_blasts.push_back(blast);
				events.push_back(impact);
			}
		}
		m_balls.erase(
			std::remove_if(m_balls.begin(), m_balls.end(),
				[](const Ball& b) { return b.exploded; }),
			m_balls.end());

		for (auto& puff : m_puffs)
		{
			puff.age += deltaSeconds;
			puff.position += puff.velocity * deltaSeconds;
		}
		m_puffs.erase(
			std::remove_if(m_puffs.begin(), m_puffs.end(),
				[](const Puff& p) { return p.age >= PUFF_LIFETIME; }),
			m_puffs.end());

		for (auto& blast : m_blasts)
			blast.age += deltaSeconds;
		m_blasts.erase(
			std::remove_if(m_blasts.begin(), m_blasts.end(),
				[](const Blast& b) { return b.age >= BLAST_LIFETIME; }),
			m_blasts.end());
	}

	/** デバッグ表示用。飛んでいる火球の位置。 */
	template <typename Fn>
	void ForEachBall(Fn&& fn) const
	{
		for (const Ball& ball : m_balls)
			fn(ball.position);
	}

	void Draw(const Matrix4x4& viewMatrix)
	{
		if (!m_initialized)
			return;
		if (m_balls.empty() && m_puffs.empty() && m_blasts.empty() && m_mouthCharge <= 0.0f)
			return;

		const Vector3 cameraRight(viewMatrix._11, viewMatrix._21, viewMatrix._31);
		const Vector3 cameraUp(viewMatrix._12, viewMatrix._22, viewMatrix._32);

		std::size_t quad = 0;
		const Color hot(1.0f, 0.95f, 0.70f, 1.0f);
		const Color flame(1.0f, 0.55f, 0.12f, 1.0f);
		const Color ember(0.75f, 0.14f, 0.03f, 1.0f);

		// 揺らぎ。炎の大きさを少しずつ変えて、燃えている感じを出す(乱数を持ち込まず時間から作る)。
		const auto flicker = [this](float phase)
		{
			return 1.0f + 0.12f * std::sin(m_time * 31.0f + phase) +
				0.07f * std::sin(m_time * 53.0f + phase * 1.7f);
		};

		// --- 口に溜まる火(吐く前の合図) ---
		// 溜まるほど大きく明るくなる。叩き付けも上体を反らすので、これで火球だと見分けられる。
		if (m_mouthCharge > 0.0f)
		{
			const float c = m_mouthCharge;
			const float size = (1.0f + 3.2f * c) * flicker(0.0f);
			quad = PushDisc(quad, m_mouthPosition, size * 2.4f,
				WithAlpha(flame, 0.30f * c), cameraRight, cameraUp);
			quad = PushDisc(quad, m_mouthPosition, size,
				WithAlpha(hot, 0.85f * c), cameraRight, cameraUp);
		}

		// --- 尾を引く炎 ---
		// 古いものほど大きく広がり、橙から暗い赤へ冷えて消える。
		for (const Puff& puff : m_puffs)
		{
			const float t = std::clamp(puff.age / PUFF_LIFETIME, 0.0f, 1.0f);
			const float size = Combat::Tuning::ENEMY_FIREBALL_RADIUS * (0.9f + 1.4f * t);
			const Color tint = LerpColor(flame, ember, t);
			quad = PushDisc(quad, puff.position, size,
				WithAlpha(tint, 0.55f * (1.0f - t) * (1.0f - t)), cameraRight, cameraUp);
		}

		// --- 火球本体 ---
		// 外側の炎・内側の炎・白熱した芯の3枚を重ねる。
		for (std::size_t i = 0; i < m_balls.size(); ++i)
		{
			const Ball& ball = m_balls[i];
			const float f = flicker(static_cast<float>(i) * 2.1f);
			const float r = Combat::Tuning::ENEMY_FIREBALL_RADIUS;
			quad = PushDisc(quad, ball.position, r * 2.6f * f,
				WithAlpha(flame, 0.35f), cameraRight, cameraUp);
			quad = PushDisc(quad, ball.position, r * 1.5f * f,
				WithAlpha(LerpColor(flame, hot, 0.4f), 0.85f), cameraRight, cameraUp);
			quad = PushDisc(quad, ball.position, r * 0.8f,
				WithAlpha(hot, 1.0f), cameraRight, cameraUp);
		}

		// --- 着弾の爆発 ---
		// 一気に爆発の半径まで膨らんで消える。最初の一瞬は白く、すぐ橙へ冷える。
		for (const Blast& blast : m_blasts)
		{
			const float t = std::clamp(blast.age / BLAST_LIFETIME, 0.0f, 1.0f);
			const float grow = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
			const float radius = Combat::Tuning::ENEMY_FIREBALL_EXPLOSION_RADIUS * (0.35f + 0.9f * grow);
			const float fade = (1.0f - t) * (1.0f - t);
			// 地面から少し浮かせる。爆発の中心が地面の高さだと、下半分が床に隠れたように見える。
			const Vector3 center = blast.position + Vector3(0.0f, radius * 0.35f, 0.0f);
			quad = PushDisc(quad, center, radius * 1.3f,
				WithAlpha(LerpColor(flame, ember, t), 0.40f * fade), cameraRight, cameraUp);
			quad = PushDisc(quad, center, radius * 0.75f,
				WithAlpha(LerpColor(hot, flame, std::min(1.0f, t * 2.5f)), 0.9f * fade),
				cameraRight, cameraUp);
		}

		// 余った分は完全に透明へ潰す。
		for (std::size_t i = quad; i < MAX_QUADS; ++i)
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
		// 加算合成。重なった炎ほど明るくなり、燃えて光っているように見える。
		Renderer::SetBlendState(BS_ADDITIVE);
		Renderer::SetDepthEnable(false);

		context->DrawIndexed(static_cast<unsigned int>(quad * 6), 0, 0);

		Renderer::SetDepthEnable(true);
		Renderer::SetBlendState(BS_ALPHABLEND);
		Renderer::DisableCulling(true);
	}

private:
	struct Ball
	{
		Vector3 position{};
		Vector3 velocity{};
		float age = 0.0f;
		float puffTimer = 0.0f;
		bool exploded = false;
	};
	struct Puff
	{
		Vector3 position{};
		Vector3 velocity{};
		float age = 0.0f;
	};
	struct Blast
	{
		Vector3 position{};
		float age = 0.0f;
	};

	static Color WithAlpha(Color color, float alpha)
	{
		color.w = alpha;
		return color;
	}

	static Color LerpColor(const Color& a, const Color& b, float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return Color(
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
			a.w + (b.w - a.w) * t);
	}

	/**
	 * @brief カメラへ向けた円盤を1枚積む。中心が明るく、縁へ向けて透明になる。
	 *
	 * 四角い板1枚だと輪郭が箱に見える(HitEffect の閃光で試して失敗した)ので、
	 * 中心から縁への扇形を DISC_SEGMENTS 枚並べる。四角形の2点を中心へ重ねて三角形として使う。
	 * @return 次に使う四角形の番号。足りなくなったら積まずにそのまま返す。
	 */
	std::size_t PushDisc(
		std::size_t quad,
		const Vector3& center,
		float radius,
		const Color& centerColor,
		const Vector3& cameraRight,
		const Vector3& cameraUp)
	{
		if (quad + DISC_SEGMENTS > MAX_QUADS || centerColor.w <= 0.001f)
			return quad;
		Color edgeColor = centerColor;
		edgeColor.w = 0.0f;
		constexpr float TWO_PI = 6.2831853f;
		for (std::size_t s = 0; s < DISC_SEGMENTS; ++s)
		{
			const float a0 = TWO_PI * static_cast<float>(s) / static_cast<float>(DISC_SEGMENTS);
			const float a1 = TWO_PI * static_cast<float>(s + 1) / static_cast<float>(DISC_SEGMENTS);
			const Vector3 rim0 = center +
				(cameraRight * std::cos(a0) + cameraUp * std::sin(a0)) * radius;
			const Vector3 rim1 = center +
				(cameraRight * std::cos(a1) + cameraUp * std::sin(a1)) * radius;
			// 添字の並びは (0,1,2) と (2,1,3)。0と1を中心に重ねると、(0,1,2)は潰れて消え、
			// (2,1,3) = 縁0・中心・縁1 の三角形だけが残る。
			const std::size_t v = quad * 4;
			m_vertices[v + 0].Position = center;
			m_vertices[v + 0].Diffuse = centerColor;
			m_vertices[v + 1].Position = center;
			m_vertices[v + 1].Diffuse = centerColor;
			m_vertices[v + 2].Position = rim0;
			m_vertices[v + 2].Diffuse = edgeColor;
			m_vertices[v + 3].Position = rim1;
			m_vertices[v + 3].Diffuse = edgeColor;
			++quad;
		}
		return quad;
	}

	static constexpr std::size_t DISC_SEGMENTS = 12;
	// 円盤1枚=12枚の四角形。火球3枚+爆発2枚+口の火2枚+尾の炎を数十枚まかなえる数にする。
	static constexpr std::size_t MAX_QUADS = DISC_SEGMENTS * 120;
	static constexpr float PUFF_INTERVAL = 0.025f;
	static constexpr float PUFF_LIFETIME = 0.32f;
	static constexpr float BLAST_LIFETIME = 0.38f;

	std::vector<Ball> m_balls;
	std::vector<Puff> m_puffs;
	std::vector<Blast> m_blasts;
	std::vector<VERTEX_3D> m_vertices;
	CVertexBuffer<VERTEX_3D> m_vertexBuffer;
	CIndexBuffer m_indexBuffer;
	CMaterial m_material;
	CShader m_shader;
	Vector3 m_mouthPosition{};
	float m_mouthCharge = 0.0f;
	float m_time = 0.0f;
	bool m_initialized = false;
};
