#include "ModelCache.h"

#include <fstream>
#include <functional>
#include <cctype>
#include <sstream>
#include <system_error>
#include <thread>

namespace ModelCache
{
	namespace
	{
		constexpr uint32_t kMagic = 0x434D5844; // "DXMC"

		// dev_settings.ini の値を読む(GameScene の GetDevSetting と同じ形式。ここからは GameScene を使えないので自前で読む)。
		std::string ReadDevSetting(const std::string& key)
		{
			std::ifstream input("dev_settings.ini");
			std::string line;
			const std::string prefix = key + "=";
			while (std::getline(input, line))
			{
				if (line.rfind(prefix, 0) != 0)
					continue;
				std::string value = line.substr(prefix.size());
				while (!value.empty() &&
					(value.back() == '\r' || value.back() == '\n' || value.back() == ' ' || value.back() == '\t'))
					value.pop_back();
				return value;
			}
			return std::string();
		}

		uint64_t Fnv1a64(const std::string& text)
		{
			uint64_t hash = 14695981039346656037ull;
			for (const unsigned char c : text)
			{
				hash ^= c;
				hash *= 1099511628211ull;
			}
			return hash;
		}

		void WriteStamp(Writer& writer, const FileStamp& stamp)
		{
			writer.Pod(stamp.size);
			writer.Pod(stamp.writeTime);
		}

		FileStamp ReadStamp(Reader& reader)
		{
			FileStamp stamp;
			stamp.size = reader.Pod<uint64_t>();
			stamp.writeTime = reader.Pod<int64_t>();
			return stamp;
		}
	}

	bool Enabled()
	{
		static const bool enabled = ReadDevSetting("model_cache") != "0";
		return enabled;
	}

	bool GetStamp(const std::string& path, FileStamp& outStamp)
	{
		std::error_code error;
		const std::filesystem::path filePath(path);
		const uintmax_t size = std::filesystem::file_size(filePath, error);
		if (error)
			return false;
		const auto writeTime = std::filesystem::last_write_time(filePath, error);
		if (error)
			return false;
		outStamp.size = static_cast<uint64_t>(size);
		outStamp.writeTime = static_cast<int64_t>(writeTime.time_since_epoch().count());
		return true;
	}

	std::filesystem::path PathFor(const std::string& kind, const std::string& source, const std::string& option)
	{
		// 見て分かるよう元の名前を残す(英数字以外は _ にする)。同じ名前の別ファイルはハッシュで分ける。
		std::string stem = std::filesystem::path(source).stem().string();
		for (char& c : stem)
		{
			const unsigned char u = static_cast<unsigned char>(c);
			if (!(std::isalnum(u) || c == '-' || c == '_'))
				c = '_';
		}
		std::ostringstream name;
		name << stem << "_" << std::hex << Fnv1a64(kind + "\n" + source + "\n" + option) << "." << kind << ".bin";
		return std::filesystem::path("cache") / "model" / name.str();
	}

	bool Writer::SaveTo(const std::filesystem::path& path) const
	{
		std::error_code error;
		std::filesystem::create_directories(path.parent_path(), error);
		// 同じ保存ファイルを2つのスレッドが同時に作っても混ざらないよう、一時ファイルの名前はスレッドごとに変える。
		std::ostringstream suffix;
		suffix << ".tmp" << std::hash<std::thread::id>{}(std::this_thread::get_id());
		std::filesystem::path temporary = path;
		temporary += suffix.str();
		{
			std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
			if (!output)
				return false;
			output.write(m_buffer.data(), static_cast<std::streamsize>(m_buffer.size()));
			if (!output)
			{
				output.close();
				std::filesystem::remove(temporary, error);
				return false;
			}
		}
		std::filesystem::rename(temporary, path, error);
		if (error)
		{
			std::filesystem::remove(temporary, error);
			return false;
		}
		return true;
	}

	bool Reader::Open(const std::filesystem::path& path)
	{
		m_buffer.clear();
		m_position = 0;
		m_ok = false;
		std::ifstream input(path, std::ios::binary | std::ios::ate);
		if (!input)
			return false;
		const std::streamsize size = input.tellg();
		if (size <= 0)
			return false;
		input.seekg(0, std::ios::beg);
		m_buffer.resize(static_cast<size_t>(size));
		if (!input.read(m_buffer.data(), size))
			return false;
		m_ok = true;
		return true;
	}

	void WriteHeader(Writer& writer, const Header& header)
	{
		writer.U32(kMagic);
		writer.U32(kVersion);
		writer.String(header.kind);
		writer.String(header.source);
		writer.String(header.option);
		writer.PodVector(header.layout);
		WriteStamp(writer, header.sourceStamp);
		writer.U32(static_cast<uint32_t>(header.dependencies.size()));
		for (const auto& [path, stamp] : header.dependencies)
		{
			writer.String(path);
			WriteStamp(writer, stamp);
		}
	}

