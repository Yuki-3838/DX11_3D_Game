#pragma once

#include <algorithm>
#include <vector>

#include "CIndexBuffer.h"
#include "CMaterial.h"
#include "CShader.h"
#include "CVertexBuffer.h"
#include "CommonTypes.h"
#include "renderer.h"

/**
 * @class WeaponTrail
 * @brief 武器の刃が通った軌跡をリボン状に描画する。
 *
 * @details
 * **特定の武器やキャラクターに依存しない。**
 * 毎フレーム「刃の根元と先端のワールド座標」を渡すだけで軌跡を作る。
 * この2点は`CAnimationMesh::GetSwordWorldSweep()`が武器ボーンから求めているため、
 * 剣モデルやプレイヤーモデルを差し替えても、`CharacterModelProfile`の
 * `weaponBone`が解決できていればそのまま動く。
 *
 * 敵やほかのキャラクターの武器にも、同じようにインスタンスを1つ持たせれば使える。
 *
 * 使い方:
 *   Initialize()         … シーンの初期化時に1度
 *   Push(base, tip)      … 攻撃判定が出ている間、毎フレーム
 *   Update(deltaSeconds) … 毎フレーム(古い軌跡を消す)
 *   Draw()               … 描画時
 */
class WeaponTrail
{
public:
	/**
	 * @param maxNodes 保持する軌跡の分割数。多いほど長く滑らかになる。
	 * @param lifetimeSeconds 1つの軌跡が消えるまでの時間。
	 */
	void Initialize(std::size_t maxNodes = 24, float lifetimeSeconds = 0.16f)
	{
		m_maxNodes = std::max<std::size_t>(maxNodes, 2);
		m_lifetime = std::max(lifetimeSeconds, 0.01f);
		m_nodes.clear();
		m_nodes.reserve(m_maxNodes);

		// 分割数ぶんの四角形(=2三角形)を張れるだけの頂点・インデックスを先に確保する。
		// 毎フレームの確保を避けるため、以後はModify()で中身だけ書き換える。
		const std::size_t vertexCount = m_maxNodes * 2;
		const std::size_t quadCount = m_maxNodes - 1;

		m_vertices.assign(vertexCount, VERTEX_3D{});
		for (auto& v : m_vertices)
		{
			v.Normal = Vector3(0.0f, 1.0f, 0.0f);
			v.TexCoord = Vector2(0.0f, 0.0f);
			v.Diffuse = Color(1.0f, 1.0f, 1.0f, 0.0f);
		}
		m_vertexBuffer.Create(m_vertices);

		std::vector<uint32_t> indices;
		indices.reserve(quadCount * 6);
		for (std::size_t i = 0; i < quadCount; ++i)
		{
			const uint32_t base = static_cast<uint32_t>(i * 2);
			// 表裏どちらから見ても消えないよう、カリングは描画時に切る。
			indices.push_back(base + 0);
			indices.push_back(base + 1);
			indices.push_back(base + 2);
			indices.push_back(base + 2);
			indices.push_back(base + 1);
			indices.push_back(base + 3);
		}
		m_indexBuffer.Create(indices);
		m_indexCount = static_cast<unsigned int>(indices.size());

		MATERIAL material{};
		material.Ambient = Color(0.0f, 0.0f, 0.0f, 0.0f);
		material.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
		material.Emission = Color(0.0f, 0.0f, 0.0f, 0.0f);
		material.Specular = Color(0.0f, 0.0f, 0.0f, 0.0f);
		material.Shiness = 0.0f;
		material.TextureEnable = FALSE;
		m_material.Create(material);

		// 面の向きで明暗が変わると軌跡が途中で暗くなるため、無照明で描く。
		m_shader.Create(
			"shader/unlitTextureVS.hlsl",
			"shader/unlitTexturePS.hlsl");

		m_initialized = true;
	}

	/** 軌跡を消す。攻撃の開始時やリセット時に呼ぶ。 */
	void Clear() { m_nodes.clear(); }

	bool IsEmpty() const { return m_nodes.empty(); }

