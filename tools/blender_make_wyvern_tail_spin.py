"""Blender で実行し、Wyvern の尾回転(TailSpin)のモーションを作って FBX へ書き出す。

購入した Wyvern には「その場で半回転して尾で薙ぐ」モーションが無い。以前は待機の立ち姿勢を
プログラムで丸ごと回し、足だけ IK で動かしていたため、腰・背骨・尾が回る動きをしておらず
「回転台に乗せて回しているだけ」に見えた(審査会・実機での指摘)。そこで、骨ごとの動きを
このスクリプトで組み立て、ほかのクリップと同じ「骨だけの FBX」として書き出す。

動きの中身(回る向きを s とする。s=+1 は左回り):
  溜め   : 腰を落とし、体を回る向きへ開いた C の字に丸める(胸と首を回る向きへ曲げ、尾も同じ側へ巻き込む)。
           回る内側の後ろ足(軸足)を体の下へ踏み込み、その上へ体を寄せる。ほかの3本も踏み替えて構え直し、
           最後に首で回る先をはっきり見て「こっちへ回る」と見せる。溜め切って待つ間は尾の先を小刻みに振る。
  半回転 : 捻りを戻しながら前半身を起こし、軸足の指の付け根を中心に体ごと回る(軸足は地面に着いたまま足先だけ向きを変える)。
           前足と外側の後ろ足は浮かせて体と一緒に運び、回り終わりに下ろす。4本が同時に地面から離れることはない
           (以前は軽く跳んで4本とも浮かせていたが、跳びはねに見えた。実機での指摘)。
           尾は最初は取り残されて遅れ、後半で回る向きへしなって振り抜く(根元から先へ遅れて伝わる)。
           前足を下ろすときに腰を沈めて受け止める。
  隙     : 沈んだ腰を戻し、寄せていた体を元の位置へ戻しながら4本を踏み替え、振り抜いた尾が揺り戻してから落ち着く。
           最後は待機の最初のコマと同じ姿勢・同じ位置(モデルの原点の上)。
  一拍   : (半回転2回のとき)前足を下ろした受け止めから、もう一度捻り直して溜める。軸足は同じ足のまま。

軸足の上へ体を寄せておくのは、回る中心(軸足)とモデルの原点を合わせるため。原点の上で回るので、半回転の前後で
体の位置がずれず、ゲーム側は向きを半回転させるだけでよい(体の位置を動かさなくてよい)。

体の向き(半回転)もこのモーションの中に入っている。足は地面に着いている間は地面の同じ場所に
留めたまま(脚は2本の骨の IK で解く)なので、モーションだけを見ても足が床を滑らない。
ゲーム側は、このクリップを再生している間だけ「描画の向き」から回った分を引く(enemy::getRenderSRT)。
体を寄せる・回る中心をずらす・腰を沈めるのは、すべて根元の骨(pelvis)の位置のキーに入っている。ゲームは位置のキーも
キーの間で補間して読む(CAnimationMesh::BuildLocalPoseMap)。補間しないと腰だけが段々に動き、軸足が床の上で揺れる。

クリップの時間割(100 コマ/秒。キーの番号 = 秒 x 100):
  [  0,115) 溜め          1.15 秒  (Combat::Tuning::ENEMY_SPIN_ANTICIPATION)
  [115,157) 1回目の半回転  0.42 秒  (ENEMY_SPIN_HALF_TURN_SECONDS)
  [157,262) 半回転1回の隙  1.05 秒  (ENEMY_SPIN_RECOVERY)
  [262,322) 一拍           0.60 秒  (ENEMY_SPIN_PAUSE_SECONDS)  ← ここから下は半回転2回のとき
  [322,364) 2回目の半回転  0.42 秒
  [364,469] 半回転2回の隙  1.05 秒
  262 から先は 157 の姿勢(1回目の着地)の続きとして作ってある。

使い方(Blender 4.x / 5.x):
  blender --background --factory-startup --python tools/blender_make_wyvern_tail_spin.py -- [--preview <フォルダ>]
  assets/model/Wyvern/anim/Anim_Wyvern_TailSpin_L.fbx(左回り)と _R.fbx(右回り)を書き出す。
  --preview を付けると、メッシュを付けて真上と斜めから撮った確認用の画像も書き出す。

注意: 書き出した FBX は購入した有料アセットの骨格から作るので、公開リポジトリへコミットしない(.gitignore 済み)。
"""

import math
import os
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WYVERN = os.path.join(REPO, "assets", "model", "Wyvern")
BASE_CLIP = os.path.join(WYVERN, "anim", "Anim_Wyvern_Idle.fbx")

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
PREVIEW_DIR = None
ONLY = None
for i, a in enumerate(argv):
    if a == "--preview" and i + 1 < len(argv):
        PREVIEW_DIR = argv[i + 1]
    if a == "--only" and i + 1 < len(argv):
        ONLY = argv[i + 1]

