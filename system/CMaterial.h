#pragma once
#include	<Windows.h>
#include	<d3d11.h>
#include	<wrl/client.h>
#include	"dx11helper.h"
#include	"renderer.h"
#include	"NonCopyable.h"

// マテリアル
/*
struct MATERIAL {
	DirectX::XMFLOAT4 Ambient;
	DirectX::XMFLOAT4 Diffuse;
	DirectX::XMFLOAT4 Specular;
	DirectX::XMFLOAT4 Emission;
	float	Shiness;
	BOOL	TextureEnable;
	float	Dummy[2];

	MATERIAL() :
		Ambient(DirectX::XMFLOAT4(0, 0, 0, 0)),
		Diffuse(DirectX::XMFLOAT4(0, 0, 0, 0)),
		Specular(DirectX::XMFLOAT4(0, 0, 0, 0)),
		Shiness(0),
		Emission(DirectX::XMFLOAT4(0, 0, 0, 0)),
		TextureEnable(FALSE)
	{
	}
};
*/

class CMaterial : NonCopyable {

	struct ConstantBufferMaterial {
		MATERIAL	Material;
	};

	MATERIAL	m_Material{};
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_pConstantBufferMaterial;

public:
	CMaterial() {}
	CMaterial(MATERIAL mtrl) {
		Create(mtrl);
	}
	~CMaterial() {
		Uninit();
	}

	bool Create(MATERIAL mtrl) {

		ID3D11Device* dev;
		dev = Renderer::GetDevice();

		// コンスタントバッファ作成
		bool sts = CreateConstantBuffer(
			dev,								// デバイス
			sizeof(ConstantBufferMaterial),		// サイズ
			m_pConstantBufferMaterial.GetAddressOf());		// コンスタントバッファ４
		if (!sts) {
			MessageBox(NULL, L"CreateBuffer(constant buffer Material) error", L"Error", MB_OK);
			return false;
		}

		m_Material.Ambient = mtrl.Ambient;
		m_Material.Diffuse = mtrl.Diffuse;
		m_Material.Specular = mtrl.Specular;
		m_Material.Emission = mtrl.Emission;
		m_Material.Shiness = mtrl.Shiness;
		m_Material.TextureEnable = mtrl.TextureEnable;

		Update();

		return true;
	}

	void Update() {
		ConstantBufferMaterial		cb{};

		cb.Material = m_Material;

		ID3D11DeviceContext* devcontext;
		devcontext = Renderer::GetDeviceContext();

		devcontext->UpdateSubresource(
			m_pConstantBufferMaterial.Get(),
			0,
			nullptr,
			&cb,
			0, 0);

		// コンスタントバッファ4をｂ3レジスタへセット（頂点シェーダー用）
		devcontext->VSSetConstantBuffers(3, 1, m_pConstantBufferMaterial.GetAddressOf());

		// コンスタントバッファ4をｂ3レジスタへセット(ピクセルシェーダー用)
		devcontext->PSSetConstantBuffers(3, 1, m_pConstantBufferMaterial.GetAddressOf());

	}

	void SetGPU() {

		ID3D11DeviceContext* devcontext;
		devcontext = Renderer::GetDeviceContext();

		// コンスタントバッファ4をｂ3レジスタへセット（頂点シェーダー用）
		devcontext->VSSetConstantBuffers(3, 1, m_pConstantBufferMaterial.GetAddressOf());

		// コンスタントバッファ4をｂ3レジスタへセット(ピクセルシェーダー用)
		devcontext->PSSetConstantBuffers(3, 1, m_pConstantBufferMaterial.GetAddressOf());
	}

	void SetMaterial(const MATERIAL& mtrl) {
		ConstantBufferMaterial		cb{};

		cb.Material = mtrl;

		ID3D11DeviceContext* devcontext;
		//		devcontext = CDirectXGraphics::GetInstance()->GetImmediateContext();
		devcontext = Renderer::GetDeviceContext();

		devcontext->UpdateSubresource(
			m_pConstantBufferMaterial.Get(),
			0,
			nullptr,
			&cb,
			0, 0);

		// コンスタントバッファ4をｂ3レジスタへセット（頂点シェーダー用）
		devcontext->VSSetConstantBuffers(3, 1, m_pConstantBufferMaterial.GetAddressOf());

		// コンスタントバッファ4をｂ3レジスタへセット(ピクセルシェーダー用)
		devcontext->PSSetConstantBuffers(3, 1, m_pConstantBufferMaterial.GetAddressOf());

	}

	void Uninit() {
	}

	void SetDiffuse(DirectX::XMFLOAT4 diffuse) {
		m_Material.Diffuse = diffuse;
	}

	void SetAmbient(DirectX::XMFLOAT4 ambient) {
		m_Material.Ambient = ambient;
	}

	void SetSpecular(DirectX::XMFLOAT4 specular) {
		m_Material.Specular = specular;
	}

	void SetEmission(DirectX::XMFLOAT4 emission) {
		m_Material.Emission = emission;
	}

	void SetShiness(float shiness) {
		m_Material.Shiness = shiness;
	}

	bool isDiffuseTextureEnable() {
		return m_Material.TextureEnable == TRUE;
	}
};