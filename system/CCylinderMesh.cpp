#include	<iostream>
#include	"CCylinderMesh.h"

/**
 * @brief 円柱メッシュの初期化処理
 *
 * 指定された分割数・半径・高さ・色に基づいて円柱の頂点とインデックスを生成します。
 *
 * @param divx 円周方向の分割数
 * @param radius 円柱の半径
 * @param height 円柱の高さ
 * @param color 頂点カラー
 */
void CCylinderMesh::Init(
	int		divx,
	float	radius,
	float	height,
	Color color)
{
	// サイズセット（幅と高さ）（XY平面）
	m_height = height;
	m_width = 2.0f * PI * radius;		// 直径×円周率
	m_radius = radius;

	// 分割数
	m_division_x = divx;

	// 頂点カラー
	m_color = color;

	// 頂点データ生成
	CreateVertex();
}

/**
 * @brief 円柱メッシュの頂点とインデックスデータを生成する
 *
 * 側面、上面、底面を構成するすべての頂点・インデックスを計算し、
 * m_vertices および m_indices に格納します。
 *
 * @note
 * 側面・上面・底面では法線方向が異なるため、
 * 同じ座標でも頂点を共有しない。
 */
void CCylinderMesh::CreateVertex()
{
	// 頂点・インデックスデータクリア
	m_vertices.clear();
	m_indices.clear();

	// 分割数の安全対策
	if (m_division_x < 3) {
		m_division_x = 3;
	}

	// 半径・高さの安全対策
	if (m_radius <= 0.0f) {
		m_radius = 1.0f;
	}

	if (m_height <= 0.0f) {
		m_height = 1.0f;
	}

	const unsigned int div = static_cast<unsigned int>(m_division_x);

	auto AddVertex = [&](const Vector3& position, const Vector3& normal)
		{
			VERTEX_3D v{};

			v.Position = position;

			v.Normal = normal;
			v.Normal.Normalize();

			v.Diffuse = m_color;

			m_vertices.emplace_back(v);
		};

	// ------------------------------------------------------------
	// 1. 側面用の頂点を作成
	//    側面の法線は、中心軸から外側へ向かうベクトル
	// ------------------------------------------------------------

	const unsigned int sideBottomStart =
		static_cast<unsigned int>(m_vertices.size());

	// 側面 下リング
	for (unsigned int i = 0; i <= div; ++i)
	{
		const float azimuth =
			(2.0f * PI * static_cast<float>(i)) /
			static_cast<float>(div);

		const float c = cosf(azimuth);
		const float s = sinf(azimuth);

		Vector3 position(
			m_radius * c,
			0.0f,
			m_radius * s
		);

		// 円柱側面の正しい法線
		// Y成分は0。横方向だけを見る。
		Vector3 normal(
			c,
			0.0f,
			s
		);

		AddVertex(position, normal);
	}

	const unsigned int sideTopStart =
		static_cast<unsigned int>(m_vertices.size());

	// 側面 上リング
	for (unsigned int i = 0; i <= div; ++i)
	{
		const float azimuth =
			(2.0f * PI * static_cast<float>(i)) /
			static_cast<float>(div);

		const float c = cosf(azimuth);
		const float s = sinf(azimuth);

		Vector3 position(
			m_radius * c,
			m_height,
			m_radius * s
		);

		// 円柱側面の正しい法線
		Vector3 normal(
			c,
			0.0f,
			s
		);

		AddVertex(position, normal);
	}

	// ------------------------------------------------------------
	// 2. 側面インデックスを作成
	//    三角形リスト
	// ------------------------------------------------------------
	for (unsigned int i = 0; i < div; ++i)
	{
		const unsigned int bottom0 = sideBottomStart + i;
		const unsigned int bottom1 = sideBottomStart + i + 1;
		const unsigned int top0 = sideTopStart + i;
		const unsigned int top1 = sideTopStart + i + 1;

		// 1枚目
		m_indices.emplace_back(bottom0);
		m_indices.emplace_back(top1);
		m_indices.emplace_back(bottom1);

		// 2枚目
		m_indices.emplace_back(bottom0);
		m_indices.emplace_back(top0);
		m_indices.emplace_back(top1);
	}

	// ------------------------------------------------------------
	// 3. 底面用の頂点を作成
	//    底面の法線はすべて下向き
	// ------------------------------------------------------------

	const unsigned int bottomCenterIndex =
		static_cast<unsigned int>(m_vertices.size());

	AddVertex(
		Vector3(0.0f, 0.0f, 0.0f),
		Vector3(0.0f, -1.0f, 0.0f)
	);

	const unsigned int bottomRingStart =
		static_cast<unsigned int>(m_vertices.size());

	for (unsigned int i = 0; i <= div; ++i)
	{
		const float azimuth =
			(2.0f * PI * static_cast<float>(i)) /
			static_cast<float>(div);

		const float c = cosf(azimuth);
		const float s = sinf(azimuth);

		Vector3 position(
			m_radius * c,
			0.0f,
			m_radius * s
		);

		// 底面なので、外周頂点も法線は下向き
		Vector3 normal(
			0.0f,
			-1.0f,
			0.0f
		);

		AddVertex(position, normal);
	}

	// 底面インデックス
	for (unsigned int i = 0; i < div; ++i)
	{
		m_indices.emplace_back(bottomCenterIndex);
		m_indices.emplace_back(bottomRingStart + i);
		m_indices.emplace_back(bottomRingStart + i + 1);
	}

	// ------------------------------------------------------------
	// 4. 上面用の頂点を作成
	//    上面の法線はすべて上向き
	// ------------------------------------------------------------

	const unsigned int topCenterIndex =
		static_cast<unsigned int>(m_vertices.size());

	AddVertex(
		Vector3(0.0f, m_height, 0.0f),
		Vector3(0.0f, 1.0f, 0.0f)
	);

	const unsigned int topRingStart =
		static_cast<unsigned int>(m_vertices.size());

	for (unsigned int i = 0; i <= div; ++i)
	{
		const float azimuth =
			(2.0f * PI * static_cast<float>(i)) /
			static_cast<float>(div);

		const float c = cosf(azimuth);
		const float s = sinf(azimuth);

		Vector3 position(
			m_radius * c,
			m_height,
			m_radius * s
		);

		// 上面なので、外周頂点も法線は上向き
		Vector3 normal(
			0.0f,
			1.0f,
			0.0f
		);

		AddVertex(position, normal);
	}

	// 上面インデックス
	for (unsigned int i = 0; i < div; ++i)
	{
		m_indices.emplace_back(topCenterIndex);
		m_indices.emplace_back(topRingStart + i + 1);
		m_indices.emplace_back(topRingStart + i);
	}
}