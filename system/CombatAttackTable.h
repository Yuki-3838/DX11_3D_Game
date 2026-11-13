#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <vector>
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

// --- 回復できる体力(体力バーの赤い部分) ---
// モンハンと同じく、被弾で減った体力の一部を赤く残し、時間が経つと緑へ戻す(実際に回復する)。
// 以前は赤を「減った分の名残」として縮めるだけで、体力は戻らなかった(ユーザー判断 2026-11-11 で変更)。
// 回復の速さはモンハンワールドの自然回復「1.5秒ごとに体力1」(攻略wikiの値。最大体力も同じ100)。
// 被ダメージのうち赤になる割合は公式・攻略の数値が見つからなかったので仮の値。
// デバッグ表示の「戦闘」タブで変えて試せる。
inline constexpr float PLAYER_RECOVERY_DELAY_SECONDS = 1.5f;   ///< 被弾してから回復が始まるまで
inline constexpr float PLAYER_RECOVERY_PER_SECOND = 1.0f / 1.5f;
inline constexpr float PLAYER_RECOVERABLE_RATIO = 0.5f;        ///< 被ダメージのうち赤(回復できる分)になる割合

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
// 薙ぎ払いは判定が0.34秒と長く、「判定が長すぎる」と指摘されたので0.25秒へ詰めた。
inline constexpr EnemyAttackClip ENEMY_SWEEP_CLIP{ 0.00f, 0.95f, 1.08f, 1.15f, 1.71f, 0.80f, 0.90f };

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
// 敵が攻撃を決める距離は32(enemy.hのATTACK_DISTANCE)、好む距離は26。
// 当たる距離が64もあると、回避で下がれる34を使っても間合いの外へ出られず、
// 「予兆を見て距離を取る」選択が成立しない(実際に遊んで「ほぼ避けられない」と指摘された)。
// 回避1回で外へ出られる距離にする。
inline constexpr float ENEMY_HIT_RANGE = 42.0f;
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
inline constexpr float ENEMY_BITE_HIT_RANGE = 34.0f;
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
inline constexpr float ENEMY_SWEEP_HIT_RANGE = 55.0f;
inline constexpr float ENEMY_SWEEP_FIRST_HIT_TIME = ENEMY_SWEEP_CLIP.FirstHitSeconds();

// --- 敵の攻撃(尾回転): 体を回して尾で薙ぐ ---
// モンスターハンターのリオレウス・リオレイアの回転尾攻撃を参考にした。
// ほかの3種類は「正面の相手を殴る」攻撃なので、正面を外して立ち回っていれば当たらない。
// それだけだと「横か後ろに居れば安全」という一本道の答えになってしまうため、
// 体ごと半回転して**背後から側面をまとめて薙ぐ**攻撃を足す。これで
//   正面に居る     → 叩き付け・噛みつき・薙ぎ払いが危ない
//   横や後ろに居る → 尾回転が危ない
// となり、「どこに立つか」も読み合いになる。
//
// この攻撃だけは当たり判定の基準が敵の正面ではなく**尾の向き(正面の反対)**で、
// 回転に合わせて危険な方向が動く(判定はOneVsOneCombat側で毎フレーム見る)。
// 予兆は4種類の中で最も長い。見えたら「離れる」か「懐へ入る」かを選べるようにする。
inline constexpr float ENEMY_SPIN_ANTICIPATION = 1.15f;
// 半回転1回分の時間。回転数(1回か2回)は攻撃ごとに敵AIが決める。
// 2回のときは判定の時間もその分だけ長くなる(enemy::activeSeconds と OneVsOneCombat の両方で倍率を掛ける)。
inline constexpr float ENEMY_SPIN_HALF_TURN_SECONDS = 0.42f;
// 半回転と半回転の間で止まっている時間(一拍)。
// リオレウス・リオレイアの回転尾攻撃は、ぐるりと一周回すのではなく
// 「半回転 → 一拍おいて構え直す → もう半回転」というリズムで来る。
// 一続きに回してしまうと、どこで止まるかが読めず、避けても踏み込めない。
// 一拍あることで「ここで終わりか、もう一度来るか」を見る間が生まれる。
//
// ここは**2回目の呼び動作(溜め直し)の長さ**でもある。0.30秒では
// 止まった次の瞬間にもう尾が来ていて、実機で「2回目が避けられない」と指摘された。
// 止まる → 尾を引き寄せて溜め直す → 振る、が見て取れる長さにする。
// 内訳の目安: 前半0.15秒で振り抜いた形から戻り、0.15〜0.50秒で溜め直し、残りは溜めたまま待つ。
inline constexpr float ENEMY_SPIN_PAUSE_SECONDS = 0.60f;
inline constexpr float ENEMY_SPIN_ACTIVE = ENEMY_SPIN_HALF_TURN_SECONDS;
inline constexpr int ENEMY_SPIN_MAX_HALF_TURNS = 2;
// 回り切った後は体の向きが変わっている(半回転1回なら背中を向けている)。
// 隙を長く取り、その間に側面や背後を殴れるようにする。反撃の本命。
inline constexpr float ENEMY_SPIN_RECOVERY = 1.05f;
inline constexpr float ENEMY_SPIN_COOLDOWN = 1.40f;
inline constexpr int ENEMY_SPIN_DAMAGE = 18;
// 尾の届く距離。薙ぎ払い(55)とほぼ同じ長さを持つ。
// ただし懐(16以内)は尾が頭上を通るので当たらない。「離れる」以外に「踏み込む」答えを残すため。
inline constexpr float ENEMY_SPIN_HIT_RANGE = 52.0f;
inline constexpr float ENEMY_SPIN_MIN_HIT_RANGE = 16.0f;
// 尾が通り過ぎる瞬間だけ当たる。判定は回転の始まりから見はじめる。
inline constexpr float ENEMY_SPIN_FIRST_HIT_TIME = 0.0f;
// 尾の通り道の幅(基準の向きからの片側角度)。約40度。細い尾なので、ほかの攻撃より狭い。
inline constexpr float ENEMY_SPIN_HIT_HALF_ANGLE = 0.70f;
// 回転中の移動。回りながら少し流れる程度にする(大きいと体ごと突っ込む攻撃に見える)。
inline constexpr float ENEMY_SPIN_LUNGE_SPEED = 18.0f;
// 尾で薙がれた側は横へ大きく飛ばされる。
inline constexpr float ENEMY_SPIN_KNOCKBACK_SPEED = 360.0f;
inline constexpr float ENEMY_SPIN_KNOCKBACK_SECONDS = 0.48f;