# ---------------------------------------------------------------- 時間割
FPS = 100
WINDUP = 1.15
TURN = 0.42
PAUSE = 0.60
RECOVER = 1.05
# 半回転のうち、勢いよく回っている区間(Combat::Tuning::ENEMY_SPIN_HOP_START / END と同じ)。
# この前は捻りを戻して前半身を起こし、この後は前足を下ろして受け止める。
HOP_START = 0.16
HOP_END = 0.84
# 前足と外側の後ろ足を浮かせて運ぶ区間(半回転の中の割合)。軸足はずっと地面に着いている。
FRONT_LIFT = 0.10
FRONT_LAND = 0.86
OUTER_LIFT = 0.12
OUTER_LAND = 0.82
# 軸足を体の下へ踏み込む量。待機の構えの横の開きを、この割合まで狭める。
PIVOT_INWARD = 0.55
# 溜めで体を軸足の上へ寄せる区間(秒)と、隙で元へ戻す区間(着地からの秒)。
SHIFT_IN = (0.10, 0.70)
SHIFT_OUT = (0.15, 0.75)

F_TURN1 = round(WINDUP * FPS)                 # 115
F_RECOVER1 = F_TURN1 + round(TURN * FPS)      # 157
F_PAUSE = F_RECOVER1 + round(RECOVER * FPS)   # 262
F_TURN2 = F_PAUSE + round(PAUSE * FPS)        # 322
F_RECOVER2 = F_TURN2 + round(TURN * FPS)      # 364
F_LAST = F_RECOVER2 + round(RECOVER * FPS)    # 469

T_TURN1 = WINDUP
T_LAND1 = WINDUP + TURN
T_TURN2 = T_LAND1 + PAUSE
T_LAND2 = T_TURN2 + TURN


def frame_to_story(frame):
    """クリップのコマ番号 → (半回転の回数, その回数の筋書きの上での秒)。"""
    if frame < F_PAUSE:
        return 1, frame / FPS
    return 2, T_LAND1 + (frame - F_PAUSE) / FPS


# ---------------------------------------------------------------- 曲線
def clamp01(t):
    return max(0.0, min(1.0, t))


def smooth(t):
    t = clamp01(t)
    return t * t * (3.0 - 2.0 * t)


def lerp(a, b, t):
    return a + (b - a) * t


def ease_out(t):
    t = clamp01(t)
    return 1.0 - (1.0 - t) * (1.0 - t)


def settle_window(r, start, length):
    """r が start を過ぎたら length かけて 1→0。隙の終わりで値を確実に0へ落とす。"""
    return 1.0 - smooth((r - start) / length)


# ---------------------------------------------------------------- 筋書き(体)
class Phase:
    """筋書きの上で、いまどの段階か。"""

    def __init__(self, turns, t):
        self.turns = turns
        self.t = t
        self.kind = "windup"
        self.u = 0.0          # 段階の中の進み(0〜1)
        self.r = 0.0          # 段階に入ってからの秒
        self.done = 0         # 回り終えた半回転の数
        if t < T_TURN1:
            self.kind, self.r, self.u = "windup", t, t / WINDUP
        elif t < T_LAND1:
            self.kind, self.r, self.u = "turn", t - T_TURN1, (t - T_TURN1) / TURN
        elif turns == 1:
            self.kind, self.r, self.done = "recover", t - T_LAND1, 1
        elif t < T_TURN2:
            self.kind, self.r, self.u, self.done = "pause", t - T_LAND1, (t - T_LAND1) / PAUSE, 1
        elif t < T_LAND2:
            self.kind, self.r, self.u, self.done = "turn", t - T_TURN2, (t - T_TURN2) / TURN, 1
        else:
            self.kind, self.r, self.done = "recover", t - T_LAND2, 2


def coil_windup(u):
    """溜めの深さ(0〜1)。1.15秒の8割ほどで溜め切り、あとは溜めたまま待つ。"""
    return smooth(u / 0.80)


def look_windup(u):
    """溜めの終わりに首を回る向きへ向ける量(0〜1)。"""
    return smooth((u - 0.62) / 0.28)


def pause_blend(r):
    """一拍: 着地の受け止め(隙の始まりと同じ)から、溜め直しの姿勢へ移る割合。"""
    return smooth((r - 0.12) / 0.33)


# 溜め切った姿勢の値。半回転の始まりと一拍の終わりはこの値になる。
# 溜めは体を「回る向きへ開いた C の字」に丸める: 胸と首を回る向きへ曲げて肩越しに回る先を見て、
# 尾も同じ側へ巻き込む(尾は回り出すと外側へ振り出される)。最初は胸を回る向きと逆へ捻っていたが、
# 実機では首がその捻りに引っぱられて回る向きと逆を向いて見え、回る向きの手がかりにならなかった。
COIL = {
    "root_yaw": -0.06,      # 体ごと少しだけ逆へ回して溜める(後ろ足は地面に留めたまま)
    "root_z": -0.22,        # 腰を落とす
    "pitch_up": -0.04,      # 少し前へかがんで踏ん張る
    "chest_yaw": 0.12,      # 胸を回る向きへ曲げる
    "neck_yaw": 0.60,       # 首で回る先をはっきり見る
    "neck_down": 0.10,
    "jaw": 0.10,
    "tail_lift": 1.0,
}


