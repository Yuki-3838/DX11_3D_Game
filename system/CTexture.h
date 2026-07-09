#pragma once

#include	<d3d11.h>
#include	<string>
#include	<wrl/client.h>							// ComPtrの定義を含むヘッダファイル
#include	<filesystem>
#include	"NonCopyable.h"

using Microsoft::WRL::ComPtr;

// テクスチャを扱うクラス
class CTexture : NonCopyable
{
	std::string m_texname{};						// ファイル名
	ComPtr<ID3D11ShaderResourceView> m_srv{};		// シェーダーリソースビュー

	int m_width{};									// 幅
	int m_height{};									// 高さ
	int m_bpp{};									// BPP
public:
	bool Load(const std::string& filename);
	bool LoadFromFemory(const unsigned char* data,int len);

	bool Load(const std::u8string& filename);		// 20250711

	void SetGPU();

	int GetWidth() { return m_width; }				// 20260414
	int GetHeight() { return m_height; }			// 20260414

};