// --- 離れた相手への攻撃 ---
// 以前の4種類はどれも「近くの相手を殴る」攻撃で、敵はプレイヤーを追いかけて向き直り続けるしかなかった。
// 距離を取ったプレイヤーは安全で、敵の側にも「離れた相手をどうするか」の答えが無い。
// モンスターハンターの大型モンスターと同じく、離れた相手には火球と突進で応じる。
// どちらも**撃った/走り出した瞬間に向きが決まり、それ以上は追いかけない**のが要点で、
// 敵が常にプレイヤーの方を向き続ける作りから抜け出す(横へ避ければ外れ、突進は相手を通り過ぎる)。
// この距離より離れていたら、近接ではなくこの2種類から選ぶ。
inline constexpr float ENEMY_RANGED_MIN_DISTANCE = 55.0f;
// 様子見を待たずに、すぐ離れた相手への攻撃を選ぶ距離(逃げ回る相手への答え)。
inline constexpr float ENEMY_RANGED_FAR_DISTANCE = 85.0f;
// 接近中、これだけ近づけずにいたら離れた相手への攻撃へ切り替える。
inline constexpr float ENEMY_RANGED_APPROACH_SECONDS = 0.9f;

// --- 敵の攻撃(火球): 首を持ち上げ口に火を溜め、プレイヤーが**いた場所**へ吐く ---
// リオレウスの火球ブレスを参考にした。狙うのは吐いた瞬間のプレイヤーの位置で、追いかけない。
// 予兆の間に口の火が大きくなるので、吐く直前に横へずれれば外れる(前転で避けてもよい)。
// 予兆の最後の一部は向き直りを止める(狙いを決めた)ので、溜めの終わり際に動いた分だけ逸れる。
inline constexpr float ENEMY_FIREBALL_ANTICIPATION = 0.95f;
inline constexpr float ENEMY_FIREBALL_ACTIVE = 0.18f;
inline constexpr float ENEMY_FIREBALL_RECOVERY = 0.85f;
inline constexpr float ENEMY_FIREBALL_COOLDOWN = 1.20f;
inline constexpr int ENEMY_FIREBALL_DAMAGE = 14;
// 予兆のうち、向き直りを止める割合(最後の25%)。ここから先に動けば狙いが外れる。
inline constexpr float ENEMY_FIREBALL_AIM_LOCK_RATIO = 0.75f;
// 火球の速さ(単位/秒)。距離80なら約0.5秒で着弾する。見てから横へ1歩ずれれば避けられる速さ。
inline constexpr float ENEMY_FIREBALL_SPEED = 170.0f;
// 火球の大きさ(当たり判定の半径)と、着弾したときの爆発の半径。
// 爆発の半径は前転(34)1回で外へ出られる大きさに留める。
inline constexpr float ENEMY_FIREBALL_RADIUS = 4.0f;
inline constexpr float ENEMY_FIREBALL_EXPLOSION_RADIUS = 14.0f;
// 正面から左右へ曲げて吐ける角度(片側)。真横のプレイヤーへは吐けない。
// 首より外へ曲がると、口とは関係ない向きへ火球が飛んで見える。
inline constexpr float ENEMY_FIREBALL_MAX_AIM_ANGLE = 0.61f; // 約35度
// 飛んでいられる最長の時間。外れて壁の外へ飛んでいった火球を消すため。
inline constexpr float ENEMY_FIREBALL_LIFETIME = 2.5f;
inline constexpr float ENEMY_FIREBALL_KNOCKBACK_SPEED = 260.0f;
inline constexpr float ENEMY_FIREBALL_KNOCKBACK_SECONDS = 0.40f;

