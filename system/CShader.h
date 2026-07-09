#pragma once
#include	<wrl/client.h>
#include	<string>

#include	<d3d11.h>
#include	"NonCopyable.h"

using Microsoft::WRL::ComPtr;

class CShader : NonCopyable{
public:
	void Create(std::string vs, std::string ps, std::string gs = "");
	void SetGPU();
private:
	ComPtr<ID3D11VertexShader> m_pVertexShader;		// 頂点シェーダー
	ComPtr<ID3D11PixelShader>  m_pPixelShader;		// ピクセルシェーダー
	ComPtr<ID3D11InputLayout>  m_pVertexLayout;		// 頂点レイアウト
	ComPtr<ID3D11GeometryShader> m_pGeometryShader;	// ジオメトリシェーダー
};