def recover_values(r):
    """着地してからの隙(半回転1回・2回とも同じ)。r=0 で着地の直後、RECOVER で待機と同じ姿勢。"""
    v = {}
    v["root_yaw"] = 0.0
    if r < 0.07:
        v["root_z"] = lerp(-0.16, -0.24, smooth(r / 0.07))
    else:
        v["root_z"] = -0.24 * (1.0 - smooth((r - 0.07) / 0.58))
    v["pitch_up"] = -0.06 * (1.0 - smooth(r / 0.50))
    v["roll"] = 0.0
    # 胸は回り過ぎた分を戻し、少し揺り戻してから落ち着く。
    v["chest_yaw"] = 0.12 * math.exp(-r / 0.22) * math.cos(2.0 * math.pi * r / 0.70) * settle_window(r, 0.55, 0.40)
    v["neck_yaw"] = 0.18 * (1.0 - smooth(r / 0.60))
    # 着地で頭が一度沈み、持ち上げてから戻る。
    v["neck_down"] = 0.10 * math.sin(math.pi * clamp01(r / 0.30)) - 0.05 * math.sin(math.pi * clamp01((r - 0.30) / 0.40))
    v["jaw"] = 0.16 * math.sin(math.pi * clamp01(r / 0.60))
    v["tail_lift"] = 1.0 - smooth((r - 0.20) / 0.70)
    return v


def body_values(ph):
    """体(根元・胸・首・あご)の値。向きは s を掛ける前(回る向きを正)。"""
    v = {"root_yaw": 0.0, "root_z": 0.0, "pitch_up": 0.0, "roll": 0.0, "chest_yaw": 0.0,
         "neck_yaw": 0.0, "neck_down": 0.0, "jaw": 0.0, "tail_lift": 0.0}
    base_yaw = math.pi * ph.done
    if ph.kind == "windup":
        c = coil_windup(ph.u)
        look = look_windup(ph.u)
        v["root_yaw"] = COIL["root_yaw"] * c
        v["root_z"] = COIL["root_z"] * c
        v["pitch_up"] = COIL["pitch_up"] * c
        v["chest_yaw"] = COIL["chest_yaw"] * c
        v["neck_yaw"] = 0.15 * c + 0.45 * look
        v["neck_down"] = COIL["neck_down"] * c
        v["jaw"] = COIL["jaw"] * look
        v["tail_lift"] = smooth(ph.u / 0.6)
    elif ph.kind == "turn":
        s = ph.u
        push = smooth(s / HOP_START)
        air = clamp01((s - HOP_START) / (HOP_END - HOP_START))
        land = clamp01((s - HOP_END) / (1.0 - HOP_END))
        # 向き: 軸足で踏ん張りながら捻りを戻し、勢いよく半回転し、前足を下ろす時には回り切っている。
        if s < HOP_START:
            yaw = COIL["root_yaw"] * (1.0 - push)
        elif s < HOP_END:
            yaw = math.pi * smooth(air)
        else:
            yaw = math.pi
        v["root_yaw"] = base_yaw + yaw
        # 高さ: 沈んだ腰を伸ばして(跳ばずに)前半身を持ち上げ、前足を下ろすときに沈んで受け止める。
        if s < HOP_START:
            v["root_z"] = lerp(COIL["root_z"], 0.02, push)
        elif s < HOP_END:
            v["root_z"] = 0.02 + 0.05 * math.sin(math.pi * air)
        else:
            v["root_z"] = lerp(0.02, -0.16, math.sin(0.5 * math.pi * land))
        # 前半身を起こして(軸足に体重を乗せて)回り、前足から下ろす。
        if s < HOP_START:
            v["pitch_up"] = lerp(COIL["pitch_up"], 0.06, push)
        elif s < HOP_END:
            v["pitch_up"] = lerp(0.06, -0.03, air) + 0.10 * math.sin(math.pi * air)
        else:
            v["pitch_up"] = -0.03 - 0.03 * math.sin(0.5 * math.pi * land)
        v["roll"] = 0.06 * math.sin(math.pi * air)  # 回る内側(軸足の側)へ少し傾ける
        # 胸: 捻りを戻し、回る向きへ先に出て、回り終わりで勢い余って行き過ぎる。
        v["chest_yaw"] = (COIL["chest_yaw"] * (1.0 - push) + 0.16 * math.sin(math.pi * s) +
                          0.12 * smooth((s - 0.6) / 0.4))
        # 首: 回る向きを見たまま先に回り、回り終わりで戻り始める(着地で隙の始まりの値 0.18 へつなぐ)。
        v["neck_yaw"] = lerp(COIL["neck_yaw"], 0.18, smooth(s)) + 0.17 * math.sin(math.pi * s)
        v["neck_down"] = COIL["neck_down"] * (1.0 - push) - 0.10 * math.sin(math.pi * air)
        v["jaw"] = COIL["jaw"] + 0.15 * math.sin(math.pi * s) - 0.10 * smooth(s)
        v["tail_lift"] = 1.0
    elif ph.kind == "recover":
        v.update(recover_values(ph.r))
        v["root_yaw"] += base_yaw
    else:  # pause
        rec = recover_values(ph.r)
        w = pause_blend(ph.r)
        for key in v:
            target = COIL.get(key, 0.0)
            v[key] = lerp(rec.get(key, 0.0), target, w)
        v["root_yaw"] += base_yaw
    v["shift"] = shift_weight(ph)
    return v