// --- 敵の攻撃(突進): 頭を下げて地面を掻き、向きを決めて一直線に走る ---
// ティガレックスやディアブロスの突進を参考にした。走り出したら曲がらず、相手を通り過ぎて止まる。
// 止まったときは背中を向けているので、そこが反撃の機会になる。
// 壁へぶつかると、その場で長くよろめく(大きな隙)。壁際へ誘って避けるのが上手な避け方になる。
inline constexpr float ENEMY_CHARGE_ANTICIPATION = 1.00f;
inline constexpr float ENEMY_CHARGE_ACTIVE = 0.95f;
inline constexpr float ENEMY_CHARGE_RECOVERY = 1.10f;
inline constexpr float ENEMY_CHARGE_COOLDOWN = 1.40f;
inline constexpr int ENEMY_CHARGE_DAMAGE = 20;
// 予兆のうち、向き直りを止める割合(最後の20%)。走り出す向きはここで決まる。
inline constexpr float ENEMY_CHARGE_AIM_LOCK_RATIO = 0.80f;
// 走る速さ(単位/秒)。接近(84)の2倍強。0.95秒で約175進む(闘技場の半径は300)。
inline constexpr float ENEMY_CHARGE_SPEED = 185.0f;
// 走り出しで最高速へ達するまでの時間。いきなり最高速だと、溜めから瞬間移動したように見える。
inline constexpr float ENEMY_CHARGE_ACCEL_SECONDS = 0.18f;
// 走り終わってから滑って止まるまでの時間(隙の前半)。
inline constexpr float ENEMY_CHARGE_SKID_SECONDS = 0.45f;
// 当たる範囲。体そのものがぶつかる攻撃なので、正面寄りの体の幅だけにする。
// 敵とプレイヤーは体の球で押し離されるので、その距離より短いと絶対に当たらない。
// 噛みつき(34)が当たっているので、それより少しだけ長くする。
inline constexpr float ENEMY_CHARGE_HIT_RANGE = 36.0f;
inline constexpr float ENEMY_CHARGE_HIT_HALF_ANGLE = 1.22f; // 約70度
inline constexpr float ENEMY_CHARGE_KNOCKBACK_SPEED = 440.0f;
inline constexpr float ENEMY_CHARGE_KNOCKBACK_SECONDS = 0.55f;
// 突進を選ぶ最大の距離。走る距離は約165(走り)+約28(滑り)なので、これより遠いと届かずに手前で止まる
// (撮影で、300離れた相手へ突進して130手前で止まるのを確認した)。それより遠い相手には火球を吐く。
// 走って逃げる相手(走りの速さ45)は予兆と走りの約2秒で約90離れるので、それでも届く距離にしている
// (170では、145から突進して逃げる相手の手前で止まった)。
inline constexpr float ENEMY_CHARGE_MAX_DISTANCE = 130.0f;
// 壁にぶつかったときに、隙へ足すよろめきの時間。
inline constexpr float ENEMY_CHARGE_WALL_STUN_SECONDS = 1.10f;

// --- 攻撃中の踏み込みの速さ(単位/秒) ---
// 判定が出ている間だけ前へ出る。判定を短くした分だけ速くして、踏み込む距離を保つ
// (叩き付けで約26、噛みつきで約11、薙ぎ払いで約34)。
// モンスターハンターの大型モンスターも、攻撃の瞬間に体ごと前へ出る。距離を取るだけでは避けられない、
// という読み合いをここで作っている。
// 踏み込みが大きいと、回避で下がっても結局間合いへ引き戻される。
// 叩き付け約16・噛みつき約7・薙ぎ払い約18進む程度にして、回避で外れる余地を残す。
inline constexpr float ENEMY_LUNGE_SPEED = 80.0f;
inline constexpr float ENEMY_BITE_LUNGE_SPEED = 60.0f;
inline constexpr float ENEMY_SWEEP_LUNGE_SPEED = 70.0f;

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

// --- 敵の怒り(モンスターハンターの怒り状態) ---
// 弱り具合(体力で決まる)が「だんだん弱る」変化なのに対し、怒りは
// 「殴り続けると、ある瞬間に切り替わる」変化にする。
// 戦いの途中で相手の圧が一段上がり、こちらの立ち回りを変えさせるのが狙い。
//
// 怒っている間に変えるのは**攻撃の間隔と動きの速さ**だけにして、
// 予兆・判定・隙の長さ(攻撃の表)には手を付けない。
// それらを敵AI側だけで縮めると、戦闘判定(OneVsOneCombat)の時間とずれて
// 「振り下ろしより先に当たる」ような食い違いが起きるためである。
//
// 怒りに入る瞬間は咆哮で知らせる。ここは攻撃が来ない安全な時間で、
// プレイヤーが距離を取り直す間になる(モンスターハンターの咆哮と同じ役割)。
inline constexpr float ENEMY_RAGE_DAMAGE_THRESHOLD = 260.0f;
// 怒るたびに必要なダメージを増やす。1戦(体力2000)で3〜4回ほど怒る計算。
//   260 → 351 → 474 → 640 ...
inline constexpr float ENEMY_RAGE_THRESHOLD_GROWTH = 1.35f;
inline constexpr float ENEMY_RAGE_SECONDS = 24.0f;
inline constexpr float ENEMY_RAGE_ROAR_SECONDS = 1.30f;
// 怒っている間の動きの速さ。近づくのも周回も速くなる。
inline constexpr float ENEMY_RAGE_MOVE_SCALE = 1.25f;
// 怒っている間の様子見の長さの倍率。短いほど攻撃が続けて来る。
inline constexpr float ENEMY_RAGE_CIRCLE_SCALE = 0.55f;
// 怒っている間は怯みにくい。殴り返して止める、が通りにくくなる。
inline constexpr float ENEMY_RAGE_FLINCH_RESIST = 1.5f;

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
 * @brief 攻撃の途中で起こす出来事(タイムラインのイベント)。
 *
 * クリップの決まった瞬間に、効果音・カメラの揺れ・火花を出す。
 * 「当たったとき」の演出とは別で、当たらなくても出る(振りの風切り音や、地面を叩いた火花など)。
 * 時刻はクリップの秒で持つ(再生速度を変えても、振りの同じ瞬間で起きる)。
 */