	/**
	 * @brief 現在の刃の位置を軌跡へ追加する。
	 * @param base 刃の根元(柄側)のワールド座標。
	 * @param tip  刃の先端のワールド座標。
	 */
	void Push(const Vector3& base, const Vector3& tip)
	{
		if (!m_initialized)
			return;

		// ほとんど動いていないフレームで点を増やすと、
		// 同じ位置に重なった面ができて濃さがまだらになる。
		if (!m_nodes.empty())
		{
			const Vector3 delta = tip - m_nodes.back().tip;
			if (delta.LengthSquared() < 0.25f)
				return;
		}

		if (m_nodes.size() >= m_maxNodes)
			m_nodes.erase(m_nodes.begin());
		m_nodes.push_back({ base, tip, 0.0f });
	}

	/** 古い軌跡を消す。毎フレーム呼ぶ。 */
	void Update(float deltaSeconds)
	{
		if (deltaSeconds <= 0.0f)
			return;
		for (auto& node : m_nodes)
			node.age += deltaSeconds;
		m_nodes.erase(
			std::remove_if(
				m_nodes.begin(),
				m_nodes.end(),
				[this](const Node& n) { return n.age >= m_lifetime; }),
			m_nodes.end());
	}

	/**
	 * @brief 軌跡を描画する。
	 * @param color 軌跡の色。アルファは古さと刃の根元へ向けて自動で落とす。
	 */
	void Draw(const Color& color)
	{
		if (!m_initialized || m_nodes.size() < 2)
			return;

		// 新しいほど濃く、根元ほど薄くする。
		for (std::size_t i = 0; i < m_nodes.size(); ++i)
		{
			const Node& node = m_nodes[i];
			const float ageRatio = std::clamp(node.age / m_lifetime, 0.0f, 1.0f);
			// 古さで薄くする。二乗にして、消え際を早くする。
			const float lifeAlpha = (1.0f - ageRatio) * (1.0f - ageRatio);

			Color tipColor = color;
			tipColor.w = color.w * lifeAlpha;
			Color baseColor = color;
			// 刃の根元側は薄くして、リボンの内側を自然に消す。
			baseColor.w = color.w * lifeAlpha * 0.15f;

			m_vertices[i * 2 + 0].Position = node.base;
			m_vertices[i * 2 + 0].Diffuse = baseColor;
			m_vertices[i * 2 + 1].Position = node.tip;
			m_vertices[i * 2 + 1].Diffuse = tipColor;
		}
		// 使わない分は完全に透明かつ最後の点へ潰し、面が残らないようにする。
		for (std::size_t i = m_nodes.size(); i < m_maxNodes; ++i)
		{
			m_vertices[i * 2 + 0].Position = m_nodes.back().base;
			m_vertices[i * 2 + 0].Diffuse = Color(0.0f, 0.0f, 0.0f, 0.0f);
			m_vertices[i * 2 + 1].Position = m_nodes.back().base;
			m_vertices[i * 2 + 1].Diffuse = Color(0.0f, 0.0f, 0.0f, 0.0f);
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

		// リボンは薄い板なので、裏返っても消えないようカリングを切る。
		// 半透明なので深度書き込みも切り、後ろの軌跡が消えないようにする。
		Renderer::DisableCulling(false);
		Renderer::SetBlendState(BS_ALPHABLEND);
		Renderer::SetDepthEnable(false);

		context->DrawIndexed(m_indexCount, 0, 0);

		// 以降の描画へ影響しないよう戻す。
		Renderer::SetDepthEnable(true);
		Renderer::DisableCulling(true);
	}

private:
	struct Node
	{
		Vector3 base{};
		Vector3 tip{};
		float age = 0.0f;
	};

	std::vector<Node> m_nodes;
	std::vector<VERTEX_3D> m_vertices;
	CVertexBuffer<VERTEX_3D> m_vertexBuffer;
	CIndexBuffer m_indexBuffer;
	CMaterial m_material;
	CShader m_shader;
	std::size_t m_maxNodes = 24;
	float m_lifetime = 0.16f;
	unsigned int m_indexCount = 0;
	bool m_initialized = false;
};
