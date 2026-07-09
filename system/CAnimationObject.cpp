#include	"CAnimationObject.h"

void CAnimationObject::Init()
{
	// ボーンコンビネーション行列初期化
	m_BoneCombMatrix.Create();							// 20240723 
}

void CAnimationObject::Update(float dt) 
{
	int frame = static_cast<int>(m_CurrentFrame);
	// アニメーションメッシュ更新
	m_AnimMesh->Update(m_BoneCombMatrix,frame);
	m_CurrentFrame+=dt;
}

void CAnimationObject::BlendUpdate(float dt)
{
	int frame = static_cast<int>(m_CurrentFrame);

	// アニメーションメッシュ更新
	m_AnimMesh->UpdateBlended(m_BoneCombMatrix, frame);

	m_CurrentFrame += dt;
}


void CAnimationObject::Draw()
{
	// ボーンコンビネーションを定数バッファへ反映させる
	m_BoneCombMatrix.Update();

	// 定数バッファGPUへセット
	m_BoneCombMatrix.SetGPU();

	// メッシュ描画
	m_AnimMesh->Draw();
}