def shift_weight(ph):
    """体を軸足の上へ寄せている割合(0〜1)。溜めで寄せ、回っている間と一拍は寄せたまま、隙で戻す。"""
    if ph.kind == "windup":
        return smooth((ph.r - SHIFT_IN[0]) / (SHIFT_IN[1] - SHIFT_IN[0]))
    if ph.kind == "recover":
        return 1.0 - smooth((ph.r - SHIFT_OUT[0]) / (SHIFT_OUT[1] - SHIFT_OUT[0]))
    return 1.0


# 尾の左右(回る向きを正、溜めで -1)。根元から先へ、少しずつ遅れて伝わる。
TAIL_BONES = ["tail", "tail_001", "tail_002", "tail_003", "tail_004",
              "tail_005", "tail_006", "tail_007", "tail_008", "tail_009"]
TAIL_WEIGHTS = [0.04, 0.06, 0.08, 0.09, 0.10, 0.10, 0.10, 0.10, 0.10, 0.10]  # 1あたりの角度(合計 0.87rad)
TAIL_LIFT = [0.05, 0.04, 0.03, 0.02, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]          # 振るときに尾を持ち上げる量
TAIL_DELAY = 0.022
TAIL_OVERSHOOT = 0.9


def tension(turns, t):
    """溜め切って待っている間の張り詰め(0〜1)。尾の先を小刻みに振る(飛び掛かる前の猫の尾のように)。"""
    ph = Phase(turns, t)
    if ph.kind == "windup":
        return smooth((ph.u - 0.30) / 0.40)
    if ph.kind == "pause":
        return pause_blend(ph.r)
    if ph.kind == "turn":
        return 1.0 - smooth(ph.u / HOP_START)
    return 0.0


TAIL_LASH = 0.07      # 尾の先を振る大きさ(rad/骨)
TAIL_LASH_HZ = 2.4


def tail_swing(turns, t):
    """尾の振れ(回る向きを正)。"""
    if t <= 0.0:
        return 0.0
    ph = Phase(turns, t)
    if ph.kind == "windup":
        return -smooth(ph.u / 0.70)
    if ph.kind == "turn":
        s = ph.u
        # 体が回り始めても尾はその場に残ろうとして、遅れが一度大きくなる。後半で回る向きへしなって振り抜く。
        return (-1.0 - 0.25 * math.sin(math.pi * clamp01(s / 0.4)) +
                (1.0 + TAIL_OVERSHOOT) * smooth((s - 0.30) / 0.70))
    rec = TAIL_OVERSHOOT * math.exp(-ph.r / 0.30) * math.cos(2.0 * math.pi * ph.r / 0.62) * settle_window(ph.r, 0.75, 0.30)
    if ph.kind == "recover":
        return rec
    return lerp(rec, -1.0, smooth((ph.r - 0.10) / 0.35))


# ---------------------------------------------------------------- 骨格
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
bpy.ops.import_scene.fbx(filepath=BASE_CLIP)
arm = [o for o in bpy.data.objects if o.type == 'ARMATURE'][0]
scene.frame_set(1)
base_world = arm.matrix_world.copy()
base_basis = {pb.name: pb.matrix_basis.copy() for pb in arm.pose.bones}
# 待機のクリップを外し、体の置き場所(wyvern_Rig)は最初のコマの値で止める。
arm.animation_data.action = None
arm.matrix_world = base_world
bpy.context.view_layer.update()

bones = arm.data.bones
REST = {b.name: b.matrix_local.copy() for b in bones}
PARENT = {b.name: (b.parent.name if b.parent else None) for b in bones}
CHILDREN = {b.name: [c.name for c in b.children] for b in bones}
ORDER = []


def _walk(name):
    ORDER.append(name)
    for c in CHILDREN[name]:
        _walk(c)


for b in bones:
    if b.parent is None:
        _walk(b.name)

# アーマチュアの空間での上向き・回る中心(ゲームのモデルの原点)。
WORLD_TO_ARM = base_world.inverted()
UP = (WORLD_TO_ARM.to_3x3() @ Vector((0.0, 0.0, 1.0))).normalized()
PIVOT = WORLD_TO_ARM @ Vector((0.0, 0.0, 0.0))
SIDE = Vector((1.0, 0.0, 0.0))   # 体の左(+X)
FORWARD_AXIS = Vector((0.0, 1.0, 0.0))


def local_frame(name, M):
    """親の今の姿勢から見た、この骨の休止の置き場所(M[name] = local_frame @ basis)。"""
    p = PARENT[name]
    if p is None:
        return REST[name].copy()
    return M[p] @ REST[p].inverted() @ REST[name]


def fk(basis, root_xform):
    M = {}
    for name in ORDER:
        if PARENT[name] is None:
            M[name] = root_xform @ REST[name] @ basis[name]
        else:
            M[name] = local_frame(name, M) @ basis[name]
    return M


def refresh_subtree(name, M, basis):
    for c in CHILDREN[name]:
        M[c] = local_frame(c, M) @ basis[c]
        refresh_subtree(c, M, basis)


def rotate_about(M, name, axis, angle, pivot=None):
    if abs(angle) < 1e-7:
        return
    h = M[name].translation.copy() if pivot is None else pivot
    R = Matrix.Rotation(angle, 4, axis.normalized())
    M[name] = Matrix.Translation(h) @ R @ Matrix.Translation(-h) @ M[name]


