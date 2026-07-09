#pragma once
#include	"CommonTypes.h"
#include	"CMesh.h"

/**
 * @brief 円柱メッシュを生成・管理するクラス
 *
 * 指定された分割数、半径、高さに基づいて、円柱の頂点データを生成します。
 * CMesh を継承し、基本的なメッシュ操作（頂点・インデックスの生成）を提供します。
 */
class CCylinderMesh : public CMesh {
public:
	/**
	 * @brief 円柱メッシュの初期化を行う
	 *
	 * @param divx 円周方向の分割数
	 * @param radius 円柱の半径
	 * @param height 円柱の高さ
	 * @param color 頂点の描画色
	 */
	void Init(
		int		divx,
		float	radius,
		float	height,
		Color color);

	/**
	 * @brief 頂点データを生成する
	 *
	 * 円柱の上下円盤および側面の頂点情報を生成します。
	 */
	void CreateVertex();

private:
	/**
	 * @brief 円周方向の分割数
	 */
	unsigned int m_division_x = 1;

	/**
	 * @brief 高さ方向の分割数（未使用）
	 */
	unsigned int m_division_y = 1;

	/**
	 * @brief 円柱の半径
	 */
	float  m_radius = 100.0f;

	/**
	 * @brief 円柱の高さ
	 */
	float  m_height = 100.0f;

	/**
	 * @brief 円周長（= 半径から自動計算）
	 */
	float  m_width;

	/**
	 * @brief 頂点カラー
	 */
	Color m_color;
};
