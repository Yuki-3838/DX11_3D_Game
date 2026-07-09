#pragma once
#include "CommonTypes.h"
#include "CMesh.h"

/**
 * @brief 円錐形状のメッシュを生成・管理するクラス
 *
 * CMesh を継承し、分割数・半径・高さに応じて頂点・インデックス（面）データを生成します。
 */
class CConeMesh : public CMesh {
	/**
	 * @brief 三角形面を構成する頂点インデックス情報
	 */
	struct Face {
		unsigned int idx[3]; ///< 三角形を構成する3つの頂点インデックス
	};

public:
	/**
	 * @brief 円錐メッシュを初期化する
	 *
	 * @param divx 横方向の分割数
	 * @param radius 円錐底面の半径
	 * @param height 円錐の高さ
	 * @param color 頂点カラー
	 * @param bottomup true: 下から上へ（底面→頂点）生成、false: 上から下へ（頂点→底面）生成
	 */
	void Init(
		int divx,
		float radius,
		float height,
		Color color,
		bool bottomup = true);

	/**
	 * @brief 頂点データを生成（上から下）
	 *
	 * 通常はこちらを使用。頂点（山頂）→底面方向に頂点データを構成。
	 */
	void CreateVertex();

	/**
	 * @brief 頂点データを生成（下から上）
	 *
	 * 底面→山頂の順で頂点データを構成したい場合に使用。
	 */
	void CreateVertexTopDown();

	// インデックス生成関数は未実装（必要に応じて追加可）

	/**
	 * @brief 面情報（インデックス）の配列
	 *
	 * 頂点インデックス3つから構成される三角形の一覧です。
	 */
	std::vector<Face> m_faces;

private:
	/**
	 * @brief 横方向（経度）の分割数
	 */
	unsigned int m_division_x = 1;

	/**
	 * @brief 縦方向（高さ方向）の分割数（未使用）
	 */
	unsigned int m_division_y = 1;

	/**
	 * @brief 円錐底面の半径
	 */
	float m_radius = 100.0f;

	/**
	 * @brief 円錐の高さ
	 */
	float m_height = 100.0f;

	/**
	 * @brief 円錐の幅（必要に応じて半径から計算）
	 */
	float m_width;

	/**
	 * @brief 頂点カラー
	 */
	Color m_color;
};
