#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "CombatAttackTable.h"

/**
 * 敵(ドラゴン)のモデルごとの違いをまとめた表。
 *
 * ゲーム側(GameScene)は骨の名前を直接書かず、ここの「役割」から引く。
 * 首・尾・脚などの骨の名前、拡大率、使うクリップ、口先や尾の先の延ばし方はモデルごとに違うため。
 *
 * - Cethiel のドラゴン(assets/model/CethielDragon): リポジトリに入っている無料のモデル。
 *   攻撃のクリップは1本だけで、種類ごとに速さと区間を変え、首・尾の構えを重ねて見分けさせる。
 * - Wyvern(assets/model/Wyvern): Fab で購入した有料のモデル。リポジトリには入れていない(.gitignore)。
 *   置いてあればこちらを使い、無ければ Cethiel に戻す。攻撃ごとに専用のクリップがある。
 */

/** ゲーム側が使う骨の役割。 */
enum class EnemyBone : unsigned char
{
    Body,        ///< 胴の中心(照準・地響きの位置・接地の確認)
    Neck1,       ///< 首の根元
    Neck2,
    Neck3,       ///< 首の先(頭の通り道の根元)
    JawUpper,    ///< 上あご(飛来の場面で頭を起こす)。無いモデルは空
    JawLower,    ///< 下あご(咆哮・火球で口を開ける)
    MouthFrom,   ///< 口先の向きの根元。MouthTo へ向かう向きの先に口先がある
    MouthTo,     ///< 口先の向きの先
    Tail1,       ///< 尾の根元寄り
    Tail2,
    Tail3,       ///< 尾の先寄り(先端はここから Tail2→Tail3 の向きへ延ばす)
    FrontUpperL,
    FrontUpperR,
    FrontLowerL,
    FrontLowerR,
    FrontFootL,  ///< 踏んだ瞬間に土煙を出す足(前左)
    FrontFootR,
    BackUpperL,
    BackUpperR,
    BackLowerL,
    BackLowerR,
    BackFootL,
    BackFootR,
    RightClaw,   ///< 右前脚の先(爪で振り下ろす攻撃の通り道)。無いモデルは空
    WingRootL,   ///< 翼の付け根(手続きで広げる翼)。無いモデルは空
    WingRootR,
    WingTipL,
    WingTipR,
    Count
};

/** 攻撃の通り道(軌跡)と、地面を打つ演出をどこから出すか。 */
enum class EnemyStrikePart : unsigned char
{
    Head,       ///< 首の先→口先
    RightClaw,  ///< 右前脚の爪
    Tail,       ///< 尾
};

/**
 * 攻撃の種類ごとの専用クリップ。
 *
 * 戦闘の時間(予兆・判定・隙)は CombatAttackTable.h の表で決まっていて、モデルを替えても変えない
 * (「予兆を見て避ける」遊びの手触りを保つため)。ここでは、その時間に合わせてクリップを伸び縮みさせる。
 * 戦闘の「当たる瞬間」に、このクリップの impact の位置が来るように再生する。
 */
struct EnemyAttackClipBinding
{
    std::string clipFile;    ///< 空なら専用クリップなし(共通の攻撃クリップを使う)
    float clipStart = 0.0f;  ///< 再生を始める位置(クリップの秒)
    float impact = 0.0f;     ///< 当たる瞬間(クリップの秒。骨の動きを測って決めた)
    float clipEnd = 0.0f;    ///< 再生を終える位置(クリップの秒)
    EnemyStrikePart strikePart = EnemyStrikePart::Head;

    bool Valid() const { return !clipFile.empty(); }
};

/**
 * 体を骨の位置の球で表すときの1個分(壁に押し出すとき)。
 * 半径は combatReferenceScale の大きさのときのワールドの長さ(実際には Combat::EnemyReachScale を掛ける)。
 */
struct EnemyBodySphere
{
    std::string bone;
    float radius = 0.0f;
};

struct EnemyModelProfile
{
    const char* id = "";
    const char* displayName = "";
    const char* meshId = "";          ///< MeshManager に登録する名前
    const char* meshPath = "";
    const char* textureDirectory = "";
    float modelScale = 1.0f;

    std::array<std::string, static_cast<size_t>(EnemyBone::Count)> bones;

