#pragma once

#include <algorithm>
#include "../DX11_3D_Game/CombatDesign.h"

/**
 * @file CombatAttackTable.h
 * @brief このゲームで実際に使う攻撃データを1箇所へまとめたテーブル。
 *
 * 攻撃の時間(予兆・判定・硬直)と威力は、以前は次の2箇所へ別々に書かれていた。
 *  - gameobject/enemy.h  : 敵AIの状態遷移(=プレイヤーが見るアニメーションの長さ)
 *  - system/OneVsOneCombat.h : 実際のダメージ判定タイミング
 * 値が食い違うと「予備動作を見てから回避する」という本作の核が壊れるため、
 * ここを唯一の定義元とし、両方から参照する。
 *
 * Tuning名前空間の定数は、constexprが必要な箇所(クラス内のstatic constexpr)から使う。
 * AttackDataを返す関数は、攻撃を種類ごとに扱いたい箇所(今後の攻撃パターン追加)から使う。
 */
namespace Combat
{
namespace Tuning
{
// --- 体力 ---
// 以前はプレイヤーと敵が同じ100を共有しており、強攻撃(40)3発で敵が倒れていた。
// それでは予兆を読んで立ち回る時間がほとんど無く、怯みも1戦で1回程度しか起きない。
// エルデンリングのボスやモンハンの大型モンスターのように、読み合いを何度も繰り返す
// 長さの戦闘にするため、敵の体力をプレイヤーと切り離して大きくした。
//
// 目安: 敵の攻撃1周期(予兆→攻撃→隙→様子見)は約4秒。隙へ3段コンボ(75)を
// 毎回入れられるわけではないので、平均の与ダメージを秒間12程度と見積もると、
// 予兆を読めるプレイヤーで約2分半〜3分の戦闘になる。
inline constexpr float PLAYER_MAX_HP = 100.0f;
inline constexpr float ENEMY_MAX_HP = 2000.0f;

// --- プレイヤーの攻撃の威力 ---
// 時間(予兆・判定・硬直)はコンボの段ごとに違うので、下の PlayerComboStepOf() に持たせている。
inline constexpr int PLAYER_WEAK_DAMAGE = 25;
inline constexpr int PLAYER_HEAVY_DAMAGE = 40;

/**
 * @brief 敵の攻撃クリップ(dragon_attack.dae)のどこで判定を出すか。
 *
 * 計測値(調査プログラム probe9。クリップ長1.71秒):
 *   0.00〜0.90 頭を上げて構える(予兆)
 *   0.90〜1.10 一気に振り下ろす(最速は1.00秒付近)
 *   1.05〜1.10 頭が地面に到達 = 当たる瞬間
 *   1.10〜1.30 頭を地面に着けたまま
 *   1.30〜1.71 頭を戻す(隙)
 *
 * 以前は判定が出ている時間が叩き付け0.90秒・噛みつき0.55秒・薙ぎ払い0.85秒もあった
 * (ユーザー指摘「当たり判定が出てる時間が長すぎる」)。
 * モンスターハンターの大型モンスターは、予兆を長く見せる一方で**当たり判定が出るのは0.1〜0.3秒ほど**で、
 * 残りは振り抜きと硬直(=反撃の機会)になっている。振り下ろしの前後だけを判定にし、その分を隙へ回す。
 *
 * 敵の攻撃クリップは1本しかないので、種類ごとに再生速度と使う区間を変えて
 * 「速い噛みつき」「遅い薙ぎ払い」を作る。アニメーションもこの表の時間で再生する
 * (見えている振り下ろしと、実際に当たる瞬間を一致させるため)。
 */
struct EnemyAttackClip
{
    float clipStart = 0.0f;   ///< 再生を始める位置(クリップの秒)
    float hitStart = 0.0f;    ///< 攻撃判定が出始める位置(クリップの秒)
    float impact = 0.0f;      ///< 実際にダメージが出る瞬間(クリップの秒)
    float hitEnd = 0.0f;      ///< 攻撃判定が消える位置(クリップの秒)
    float clipEnd = 0.0f;     ///< 再生を終える位置(クリップの秒)
    float playbackRate = 1.0f;///< 再生速度(1 = クリップ本来の速さ)
    float extraRecoverySeconds = 0.0f; ///< クリップを再生し終えた後、さらに動かずに硬直する時間

