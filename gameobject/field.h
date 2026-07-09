#pragma once

#include	<cstdint>
#include	"gameobject.h"
#include	"../system/commontypes.h"	
#include	"../system/IScene.h"
#include	"../system/CIndexBuffer.h"
#include	"../system/CVertexBuffer.h"
#include	"../system/CShader.h"
#include	"../system/CMaterial.h"
#include	"../system/CTexture.h"

class field : public gameobject {
public:
	// ISceneのポインタを受け取るコンストラクタを追加
	field(IScene* scene) :gameobject(scene) {}

	void update(uint64_t delta) override;
	void draw(uint64_t delta) override;
	void init() override;
	void dispose() override;

private:
	// 描画の為の情報（メッシュに関わる情報）
	CIndexBuffer				m_IndexBuffer;				// インデックスバッファ
	CVertexBuffer<VERTEX_3D>	m_VertexBuffer;				// 頂点バッファ

	// 描画の為の情報（見た目に関わる部分）
	CShader						m_Shader;					// シェーダー
	CMaterial					m_Material;					// マテリアル
	CTexture					m_Texture;					// テクスチャ
};