def carried_axis(M, name, axis):
    """親が休止の姿勢から回った分だけ回した軸。背骨や尾が曲がっていても、その骨から見た上・横になる。"""
    p = PARENT[name]
    if p is None:
        return axis.copy()
    D = M[p].to_3x3() @ REST[p].to_3x3().inverted()
    return (D @ axis).normalized()


def root_transform(v, sign):
    hip = (REST["pelvis"] @ base_basis["pelvis"]).translation
    tilt = (Matrix.Translation(hip) @ Matrix.Rotation(-v["pitch_up"], 4, SIDE) @
            Matrix.Rotation(sign * v["roll"], 4, FORWARD_AXIS) @ Matrix.Translation(-hip))
    turn = Matrix.Translation(PIVOT) @ Matrix.Rotation(sign * v["root_yaw"], 4, UP) @ Matrix.Translation(-PIVOT)
    # 体を軸足の上へ寄せる: 軸足の指の付け根(体から見て pivot_offset の所)が、モデルの原点の真上に来るように動かす。
    # 回っている間もこの関係を保つので、体は軸足を中心に回る(軸足は地面の同じ所に留まる)。
    shift = -(Matrix.Rotation(sign * v["root_yaw"], 3, UP) @ pivot_offset(sign)) * v.get("shift", 0.0)
    return Matrix.Translation(UP * v["root_z"] + shift) @ turn @ tilt


CHEST = [("spine2", 0.25), ("spine3", 0.35), ("spine4", 0.40)]
NECK = [("neck1", 0.30), ("neck2", 0.35), ("Head", 0.35)]


def pose_body(turns, t, sign):
    """脚を除いた体の姿勢(アーマチュアの空間の行列)。"""
    ph = Phase(turns, t)
    v = body_values(ph)
    root = root_transform(v, sign)
    M = fk(base_basis, root)
    # 骨を根元から順に回す(子は親に付いてくるよう、回すたびに下を計算し直す)。
    for name, w in CHEST:
        rotate_about(M, name, carried_axis(M, name, UP), sign * v["chest_yaw"] * w)
        refresh_subtree(name, M, base_basis)
    for name, w in NECK:
        rotate_about(M, name, carried_axis(M, name, UP), sign * v["neck_yaw"] * w)
        rotate_about(M, name, carried_axis(M, name, SIDE), v["neck_down"] * w)
        refresh_subtree(name, M, base_basis)
    rotate_about(M, "Jaw", carried_axis(M, "Jaw", SIDE), v["jaw"])
    refresh_subtree("Jaw", M, base_basis)
    lash = tension(turns, t)
    for k, name in enumerate(TAIL_BONES):
        swing = tail_swing(turns, t - k * TAIL_DELAY)
        # 尾の先(後ろ半分)だけ、根元から先へ伝わる小さな波で振る。
        tip = TAIL_LASH * lash * clamp01((k - 4) / 3.0) * math.sin(2.0 * math.pi * TAIL_LASH_HZ * t - 0.7 * k)
        rotate_about(M, name, carried_axis(M, name, UP), sign * swing * TAIL_WEIGHTS[k] + tip)
        rotate_about(M, name, carried_axis(M, name, SIDE), TAIL_LIFT[k] * v["tail_lift"])
        refresh_subtree(name, M, base_basis)
    return M, v


# ---------------------------------------------------------------- 足
LEGS = {
    # 名前: (付け根, 上の骨, 下の骨, 足首の骨, 体のどこに付いているか, 前足か)
    "FL": ("clavicle_L", "front_UpperLeg_L", "front_Shin_L", "front_Foot_L", "spine4", True),
    "FR": ("clavicle_R", "front_UpperLeg_R", "front_Shin_R", "front_Foot_R", "spine4", True),
    "BL": ("hip_L", "back_UpperLeg_L", "back_Shin_L", "back_Foot_L", "pelvis", False),
    "BR": ("hip_R", "back_UpperLeg_R", "back_Shin_R", "back_Foot_R", "pelvis", False),
}

M0 = fk(base_basis, Matrix.Identity(4))

# 足の指の付け根(地面に着く所)。軸足はここを中心に向きを変える。
BALL = {"FL": "front_toe_Middle1_L", "FR": "front_toe_Middle1_R",
        "BL": "back_toe_Middle1_L", "BR": "back_toe_Middle1_R"}
# 足首から見た指の付け根の位置(足首の向きで見た値)。
BALL_LOCAL = {leg: M0[LEGS[leg][3]].to_3x3().inverted() @ (M0[BALL[leg]].translation - M0[LEGS[leg][3]].translation)
              for leg in LEGS}


def horizontal(v):
    return v - UP * v.dot(UP)


def pivot_leg(sign):
    """軸足: 回る内側の後ろ足(左回りなら左)。"""
    return "BL" if sign > 0 else "BR"


def outer_leg(sign):
    return "BR" if sign > 0 else "BL"


def pivot_offset(sign):
    """溜めで踏み込んだ後の軸足の指の付け根の位置(モデルの原点から見た水平の向き。体の向きで見た値)。"""
    v = horizontal(M0[BALL[pivot_leg(sign)]].translation - PIVOT)
    lateral = v.dot(SIDE)
    return v - SIDE * lateral * (1.0 - PIVOT_INWARD)


def ball_of(leg, pos, rot):
    return pos + rot.to_matrix() @ BALL_LOCAL[leg]