    float mouthExtend = 0.0f;      ///< 口先 = MouthTo + (MouthTo - MouthFrom) * この値
    float tailTipExtend = 0.0f;    ///< 尾の先 = Tail3 + (Tail3 - Tail2) * この値
    float headTrailExtend = 0.0f;  ///< 頭の通り道の先 = 口先 + (口先 - Neck3) * この値
    float mouthRadius = 4.0f;
    float tailTipRadius = 3.0f;
    std::vector<EnemyBodySphere> bodySpheres;

    // クリップ。空なら使わない。
    std::string idleClip;
    std::string walkClip;
    std::string runClip;     ///< 速く動く間(近づく・下がる・突進)に使う
    std::string attackClip;  ///< 共通の攻撃クリップ(専用が無い攻撃に使う)
    std::string dieClip;
    std::string flyClip;     ///< 飛来の場面・登場の落下中。無ければ待機の姿勢に手続きで翼を広げる

    EnemyAttackClipBinding slamClip;
    EnemyAttackClipBinding biteClip;
    EnemyAttackClipBinding sweepClip;

    /**
     * クリップを1倍で再生したときに、地面に着いた足が後ろへ流れる速さ(モデルの長さ/秒)。
     * 0 なら従来どおり決め打ちの速さでキーを送る(Cethiel)。
     * 0 より大きければ、実際の移動の速さに合わせて再生の速さを決める(足が地面を滑らないように)。
     */
    float walkGroundSpeed = 0.0f;
    float runGroundSpeed = 0.0f;

    /**
     * 戦闘の距離(間合い・攻撃の届く距離)と移動の速さの表(CombatAttackTable.h・enemy.h)が
     * そのまま合う拡大率。実際の拡大率がこの何倍かを Combat::EnemyReachScale にする。
     * 0 なら倍率1(Cethiel。表はこのドラゴンで決めた)。
     */
    float combatReferenceScale = 0.0f;
    // 闘技場の広さ・戦闘カメラの引き・登場の位置は、この倍率から GameScene が決める
    // (enemy_scale で大きさを変えても、それらが一緒に付いてくるように)。

    /**
     * プレイヤーと押し合う胴の球(モデルの長さ。向きは敵の位置の座標で、前が -Z)。
     * 半径 0 ならモデル全体を包む球を使う(従来。Cethiel は描画の倍率を8までに抑えて胴の大きさにしていた)。
     * 尾や翼まで包む球だと、長い体の Wyvern ではプレイヤーが頭へ近づけない。
     */
    float bodyCollisionForward = 0.0f;
    float bodyCollisionRadius = 0.0f;

    // 登場。空から降りてきて着地し、少し間を置いて吠える。
    float introFallSeconds = 1.35f;   ///< 降りてくる時間
    float introFallHeight = 150.0f;   ///< 降り始めの高さ
    float introFallBack = 0.0f;       ///< 降り始めの位置を着地点からどれだけ奥(プレイヤーと反対)へ置くか
    float introRoarDelay = 0.12f;     ///< 着地から咆哮の声までの間

    /**
     * 咆哮のクリップ。空なら待機の姿勢に手続きで首とあごを動かす(Cethiel)。
     * 声(SoundManager::PlayEnemyRoar)の鳴り始めに roarSoundStart、声が大きい間の終わりに roarHoldEnd が
     * 来るように伸び縮みさせる(どちらもクリップの秒)。
     */
    std::string roarClip;
    float roarOpenStart = 0.0f;  ///< 大きく口を開け始める位置
    float roarSoundStart = 0.0f; ///< 声の鳴り始めに合わせる位置
    float roarHoldEnd = 0.0f;    ///< 口を閉じ始める位置
    float roarClipEnd = 0.0f;    ///< 終わりの位置

    /// クリップの拡大のキーを使う(CAnimationMesh::SetUseAnimationScaleKeys)。
    /// Unreal Engine の FBX はアーマチュアのキーに100倍の拡大があり、使わないと体が1/100に縮む。
    bool useAnimationScaleKeys = false;

    const std::string& Bone(EnemyBone role) const { return bones[static_cast<size_t>(role)]; }
    const char* BoneName(EnemyBone role) const { return Bone(role).c_str(); }
    bool HasBone(EnemyBone role) const { return !Bone(role).empty(); }

    const EnemyAttackClipBinding* AttackClipFor(Combat::EnemyAttackKind kind) const
    {
        const EnemyAttackClipBinding* binding = nullptr;
        switch (kind)
        {
        case Combat::EnemyAttackKind::Slam:  binding = &slamClip; break;
        case Combat::EnemyAttackKind::Bite:  binding = &biteClip; break;
        case Combat::EnemyAttackKind::Sweep: binding = &sweepClip; break;
        default: break;
        }
        return binding != nullptr && binding->Valid() ? binding : nullptr;
    }
};

