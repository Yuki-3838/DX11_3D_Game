#include	<iostream>
#include	"CConeMesh.h"

/**
 * @brief 円錐メッシュを初期化する
 *
 * 指定された分割数、半径、高さ、頂点カラーに基づいて、円錐の頂点・インデックスを生成します。
 * @param divx 横方向の分割数
 * @param radius 円錐底面の半径
 * @param height 円錐の高さ
 * @param color 頂点の色
 * @param bottomup true: 底面→頂点の順で頂点生成、false: 頂点→底面の順で頂点生成
 */
void CConeMesh::Init(
	int		divx,
	float	radius,
	float	height,
	Color color,
	bool bottomup)
{
	// サイズセット（幅と高さ）（XY平面）
	m_height = height;
	m_width = 2.0f * PI * radius;		// 直径×円周率
	m_radius = radius;

	// 分割数
	m_division_x = divx;

	// 頂点カラー
	m_color = color;

	// 頂点デタ生成
	if (bottomup) {
		CreateVertex();
	}
	else {
		CreateVertexTopDown();
	}
}

/**
 * @brief 頂点を「底面→頂点」の順で生成する
 *
 * 円錐の頂点配列を底面から山頂に向かって構成し、インデックスも追加します。
 * 底面周辺の三角形および底面自体のポリゴンも含めて構築します。
 */
void CConeMesh::CreateVertex()
{
	m_vertices.clear();
	m_indices.clear();

	if (m_division_x < 3) {
		m_division_x = 3;
	}

	if (m_radius <= 0.0f) {
		m_radius = 1.0f;
	}

	if (m_height <= 0.0f) {
		m_height = 1.0f;
	}

	auto AddVertex = [&](const Vector3& pos, const Vector3& normal)
		{
			VERTEX_3D v{};

			v.Position = pos;
			v.Normal = normal;
			v.Normal.Normalize();
			v.Diffuse = m_color;

			m_vertices.emplace_back(v);
		};

	auto MakeSideNormal = [&](float azimuth)
		{
			float c = cosf(azimuth);
			float s = sinf(azimuth);

			Vector3 n(
				c,
				m_radius / m_height,
				s
			);

			n.Normalize();
			return n;
		};

	// ------------------------------------------------------------
	// 側面
	// ------------------------------------------------------------
	for (unsigned int i = 0; i < m_division_x; i++) {
		float azimuth0 = (2.0f * PI * static_cast<float>(i)) / static_cast<float>(m_division_x);
		float azimuth1 = (2.0f * PI * static_cast<float>(i + 1)) / static_cast<float>(m_division_x);

		Vector3 topPos(0.0f, m_height, 0.0f);

		Vector3 p0(
			m_radius * cosf(azimuth0),
			0.0f,
			m_radius * sinf(azimuth0)
		);

		Vector3 p1(
			m_radius * cosf(azimuth1),
			0.0f,
			m_radius * sinf(azimuth1)
		);

		Vector3 n0 = MakeSideNormal(azimuth0);
		Vector3 n1 = MakeSideNormal(azimuth1);

		// 頂点の法線は、その面の中間方向にしておく
		Vector3 topNormal = n0 + n1;
		topNormal.Normalize();

		unsigned int start = static_cast<unsigned int>(m_vertices.size());

		AddVertex(topPos, topNormal);
		AddVertex(p1, n1);
		AddVertex(p0, n0);

		m_indices.emplace_back(start + 0);
		m_indices.emplace_back(start + 1);
		m_indices.emplace_back(start + 2);
	}

	// ------------------------------------------------------------
	// 底面
	// ------------------------------------------------------------
	unsigned int bottomCenterIndex = static_cast<unsigned int>(m_vertices.size());

	AddVertex(
		Vector3(0.0f, 0.0f, 0.0f),
		Vector3(0.0f, -1.0f, 0.0f)
	);

	unsigned int bottomRingStart = static_cast<unsigned int>(m_vertices.size());

	for (unsigned int i = 0; i <= m_division_x; i++) {
		float azimuth = (2.0f * PI * static_cast<float>(i)) / static_cast<float>(m_division_x);

		Vector3 p(
			m_radius * cosf(azimuth),
			0.0f,
			m_radius * sinf(azimuth)
		);

		AddVertex(
			p,
			Vector3(0.0f, -1.0f, 0.0f)
		);
	}

	for (unsigned int i = 0; i < m_division_x; i++) {
		m_indices.emplace_back(bottomCenterIndex);
		m_indices.emplace_back(bottomRingStart + i);
		m_indices.emplace_back(bottomRingStart + i + 1);
	}
}
/**
 * @brief 頂点を「頂点→底面」の順で生成する
 *
 * 円錐の頂点配列を山頂から底面に向かって構成し、インデックスも追加します。
 * 底面周辺の三角形および底面自体のポリゴンも含めて構築します。
 */
