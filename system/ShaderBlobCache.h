#pragma once

#include <algorithm>
#include <cstdio>
#include <d3dcompiler.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <tuple>
#include <wrl/client.h>

/**
 * @brief 実行時にコンパイルしたシェーダーのバイトコードを、プロセスの間だけ取っておく。
 *
 * GameScene はリトライのたびに作り直され、影・モーションブラーのシェーダーを毎回コンパイルしていた。
 * 実測(Release版)で、1回の読み込み約7.4秒のうち約6.9秒がこのコンパイルだった
 * (モーションブラーが約5.2秒、影が約1.7秒。モデルとクリップの読み込みは約0.2秒)。
 * 2回目からはここから返すので、リトライの読み込みがほぼコンパイル分だけ短くなる。
 *
 * シェーダーを書き換えながら試す作業を壊さないよう、shaderフォルダの中で一番新しい更新時刻も鍵に含める。
 * どれか1つでも保存し直せば(インクルードされるファイルでも)、次の読み込みでコンパイルし直す。
 *
 * 裏のスレッド(飛来の場面の読み込み)とメインスレッドの両方から呼ばれるので、表の出し入れは排他する。
 * コンパイル自体は排他の外で行う(同じものを同時に頼まれたら、両方がコンパイルして後の方が上書きするだけ)。
 */
namespace ShaderBlobCache
{
    using Key = std::tuple<std::string, std::string, std::string, long long>;

    inline std::mutex& Mutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    inline std::map<Key, Microsoft::WRL::ComPtr<ID3DBlob>>& Table()
    {
        static std::map<Key, Microsoft::WRL::ComPtr<ID3DBlob>> table;
        return table;
    }

    // shaderフォルダの中で一番新しい更新時刻。読めなければ0(その場合も鍵としては一定なので害はない)。
    inline long long NewestShaderWriteTime(const char* fileName)
    {
        std::error_code error;
        const std::filesystem::path directory = std::filesystem::path(fileName).parent_path();
        long long newest = 0;
        for (const auto& entry : std::filesystem::directory_iterator(directory.empty() ? "." : directory, error))
        {
            const auto time = entry.last_write_time(error);
            if (!error)
                newest = (std::max)(newest, static_cast<long long>(time.time_since_epoch().count()));
        }
        return newest;
    }

    inline Key MakeKey(const char* fileName, const char* entryPoint, const char* shaderModel)
    {
        return Key(fileName ? fileName : "", entryPoint ? entryPoint : "", shaderModel ? shaderModel : "",
            NewestShaderWriteTime(fileName ? fileName : ""));
    }

    /**
     * @brief ディスクにも置く(起動のたびのコンパイルを省く)。
     *
     * プロセスの間だけの表では、起動するたびにコンパイルし直していた
     * (Debug版で、タイトルの2組だけで約2.1秒。起動直後の黒い画面の大半)。
     * 鍵(ファイル名・入口・シェーダーモデル・shaderフォルダの一番新しい更新時刻・Debugかどうか)から
     * ファイル名を作り、shader_cache フォルダへバイトコードを書く。シェーダーを保存し直すと鍵が変わるので、
     * 古いファイルは使われない(残るだけ。消してもよい)。
     * shader フォルダの中に置くと、書き込むたびにフォルダの更新時刻が変わって鍵が毎回変わるので、外に置く。
     */
    inline std::filesystem::path DiskPath(const Key& key)
    {
#if defined(DEBUG) || defined(_DEBUG)
        constexpr const char* build = "debug";
#else
        constexpr const char* build = "release";
#endif
        const std::string text = std::get<0>(key) + "|" + std::get<1>(key) + "|" + std::get<2>(key) + "|" +
            std::to_string(std::get<3>(key)) + "|" + build;
        char name[32]{};
        std::snprintf(name, sizeof(name), "%016llx.cso",
            static_cast<unsigned long long>(std::hash<std::string>{}(text)));
        return std::filesystem::path("shader_cache") / name;
    }

    inline bool LoadFromDisk(const Key& key, Microsoft::WRL::ComPtr<ID3DBlob>& out)
    {
        std::ifstream input(DiskPath(key), std::ios::binary | std::ios::ate);
        if (!input)
            return false;
        const std::streamsize size = input.tellg();
        if (size <= 0)
            return false;
        input.seekg(0, std::ios::beg);
        Microsoft::WRL::ComPtr<ID3DBlob> blob;
        if (FAILED(D3DCreateBlob(static_cast<SIZE_T>(size), &blob)))
            return false;
        if (!input.read(static_cast<char*>(blob->GetBufferPointer()), size))
            return false;
        out = blob;
        return true;
    }

    inline void SaveToDisk(const Key& key, ID3DBlob* blob)
    {
        std::error_code error;
        const std::filesystem::path path = DiskPath(key);
        std::filesystem::create_directories(path.parent_path(), error);
        // 書きかけのファイルを別の起動が読まないよう、別名で書いてから置き換える。
        const std::filesystem::path temporary = path.string() + ".tmp";
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output)
                return;
            output.write(static_cast<const char*>(blob->GetBufferPointer()),
                static_cast<std::streamsize>(blob->GetBufferSize()));
            if (!output)
                return;
        }
        std::filesystem::rename(temporary, path, error);
        if (error)
            std::filesystem::remove(temporary, error);
    }

    /** 見つかれば参照を1つ増やして返す(呼び出し側はいつもどおり Release してよい)。 */
    inline bool Find(const Key& key, ID3DBlob** out)
    {
        {
            std::lock_guard<std::mutex> lock(Mutex());
            const auto found = Table().find(key);
            if (found != Table().end())
            {
                *out = found->second.Get();
                (*out)->AddRef();
                return true;
            }
        }
        Microsoft::WRL::ComPtr<ID3DBlob> blob;
        if (!LoadFromDisk(key, blob))
            return false;
        std::lock_guard<std::mutex> lock(Mutex());
        Table()[key] = blob;
        *out = blob.Get();
        (*out)->AddRef();
        return true;
    }

    inline void Store(const Key& key, ID3DBlob* blob)
    {
        if (!blob)
            return;
        {
            std::lock_guard<std::mutex> lock(Mutex());
            Table()[key] = blob;
        }
        SaveToDisk(key, blob);
    }
}
