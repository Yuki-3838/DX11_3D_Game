#pragma once

#include	<d3d11.h>
#include	<string>
#include	<wrl/client.h>							// ComPtrの定義を含むヘッダファイル
#include	<filesystem>
#include	<vector>
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

	// Model cache (ModelCache.h). While s_keepPixels is true on this thread, Load keeps the decoded RGBA pixels
	// so the loader can save them. CreateFromRGBA builds the texture from saved pixels (defined in ModelCache.cpp).
	static inline thread_local bool s_keepPixels = false;
	bool CreateFromRGBA(const unsigned char* pixels, int width, int height, const std::string& name);
	const std::vector<unsigned char>& GetPixels() const { return m_pixels; }
	void ReleasePixels() { m_pixels.clear(); m_pixels.shrink_to_fit(); }
	const std::string& GetName() const { return m_texname; }
private:
	std::vector<unsigned char> m_pixels{};
public:

};