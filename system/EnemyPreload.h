#pragma once

#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>

#include "CAnimationData.h"
#include "CAnimationMesh.h"
#include "EnemyModelProfile.h"

/**
 * 敵のモデルとクリップを、前の場面(タイトル・リザルト)を見せている間に裏のスレッドで読んでおく。
 *
 * Wyvern は頂点が多く、Debug版ではモデルの読み込みに約2.7秒、クリップに約0.9秒かかる。
 * タイトルで START を押してから飛来の場面が出るまで、その間ずっと黒い画面になっていた。
 * タイトル・リザルトの画面は少なくとも数秒は出ているので、その間に読んでおけば待たずに済む。
 *
 * 読むのは描画文脈(ID3D11DeviceContext)に触らない部分だけ(CAnimationMesh::LoadWithoutRenderResources)。
 * 描画の準備(InitRenderResources)は、受け取った GameScene がメインスレッドで行う。
 *
 * モデルの読み込み(AssimpPerse)は共有の作業領域を使うので、同時に2つ読んではいけない。
 * GameScene::init は、ほかのモデルを読む前に Take() で受け取る(読みかけなら終わるまで待つ)。
 */
struct EnemyPreloadData
{
    const EnemyModelProfile* profile = nullptr;
    std::unique_ptr<CAnimationMesh> mesh;   ///< 描画の準備はまだ(InitRenderResources を呼ぶこと)
    std::unique_ptr<CAnimationData> clips;  ///< LoadEnemyClips の名前で入っている
};

namespace EnemyPreload
{
    /** クリップを決まった名前で読む(ファイルが無いものは読まない。CAnimationData は失敗すると assert で止まるため)。 */
    inline void LoadEnemyClips(CAnimationData& clips, const EnemyModelProfile& profile)
    {
        const auto load = [&clips](const std::string& path, const char* name)
        {
            std::error_code error;
            if (!path.empty() && std::filesystem::exists(path, error))
                clips.LoadAnimation(path, name);
        };
        load(profile.idleClip, "idle");
        load(profile.walkClip, "walk");
        load(profile.runClip, "run");
        load(profile.attackClip, "attack");
        load(profile.dieClip, "die");
        load(profile.flyClip, "fly");
        load(profile.slamClip.clipFile, "attack_slam");
        load(profile.biteClip.clipFile, "attack_bite");
        load(profile.sweepClip.clipFile, "attack_sweep");
        load(profile.roarClip, "roar");
    }

    /** 読んだものを返す。読み込みの中身は GameScene と同じ。 */
    inline std::unique_ptr<EnemyPreloadData> LoadNow(const EnemyModelProfile& profile)
    {
        auto data = std::make_unique<EnemyPreloadData>();
        data->profile = &profile;
        data->mesh = std::make_unique<CAnimationMesh>();
        data->mesh->LoadWithoutRenderResources(profile.meshPath, profile.textureDirectory);
        data->clips = std::make_unique<CAnimationData>();
        LoadEnemyClips(*data->clips, profile);
        return data;
    }

    inline std::mutex& Mutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    inline std::future<std::unique_ptr<EnemyPreloadData>>& Pending()
    {
        static std::future<std::unique_ptr<EnemyPreloadData>> pending;
        return pending;
    }

    /** 裏で読み始める。読みかけ・受け取られていないものがあれば何もしない。 */
    inline void Start(const EnemyModelProfile& profile)
    {
        std::lock_guard<std::mutex> lock(Mutex());
        if (Pending().valid())
            return;
        const EnemyModelProfile* target = &profile;
        Pending() = std::async(std::launch::async, [target]() { return LoadNow(*target); });
    }

    /**
     * 次の戦いで使うモデル(dev_settings.ini の enemy_model。GameScene と同じ選び方)を裏で読み始める。
     * タイトル・リザルトの init の最後(その場面のモデルを読み終えた後)に呼ぶ。
     */
    inline void StartForNextBattle()
    {
        std::string requested;
        std::ifstream input("dev_settings.ini");
        std::string line;
        while (std::getline(input, line))
        {
            if (line.rfind("enemy_model=", 0) != 0)
                continue;
            requested = line.substr(12);
            while (!requested.empty() &&
                (requested.back() == '\r' || requested.back() == ' ' || requested.back() == '\t'))
                requested.pop_back();
            break;
        }
        Start(SelectEnemyModelProfile(requested.empty() ? "wyvern" : requested));
    }

    /**
     * 読んでおいたものを受け取る(読みかけなら終わるまで待つ)。
     * 読んでいない、または別のモデルを読んでいたら nullptr(呼び出し側がその場で読む)。
     */
    inline std::unique_ptr<EnemyPreloadData> Take(const EnemyModelProfile& profile)
    {
        std::lock_guard<std::mutex> lock(Mutex());
        if (!Pending().valid())
            return nullptr;
        std::unique_ptr<EnemyPreloadData> data = Pending().get();
        if (!data || data->profile != &profile)
            return nullptr;
        return data;
    }
}