def yaw_delta(rot_from, rot_to):
    """rot_from から rot_to へ回った分のうち、上向きの軸まわりの角度(左回りが正)。"""
    f = (rot_to @ rot_from.inverted()) @ Vector((0.0, -1.0, 0.0))
    f = f - UP * f.dot(UP)
    return math.atan2(f.x, -f.y) if f.length > 1e-6 else 0.0


def natural_foot(leg, M):
    """体のいまの向き・位置に合わせて、待機の構えのまま足を下ろしたときの足首(地面の上)と向き。"""
    girdle = LEGS[leg][4]
    ankle = LEGS[leg][3]
    g0 = M0[girdle]
    g1 = M[girdle]
    dyaw = yaw_delta(g0.to_3x3(), g1.to_3x3())
    R = Matrix.Rotation(dyaw, 4, UP)
    off = M0[ankle].translation - g0.translation
    off = R @ off
    pos = g1.translation + off
    # 高さは待機のときの足首の高さ(地面に着いている)。
    h0 = M0[ankle].translation.dot(UP)
    pos = pos - UP * (pos.dot(UP) - h0)
    rot = (R.to_3x3() @ M0[ankle].to_3x3()).to_quaternion()
    return pos, rot


def is_front(leg):
    return LEGS[leg][5]


def foot_events(turns, sign):
    """足を動かす区間(筋書きの秒)。 (始め, 終わり, 'step'|'hop'|'pivot', 下ろす所の決め方)

    step : 地面の上で弧を描いて踏み替える。
    hop  : 浮かせたまま体と一緒に運ぶ(体から見た位置で動かし、脚をたたむ)。
    pivot: 指の付け根を地面の同じ所に留めたまま、体が回った分だけ足の向きを変える(軸足)。
    下ろす所は None なら下ろす時の体の姿勢で待機の構えのまま置く所。
    {"eval": 秒} ならその秒の体の姿勢で決める(体を動かし終える時に合う所へ先に置く)。
    {"pivot_spot": True} なら指の付け根をモデルの原点の真上へ置く(軸足の踏み込み)。
    """
    lead = "FL" if sign > 0 else "FR"     # 回る向きの側の前足
    trail = "FR" if sign > 0 else "FL"
    pivot = pivot_leg(sign)
    outer = outer_leg(sign)
    ev = {k: [] for k in LEGS}
    # 溜め: 軸足を体の下へ踏み込み、体をその上へ寄せながら、ほかの3本も1本ずつ踏み替えて構え直す。
    settle_in = SHIFT_IN[1] + 0.02
    # 1本ずつ順に踏む(同時に浮かせるのは2本まで。3本浮くと1本足で立っているように見える)。
    ev[pivot].append((0.10, 0.30, "step", {"eval": settle_in, "pivot_spot": True}))
    ev[outer].append((0.28, 0.46, "step", {"eval": settle_in}))
    ev[lead].append((0.44, 0.62, "step", {"eval": settle_in}))
    ev[trail].append((0.60, 0.78, "step", {"eval": settle_in}))
    starts = [T_TURN1] if turns == 1 else [T_TURN1, T_TURN2]
    for i, start in enumerate(starts):
        if i == 1:
            # 一拍: 溜め直しで前足をもう一度踏み替える(軸足はそのまま)。
            ev[lead].append((T_LAND1 + 0.16, T_LAND1 + 0.31, "step", None))
            ev[trail].append((T_LAND1 + 0.27, T_LAND1 + 0.42, "step", None))
        # 半回転: 軸足は地面に着けたまま向きだけ変え、ほかの3本は浮かせて体と一緒に運ぶ。
        ev[pivot].append((start, start + TURN, "pivot", None))
        ev[outer].append((start + OUTER_LIFT * TURN, start + OUTER_LAND * TURN, "hop", None))
        for leg in (lead, trail):
            ev[leg].append((start + FRONT_LIFT * TURN, start + FRONT_LAND * TURN, "hop", None))
    # 隙: 寄せていた体を戻しながら、4本を1本ずつ待機の構えの所(隙の終わりの姿勢で決める)へ踏み替える。
    land = T_LAND1 if turns == 1 else T_LAND2
    settle_out = land + RECOVER
    ev[lead].append((land + 0.22, land + 0.42, "step", {"eval": settle_out}))
    ev[trail].append((land + 0.36, land + 0.56, "step", {"eval": settle_out}))
    ev[pivot].append((land + 0.46, land + 0.68, "step", {"eval": settle_out}))
    ev[outer].append((land + 0.60, land + 0.82, "step", {"eval": settle_out}))
    return ev


_body_cache = {}


def body_at(turns, t, sign):
    key = (turns, round(t * 10000), sign)
    if key not in _body_cache:
        _body_cache[key] = pose_body(turns, t, sign)
    return _body_cache[key]


def root_at(turns, t, sign):
    _, v = body_at(turns, t, sign)
    return root_transform(v, sign)


STEP_HEIGHT = {True: 0.30, False: 0.25}
TUCK_HEIGHT = {True: 0.45, False: 0.35}


def landing(leg, turns, sign, b, land):
    """足を下ろす所(足首の位置と向き)。"""
    t_eval = b if land is None else max(b, land.get("eval", b))
    pos, rot = natural_foot(leg, body_at(turns, t_eval, sign)[0])
    if land is not None and land.get("pivot_spot"):
        pos = pos + horizontal(PIVOT - ball_of(leg, pos, rot))
    return pos, rot


