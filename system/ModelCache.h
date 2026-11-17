#pragma once

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <type_traits>
#include <vector>

/**
 * @brief 読み込んだモデル・モーションを、そのまま使える形でファイルへ保存しておく(読み込みの保存ファイル)。
 *
 * モデル(FBX)を読むたびに、Assimp でファイルを解析し、PNG のテクスチャを展開し、ゲーム用のデータへ変換していた。
 * 騎士では Debug 版で1回に約3.5秒(解析約1.4秒・テクスチャ約1.2秒・変換約0.9秒)、
 * モーションは使う分だけで約1.7秒かかり、リトライのたびにも同じことをしていた。
 *
 * 1回目に読んだ結果を cache/ フォルダへ書き出し、次からはそれを読むだけにする。
 * 起動し直した後の1回目も速くなる(一番最初の1回だけは今までどおり時間がかかり、そのときに作る)。
 * リトライでは、OS が直前に読んだファイルを覚えているので、ディスクもほとんど読まない。
 *
 * 保存ファイルが古くならないよう、次のどれかが変わったら使わずに読み直して作り直す。
 *   - 元のファイル(モデル・モーション・テクスチャ)の大きさと更新時刻
 *   - 下の kVersion (読み込みの処理を変えたときに上げる)
 *   - 構造体の大きさ(VERTEX_3D など)
 * dev_settings.ini の model_cache=0 で使わない(作らない)ようにできる。保存ファイルは cache/ を消せばよい。
 *
 * 保存の中身は CStaticMesh・CAnimationMesh・CAnimationData がそれぞれ書く(ここは読み書きの道具だけ)。
 */
namespace ModelCache
{
	// 読み込みの処理(AssimpPerse・CStaticMesh::Load・CAnimationMesh の読み込み・CAnimationData)を
	// 変えて結果が変わるときは、この数を1つ上げる。古い保存ファイルはすべて使われなくなる。
	constexpr uint32_t kVersion = 1;

	/** 保存ファイルを使うか(dev_settings.ini の model_cache=0 で使わない)。 */
	bool Enabled();

	/** ファイルの大きさと更新時刻。保存ファイルが元のファイルと合っているかを確かめるのに使う。 */
	struct FileStamp
	{
		uint64_t size = 0;
		int64_t writeTime = 0;
		bool operator==(const FileStamp& other) const { return size == other.size && writeTime == other.writeTime; }
	};
	bool GetStamp(const std::string& path, FileStamp& outStamp);

	/** 書き込み。メモリへ溜めて、最後にまとめてファイルへ書く。 */
	class Writer
	{
	public:
		void Bytes(const void* data, size_t size)
		{
			const char* bytes = static_cast<const char*>(data);
			m_buffer.insert(m_buffer.end(), bytes, bytes + size);
		}
		template <class T>
		void Pod(const T& value)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			Bytes(&value, sizeof(T));
		}
		void U32(uint32_t value) { Pod(value); }
		void String(const std::string& value)
		{
			U32(static_cast<uint32_t>(value.size()));
			Bytes(value.data(), value.size());
		}
		template <class T>
		void PodVector(const std::vector<T>& values)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			U32(static_cast<uint32_t>(values.size()));
			if (!values.empty())
				Bytes(values.data(), values.size() * sizeof(T));
		}
		/** 一時ファイルへ書いてから置き換える(途中で落ちても、壊れた保存ファイルを残さない)。 */
		bool SaveTo(const std::filesystem::path& path) const;
		size_t Size() const { return m_buffer.size(); }
		const char* Data() const { return m_buffer.data(); }

	private:
		std::vector<char> m_buffer;
	};

	/** 読み出し。途中で足りなくなったら以降はすべて失敗(Ok() が false)になる。 */
	class Reader
	{
	public:
		bool Open(const std::filesystem::path& path);
		// メモリ上のデータを読む(写して持つ)。
		void Assign(const char* data, size_t size)
		{
			m_buffer.assign(data, data + size);
			m_position = 0;
			m_ok = true;
		}
		bool Ok() const { return m_ok; }
		bool AtEnd() const { return m_ok && m_position == m_buffer.size(); }

		const char* Bytes(size_t size)
		{
			if (!m_ok || m_buffer.size() - m_position < size)
			{
				m_ok = false;
				return nullptr;
			}
			const char* data = m_buffer.data() + m_position;
			m_position += size;
			return data;
		}
		template <class T>
		T Pod()
		{
			static_assert(std::is_trivially_copyable_v<T>);
			T value{};
			if (const char* data = Bytes(sizeof(T)))
				std::memcpy(&value, data, sizeof(T));
			return value;
		}
		uint32_t U32() { return Pod<uint32_t>(); }
		std::string String()
		{
			const uint32_t size = U32();
			const char* data = Bytes(size);
			return data != nullptr ? std::string(data, size) : std::string();
		}
		template <class T>
		bool PodVector(std::vector<T>& out)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			const uint32_t count = U32();
			const char* data = Bytes(static_cast<size_t>(count) * sizeof(T));
			if (data == nullptr)
				return false;
			out.resize(count);
			if (count > 0)
				std::memcpy(out.data(), data, static_cast<size_t>(count) * sizeof(T));
			return true;
		}

	private:
		std::vector<char> m_buffer;
		size_t m_position = 0;
		bool m_ok = false;
	};

	/**
	 * @brief 保存ファイルの見出し。種類・元のファイル・読み込みの条件・構造体の大きさ・元のファイルの大きさと更新時刻。
	 *
	 * dependencies は元のファイル以外に結果へ効くファイル(テクスチャなど)。読むときに今のものと比べる。
	 */
	struct Header
	{
		std::string kind;        ///< "static" / "anim" / "clip"
		std::string source;      ///< 元のファイル(読み込みに渡したパス)
		std::string option;      ///< テクスチャのフォルダなど、同じファイルでも結果を変える条件
		std::vector<uint32_t> layout; ///< 構造体の大きさ(変わったら読まない)
		FileStamp sourceStamp;
		std::vector<std::pair<std::string, FileStamp>> dependencies;
	};

	/** 保存ファイルの場所(cache/model/元の名前_条件のハッシュ.bin)。 */
	std::filesystem::path PathFor(const std::string& kind, const std::string& source, const std::string& option);

	void WriteHeader(Writer& writer, const Header& header);
	/**
	 * 見出しを読み、expected と同じか(元のファイル・テクスチャが変わっていないか)を確かめる。
	 * 合っていなければ false(呼び出し側は元のファイルから読み直して保存し直す)。
	 */
	bool ReadAndCheckHeader(Reader& reader, const Header& expected);

	/** 見出しを作る。元のファイルが無ければ false(保存ファイルは使わない)。 */
	bool MakeHeader(const std::string& kind, const std::string& source, const std::string& option,
		std::vector<uint32_t> layout, Header& outHeader);
}

#include "CTexture.h"

namespace ModelCache
{
	/** この間に(このスレッドで)読んだテクスチャは、展開した画素を残す(保存ファイルへ書くため)。 */
	struct KeepPixelsScope
	{
		bool previous;
		KeepPixelsScope() : previous(CTexture::s_keepPixels) { CTexture::s_keepPixels = true; }
		~KeepPixelsScope() { CTexture::s_keepPixels = previous; }
		KeepPixelsScope(const KeepPixelsScope&) = delete;
		KeepPixelsScope& operator=(const KeepPixelsScope&) = delete;
	};
}