enum class TimelineEventType
{
    Sound,       ///< 効果音
    CameraShake, ///< カメラの揺れ
    Sparks,      ///< 剣先から火花
    Count
};

/** イベントで鳴らせる効果音(SoundManagerにある音)。 */
enum class TimelineSound
{
    Whoosh,       ///< 風切り音(回避と同じ音)
    SwordHit,     ///< 斬撃音(命中時と同じ音)
    DragonAttack, ///< 竜の攻撃の音
    Roar,         ///< 咆哮
    Count
};

struct TimelineEvent
{
    float time = 0.0f;            ///< 起こす時刻(クリップの秒)
    TimelineEventType type = TimelineEventType::Sound;
    TimelineSound sound = TimelineSound::Whoosh; ///< Sound用
    float strength = 1.0f;        ///< CameraShake: 揺れの強さ / Sparks: 火花の大きさ
    float duration = 0.2f;        ///< CameraShake: 揺れる時間(秒)
    int count = 12;               ///< Sparks: 火花の本数
};

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
 *
 * 2026-10-26から、値は assets/data/attack_timeline.txt で上書きできる
 * (ゲーム内のタイムラインエディター(F6)で編集・保存する)。下の表はファイルが無いときの既定値。
 */
struct PlayerComboStep
{
    std::string clipFile;        ///< 使うクリップ(assets/motion の .fbx)
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
    /// 剣の軌跡を出す区間(クリップの秒)。負の値なら攻撃判定と同じ区間。
    float trailStart = -1.0f;
    float trailEnd = -1.0f;
    /// 途中で起こすイベント(効果音・カメラの揺れ・火花)。
    std::vector<TimelineEvent> events;

    float WindupSeconds() const { return (hitStart - clipStart) / playbackRate; }
    float ActiveSeconds() const { return (hitEnd - hitStart) / playbackRate; }
    float RecoverySeconds() const { return (clipEnd - hitEnd) / playbackRate; }
    float TotalSeconds() const { return (clipEnd - clipStart) / playbackRate; }
    float TrailStart() const { return trailStart >= 0.0f ? trailStart : hitStart; }
    float TrailEnd() const { return trailEnd >= 0.0f ? trailEnd : hitEnd; }
    /// 予約した次の段へ移れるようになる位置(クリップの秒)。
    float ComboLinkClipTime() const { return hitEnd + comboLinkSeconds * playbackRate; }
};

/** 編集できるプレイヤーの技。タイムラインエディターとファイルの並び順。 */
enum class PlayerAttackSlot
{
    Weak1, Weak2, Weak3,
    Heavy1, Heavy2, Heavy3,
    Dash,
    Count
};

inline constexpr int PLAYER_ATTACK_SLOT_COUNT = static_cast<int>(PlayerAttackSlot::Count);

/** ファイルに書く名前(英数字)。 */
inline const char* PlayerAttackSlotKey(PlayerAttackSlot slot)
{
    static const char* keys[PLAYER_ATTACK_SLOT_COUNT] = {
        "weak1", "weak2", "weak3", "heavy1", "heavy2", "heavy3", "dash" };
    return keys[static_cast<int>(slot)];
}

/** 画面に出す名前。 */
inline const char* PlayerAttackSlotLabel(PlayerAttackSlot slot)
{
    static const char* labels[PLAYER_ATTACK_SLOT_COUNT] = {
        "弱1", "弱2", "弱3", "強1", "強2", "強3", "ダッシュ" };
    return labels[static_cast<int>(slot)];
}

inline PlayerAttackSlot PlayerAttackSlotOf(bool heavy, int comboStep, bool dash = false)
{
    if (dash)
        return PlayerAttackSlot::Dash;
    const int index = std::clamp(comboStep, 1, 3) - 1;
    return static_cast<PlayerAttackSlot>((heavy ? 3 : 0) + index);
}

