#include	<iostream>
#include	<fstream>
#include	"CTexture.h"
#include	"stb_image.h"
#include	"renderer.h"

// u8対応
bool CTexture::Load(const std::u8string& filename)
{
	std::filesystem::path filepath = filename;
	m_texname = filepath.string();  // UTF-8として保持

	std::ifstream ifs(filepath, std::ios::binary | std::ios::ate);
	if (!ifs) {
		std::cerr << "Failed to open file: " << filepath << std::endl;
		return false;
	}

	std::streamsize size = ifs.tellg();
	ifs.seekg(0, std::ios::beg);

	std::vector<unsigned char> buffer(size);
	if (!ifs.read(reinterpret_cast<char*>(buffer.data()), size)) {
		std::cerr << "Failed to read file: " << filepath << std::endl;
		return false;
	}

	unsigned char* pixels = stbi_load_from_memory(
		buffer.data(), static_cast<int>(buffer.size()),
		&m_width, &m_height, &m_bpp, STBI_rgb_alpha);

	if (!pixels) {
		std::cerr << "Failed to decode image: " << filepath << std::endl;
		return false;
	}

	ComPtr<ID3D11Texture2D> pTexture;
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = m_width;
	desc.Height = m_height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA subResource{};
	subResource.pSysMem = pixels;
	subResource.SysMemPitch = m_width * 4;

	HRESULT hr = Renderer::GetDevice()->CreateTexture2D(&desc, &subResource, pTexture.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	hr = Renderer::GetDevice()->CreateShaderResourceView(pTexture.Get(), nullptr, m_srv.GetAddressOf());
	stbi_image_free(pixels);

	return SUCCEEDED(hr);
}

// テクスチャをロード
bool CTexture::Load(const std::string& filename)
{
	bool sts = true;


	// 画像読み込み
//	pixels = stbi_load(filename.c_str(), &m_width, &m_height, &m_bpp, 4);
//	if (pixels == nullptr) {
//		std::cout << filename.c_str() << " Load error " << std::endl;

		// u8文字文字列に　2025/7/12 by suzuki.tomoki
		std::u8string u8s(filename.begin(),filename.end());
		sts =Load(u8s);
		if (sts == false) {
			return false;
		}
		return true;
//	}

/*
// テクスチャ2Dリソース生成
	ComPtr<ID3D11Texture2D> pTexture;

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	desc.Width = m_width;
	desc.Height = m_height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;			// RGBA
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA subResource{};
	subResource.pSysMem = pixels;
	subResource.SysMemPitch = desc.Width * 4;			// RGBA = 4 bytes per pixel
	subResource.SysMemSlicePitch = 0;

	ID3D11Device* device = Renderer::GetDevice();

	HRESULT hr = device->CreateTexture2D(&desc, &subResource, pTexture.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	// SRV生成
	hr = device->CreateShaderResourceView(pTexture.Get(), nullptr, m_srv.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	// ピクセルイメージ解放
	stbi_image_free(pixels);

	return true;


*/
}

// テクスチャをメモリからロード
bool CTexture::LoadFromFemory(const unsigned char* Data,int len) {

	bool sts = true;
	unsigned char* pixels;

	// 画像読み込み
	pixels = stbi_load_from_memory(Data, 
		len, 
		&m_width, 
		&m_height, 
		&m_bpp, 
		STBI_rgb_alpha);

	// テクスチャ2Dリソース生成
	ComPtr<ID3D11Texture2D> pTexture;

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	desc.Width = m_width;
	desc.Height = m_height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;			// RGBA
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA subResource{};
	subResource.pSysMem = pixels;
	subResource.SysMemPitch = desc.Width * 4;			// RGBA = 4 bytes per pixel
	subResource.SysMemSlicePitch = 0;

	ID3D11Device* device = Renderer::GetDevice();

	HRESULT hr = device->CreateTexture2D(&desc, &subResource, pTexture.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	// SRV生成
	hr = device->CreateShaderResourceView(pTexture.Get(), nullptr, m_srv.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	// ピクセルイメージ解放
	stbi_image_free(pixels);

	return true;
}

// テクスチャをGPUにセット
void CTexture::SetGPU()
{
	ID3D11DeviceContext* devicecontext = Renderer::GetDeviceContext();
	devicecontext->PSSetShaderResources(0, 1, m_srv.GetAddressOf());
}
