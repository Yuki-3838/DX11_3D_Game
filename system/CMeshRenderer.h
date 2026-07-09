#pragma once
#include	"CVertexBuffer.h"
#include	"CIndexBuffer.h"
#include	"CMesh.h"

class CMeshRenderer {
protected:
	CVertexBuffer<VERTEX_3D>	m_VertexBuffer;		// 頂点バッファ
	CIndexBuffer				m_IndexBuffer;		// インデックスバッファ
	int							m_IndexNum = 0;		// インデックス数
public:
	virtual ~CMeshRenderer(){}

	virtual void Init(CMesh& mesh) 
	{
		m_VertexBuffer.Create(mesh.GetVertices());
		m_IndexBuffer.Create(mesh.GetIndices());
		m_IndexNum = static_cast<int>(mesh.GetIndices().size());
	}

	// 描画前処理
	virtual void BeforeDraw(D3D_PRIMITIVE_TOPOLOGY primtype)
	{
		ID3D11DeviceContext* devicecontext;

		devicecontext = Renderer::GetDeviceContext();

		// トポロジーをセット（旧プリミティブタイプ）
		devicecontext->IASetPrimitiveTopology(primtype);

		m_VertexBuffer.SetGPU();			// 頂点バッファをセット
		m_IndexBuffer.SetGPU();				// インデックスバッファをセット
	}

	// 描画前処理
	virtual void BeforeDraw()
	{
		ID3D11DeviceContext* devicecontext;

		devicecontext = Renderer::GetDeviceContext();

		// トポロジーをセット（旧プリミティブタイプ）
		devicecontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		m_VertexBuffer.SetGPU();			// 頂点バッファをセット
		m_IndexBuffer.SetGPU();				// インデックスバッファをセット
	}

	// サブセット描画
	virtual void DrawSubset(unsigned int indexnum,unsigned int baseindex,unsigned int basevertexindex ) 
	{
		Renderer::GetDeviceContext()->DrawIndexed(
			indexnum,								// 描画するインデックス数（面数×３）
			baseindex,								// 最初のインデックスバッファの位置
			basevertexindex);						// 頂点バッファの最初から使う
	}

	// 描画
	virtual void Draw() 
	{
		BeforeDraw();								// 描画前処理

		Renderer::GetDeviceContext()->DrawIndexed(
			m_IndexNum,								// 描画するインデックス数（面数×３）
			0,										// 最初のインデックスバッファの位置
			0);										// 頂点バッファの最初から使う
	}

	// 描画
	virtual void Draw(D3D_PRIMITIVE_TOPOLOGY primtype)
	{
		BeforeDraw(primtype);						// 描画前処理

		Renderer::GetDeviceContext()->DrawIndexed(
			m_IndexNum*2,							// 描画するインデックス数（面数×３）
			0,										// 最初のインデックスバッファの位置
			0);										// 頂点バッファの最初から使う
	}

	// 頂点バッファを更新
	void Modify(const std::vector<VERTEX_3D>& vertices)
	{
		m_VertexBuffer.Modify(vertices);
	}
};