inline const EnemyModelProfile& GetCethielDragonProfile()
{
    static const EnemyModelProfile profile = [] {
        EnemyModelProfile p;
        p.id = "cethiel";
        p.displayName = "Cethiel Dragon";
        p.meshId = "cethiel_dragon";
        p.meshPath = "assets/model/CethielDragon/dragon.dae";
        p.textureDirectory = "assets/model/CethielDragon/";
        // ほぼ1単位の大きさで作られているため、プレイヤーと並べたときに大型の敵として見える倍率へ拡大する。
        p.modelScale = 60.0f;
        const auto set = [&p](EnemyBone role, const char* name) { p.bones[static_cast<size_t>(role)] = name; };
        set(EnemyBone::Body, "Armature_back2");
        set(EnemyBone::Neck1, "Armature_neck1");
        set(EnemyBone::Neck2, "Armature_neck2");
        set(EnemyBone::Neck3, "Armature_neck3");
        set(EnemyBone::JawUpper, "Armature_jaw_upper");
        set(EnemyBone::JawLower, "Armature_jaw_lower");
        set(EnemyBone::MouthFrom, "Armature_neck3");
        set(EnemyBone::MouthTo, "Armature_jaw_upper");
        set(EnemyBone::Tail1, "Armature_tail1");
        set(EnemyBone::Tail2, "Armature_tail2");
        set(EnemyBone::Tail3, "Armature_tail3");
        set(EnemyBone::FrontUpperL, "Armature_leg_front_upper_L");
        set(EnemyBone::FrontUpperR, "Armature_leg_front_upper_R");
        set(EnemyBone::FrontLowerL, "Armature_leg_front_lower_L");
        set(EnemyBone::FrontLowerR, "Armature_leg_front_lower_R");
        set(EnemyBone::FrontFootL, "Armature_leg_front_foot_L");
        set(EnemyBone::FrontFootR, "Armature_leg_front_foot_R");
        set(EnemyBone::BackUpperL, "Armature_leg_back_upper_L");
        set(EnemyBone::BackUpperR, "Armature_leg_back_upper_R");
        set(EnemyBone::BackLowerL, "Armature_leg_back_lower_L");
        set(EnemyBone::BackLowerR, "Armature_leg_back_lower_R");
        set(EnemyBone::BackFootL, "Armature_leg_back_foot_L");
        set(EnemyBone::BackFootR, "Armature_leg_back_foot_R");
        set(EnemyBone::WingRootL, "Armature_wing_lower_L");
        set(EnemyBone::WingRootR, "Armature_wing_lower_R");
        set(EnemyBone::WingTipL, "Armature_wing_upper_L");
        set(EnemyBone::WingTipR, "Armature_wing_upper_R");
        // 上あごの骨はあごの付け根にあるので、延ばさないと火球が頭の中から出てくる。
        p.mouthExtend = 0.6f;
        // 最後の骨(tail3)は尾の途中にあるので、tail2→tail3 の長さ分だけ延ばす。
        p.tailTipExtend = 1.0f;
        // 口先までだと帯がほとんど頭の中に入ってしまい、頭に隠れて見えなかった(撮影で確認)。
        p.headTrailExtend = 0.35f;
        p.mouthRadius = 4.0f;
        p.tailTipRadius = 3.0f;
        // 半径は体の太さ(胴は横幅約23の半分)。
        p.bodySpheres = {
            { "Armature_jaw_upper", 6.0f },
            { "Armature_neck3", 7.0f },
            { "Armature_neck2", 7.0f },
            { "Armature_neck1", 8.0f },
            { "Armature_back1", 11.0f },
            { "Armature_back2", 11.0f },
            { "Armature_tail1", 7.0f },
            { "Armature_tail2", 5.0f },
            { "Armature_tail3", 4.0f },
            { "Armature_leg_front_foot_L", 4.0f },
            { "Armature_leg_front_foot_R", 4.0f },
            { "Armature_leg_back_foot_L", 4.0f },
            { "Armature_leg_back_foot_R", 4.0f },
        };
        p.idleClip = "assets/model/CethielDragon/dragon_idle.dae";
        p.walkClip = "assets/model/CethielDragon/dragon_walk.dae";
        p.attackClip = "assets/model/CethielDragon/dragon_attack.dae";
        p.dieClip = "assets/model/CethielDragon/dragon_die.dae";
        return p;
    }();
    return profile;
}