    constexpr float WindupSeconds() const { return (hitStart - clipStart) / playbackRate; }
    constexpr float ActiveSeconds() const { return (hitEnd - hitStart) / playbackRate; }
    constexpr float RecoverySeconds() const
    {
        return (clipEnd - hitEnd) / playbackRate + extraRecoverySeconds;
    }
    /// Active開始から実際にダメージが出るまでの時間。
    constexpr float FirstHitSeconds() const { return (impact - hitStart) / playbackRate; }
    /// クリップを再生している長さ(予兆+判定+戻り。extraRecoveryは含まない)。
    constexpr float ClipPlaySeconds() const { return (clipEnd - clipStart) / playbackRate; }
};

// 叩き付け: 等速。振り下ろしの手前から地面に着いた直後までを判定にする。
inline constexpr EnemyAttackClip ENEMY_SLAM_CLIP{ 0.00f, 0.98f, 1.06f, 1.18f, 1.71f, 1.00f, 0.55f };
// 噛みつき: 1.5倍速。構えを飛ばして0.45秒から再生し、判定も短い。
inline constexpr EnemyAttackClip ENEMY_BITE_CLIP{ 0.45f, 0.98f, 1.04f, 1.16f, 1.60f, 1.50f, 0.10f };
// 薙ぎ払い: 0.8倍速。予兆が最も長く、横へ振り抜く分だけ判定も少し長い。隙も最大。
inline constexpr EnemyAttackClip ENEMY_SWEEP_CLIP{ 0.00f, 0.95f, 1.08f, 1.22f, 1.71f, 0.80f, 0.90f };

// --- 敵の攻撃(叩き付け): 標準。予兆が長く、隙も大きい ---
// 予兆(ANTICIPATION)は敵AIのWindup状態の長さと必ず一致させること。
// 以前はAI・アニメーション側が0.80秒、戦闘判定側が0.72秒とズレており、
// 予備動作アニメーションが終わる前に攻撃判定が始まっていた。
// 「予兆を見てから回避するか踏み込むかを決める」という本作の核を守るため、
// 長い方(0.80秒)へ統一し、判定がアニメーションより先に出ないようにする。
// 時間はクリップの表から求める(予兆0.98秒 / 判定0.20秒 / 隙1.08秒)。
inline constexpr float ENEMY_ANTICIPATION = ENEMY_SLAM_CLIP.WindupSeconds();
inline constexpr float ENEMY_ACTIVE = ENEMY_SLAM_CLIP.ActiveSeconds();
inline constexpr float ENEMY_RECOVERY = ENEMY_SLAM_CLIP.RecoverySeconds();
inline constexpr float ENEMY_COOLDOWN = 1.15f;
inline constexpr int ENEMY_DAMAGE = 15;
// 攻撃判定が届く距離。敵AIが攻撃を決断する距離(enemy.hのATTACK_DISTANCE)とは別物。
inline constexpr float ENEMY_HIT_RANGE = 64.0f;
// Active開始から実際にダメージが発生するまでの時間(叩き付けが当たる瞬間)。
inline constexpr float ENEMY_FIRST_HIT_TIME = ENEMY_SLAM_CLIP.FirstHitSeconds();

// --- 敵の攻撃(噛みつき): 速い。予兆が短く、隙も小さい ---
// 「様子見しすぎると刺される」役割。射程は短いので、距離を取っていれば安全。
// 隙が小さいので、これに反撃を欲張ると次の攻撃を食らう。
// 予兆0.35秒 / 判定0.12秒 / 隙0.39秒。
inline constexpr float ENEMY_BITE_ANTICIPATION = ENEMY_BITE_CLIP.WindupSeconds();
inline constexpr float ENEMY_BITE_ACTIVE = ENEMY_BITE_CLIP.ActiveSeconds();
inline constexpr float ENEMY_BITE_RECOVERY = ENEMY_BITE_CLIP.RecoverySeconds();
inline constexpr float ENEMY_BITE_COOLDOWN = 0.95f;
inline constexpr int ENEMY_BITE_DAMAGE = 10;
inline constexpr float ENEMY_BITE_HIT_RANGE = 48.0f;
inline constexpr float ENEMY_BITE_FIRST_HIT_TIME = ENEMY_BITE_CLIP.FirstHitSeconds();

// --- 敵の攻撃(薙ぎ払い): 遅い。予兆が非常に長く、隙が最大 ---
// 「見えたら必ず回避、そして大きく反撃できる」役割。
// 射程が広いので、回避せず距離を取るだけでは避けきれない。
// 予兆1.19秒 / 判定0.34秒 / 隙1.51秒。
inline constexpr float ENEMY_SWEEP_ANTICIPATION = ENEMY_SWEEP_CLIP.WindupSeconds();
inline constexpr float ENEMY_SWEEP_ACTIVE = ENEMY_SWEEP_CLIP.ActiveSeconds();
inline constexpr float ENEMY_SWEEP_RECOVERY = ENEMY_SWEEP_CLIP.RecoverySeconds();
inline constexpr float ENEMY_SWEEP_COOLDOWN = 1.30f;
inline constexpr int ENEMY_SWEEP_DAMAGE = 22;
inline constexpr float ENEMY_SWEEP_HIT_RANGE = 78.0f;
inline constexpr float ENEMY_SWEEP_FIRST_HIT_TIME = ENEMY_SWEEP_CLIP.FirstHitSeconds();

// --- 攻撃中の踏み込みの速さ(単位/秒) ---
// 判定が出ている間だけ前へ出る。判定を短くした分だけ速くして、踏み込む距離を保つ
// (叩き付けで約26、噛みつきで約11、薙ぎ払いで約34)。
// モンスターハンターの大型モンスターも、攻撃の瞬間に体ごと前へ出る。距離を取るだけでは避けられない、
// という読み合いをここで作っている。
inline constexpr float ENEMY_LUNGE_SPEED = 130.0f;
inline constexpr float ENEMY_BITE_LUNGE_SPEED = 90.0f;
inline constexpr float ENEMY_SWEEP_LUNGE_SPEED = 100.0f;

// --- 当たり判定の左右の広さ(正面からの片側角度・ラジアン) ---
// 敵の攻撃判定は元々「距離だけ」で、どの攻撃も全方位に当たっていた。
// 見た目上は薙ぎ払いだけが横へ振り抜くのに、噛みつきでも真横で当たるのでは
// 「横へ回り込んで避ける」という判断が成立しない。
// 攻撃ごとに広さを変えることで、予兆から読み取れる形と実際の危険範囲を一致させる。
//   噛みつき : 狭い。正面を外せば当たらない。
//   叩き付け : 標準。正面寄りだけ。
//   薙ぎ払い : 広い。横へ回り込んでも当たるので、回避か距離で対処する。
inline constexpr float ENEMY_HIT_HALF_ANGLE = 0.96f;       // 約55度
inline constexpr float ENEMY_BITE_HIT_HALF_ANGLE = 0.61f;  // 約35度
inline constexpr float ENEMY_SWEEP_HIT_HALF_ANGLE = 1.92f; // 約110度

// --- 敵の怯み ---
// 読みが当たって隙へ攻撃を入れたとき、敵の体が反応しないと「反撃できた」手応えが無い。
// 攻撃ごとの怯み値(AttackData::postureDamage)を敵に溜め、しきい値を超えたら怯ませる。
// 1発ごとに必ず怯むと、敵が何もできずに一方的な戦いになるため、溜める方式にしている。
inline constexpr int PLAYER_WEAK_POSTURE_DAMAGE = 14;
inline constexpr int PLAYER_HEAVY_POSTURE_DAMAGE = 34;
// 敵の隙(Recovery)へ入れた攻撃は怯み値を増やす。「読んで踏み込んだ」ことへのご褒美。
inline constexpr float PUNISH_POSTURE_MULTIPLIER = 2.0f;
// 怯むたびにしきい値を上げる(モンスターハンターの怯み耐性と同じ考え方)。
// 体力を2000へ増やしたとき、固定のしきい値60のままだと1戦で40回以上怯み、
// 敵が何もできないまま殴られ続ける。上げていくことで、序盤は怯みやすく
// 終盤ほど「狙って溜めないと怯まない」ようにし、1戦で8回前後に収める。
//   150 → 188 → 234 → 293 → 366 → 375(上限)...
inline constexpr float ENEMY_FLINCH_BASE_THRESHOLD = 150.0f;
inline constexpr float ENEMY_FLINCH_THRESHOLD_GROWTH = 1.25f;
inline constexpr float ENEMY_FLINCH_THRESHOLD_MAX = 375.0f;
// 攻撃を当てない時間が続くと怯み値は抜けていく。散発的な攻撃でいつの間にか怯むのを防ぐ。
inline constexpr float ENEMY_POSTURE_RECOVERY_PER_SECOND = 6.0f;
// 怯んでいる時間。短すぎると気付けず、長すぎると反撃が一方的になる。
inline constexpr float ENEMY_FLINCH_SECONDS = 0.90f;

// --- 敵の弱り具合(体力ゲージの代わり) ---
// 敵の体力は画面に数値やゲージで出さない。モンスターハンターのように、
// 「足を引きずる」「息が荒い」「隙が長くなる」といった体の変化で弱ってきたことを伝える。
//   通常 : 体力50%より上
//   疲れ : 50%以下。息が荒く頭が下がり、動きが少し鈍る
//   瀕死 : 20%以下。足を引きずり、動きが遅く、攻撃後の隙が長い
// 段階が変わった瞬間はよろめかせる(怯みと同じ動き)。変化に気付かせるため。
inline constexpr float ENEMY_TIRED_HP_RATIO = 0.50f;
inline constexpr float ENEMY_DYING_HP_RATIO = 0.20f;
inline constexpr float ENEMY_TIRED_MOVE_SCALE = 0.85f;
inline constexpr float ENEMY_DYING_MOVE_SCALE = 0.55f;
// 隙(Recovery)を伸ばす倍率。敵AIの側だけを伸ばす。戦闘判定側の攻撃は先に終わって
// 待機へ戻るので、次の攻撃の受け付けとは食い違わない。
inline constexpr float ENEMY_TIRED_RECOVERY_SCALE = 1.20f;
inline constexpr float ENEMY_DYING_RECOVERY_SCALE = 1.50f;

// --- プレイヤーの吹き飛ばし ---
// 食らった攻撃の重さを、体が飛ばされる距離で伝える。
// 同じダメージ表現(画面の揺れ・赤み)だけだと、どの攻撃を食らったか体で分からない。
//   噛みつき : 軽く押し戻されるだけ。すぐ動ける。
//   叩き付け : 大きく吹き飛ぶ。最も重い。
//   薙ぎ払い : 横薙ぎで大きく飛ばされる。
// 初速(単位/秒)と時間(秒)。速度は時間とともに二乗で落ちるので、
// 飛ばされる距離は 初速 x 時間 / 3 になる(叩き付けで約77、薙ぎ払いで約51、噛みつきで約11)。
// 初版(叩き付け260=約48)は実機で「押し戻された」程度にしか見えなかったため強めた。
inline constexpr float ENEMY_KNOCKBACK_SPEED = 420.0f;
inline constexpr float ENEMY_KNOCKBACK_SECONDS = 0.55f;
inline constexpr float ENEMY_BITE_KNOCKBACK_SPEED = 150.0f;
inline constexpr float ENEMY_BITE_KNOCKBACK_SECONDS = 0.22f;
inline constexpr float ENEMY_SWEEP_KNOCKBACK_SPEED = 340.0f;
inline constexpr float ENEMY_SWEEP_KNOCKBACK_SECONDS = 0.45f;

// --- プレイヤーの回避(前転) ---
// 移動の速さを体格に合わせた(歩き20・ダッシュ45、身長約18)のに、回避だけ速さ180で0.4秒=約72(身長の4倍、
// 人間なら約7m)も飛んでいた。モンスターハンターの前転は約3m(身長の2倍弱)なので、それに合わせる。
// 移動は出だしが最も速く、止まり際へ向けて落ちる(等速だと滑って見える)。
// 距離と時間はプレイヤーの移動(player.cpp)と前転のモーション(CCharacterAnimator)の両方がここを見る。
inline constexpr float PLAYER_DODGE_DISTANCE = 34.0f;
inline constexpr float PLAYER_DODGE_SECONDS = 0.50f;
} // namespace Tuning

/**
 * @brief プレイヤーのコンボ1段分の、クリップと時間の対応。
 *
 * 以前は攻撃クリップを元の長さに関係なく0.95秒(強攻撃1.10秒)へ詰めて再生し、
 * 予兆・判定・硬直も全段同じ値だった。3.5秒ある回転斬りを3.7倍速で回すことになり、
 * モーションがぎこちなく見える大きな原因だった。
 *
 * ここでは段ごとに「クリップのどこから再生し、どこで振り、どこで終わるか」を持たせ、
 * 戦闘判定の時間はそこから計算する(見た目の振りと判定がずれない)。
 * 振っている区間は、クリップの右腕の回転の速さが最大の50%を超える区間を
 * 調査プログラムで測り、その前後へ少し余裕を持たせた値にしている。
 *
 * 構成はモンスターハンターの片手剣を参考にした。
 *   弱: 低く踏み込む振り下ろし → 斬り上げ → 反対側からの横薙ぎ(走りからはダッシュ攻撃)
 *   強: 振りかぶって踏み込む振り下ろし → 前進しながらの回転斬り → 沈み込んで振り抜く回転斬り
 * 片手剣は「出が早く、振り終わりからすぐ次へつながる」ので、
 * 振りの手前から再生を始め(溜めを短く)、振り終わった直後から次の段へつなげる。
 */
struct PlayerComboStep
{
    const char* clipFile = "";   ///< 既定のクリップ(dev_settings.ini の weak1..3 / heavy1..3 で差し替え可)
    float clipStart = 0.0f;      ///< 再生を始める位置(クリップの秒)。溜めの前半を飛ばす
    float hitStart = 0.0f;       ///< 攻撃判定が出始める位置(クリップの秒)
    float hitEnd = 0.0f;         ///< 攻撃判定が消える位置(クリップの秒)
    float clipEnd = 0.0f;        ///< 次の入力が無いときに攻撃が終わる位置(クリップの秒)
    float playbackRate = 1.0f;   ///< 再生速度(1 = クリップ本来の速さ)
    float comboLinkSeconds = 0.0f; ///< 判定が消えてから、予約した次の段へ移れるまでの時間(ゲーム内の秒)
    /// 踏み込み(ルートモーション)の倍率。1 = クリップどおり。
    /// クリップの移動量そのままでは足りない技だけ上げる。地面を蹴らずに滑って見えるので、
    /// 跳んでいる間の移動が主な技(ダッシュ攻撃)以外では1のままにすること。
    float rootMotionScale = 1.0f;