/** 既定値(ファイルが無いとき・「既定に戻す」で使う)。 */
inline const PlayerComboStep& DefaultPlayerAttack(PlayerAttackSlot slot)
{
    // 右腕の振りの計測値(クリップの秒): slash(5) 0.49-0.64 / slash 0.55-0.64 / slash(3) 0.74-0.86 /
    // attack 1.05-1.28 / attack(4) 0.17-0.55 / attack(3) 0.68-0.81 / slash(4) 1.28-1.40
    static const PlayerComboStep defaults[PLAYER_ATTACK_SLOT_COUNT] = {
        // 弱1: slash (5)。低く踏み込んで斬り、戻る。出が早い基本の一撃。
        // (以前は跳躍斬りの attack を1段目にしていたが、立ち止まった状態からいきなり跳ぶのは
        //  弱攻撃の出だしにふさわしくない、とユーザー判断。跳躍斬りはダッシュ攻撃へ移した)
        { "assets/motion/sword and shield slash (5).fbx", 0.24f, 0.44f, 0.70f, 1.20f, 1.30f, 0.05f },
        // 弱2: slash (3)。低い位置から斬り上げる。
        // (以前は slash にしていたが、1段目と同じく「剣を同じ側へ振り上げて斜めに振り下ろす」軌道で、
        //  どちらの段か見分けられない、とユーザー指摘。右手の軌跡を測って、軌道が逆向きのものを選んだ)
        { "assets/motion/sword and shield slash (3).fbx", 0.44f, 0.68f, 0.92f, 1.60f, 1.25f, 0.08f },
        // 弱3: attack (4)。反対側から横へ薙ぐ。振り下ろし → 斬り上げ → 横薙ぎと、段ごとに剣の向きを変える。
        { "assets/motion/sword and shield attack (4).fbx", 0.02f, 0.15f, 0.58f, 0.95f, 1.20f, 0.10f },
        // 強1: slash。高く振りかぶり、約42cm踏み込みながら振り下ろす。
        // 溜めを長めに見せて(0.36秒)、弱攻撃より重い一撃にする。
        // (以前は attack (4) だったが、弱攻撃の3段目へ回したので差し替えた)
        { "assets/motion/sword and shield slash.fbx",      0.10f, 0.50f, 0.72f, 1.30f, 1.10f, 0.08f },
        // 強2: attack (3)。約226cm前進しながら回転して斬る。
        // 回転しながら2回振るクリップで、2回目の振りは0.75〜1.05秒(手の速さが最大なのは0.95秒)。
        // 以前は判定の終わりを0.90秒にしていたため、次の段へ1.0秒で移り、振り切る前に3段目が始まっていた。
        // (右腕の関節の回転だけで振りの区間を測っていたので、体の回転で剣が動く分を見落としていた)
        { "assets/motion/sword and shield attack (3).fbx", 0.34f, 0.62f, 1.05f, 1.60f, 1.25f, 0.12f },
        // 強3: slash (4)。沈み込んで振り抜く回転斬り。締めの大技なので硬直が最も長い。
        { "assets/motion/sword and shield slash (4).fbx",  0.84f, 1.22f, 1.50f, 2.30f, 1.25f, 0.10f },
        // ダッシュ攻撃: attack(跳び上がって約223cm前へ出る斬り下ろし)。
        // 踏み込みはクリップのままだと約22(身長の1.2倍)で、走りから出す技としては物足りない
        // (ユーザー指摘「ダッシュattackはちょっと前に進んでほしい」)。跳んでいる間の移動なので、
        // 1.6倍(約36、身長の2倍)まで伸ばしても地面を滑って見えない。
        { "assets/motion/sword and shield attack.fbx", 0.55f, 0.98f, 1.34f, 2.05f, 1.45f, 0.06f, 1.6f },
    };
    return defaults[static_cast<int>(slot)];
}

/**
 * @brief ゲームが実際に使う技のデータ(書き換えられる)。
 *
 * 起動時は既定値で始まり、assets/data/attack_timeline.txt があればその値で上書きされる
 * (AttackTimelineFile::Load)。タイムラインエディターはここを直接書き換えるので、
 * 編集した値はその場でゲームの攻撃にも効く。
 */
inline PlayerComboStep& EditablePlayerAttack(PlayerAttackSlot slot)
{
    static std::array<PlayerComboStep, PLAYER_ATTACK_SLOT_COUNT> steps = [] {
        std::array<PlayerComboStep, PLAYER_ATTACK_SLOT_COUNT> result;
        for (int i = 0; i < PLAYER_ATTACK_SLOT_COUNT; ++i)
            result[i] = DefaultPlayerAttack(static_cast<PlayerAttackSlot>(i));
        return result;
    }();
    return steps[static_cast<int>(slot)];
}

/** コンボの段(1〜3)の設定を引く。 */
inline const PlayerComboStep& PlayerComboStepOf(bool heavy, int comboStep)
{
    return EditablePlayerAttack(PlayerAttackSlotOf(heavy, comboStep));
}

/**
 * @brief ダッシュ攻撃(走っているときに弱攻撃を押すと出る)。
 *
 * モンスターハンターの片手剣の突進斬りと同じく、走りから派生して大きく踏み込み、距離を詰めて斬る。
 * 立ち止まった状態の弱攻撃1段目の代わりに出て、そのまま弱攻撃の2段目へつなげられる。
 */