	bool ReadAndCheckHeader(Reader& reader, const Header& expected)
	{
		if (reader.U32() != kMagic || reader.U32() != kVersion)
			return false;
		if (reader.String() != expected.kind ||
			reader.String() != expected.source ||
			reader.String() != expected.option)
			return false;
		std::vector<uint32_t> layout;
		if (!reader.PodVector(layout) || layout != expected.layout)
			return false;
		if (!(ReadStamp(reader) == expected.sourceStamp))
			return false;
		// テクスチャなど、元のファイル以外に結果へ効くファイルが変わっていないか。
		const uint32_t dependencyCount = reader.U32();
		for (uint32_t i = 0; i < dependencyCount && reader.Ok(); ++i)
		{
			const std::string path = reader.String();
			const FileStamp stored = ReadStamp(reader);
			FileStamp current;
			if (!GetStamp(path, current) || !(current == stored))
				return false;
		}
		return reader.Ok();
	}

	bool MakeHeader(const std::string& kind, const std::string& source, const std::string& option,
		std::vector<uint32_t> layout, Header& outHeader)
	{
		outHeader.kind = kind;
		outHeader.source = source;
		outHeader.option = option;
		outHeader.layout = std::move(layout);
		outHeader.dependencies.clear();
		return GetStamp(source, outHeader.sourceStamp);
	}
}

// ---------------------------------------------------------------------------
// CTexture・CStaticMesh の保存ファイルの読み書き。
// (CTexture.cpp・CStaticMesh.cpp は Shift_JIS で書かれているので、新しく足す部分はこちらへ置く)
// ---------------------------------------------------------------------------
#include <iostream>
#include "CStaticMesh.h"
#include "renderer.h"

bool CTexture::CreateFromRGBA(const unsigned char* pixels, int width, int height, const std::string& name)
{
	m_texname = name;
	m_width = width;
	m_height = height;
	m_bpp = 4;
	m_pixels.clear();

	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = static_cast<UINT>(width);
	desc.Height = static_cast<UINT>(height);
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA subResource{};
	subResource.pSysMem = pixels;
	subResource.SysMemPitch = static_cast<UINT>(width) * 4;

	ComPtr<ID3D11Texture2D> texture;
	if (FAILED(Renderer::GetDevice()->CreateTexture2D(&desc, &subResource, texture.GetAddressOf())))
		return false;
	return SUCCEEDED(Renderer::GetDevice()->CreateShaderResourceView(texture.Get(), nullptr, m_srv.GetAddressOf()));
}

namespace
{
	std::vector<uint32_t> StaticMeshLayout()
	{
		return {
			static_cast<uint32_t>(sizeof(VERTEX_3D)),
			static_cast<uint32_t>(sizeof(MATERIAL)),
			static_cast<uint32_t>(sizeof(Matrix4x4)) };
	}

	// テクスチャの書き方。同じ画像を使うマテリアルが複数あれば、画素は1回だけ書く。
	enum : uint8_t { TextureNone = 0, TexturePixels = 1, TextureSameAs = 2 };
}

void CStaticMesh::Load(std::string filename, std::string texturedirectory, bool pretransformVertices)
{
	const std::string option = texturedirectory + (pretransformVertices ? "|pretransform" : "");
	ModelCache::Header header;
	if (!ModelCache::Enabled() ||
		!ModelCache::MakeHeader("static", filename, option, StaticMeshLayout(), header))
	{
		LoadWithAssimp(filename, texturedirectory, pretransformVertices);
		return;
	}

	const std::filesystem::path cachePath = ModelCache::PathFor(header.kind, filename, option);
	{
		ModelCache::Reader reader;
		if (reader.Open(cachePath) && ModelCache::ReadAndCheckHeader(reader, header) &&
			ReadMeshCache(reader) && reader.AtEnd())
			return;
	}

	{
		ModelCache::KeepPixelsScope keepPixels;
		LoadWithAssimp(filename, texturedirectory, pretransformVertices);
	}
	header.dependencies = TextureDependencies();
	ModelCache::Writer writer;
	ModelCache::WriteHeader(writer, header);
	const bool written = WriteMeshCache(writer);
	ReleaseTexturePixels();
	if (written && writer.SaveTo(cachePath))
		std::cout << "[ModelCache] saved " << cachePath.string() << std::endl;
}