inline const EnemyModelProfile& GetWyvernProfile()
{
    static const EnemyModelProfile profile = [] {
        EnemyModelProfile p;
        p.id = "wyvern";
        p.displayName = "Wyvern";
        p.meshId = "wyvern";
        // Unreal Engine から tools/ue_export_fbx.py で書き出した FBX(cm 単位・Z上向き)。
        // 頭が -Y・上が -Z(左手系へ変換後)で、Cethiel のドラゴンと同じ向きなので回転の補正は要らない。
        p.meshPath = "assets/model/Wyvern/SK_Wyvern.FBX";
        // マテリアルにベースカラーのテクスチャが書かれていないので、
        // マテリアル名と同じ名前の画像(M_Wyvern_Body1_Inst.jpg など)をここから読む(AssimpPerse)。
        p.textureDirectory = "assets/model/Wyvern/";
        // 狩りゲームの大型飛竜とハンターの大きさの比に合わせる。
        // 代表的な飛竜(全長 約17m)はハンター(約1.7m)の約10倍の長さなので、
        // 頭から尾の先まで(約2250cm)がプレイヤーの身長(約18)の10倍 = 約180 になる倍率にした。
        // このとき背中の高さ(骨盤 約291cm)は約23で、プレイヤーの約1.3倍。
        // (0.11 では長さが14倍・背中が1.8倍で大きすぎ、0.055 では長さが7倍で小さすぎた)
        p.modelScale = 0.08f;
        // 0.055 のとき(背中の高さが Cethiel のドラゴンと同じ約17)、噛みつきで頭が届く距離(約39)・
        // 尾の薙ぎ払い(約58)が、攻撃の届く距離の表(噛みつき34・薙ぎ払い55 + プレイヤーの太さ5)とほぼ合う。
        // それより大きくした分だけ、間合い・届く距離・歩幅を同じ割合で伸ばす(0.08 なら約1.45倍)。
        p.combatReferenceScale = 0.055f;
        // 胴(肩 約-313cm 〜 腰 約+65cm)を包む球。中心は胸と腰の間。
        p.bodyCollisionForward = -150.0f;
        p.bodyCollisionRadius = 230.0f;
        // 奥の空から滑るように降りてきて、羽ばたきで速さを殺しながら着地する。
        p.introFallSeconds = 2.6f;
        p.introFallHeight = 230.0f;
        p.introFallBack = 260.0f;
        // 着地してから、頭を低く下げてうなり(Scream の0〜1秒)、首を持ち上げて吠える。
        p.introRoarDelay = 1.0f;
        // Scream: 0.1〜0.8秒に小さくうなり、1.1秒から口を大きく開け(1.7秒で開き切る)、
        // 2.7秒まで開いたまま吠え、3.3秒で閉じる(あごの骨の回転を測った値)。
        p.roarClip = "assets/model/Wyvern/anim/Anim_Wyvern_Scream.fbx";
        p.roarOpenStart = 1.05f;
        p.roarSoundStart = 1.40f;
        p.roarHoldEnd = 2.75f;
        p.roarClipEnd = 3.40f;
        const auto set = [&p](EnemyBone role, const char* name) { p.bones[static_cast<size_t>(role)] = name; };
        set(EnemyBone::Body, "spine1");
        set(EnemyBone::Neck1, "neck1");
        set(EnemyBone::Neck2, "neck2");
        set(EnemyBone::Neck3, "Head");
        set(EnemyBone::JawLower, "Jaw");
        // Head の骨は頭の付け根にあり、鼻先(約216cm先)までは首の最後の骨の長さ(約83cm)の約2.6倍。
        set(EnemyBone::MouthFrom, "neck2");
        set(EnemyBone::MouthTo, "Head");
        // 尾は10本の骨。構えで曲げる3本を根元・中・先寄りから選ぶ。
        set(EnemyBone::Tail1, "tail_002");
        set(EnemyBone::Tail2, "tail_005");
        set(EnemyBone::Tail3, "tail_008");
        set(EnemyBone::FrontUpperL, "front_UpperLeg_L");
        set(EnemyBone::FrontUpperR, "front_UpperLeg_R");
        set(EnemyBone::FrontLowerL, "front_Shin_L");
        set(EnemyBone::FrontLowerR, "front_Shin_R");
        // 足首の骨は浮いている(後脚はかかとが高い)ので、地面に着く指の付け根で踏んだかを見る。
        set(EnemyBone::FrontFootL, "front_toe_Middle1_L");
        set(EnemyBone::FrontFootR, "front_toe_Middle1_R");
        set(EnemyBone::BackUpperL, "back_UpperLeg_L");
        set(EnemyBone::BackUpperR, "back_UpperLeg_R");
        set(EnemyBone::BackLowerL, "back_Shin_L");
        set(EnemyBone::BackLowerR, "back_Shin_R");
        set(EnemyBone::BackFootL, "back_toe_Middle1_L");
        set(EnemyBone::BackFootR, "back_toe_Middle1_R");
        set(EnemyBone::RightClaw, "front_toe_Middle3_R");
        // 翼は前脚に付いていて、飛ぶ動きはクリップ(Fly)にある。手続きで広げる翼は使わない。
        p.mouthExtend = 2.6f;
        // tail_008 の先、尾の先端(約256cm先)までは tail_005→tail_008(約345cm)の約0.75倍。
        p.tailTipExtend = 0.75f;
        p.headTrailExtend = 0.1f;
        p.mouthRadius = 4.0f;
        p.tailTipRadius = 3.0f;
        p.bodySpheres = {
            { "Head", 5.0f },
            { "neck2", 5.0f },
            { "neck1", 6.0f },
            { "spine4", 8.0f },
            { "spine2", 8.0f },
            { "spine1", 8.0f },
            { "pelvis", 8.0f },
            { "tail_002", 6.0f },
            { "tail_005", 4.0f },
            { "tail_008", 3.0f },
            { "front_toe_Middle1_L", 3.0f },
            { "front_toe_Middle1_R", 3.0f },
            { "back_toe_Middle1_L", 3.0f },
            { "back_toe_Middle1_R", 3.0f },
        };
        p.idleClip = "assets/model/Wyvern/anim/Anim_Wyvern_Idle.fbx";
        p.walkClip = "assets/model/Wyvern/anim/Anim_Wyvern_Walk.fbx";
        p.runClip = "assets/model/Wyvern/anim/Anim_Wyvern_Run.fbx";
        p.dieClip = "assets/model/Wyvern/anim/Anim_Wyvern_Death.fbx";
        p.flyClip = "assets/model/Wyvern/anim/Anim_Wyvern_Fly.fbx";
        // 当たる瞬間は骨の動きを測って決めた(右前脚の爪・頭・尾の先が一番前へ出る/地面を打つ時刻)。
        // 叩き付け: 右前脚を高く振り上げて(0.5秒)、前へ振り下ろす(0.72秒で爪が一番前・下)。
        p.slamClip = { "assets/model/Wyvern/anim/Anim_Wyvern_Swipe.fbx", 0.0f, 0.72f, 1.17f, EnemyStrikePart::RightClaw };
        // 噛みつき: 頭を引いて(0.35秒)、前へ突き出す(0.63秒で頭が一番前)。
        p.biteClip = { "assets/model/Wyvern/anim/Anim_Wyvern_Bite.fbx", 0.0f, 0.63f, 1.33f, EnemyStrikePart::Head };
        // 薙ぎ払い: 体をひねって尾を右から前へ低く振り抜く(0.58秒で尾の先が正面)。
        p.sweepClip = { "assets/model/Wyvern/anim/Anim_Wyvern_Tail_Slam.fbx", 0.0f, 0.58f, 1.50f, EnemyStrikePart::Tail };
        // 地面に着いた前脚の指が後ろへ流れる速さ(クリップを測った値。歩き 約336cm/秒、走り 約1160cm/秒)。
        p.walkGroundSpeed = 336.0f;
        p.runGroundSpeed = 1160.0f;
        p.useAnimationScaleKeys = true;
        return p;
    }();
    return profile;
}

/**
 * 使う敵のモデルを選ぶ。
 * requestedId が "cethiel" なら Cethiel。それ以外は、Wyvern のファイルが置いてあれば Wyvern、無ければ Cethiel。
 */
inline const EnemyModelProfile& SelectEnemyModelProfile(const std::string& requestedId)
{
    if (requestedId == "cethiel")
        return GetCethielDragonProfile();
    const EnemyModelProfile& wyvern = GetWyvernProfile();
    std::error_code error;
    if (std::filesystem::exists(wyvern.meshPath, error))
        return wyvern;
    return GetCethielDragonProfile();
}