def pivot_pose(leg, turns, sign, a, t, p_lift, q_lift):
    """軸足: 指の付け根を中心に、体が a から t までに回った分だけ足を回す。"""
    _, va = body_at(turns, a, sign)
    _, vt = body_at(turns, t, sign)
    R = Matrix.Rotation(sign * (vt["root_yaw"] - va["root_yaw"]), 3, UP)
    ball = ball_of(leg, p_lift, q_lift)
    pos = ball + R @ (p_lift - ball)
    rot = (R @ q_lift.to_matrix()).to_quaternion()
    return pos, rot


def foot_target(leg, turns, t, sign, events):
    """足首を置く場所と向き(アーマチュアの空間)。"""
    M, _ = body_at(turns, t, sign)
    pos, rot = natural_foot(leg, body_at(turns, 0.0, sign)[0])
    for (a, b, mode, land) in events[leg]:
        if t < a:
            break
        p_lift, q_lift = pos, rot
        if mode == "pivot":
            pos, rot = pivot_pose(leg, turns, sign, a, min(t, b), p_lift, q_lift)
            if t >= b:
                continue
            return pos, rot
        p_land, q_land = landing(leg, turns, sign, b, land)
        if t >= b:
            pos, rot = p_land, q_land
            continue
        s = (t - a) / (b - a)
        if mode == "step":
            k = smooth(s)
            pos = p_lift.lerp(p_land, k) + UP * (STEP_HEIGHT[is_front(leg)] * math.sin(math.pi * s))
            rot = q_lift.slerp(q_land, k)
        else:
            # 空中は体と一緒に回る。体から見た位置で上げた所から下ろす所へ運び、脚をたたむ。
            ra, rb, rt = root_at(turns, a, sign), root_at(turns, b, sign), root_at(turns, t, sign)
            bl = ra.inverted() @ p_lift
            bb = rb.inverted() @ p_land
            k = smooth(s)
            body_pos = bl.lerp(bb, k) + UP * (TUCK_HEIGHT[is_front(leg)] * math.sin(math.pi * s) ** 0.8)
            pos = rt @ body_pos
            qa = ra.to_quaternion().inverted() @ q_lift
            qb = rb.to_quaternion().inverted() @ q_land
            rot = rt.to_quaternion() @ qa.slerp(qb, k)
        return pos, rot
    return pos, rot


def solve_leg(M, leg, target, rot):
    _, upper, lower, ankle, _, _ = LEGS[leg]
    A = M[upper].translation.copy()
    B = M[lower].translation.copy()
    C = M[ankle].translation.copy()
    l1 = (M0[lower].translation - M0[upper].translation).length
    l2 = (M0[ankle].translation - M0[lower].translation).length
    to = target - A
    d = max(abs(l1 - l2) + 1e-4, min(to.length, (l1 + l2) * 0.999))
    direction = to.normalized()
    pole = (B - A) - direction * (B - A).dot(direction)
    if pole.length < 1e-6:
        pole = UP.cross(direction)
    pole.normalize()
    cos_a = (l1 * l1 + d * d - l2 * l2) / (2.0 * l1 * d)
    sin_a = math.sqrt(max(0.0, 1.0 - cos_a * cos_a))
    knee = A + direction * (l1 * cos_a) + pole * (l1 * sin_a)
    reach = A + direction * d
    q1 = (B - A).rotation_difference(knee - A)
    M[upper] = Matrix.Translation(A) @ q1.to_matrix().to_4x4() @ Matrix.Translation(-A) @ M[upper]
    refresh_subtree(upper, M, base_basis)
    C = M[ankle].translation.copy()
    q2 = (C - knee).rotation_difference(reach - knee)
    M[lower] = Matrix.Translation(knee) @ q2.to_matrix().to_4x4() @ Matrix.Translation(-knee) @ M[lower]
    refresh_subtree(lower, M, base_basis)
    M[ankle] = Matrix.Translation(reach) @ rot.to_matrix().to_4x4()
    refresh_subtree(ankle, M, base_basis)
    return (target - reach).length


def to_basis(M):
    out = {}
    for name in ORDER:
        out[name] = local_frame(name, M).inverted() @ M[name]
    return out


