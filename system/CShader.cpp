#include	"CShader.h"
#include	"dx11helper.h"
#include	"renderer.h"

void CShader::Create(std::string vs, std::string ps, std::string gs)
{

	// 頂点データの定義
//	D3D11_INPUT_ELEMENT_DESC layout[] =
//	{
//		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
//		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
//		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
//		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,			0,	D3D11_APPEND_ALIGNED_ELEMENT,   D3D11_INPUT_PER_VERTEX_DATA, 0 }
//	};

	// ワンスキン対応　20231227追加
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT,		0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",		0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BONEINDEX",	0, DXGI_FORMAT_R32G32B32A32_SINT,	0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BONEWEIGHT",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	unsigned int numElements = ARRAYSIZE(layout);

	ID3D11Device* device;
	device = Renderer::GetDevice();

	// 頂点シェーダーオブジェクトを生成、同時に頂点レイアウトも生成
	bool sts = CreateVertexShader(device,
			vs.c_str(),
			"main",
			"vs_5_0",
			layout,
			numElements,
			&m_pVertexShader,
			&m_pVertexLayout);
	if (!sts) {
		MessageBox(nullptr, L"CreateVertexShader error", L"error", MB_OK);
		return;
	}

	// ピクセルシェーダーを生成
	sts = CreatePixelShader(			// ピクセルシェーダーオブジェクトを生成
		device,							// デバイスオブジェクト
		ps.c_str(),
		"main",
		"ps_5_0",
		&m_pPixelShader);
	if (!sts) {
		MessageBox(nullptr, L"CreatePixelShader error", L"error", MB_OK);
		return;
	}

	if (gs != "") {
		// ピクセルシェーダーを生成
		sts = CreateGeometryShader(			// ピクセルシェーダーオブジェクトを生成
			device,							// デバイスオブジェクト
			gs.c_str(),
			"main",
			"gs_5_0",
			&m_pGeometryShader);
		if (!sts) {
			MessageBox(nullptr, L"CreateGeometryShader error", L"error", MB_OK);
			return;
		}
	}

	return;
}

void CShader::SetGPU() {

	ID3D11DeviceContext* devicecontext;

	devicecontext = Renderer::GetDeviceContext();

	devicecontext->VSSetShader(m_pVertexShader.Get(), nullptr, 0);		// 頂点シェーダーをセット
	devicecontext->PSSetShader(m_pPixelShader.Get(), nullptr, 0);		// ピクセルシェーダーをセット
	devicecontext->GSSetShader(m_pGeometryShader.Get(), nullptr, 0);	// ジオメトリシェーダーをセット
	devicecontext->IASetInputLayout(m_pVertexLayout.Get());				// 頂点レイアウトセット
}

