#pragma once

#include	"BoneCombMatrix.h"
#include	"dx11helper.h"

// 定数バッファ生成し内容を更新する
bool BoneCombMatrix::Create() {

	ID3D11Device* dev;
	dev = Renderer::GetDevice();

	// コンスタントバッファ作成
	bool sts = CreateConstantBufferWrite(
		dev,								// デバイス
		sizeof(CBBoneCombMatrix),			// サイズ
		ConstantBuffer.GetAddressOf());		// コンスタントバッファ
	if (!sts) {
		MessageBox(NULL, L"CreateBuffer(constant buffer BoneMatrices) error", L"Error", MB_OK);
		return false;
	}

	// 行列群を定数バッファへセット
	Update();

	return true;
}

// 定数バッファを更新する
void BoneCombMatrix::Update()
{
	ID3D11DeviceContext* devcontext;
	devcontext = Renderer::GetDeviceContext();

	//定数バッファ書き換え
	D3D11_MAPPED_SUBRESOURCE msr;

	HRESULT hr = devcontext->Map(
		ConstantBuffer.Get(),
		0,
		D3D11_MAP_WRITE_DISCARD, 0, &msr);

	if (SUCCEEDED(hr)) 
	{
		memcpy(msr.pData, &ConstantBufferMemory, sizeof(CBBoneCombMatrix));
		Renderer::GetDeviceContext()->Unmap(ConstantBuffer.Get(), 0);
	}
}

// GPUにセット
void BoneCombMatrix::SetGPU() {

	ID3D11DeviceContext* devcontext;
	devcontext = Renderer::GetDeviceContext();

	// コンスタントバッファをｂ５レジスタへセット（頂点シェーダー用）
	devcontext->VSSetConstantBuffers(5, 1, ConstantBuffer.GetAddressOf());
}