    float WindupSeconds() const { return (hitStart - clipStart) / playbackRate; }
    float ActiveSeconds() const { return (hitEnd - hitStart) / playbackRate; }
    float RecoverySeconds() const { return (clipEnd - hitEnd) / playbackRate; }
    float TotalSeconds() const { return (clipEnd - clipStart) / playbackRate; }
};

/** コンボの段(1〜3)の設定を引く。 */
inline const PlayerComboStep& PlayerComboStepOf(bool heavy, int comboStep)
{
    // 右腕の振りの計測値(クリップの秒): slash(5) 0.49-0.64 / slash 0.55-0.64 / slash(3) 0.74-0.86 /
    // attack 1.05-1.28 / attack(4) 0.17-0.55 / attack(3) 0.68-0.81 / slash(4) 1.28-1.40
    static const PlayerComboStep weak[3] = {
        // 1段目: slash (5)。低く踏み込んで斬り、戻る。出が早い基本の一撃。
        // (以前は跳躍斬りの attack を1段目にしていたが、立ち止まった状態からいきなり跳ぶのは
        //  弱攻撃の出だしにふさわしくない、とユーザー判断。跳躍斬りはダッシュ攻撃へ移した)
        { "assets/motion/sword and shield slash (5).fbx", 0.24f, 0.44f, 0.70f, 1.20f, 1.30f, 0.05f },
        // 2段目: slash (3)。低い位置から斬り上げる。
        // (以前は slash にしていたが、1段目と同じく「剣を同じ側へ振り上げて斜めに振り下ろす」軌道で、
        //  どちらの段か見分けられない、とユーザー指摘。右手の軌跡を測って、軌道が逆向きのものを選んだ)
        { "assets/motion/sword and shield slash (3).fbx", 0.44f, 0.68f, 0.92f, 1.60f, 1.25f, 0.08f },
        // 3段目: attack (4)。反対側から横へ薙ぐ。振り下ろし → 斬り上げ → 横薙ぎと、段ごとに剣の向きを変える。
        { "assets/motion/sword and shield attack (4).fbx", 0.02f, 0.15f, 0.58f, 0.95f, 1.20f, 0.10f },
    };
    static const PlayerComboStep heavyTable[3] = {
        // 1段目: slash。高く振りかぶり、約42cm踏み込みながら振り下ろす。
        // 溜めを長めに見せて(0.36秒)、弱攻撃より重い一撃にする。
        // (以前は attack (4) だったが、弱攻撃の3段目へ回したので差し替えた)
        { "assets/motion/sword and shield slash.fbx",      0.10f, 0.50f, 0.72f, 1.30f, 1.10f, 0.08f },
        // 2段目: attack (3)。約226cm前進しながら回転して斬る。
        // 回転しながら2回振るクリップで、2回目の振りは0.75〜1.05秒(手の速さが最大なのは0.95秒)。
        // 以前は判定の終わりを0.90秒にしていたため、次の段へ1.0秒で移り、振り切る前に3段目が始まっていた。
        // (右腕の関節の回転だけで振りの区間を測っていたので、体の回転で剣が動く分を見落としていた)
        { "assets/motion/sword and shield attack (3).fbx", 0.34f, 0.62f, 1.05f, 1.60f, 1.25f, 0.12f },
        // 3段目: slash (4)。沈み込んで振り抜く回転斬り。締めの大技なので硬直が最も長い。
        { "assets/motion/sword and shield slash (4).fbx",  0.84f, 1.22f, 1.50f, 2.30f, 1.25f, 0.10f },
    };
    const int index = std::clamp(comboStep, 1, 3) - 1;
    return heavy ? heavyTable[index] : weak[index];
}

/**
 * @brief ダッシュ攻撃(走っているときに弱攻撃を押すと出る)。
 *
 * モンスターハンターの片手剣の突進斬りと同じく、走りから派生して大きく踏み込み、距離を詰めて斬る。
 * 立ち止まった状態の弱攻撃1段目の代わりに出て、そのまま弱攻撃の2段目へつなげられる。
 * クリップは attack(跳び上がって約223cm前へ出る斬り下ろし)。
 */
inline const PlayerComboStep& PlayerDashAttackStep()
{
    // 踏み込みはクリップのままだと約22(身長の1.2倍)で、走りから出す技としては物足りない
    // (ユーザー指摘「ダッシュattackはちょっと前に進んでほしい」)。跳んでいる間の移動なので、
    // 1.6倍(約36、身長の2倍)まで伸ばしても地面を滑って見えない。
    static const PlayerComboStep step =
        { "assets/motion/sword and shield attack.fbx", 0.55f, 0.98f, 1.34f, 2.05f, 1.45f, 0.06f, 1.6f };
    return step;
}

/** プレイヤーの通常攻撃1段分。 */
inline const AttackData& PlayerWeakAttack()
{
    static const AttackData data = [] {
        AttackData attack;
        attack.attackId = "player_weak";
        attack.animationName = "sword_shield_slash";
        attack.damage = Tuning::PLAYER_WEAK_DAMAGE;
        attack.postureDamage = Tuning::PLAYER_WEAK_POSTURE_DAMAGE;
        return attack;
    }();
    return data;
}

/** プレイヤーの強攻撃1段分。 */
inline const AttackData& PlayerHeavyAttack()
{
    static const AttackData data = [] {
        AttackData attack;
        attack.attackId = "player_heavy";
        attack.animationName = "sword_shield_attack";
        attack.damage = Tuning::PLAYER_HEAVY_DAMAGE;
        attack.postureDamage = Tuning::PLAYER_HEAVY_POSTURE_DAMAGE;
        return attack;
    }();
    return data;
}

/**
 * @brief 敵の攻撃の種類。
 *
 * 攻撃が1種類しかないと「待てば必ず同じことが起きる」ため読み合いにならない。
 * 予兆の長さ・射程・隙の大きさを変えた3種類を用意し、
 * プレイヤーが予兆を見て「回避する/踏み込む/距離を取る」を選べるようにする。
 */
enum class EnemyAttackKind
{
    Slam,  ///< 標準。予兆が長く、隙も大きい。
    Bite,  ///< 速い。予兆が短く隙も小さいが、射程が短い。
    Sweep, ///< 遅い。予兆が非常に長いが、射程が広く隙が最大。
};

/** 敵の攻撃(叩き付け)。 */
inline const AttackData& EnemyBasicAttack()
{
    static const AttackData data = [] {
        AttackData attack;
        attack.attackId = "enemy_slam";
        attack.animationName = "attack";
        attack.frames.anticipationSeconds = Tuning::ENEMY_ANTICIPATION;
        attack.frames.activeSeconds = Tuning::ENEMY_ACTIVE;
        attack.frames.recoverySeconds = Tuning::ENEMY_RECOVERY;
        attack.frames.cooldownSeconds = Tuning::ENEMY_COOLDOWN;
        attack.broadPhaseFilter.maxDistance = Tuning::ENEMY_HIT_RANGE;
        attack.damage = Tuning::ENEMY_DAMAGE;
        attack.sfx = "dragon_attack";
        return attack;
    }();
    return data;
}

/** 敵の攻撃(噛みつき)。速いが射程が短く、隙も小さい。 */
inline const AttackData& EnemyBiteAttack()
{
    static const AttackData data = [] {
        AttackData attack;
        attack.attackId = "enemy_bite";
        attack.animationName = "attack";
        attack.frames.anticipationSeconds = Tuning::ENEMY_BITE_ANTICIPATION;
        attack.frames.activeSeconds = Tuning::ENEMY_BITE_ACTIVE;
        attack.frames.recoverySeconds = Tuning::ENEMY_BITE_RECOVERY;
        attack.frames.cooldownSeconds = Tuning::ENEMY_BITE_COOLDOWN;
        attack.broadPhaseFilter.maxDistance = Tuning::ENEMY_BITE_HIT_RANGE;
        attack.damage = Tuning::ENEMY_BITE_DAMAGE;
        attack.sfx = "dragon_attack";
        return attack;
    }();
    return data;
}

/** 敵の攻撃(薙ぎ払い)。予兆が長く射程が広い。当てられると痛いが隙も最大。 */
inline const AttackData& EnemySweepAttack()
{
    static const AttackData data = [] {
        AttackData attack;
        attack.attackId = "enemy_sweep";
        attack.animationName = "attack";
        attack.frames.anticipationSeconds = Tuning::ENEMY_SWEEP_ANTICIPATION;
        attack.frames.activeSeconds = Tuning::ENEMY_SWEEP_ACTIVE;
        attack.frames.recoverySeconds = Tuning::ENEMY_SWEEP_RECOVERY;
        attack.frames.cooldownSeconds = Tuning::ENEMY_SWEEP_COOLDOWN;
        attack.broadPhaseFilter.maxDistance = Tuning::ENEMY_SWEEP_HIT_RANGE;
        attack.damage = Tuning::ENEMY_SWEEP_DAMAGE;
        attack.sfx = "dragon_attack";
        return attack;
    }();
    return data;
}

/** 種類から攻撃データを引く。 */
inline const AttackData& EnemyAttackOf(EnemyAttackKind kind)
{
    switch (kind)
    {
    case EnemyAttackKind::Bite:  return EnemyBiteAttack();
    case EnemyAttackKind::Sweep: return EnemySweepAttack();
    case EnemyAttackKind::Slam:
    default:                     return EnemyBasicAttack();
    }
}

/**
 * @brief 敵の攻撃クリップの使う区間と再生速度を種類から引く。
 *
 * 戦闘判定(予兆・判定・隙の長さ)も、アニメーションの再生位置も、この1つの表から決める。
 * 別々に持つと「見えている振り下ろし」と「実際に当たる瞬間」がずれる。
 */
inline const Tuning::EnemyAttackClip& EnemyAttackClipOf(EnemyAttackKind kind)
{
    switch (kind)
    {
    case EnemyAttackKind::Bite:  return Tuning::ENEMY_BITE_CLIP;
    case EnemyAttackKind::Sweep: return Tuning::ENEMY_SWEEP_CLIP;
    case EnemyAttackKind::Slam:
    default:                     return Tuning::ENEMY_SLAM_CLIP;
    }
}

/** 攻撃判定が出ている間の踏み込みの速さ(単位/秒)を種類から引く。 */
inline float EnemyLungeSpeedOf(EnemyAttackKind kind)
{
    switch (kind)
    {
    case EnemyAttackKind::Bite:  return Tuning::ENEMY_BITE_LUNGE_SPEED;
    case EnemyAttackKind::Sweep: return Tuning::ENEMY_SWEEP_LUNGE_SPEED;
    case EnemyAttackKind::Slam:
    default:                     return Tuning::ENEMY_LUNGE_SPEED;
    }
}

/** Active開始からダメージが発生するまでの時間を種類から引く。 */
inline float EnemyFirstHitTimeOf(EnemyAttackKind kind)
{
    switch (kind)
    {
    case EnemyAttackKind::Bite:  return Tuning::ENEMY_BITE_FIRST_HIT_TIME;
    case EnemyAttackKind::Sweep: return Tuning::ENEMY_SWEEP_FIRST_HIT_TIME;
    case EnemyAttackKind::Slam:
    default:                     return Tuning::ENEMY_FIRST_HIT_TIME;
    }
}

/**
 * @brief 当たり判定の左右の広さ(正面からの片側角度)を種類から引く。
 *
 * 敵の正面方向とプレイヤーへの方向の角度差がこの値以内なら当たる。
 * 距離だけの判定にすると、横へ回り込んでも当たってしまい、
 * 「予兆の形を見て回り込む/回避する」という選択が意味を持たなくなる。
 */
inline float EnemyHitHalfAngleOf(EnemyAttackKind kind)
{
    switch (kind)
    {
    case EnemyAttackKind::Bite:  return Tuning::ENEMY_BITE_HIT_HALF_ANGLE;
    case EnemyAttackKind::Sweep: return Tuning::ENEMY_SWEEP_HIT_HALF_ANGLE;
    case EnemyAttackKind::Slam:
    default:                     return Tuning::ENEMY_HIT_HALF_ANGLE;
    }
}

/** プレイヤーが吹き飛ばされる強さ。 */
struct KnockbackData
{
    float speed = 0.0f;   ///< 初速(単位/秒)
    float seconds = 0.0f; ///< 飛ばされている時間(秒)
};

/** 敵の攻撃を食らったときの吹き飛ばしを種類から引く。 */
inline KnockbackData EnemyKnockbackOf(EnemyAttackKind kind)
{
    switch (kind)
    {
    case EnemyAttackKind::Bite:
        return { Tuning::ENEMY_BITE_KNOCKBACK_SPEED, Tuning::ENEMY_BITE_KNOCKBACK_SECONDS };
    case EnemyAttackKind::Sweep:
        return { Tuning::ENEMY_SWEEP_KNOCKBACK_SPEED, Tuning::ENEMY_SWEEP_KNOCKBACK_SECONDS };
    case EnemyAttackKind::Slam:
    default:
        return { Tuning::ENEMY_KNOCKBACK_SPEED, Tuning::ENEMY_KNOCKBACK_SECONDS };
    }
}

/**
 * @brief デバッグ表示用の日本語名。
 *
 * HUDは英字で統一しているため、`EnemyAttackDisplayName()`とは別に持つ。
 * デバッグ表示は開発者が読むものなので日本語にする。
 */
inline const char* EnemyAttackDebugName(EnemyAttackKind kind)
{
    switch (kind)
    {
    case EnemyAttackKind::Bite:  return "噛みつき";
    case EnemyAttackKind::Sweep: return "薙ぎ払い";
    case EnemyAttackKind::Slam:
    default:                     return "叩き付け";
    }
}

/** HUDへ予兆を表示するときの短い名前。プレイヤーが「何が来るか」を読む手がかり。 */
inline const char* EnemyAttackDisplayName(EnemyAttackKind kind)
{
    switch (kind)
    {
    case EnemyAttackKind::Bite:  return "QUICK BITE";
    case EnemyAttackKind::Sweep: return "WIDE SWEEP";
    case EnemyAttackKind::Slam:
    default:                     return "HEAVY SLAM";
    }
}
} // namespace Combat