void CConeMesh::CreateVertexTopDown()
{
	// 頂点・インデックスデータクリア
	m_vertices.clear();
	m_indices.clear();

	// 安全対策
	if (m_division_x < 3) {
		m_division_x = 3;
	}

	if (m_radius <= 0.0f) {
		m_radius = 1.0f;
	}

	if (m_height <= 0.0f) {
		m_height = 1.0f;
	}

	auto AddVertex = [&](const Vector3& pos, const Vector3& normal)
		{
			VERTEX_3D v{};

			v.Position = pos;
			v.Normal = normal;
			v.Normal.Normalize();
			v.Diffuse = m_color;

			m_vertices.emplace_back(v);
		};

	auto MakeSideNormal = [&](float azimuth)
		{
			float c = cosf(azimuth);
			float s = sinf(azimuth);

			// ----------------------------------------------------
			// TopDown版
			// 頂点が Y=0、底面が Y=m_height にあるので、
			// 側面法線のY成分はマイナス方向になる。
			// ----------------------------------------------------
			Vector3 n(
				c,
				-(m_radius / m_height),
				s
			);

			n.Normalize();
			return n;
		};

	// ------------------------------------------------------------
	// 1. 側面用の頂点・インデックスを作成
	// ------------------------------------------------------------
	for (unsigned int i = 0; i < static_cast<unsigned int>(m_division_x); i++)
	{
		float azimuth0 =
			(2.0f * PI * static_cast<float>(i)) /
			static_cast<float>(m_division_x);

		float azimuth1 =
			(2.0f * PI * static_cast<float>(i + 1)) /
			static_cast<float>(m_division_x);

		Vector3 topPos(
			0.0f,
			0.0f,
			0.0f
		);

		Vector3 p0(
			m_radius * cosf(azimuth0),
			m_height,
			m_radius * sinf(azimuth0)
		);

		Vector3 p1(
			m_radius * cosf(azimuth1),
			m_height,
			m_radius * sinf(azimuth1)
		);

		Vector3 n0 = MakeSideNormal(azimuth0);
		Vector3 n1 = MakeSideNormal(azimuth1);

		// 頂点部分の法線は、その三角形の中間方向にする
		Vector3 topNormal = n0 + n1;
		topNormal.Normalize();

		unsigned int start =
			static_cast<unsigned int>(m_vertices.size());

		// 側面用頂点
		AddVertex(topPos, topNormal);
		AddVertex(p0, n0);
		AddVertex(p1, n1);

		// 側面三角形
		m_indices.emplace_back(start + 0);	// 頂点
		m_indices.emplace_back(start + 1);	// 現在の底面頂点
		m_indices.emplace_back(start + 2);	// 次の底面頂点
	}

	// ------------------------------------------------------------
	// 2. 底面用の頂点・インデックスを作成
	// ------------------------------------------------------------

	// 底面中心
	unsigned int bottomCenterIndex =
		static_cast<unsigned int>(m_vertices.size());

	// TopDown版では底面が Y=m_height 側にあるため、
	// 外向き法線は +Y
	AddVertex(
		Vector3(0.0f, m_height, 0.0f),
		Vector3(0.0f, 1.0f, 0.0f)
	);

	// 底面外周
	unsigned int bottomRingStart =
		static_cast<unsigned int>(m_vertices.size());

	for (unsigned int i = 0; i <= static_cast<unsigned int>(m_division_x); i++)
	{
		float azimuth =
			(2.0f * PI * static_cast<float>(i)) /
			static_cast<float>(m_division_x);

		Vector3 p(
			m_radius * cosf(azimuth),
			m_height,
			m_radius * sinf(azimuth)
		);

		// 底面用頂点なので、法線はすべて +Y
		AddVertex(
			p,
			Vector3(0.0f, 1.0f, 0.0f)
		);
	}

	// 底面三角形
	for (unsigned int i = 0; i < static_cast<unsigned int>(m_division_x); i++)
	{
		m_indices.emplace_back(bottomCenterIndex);
		m_indices.emplace_back(bottomRingStart + i + 1);
		m_indices.emplace_back(bottomRingStart + i);
	}
}