# ---------------------------------------------------------------- 書き出し
def build(sign):
    _body_cache.clear()
    events = {1: foot_events(1, sign), 2: foot_events(2, sign)}
    frames = []
    worst = 0.0
    # 確かめ: 地面に着いている指の付け根が、コマの間にどれだけ横へ動いたか(m/秒。足が滑っていないか)と、
    # 地面に着いている足の数(4本とも浮くコマが無いか)。
    ball_ground = {leg: M0[name].translation.dot(UP) for leg, name in BALL.items()}
    previous_ball = {}
    slide = {leg: 0.0 for leg in BALL}
    fewest_grounded = (5, -1)
    fewest_still = (5, -1)   # 半回転の外(溜め・一拍・隙)
    highest_lowest = (-1.0, -1)
    for f in range(0, F_LAST + 1):
        turns, t = frame_to_story(f)
        M, _ = body_at(turns, t, sign)
        M = {k: m.copy() for k, m in M.items()}
        for leg in LEGS:
            target, rot = foot_target(leg, turns, t, sign, events[turns])
            worst = max(worst, solve_leg(M, leg, target, rot))
        grounded = 0
        lowest = 1e9
        for leg, name in BALL.items():
            p = M[name].translation.copy()
            above = p.dot(UP) - ball_ground[leg]
            lowest = min(lowest, above)
            on_ground = above < 0.03
            grounded += 1 if on_ground else 0
            q = previous_ball.get(leg)
            # 一拍の始まり(262)は別の筋書きの続きなので、そこをまたぐ差は見ない。
            if q is not None and f != F_PAUSE and on_ground and q.dot(UP) - ball_ground[leg] < 0.03:
                d = horizontal(p - q)
                slide[leg] = max(slide[leg], d.length * FPS)
            previous_ball[leg] = p
        fewest_grounded = min(fewest_grounded, (grounded, f))
        if Phase(turns, t).kind != "turn":
            fewest_still = min(fewest_still, (grounded, f))
        highest_lowest = max(highest_lowest, (lowest, f))
        frames.append(to_basis(M))
    print("tail spin sign", sign, "frames", len(frames), "worst IK miss", round(worst, 4),
          "max ball slide on ground (m/s)", {k: round(v, 3) for k, v in slide.items()},
          "fewest feet on ground (count, frame)", fewest_grounded, "outside half turns", fewest_still,
          "highest lowest ball (m, frame)", (round(highest_lowest[0], 3), highest_lowest[1]))
    return frames


def apply_action(frames, name):
    action = bpy.data.actions.new(name)
    arm.animation_data_create()
    arm.animation_data.action = action
    previous = {}
    for pb in arm.pose.bones:
        pb.rotation_mode = 'QUATERNION'
    for f, basis in enumerate(frames):
        for pb in arm.pose.bones:
            m = basis[pb.name]
            loc, q, scl = m.decompose()
            prev = previous.get(pb.name)
            if prev is not None and prev.dot(q) < 0.0:
                q.negate()
            previous[pb.name] = q
            pb.location = loc
            pb.rotation_quaternion = q
            pb.scale = scl
            pb.keyframe_insert("location", frame=f)
            pb.keyframe_insert("rotation_quaternion", frame=f)
            pb.keyframe_insert("scale", frame=f)
    return action


def export(path):
    scene.render.fps = FPS
    scene.render.fps_base = 1.0
    scene.frame_start = 0
    scene.frame_end = F_LAST
    for o in bpy.data.objects:
        o.select_set(o == arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={'ARMATURE'}, add_leaf_bones=False,
        axis_forward='Y', axis_up='Z',
        bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
        bake_anim_step=1.0, bake_anim_simplify_factor=0.0, bake_anim_force_startend_keying=True)
    print("EXPORTED", path)


def render_preview(out_dir, tag):
    os.makedirs(out_dir, exist_ok=True)
    meshes_before = set(bpy.data.objects)
    if not any(o.type == 'MESH' for o in bpy.data.objects):
        bpy.ops.import_scene.fbx(filepath=os.path.join(WYVERN, "SK_Wyvern.FBX"))
        new_objects = set(bpy.data.objects) - meshes_before
        sk_arm = [o for o in new_objects if o.type == 'ARMATURE'][0]
        # メッシュとモデルの骨格の関係を、そのままこのアニメーションの骨格へ移す(大きさの単位が違うため)。
        swap = arm.matrix_world @ sk_arm.matrix_world.inverted()
        for o in new_objects:
            if o.type == 'MESH':
                world = o.matrix_world.copy()
                o.parent = None
                o.matrix_world = swap @ world
                for mod in o.modifiers:
                    if mod.type == 'ARMATURE':
                        mod.object = arm
        sk_arm.hide_render = True
        sk_arm.hide_viewport = True
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.color_type = 'SINGLE'
    scene.render.resolution_x = 480
    scene.render.resolution_y = 360
    scene.render.film_transparent = False
    if "PreviewCam" not in bpy.data.objects:
        bpy.ops.mesh.primitive_plane_add(size=40.0, location=(0.0, 0.0, 0.0))
        bpy.ops.object.camera_add()
        cam = bpy.context.object
        cam.name = "PreviewCam"
        scene.camera = cam
    cam = bpy.data.objects["PreviewCam"]
    views = {
        "top": ((0.0, -1.0, 26.0), (0.0, -1.0, 0.0)),
        "side": ((-15.0, -14.0, 7.0), (0.0, -1.5, 1.5)),
    }
    for view, (loc, look) in views.items():
        cam.location = loc
        direction = Vector(look) - Vector(loc)
        cam.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
        for f in range(0, F_LAST + 1, 5):
            scene.frame_set(f)
            scene.render.filepath = os.path.join(out_dir, f"{tag}_{view}_{f:03d}.png")
            bpy.ops.render.render(write_still=True)


for sign, suffix in ((1.0, "L"), (-1.0, "R")):
    if ONLY and ONLY != suffix:
        continue
    frames = build(sign)
    apply_action(frames, f"TailSpin_{suffix}")
    export(os.path.join(WYVERN, "anim", f"Anim_Wyvern_TailSpin_{suffix}.fbx"))
    if PREVIEW_DIR:
        render_preview(PREVIEW_DIR, suffix)