inline const PlayerComboStep& PlayerDashAttackStep()
{
    return EditablePlayerAttack(PlayerAttackSlot::Dash);
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
    Slam,     ///< 標準。予兆が長く、隙も大きい。
    Bite,     ///< 速い。予兆が短く隙も小さいが、射程が短い。
    Sweep,    ///< 遅い。予兆が非常に長いが、射程が広く隙が最大。
    // 体ごと半回転して尾で薙ぐ。危ないのは正面ではなく側面と背後。
    // ほかの3種類とは危険な方向が逆なので、「どこに立つか」の読み合いを作る。
    TailSpin,
    // 離れた相手への攻撃。どちらも向きを決めたら追いかけない。
    Fireball, ///< 口から火球を吐く。吐いた瞬間にプレイヤーがいた場所へ飛ぶ。
    Charge,   ///< 向きを決めて一直線に走る。相手を通り過ぎて止まる。
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

/**
 * @brief 敵の攻撃(尾回転)。体ごと半回転して尾で薙ぐ。
 *
 * 危ないのは正面ではなく側面と背後。ほかの3種類を正面から避けて
 * 「横へ回り込む」のが答えになりすぎないよう、その逃げ場を刈る役割を持つ。
 * 当たり判定の基準の向きだけが特別(正面ではなく尾の向き)で、
 * それは OneVsOneCombat が EnemyAttackUsesTailArc() を見て切り替える。
 */
inline const AttackData& EnemyTailSpinAttack()
{
    static const AttackData data = [] {
        AttackData attack;
        attack.attackId = "enemy_tail_spin";
        attack.animationName = "walk"; // 回転中は歩きのクリップで脚を動かす(専用モーションが無い)
        attack.frames.anticipationSeconds = Tuning::ENEMY_SPIN_ANTICIPATION;
        attack.frames.activeSeconds = Tuning::ENEMY_SPIN_ACTIVE;
        attack.frames.recoverySeconds = Tuning::ENEMY_SPIN_RECOVERY;
        attack.frames.cooldownSeconds = Tuning::ENEMY_SPIN_COOLDOWN;
        attack.broadPhaseFilter.maxDistance = Tuning::ENEMY_SPIN_HIT_RANGE;
        attack.damage = Tuning::ENEMY_SPIN_DAMAGE;
        attack.sfx = "dragon_attack";
        return attack;
    }();
    return data;
}

/**
 * @brief 敵の攻撃(火球)。当たり判定は敵の体ではなく、飛んでいく火球が持つ。
 *
 * 戦闘判定(OneVsOneCombat)は、この攻撃の判定時間には何も当てない(EnemyMaxHitsOf が0)。
 * ダメージは火球が着弾したときに、シーン側が OneVsOneCombat::ApplyEnemyProjectileHit() で入れる。
 * 火球は吐いた後も飛び続けるので、敵の攻撃の段階(隙など)とは別に当たる必要があるためである。
 */
inline const AttackData& EnemyFireballAttack()
{
    static const AttackData data = [] {
        AttackData attack;
        attack.attackId = "enemy_fireball";
        attack.animationName = "idle"; // 立ち姿勢に首・あごの構えを重ねる(専用モーションが無い)
        attack.frames.anticipationSeconds = Tuning::ENEMY_FIREBALL_ANTICIPATION;
        attack.frames.activeSeconds = Tuning::ENEMY_FIREBALL_ACTIVE;
        attack.frames.recoverySeconds = Tuning::ENEMY_FIREBALL_RECOVERY;
        attack.frames.cooldownSeconds = Tuning::ENEMY_FIREBALL_COOLDOWN;
        attack.broadPhaseFilter.maxDistance = 0.0f;
        attack.damage = Tuning::ENEMY_FIREBALL_DAMAGE;
        attack.sfx = "dragon_attack";
        return attack;
    }();
    return data;
}

/** 敵の攻撃(突進)。向きを決めて一直線に走り、体でぶつかる。 */
inline const AttackData& EnemyChargeAttack()
{
    static const AttackData data = [] {
        AttackData attack;
        attack.attackId = "enemy_charge";
        attack.animationName = "walk"; // 走っている間は歩きのクリップを速く回す(専用モーションが無い)
        attack.frames.anticipationSeconds = Tuning::ENEMY_CHARGE_ANTICIPATION;
        attack.frames.activeSeconds = Tuning::ENEMY_CHARGE_ACTIVE;
        attack.frames.recoverySeconds = Tuning::ENEMY_CHARGE_RECOVERY;
        attack.frames.cooldownSeconds = Tuning::ENEMY_CHARGE_COOLDOWN;
        attack.broadPhaseFilter.maxDistance = Tuning::ENEMY_CHARGE_HIT_RANGE;
        attack.damage = Tuning::ENEMY_CHARGE_DAMAGE;
        attack.sfx = "dragon_attack";
        return attack;
    }();
    return data;
}

/**
 * @brief 攻撃クリップ(dragon_attack.dae)で動かす攻撃か。
 *
 * 攻撃クリップは「頭を持ち上げて地面へ振り下ろす」1本だけで、体ごと回る尾回転・
 * 上を向いて火を吐く火球・走る突進とは形が合わない。それらは立ち姿勢(または歩き)に
 * 首・あご・尾の構え(EnemyTellPose)を重ねて見せる。
 */
inline bool EnemyAttackUsesAttackClip(EnemyAttackKind kind)
{
    return kind == EnemyAttackKind::Slam ||
        kind == EnemyAttackKind::Bite ||
        kind == EnemyAttackKind::Sweep;
}

/**
 * @brief 判定時間の間、毎フレーム当たりを見る攻撃か。
 *
 * ほかの攻撃は「当たる瞬間」に1回だけ見る。尾回転は危険な向きが回転につれて動き、
 * 突進は体ごと走って危険な場所そのものが動くので、どちらも判定時間の間ずっと見る。
 */
inline bool EnemyAttackHitsContinuously(EnemyAttackKind kind)
{
    return kind == EnemyAttackKind::TailSpin || kind == EnemyAttackKind::Charge;
}

/**
 * @brief 当たり判定の基準を「尾の向き(正面の反対)」にする攻撃か。
 *
 * これがtrueの攻撃は、当たる方向が回転につれて動くので、
 * 判定を「当たる瞬間の1回」ではなく判定時間中は毎フレーム見る必要がある。
 */
inline bool EnemyAttackUsesTailArc(EnemyAttackKind kind)
{
    return kind == EnemyAttackKind::TailSpin;
}

/** 尾回転の判定(=回っている)時間。半回転の回数と、その間の一拍を足した長さ。 */
inline float EnemySpinActiveSeconds(int halfTurns)
{
    const int turns = std::clamp(halfTurns, 1, Tuning::ENEMY_SPIN_MAX_HALF_TURNS);
    return static_cast<float>(turns) * Tuning::ENEMY_SPIN_HALF_TURN_SECONDS +
        static_cast<float>(turns - 1) * Tuning::ENEMY_SPIN_PAUSE_SECONDS;
}

/**
 * @brief 尾回転の進み具合。いま回っているのか、半回転の合間で止まっているのか。
 *
 * 回転・一拍・回転…という区切りのどこにいるかを、判定側(OneVsOneCombat)・
 * 敵AI(enemy)・構え(EnemyAttackPose.h)がすべてこの1つの計算から取る。
 * 別々に持つと「見た目は止まっているのに当たる」といったずれが生まれる。
 */
struct EnemySpinPhase
{
    float turnedHalfTurns = 0.0f; ///< これまでに回った半回転の数(小数。0→halfTurns)
    bool pausing = false;         ///< 半回転と半回転の間で止まっているか
    int segmentIndex = 0;         ///< 何回目の半回転か(0から)
    float segmentT = 0.0f;        ///< いまの区切りの進み具合(0→1)
};

/** 判定開始からの経過秒と半回転の回数から、いまどこにいるかを求める。 */
inline EnemySpinPhase EnemySpinPhaseAt(float activeTime, int halfTurns)
{
    // 出だしと止まり際をなめらかにする(半回転ごとに加速して減速する)。
    // 等速で回すと、止まった瞬間が分からず「一拍おいた」ように見えない。
    const auto smoothStep = [](float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };
    EnemySpinPhase out{};
    const int turns = std::clamp(halfTurns, 1, Tuning::ENEMY_SPIN_MAX_HALF_TURNS);
    float time = std::max(0.0f, activeTime);
    for (int index = 0; index < turns; ++index)
    {
        // 回っている最中(最後の半回転は、時間を過ぎても回り切った状態で留める)
        if (time < Tuning::ENEMY_SPIN_HALF_TURN_SECONDS || index == turns - 1)
        {
            out.segmentIndex = index;
            out.segmentT = std::clamp(time / Tuning::ENEMY_SPIN_HALF_TURN_SECONDS, 0.0f, 1.0f);
            out.turnedHalfTurns = static_cast<float>(index) + smoothStep(out.segmentT);
            return out;
        }
        time -= Tuning::ENEMY_SPIN_HALF_TURN_SECONDS;
        // 一拍(止まっている)
        if (time < Tuning::ENEMY_SPIN_PAUSE_SECONDS)
        {
            out.segmentIndex = index;
            out.segmentT = std::clamp(time / Tuning::ENEMY_SPIN_PAUSE_SECONDS, 0.0f, 1.0f);
            out.pausing = true;
            out.turnedHalfTurns = static_cast<float>(index + 1);
            return out;
        }
        time -= Tuning::ENEMY_SPIN_PAUSE_SECONDS;
    }
    out.segmentIndex = turns - 1;
    out.segmentT = 1.0f;
    out.turnedHalfTurns = static_cast<float>(turns);
    return out;
}

/**
 * @brief 1回の攻撃で当てられる回数。
 *
 * 尾回転は半回転ごとに1回まで当たる。半回転の間に一拍あるので、
 * 1回目を食らっても2回目は避けられる(吹き飛ばしの無敵が切れてから尾が来る)。
 * 2回目が当たらない作りにすると「1回避けたら残りは無視してよい」ことになり、
 * 「回り切るまで踏み込まない」という読み合いが消える。
 */
inline int EnemyMaxHitsOf(EnemyAttackKind kind, int spinHalfTurns)
{
    // 火球は敵の体では当てない(当たり判定は飛んでいく火球が持つ)。
    if (kind == EnemyAttackKind::Fireball)
        return 0;
    if (kind != EnemyAttackKind::TailSpin)
        return 1;
    return std::clamp(spinHalfTurns, 1, Tuning::ENEMY_SPIN_MAX_HALF_TURNS);
}

/** 尾の判定が当たらない懐の距離。これより近いと尾は頭上を通る。 */
inline float EnemyMinHitRangeOf(EnemyAttackKind kind)
{
    return kind == EnemyAttackKind::TailSpin ? Tuning::ENEMY_SPIN_MIN_HIT_RANGE : 0.0f;
}

/** 種類から攻撃データを引く。 */
inline const AttackData& EnemyAttackOf(EnemyAttackKind kind)
{
    switch (kind)
    {
    case EnemyAttackKind::TailSpin: return EnemyTailSpinAttack();
    case EnemyAttackKind::Fireball: return EnemyFireballAttack();
    case EnemyAttackKind::Charge:   return EnemyChargeAttack();
    case EnemyAttackKind::Bite:  return EnemyBiteAttack();
    case EnemyAttackKind::Sweep: return EnemySweepAttack();
    case EnemyAttackKind::Slam:
    default:                     return EnemyBasicAttack();
    }
}

/**
 * @brief 敵の大きさに合わせて、間合い・攻撃の届く距離・移動の速さを伸ばす倍率。
 *
 * 上の表の距離と速さは、背中の高さ約17の敵で決めた値。大型のモデル(Wyvern)は体ごと大きいので、
 * 頭や爪が届く距離も歩幅も同じ割合で大きい。表の値はそのままにして、
 * 敵と相手の距離をこの倍率で割り(enemy::distanceToTarget・OneVsOneCombat)、
 * 敵が動く量をこの倍率で掛ける(enemy::moveInFacingDirection)。
 * 時間(予兆・判定・隙)は変えない。既定の1なら従来どおり。
 */
inline float& EnemyReachScale()
{
    static float scale = 1.0f;
    return scale;
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
    // 尾回転・火球・突進は攻撃クリップを使わない(EnemyAttackUsesAttackClip)。
    // 呼ばれても困らないように叩き付けの区間を返すだけにしておく。
    case EnemyAttackKind::TailSpin:
    case EnemyAttackKind::Fireball:
    case EnemyAttackKind::Charge:
    case EnemyAttackKind::Slam:
    default:                     return Tuning::ENEMY_SLAM_CLIP;
    }
}

