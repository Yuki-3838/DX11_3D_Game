#pragma once

#include	<Windows.h>
#include	<directxmath.h>
#include	<d3d11.h>
#include	<wrl/client.h>
#include	"renderer.h"
#include	"NonCopyable.h"

struct BoneCombMatrix : NonCopyable 
{
	CBBoneCombMatrix	 ConstantBufferMemory{};					// 行列群
	Microsoft::WRL::ComPtr<ID3D11Buffer> ConstantBuffer{};			// 定数バッファ

	BoneCombMatrix() {}
	~BoneCombMatrix() 
	{
	}

	// 定数バッファ生成し内容を更新する
	bool Create();

	// 定数バッファを更新する
	void Update();

	// GPUにセット
	void SetGPU();
};