bool CStaticMesh::WriteMeshCache(ModelCache::Writer& writer) const
{
	writer.PodVector(m_vertices);
	writer.PodVector(m_indices);
	writer.U32(static_cast<uint32_t>(m_subsets.size()));
	for (const SUBSET& subset : m_subsets)
	{
		writer.String(subset.MeshName);
		writer.String(subset.MtrlName);
		writer.U32(subset.IndexNum);
		writer.U32(subset.VertexNum);
		writer.U32(subset.IndexBase);
		writer.U32(subset.VertexBase);
		writer.U32(subset.MaterialIdx);
	}
	writer.PodVector(m_materials);
	writer.U32(static_cast<uint32_t>(m_diffusetexturenames.size()));
	for (const std::string& name : m_diffusetexturenames)
		writer.String(name);

	writer.U32(static_cast<uint32_t>(m_diffusetextures.size()));
	for (size_t i = 0; i < m_diffusetextures.size(); ++i)
	{
		CTexture* texture = m_diffusetextures[i].get();
		if (texture == nullptr)
		{
			writer.Pod(static_cast<uint8_t>(TextureNone));
			continue;
		}
		// 同じファイルから読んだテクスチャが前にあれば、その番号だけ書く。
		size_t sameAs = i;
		if (!texture->GetName().empty())
		{
			for (size_t j = 0; j < i; ++j)
			{
				const CTexture* other = m_diffusetextures[j].get();
				if (other != nullptr && other->GetName() == texture->GetName())
				{
					sameAs = j;
					break;
				}
			}
		}
		if (sameAs != i)
		{
			writer.Pod(static_cast<uint8_t>(TextureSameAs));
			writer.U32(static_cast<uint32_t>(sameAs));
			continue;
		}
		const std::vector<unsigned char>& pixels = texture->GetPixels();
		const size_t expected = static_cast<size_t>(texture->GetWidth()) * texture->GetHeight() * 4;
		if (expected == 0 || pixels.size() != expected)
			return false; // 画素が残っていない(読み方が想定と違う)。保存しない。
		writer.Pod(static_cast<uint8_t>(TexturePixels));
		writer.String(texture->GetName());
		writer.Pod(static_cast<int32_t>(texture->GetWidth()));
		writer.Pod(static_cast<int32_t>(texture->GetHeight()));
		writer.Bytes(pixels.data(), pixels.size());
	}
	return true;
}

bool CStaticMesh::ReadMeshCache(ModelCache::Reader& reader)
{
	m_vertices.clear();
	m_indices.clear();
	m_subsets.clear();
	m_materials.clear();
	m_diffusetexturenames.clear();
	m_diffusetextures.clear();

	if (!reader.PodVector(m_vertices) || !reader.PodVector(m_indices))
		return false;
	const uint32_t subsetCount = reader.U32();
	for (uint32_t i = 0; i < subsetCount && reader.Ok(); ++i)
	{
		SUBSET subset{};
		subset.MeshName = reader.String();
		subset.MtrlName = reader.String();
		subset.IndexNum = reader.U32();
		subset.VertexNum = reader.U32();
		subset.IndexBase = reader.U32();
		subset.VertexBase = reader.U32();
		subset.MaterialIdx = reader.U32();
		m_subsets.push_back(std::move(subset));
	}
	if (!reader.PodVector(m_materials))
		return false;
	const uint32_t nameCount = reader.U32();
	for (uint32_t i = 0; i < nameCount && reader.Ok(); ++i)
		m_diffusetexturenames.push_back(reader.String());

	struct SavedPixels
	{
		const unsigned char* data = nullptr;
		int width = 0;
		int height = 0;
		std::string name;
	};
	const uint32_t textureCount = reader.U32();
	if (!reader.Ok())
		return false;
	std::vector<SavedPixels> saved(textureCount);
	for (uint32_t i = 0; i < textureCount && reader.Ok(); ++i)
	{
		const uint8_t kind = reader.Pod<uint8_t>();
		if (kind == TextureNone)
		{
			m_diffusetextures.push_back(nullptr);
			continue;
		}
		if (kind == TextureSameAs)
		{
			const uint32_t index = reader.U32();
			if (index >= i || saved[index].data == nullptr)
				return false;
			saved[i] = saved[index];
		}
		else if (kind == TexturePixels)
		{
			saved[i].name = reader.String();
			saved[i].width = reader.Pod<int32_t>();
			saved[i].height = reader.Pod<int32_t>();
			if (saved[i].width <= 0 || saved[i].height <= 0)
				return false;
			saved[i].data = reinterpret_cast<const unsigned char*>(
				reader.Bytes(static_cast<size_t>(saved[i].width) * saved[i].height * 4));
			if (saved[i].data == nullptr)
				return false;
		}
		else
		{
			return false;
		}
		auto texture = std::make_unique<CTexture>();
		if (!texture->CreateFromRGBA(saved[i].data, saved[i].width, saved[i].height, saved[i].name))
			return false;
		m_diffusetextures.push_back(std::move(texture));
	}
	return reader.Ok();
}

std::vector<std::pair<std::string, ModelCache::FileStamp>> CStaticMesh::TextureDependencies() const
{
	std::vector<std::pair<std::string, ModelCache::FileStamp>> dependencies;
	for (const auto& texture : m_diffusetextures)
	{
		if (texture == nullptr || texture->GetName().empty())
			continue; // モデルに埋め込まれたテクスチャは、モデルのファイルの更新時刻で分かる
		ModelCache::FileStamp stamp;
		if (ModelCache::GetStamp(texture->GetName(), stamp))
			dependencies.emplace_back(texture->GetName(), stamp);
	}
	return dependencies;
}

void CStaticMesh::ReleaseTexturePixels()
{
	for (auto& texture : m_diffusetextures)
	{
		if (texture != nullptr)
			texture->ReleasePixels();
	}
}