/** 攻撃判定が出ている間の踏み込みの速さ(単位/秒)を種類から引く。 */
inline float EnemyLungeSpeedOf(EnemyAttackKind kind)
{
    switch (kind)
    {
    case EnemyAttackKind::TailSpin: return Tuning::ENEMY_SPIN_LUNGE_SPEED;
    // 火球はその場で吐く。突進の走りは敵AIが加速・減速を付けて動かすので、ここは最高速を返す。
    case EnemyAttackKind::Fireball: return 0.0f;
    case EnemyAttackKind::Charge:   return Tuning::ENEMY_CHARGE_SPEED;
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
    case EnemyAttackKind::TailSpin: return Tuning::ENEMY_SPIN_FIRST_HIT_TIME;
    case EnemyAttackKind::Fireball:
    case EnemyAttackKind::Charge:   return 0.0f;
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
    case EnemyAttackKind::TailSpin: return Tuning::ENEMY_SPIN_HIT_HALF_ANGLE;
    case EnemyAttackKind::Charge:   return Tuning::ENEMY_CHARGE_HIT_HALF_ANGLE;
    case EnemyAttackKind::Fireball: return 0.0f;
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
    case EnemyAttackKind::TailSpin:
        return { Tuning::ENEMY_SPIN_KNOCKBACK_SPEED, Tuning::ENEMY_SPIN_KNOCKBACK_SECONDS };
    case EnemyAttackKind::Fireball:
        return { Tuning::ENEMY_FIREBALL_KNOCKBACK_SPEED, Tuning::ENEMY_FIREBALL_KNOCKBACK_SECONDS };
    case EnemyAttackKind::Charge:
        return { Tuning::ENEMY_CHARGE_KNOCKBACK_SPEED, Tuning::ENEMY_CHARGE_KNOCKBACK_SECONDS };
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
    case EnemyAttackKind::TailSpin: return "尾回転";
    case EnemyAttackKind::Fireball: return "火球";
    case EnemyAttackKind::Charge:   return "突進";
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
    case EnemyAttackKind::TailSpin: return "TAIL SPIN";
    case EnemyAttackKind::Fireball: return "FIREBALL";
    case EnemyAttackKind::Charge:   return "CHARGE";
    case EnemyAttackKind::Bite:  return "QUICK BITE";
    case EnemyAttackKind::Sweep: return "WIDE SWEEP";
    case EnemyAttackKind::Slam:
    default:                     return "HEAVY SLAM";
    }
}
} // namespace Combat
