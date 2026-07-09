#pragma once
#include "CommonTypes.h"
#include "CMesh.h"

/**
 * @brief 球体メッシュを生成・管理するクラス
 *
 * CMesh を継承し、指定された分割数と半径に基づいて球体の頂点・インデックスを生成します。
 */
class CSphereMesh : public CMesh {
public:
    /**
     * @brief 球メッシュの初期化を行う
     *
     * @param radius 球の半径
     * @param color 頂点カラー
     * @param division_horizontal 経度方向（横）の分割数
     * @param division_vertical 緯度方向（縦）の分割数
     */
    void Init(float radius,
        Color color,
        int division_horizontal,
        int division_vertical)
    {
        m_divX = division_horizontal;
        m_divY = division_vertical;
        m_radius = radius;
        m_color = color;

        CreateIndex();
        CreateVertex();
    }

    /**
     * @brief 球体の頂点データを生成する
     *
     * 仰角と方位角に基づき球体の頂点位置と法線ベクトルを計算して格納します。
     */
    void CreateVertex() {
        m_vertices.clear();

        float azimuth = 0.0f;   ///< 方位角
        float elevation = 0.0f; ///< 仰角

        Vector3 Normal;

        for (unsigned int y = 0; y <= m_divY; y++) {
            elevation = (PI * (float)y) / (float)m_divY;
            float r = m_radius * sinf(elevation);

            for (unsigned int x = 0; x <= m_divX; x++) {
                azimuth = (2 * PI * (float)x) / (float)m_divX;

                VERTEX_3D v;
                v.Position.x = r * cosf(azimuth);
                v.Position.y = m_radius * cosf(elevation);
                v.Position.z = r * sinf(azimuth);

                Normalize(v.Position, Normal); ///< 法線ベクトルを正規化
                v.Normal = Normal;
                v.Diffuse = m_color;

                m_vertices.emplace_back(v);
            }
        }
    }

    /**
     * @brief 球体のインデックスデータを生成する
     *
     * 頂点インデックスを三角形ポリゴンのリストとして生成します。
     */
    void CreateIndex() {
        struct FACE {
            unsigned int idx[3];
        };

        m_indices.clear();

        for (unsigned int y = 0; y < m_divY; y++) {
            for (unsigned int x = 0; x < m_divX; x++) {
                int count = (m_divX + 1) * y + x;

                // 上三角形
                m_indices.emplace_back(count);
                m_indices.emplace_back(count + 1);
                m_indices.emplace_back(count + 1 + (m_divX + 1));

                // 下三角形
                m_indices.emplace_back(count);
                m_indices.emplace_back(count + (m_divX + 1) + 1);
                m_indices.emplace_back(count + (m_divX + 1));
            }
        }
    }

private:
    /**
     * @brief 法線ベクトルを正規化する
     *
     * @param vec 入力ベクトル
     * @param Normal 出力される正規化ベクトル（参照渡し）
     */
    void Normalize(Vector3 vec, Vector3& Normal) {
        vec.Normalize();
        Normal = vec;
    }

    unsigned int m_divX = 1;   ///< 横方向の分割数（経度）
    unsigned int m_divY = 1;   ///< 縦方向の分割数（緯度）
    float m_radius = 100.0f;   ///< 球の半径
    Color m_color;             ///< 頂点カラー
};
