#pragma once
#include <array>
#include "CommonTypes.h"
#include "CTexture.h"
#include "CMaterial.h"
#include "CShader.h"
#include "CVertexBuffer.h"
#include "CIndexBuffer.h"

/**
 * @brief 2Dスプライトを描画するクラス
 *
 * テクスチャ、マテリアル、頂点・インデックスバッファを利用して、
 * スプライト（矩形ポリゴン）を描画する機能を提供します。
 */
class CSprite
{
	/// @brief スプライトで使用する共通シェーダ（全インスタンスで共有）
	static CShader m_Shader;

	/// @brief スプライトに使用されるテクスチャ
	CTexture m_Texture{};

	/// @brief スプライトに適用されるマテリアル
	CMaterial m_Material{};

	/// @brief スプライトの横幅（ピクセル）
	int m_Width = 0;

	/// @brief スプライトの縦幅（ピクセル）
	int m_Height = 0;

	/// @brief インデックスバッファ
	CIndexBuffer m_IndexBuffer;

	/// @brief 頂点バッファ（4頂点）
	CVertexBuffer<VERTEX_3D> m_VertexBuffer;

	/// @brief 頂点情報（位置・UV・カラーなど）
	std::vector<VERTEX_3D> m_Vertices{};

	/// @brief マテリアルの生成（内部用）
	void CreateMtrl();

	/// @brief シェーダの生成（内部用）
	void CreateShader();

	/**
	 * @brief テクスチャの読み込みと初期化（内部用）
	 * @param texfilename テクスチャファイル名
	 */
	void CreateTexture(const std::string& texfilename);

public:
	/**
	 * @brief スプライトのコンストラクタ
	 *
	 * @param width スプライトの幅
	 * @param height スプライトの高さ
	 * @param texfilename 使用するテクスチャファイルの名前
	 * @param uv UV座標（左上、右上、左下、右下）の配列。省略時は全体（0,0〜1,1）。
	 */
	CSprite(int width, int height, std::string texfilename,
		std::array<Vector2, 4> uv = { Vector2(0,0), Vector2(1,0), Vector2(0,1), Vector2(1,1) });
	
	/// @brief デストラクタ
	virtual ~CSprite() {}

	/**
	 * @brief スプライトを描画
	 *
	 * @param scale スケール（X, Y, Z）
	 * @param rotation 回転（ラジアン、X, Y, Z）
	 * @param pos ワールド座標位置
	 */
	void Draw(Vector3 scale, Vector3 rotation, Vector3 pos);

	/**
	 * @brief スプライトを描画
	 *
	 * @param Matrix4x4 行列
	 */
	void Draw(Matrix4x4 mtx);


	/// @brief リソースの解放（明示的に呼び出すことを推奨）
	void Dispose();

	/**
	 * @brief UV座標を変更
	 *
	 * @param uv 新しいUV座標（配列長4、左上・右上・左下・右下の順）
	 */
	void ModifyUV(const Vector2* uv);

	/**
	 * @brief MATERIALを変更
	 *
	 * @param mtrl 新しいマテリアル
	 */
	void ModifyMtrl(MATERIAL mtrl);

	int GetWidth() { return m_Width; }

	int GetHeight() { return m_Height; }
};
