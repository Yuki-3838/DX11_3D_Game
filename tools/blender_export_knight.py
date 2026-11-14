"""Blender で実行し、購入した騎士のモデル(knight_rigged_3.6.blend)を FBX へ書き出す。

販売元の knight.fbx には待機(idle2)しか動きが入っておらず、他のモーション
(歩き・走り・攻撃・回避・被弾など)は .blend の中のアクションにしか無い。
このゲームは Assimp で「モデル本体」と「1クリップ1ファイル」を別々に読むので、その形で書き出す。

使い方(Blender 4.x / 5.x):
  blender --background --disable-autoexec --factory-startup "<knight_rigged_3.6.blend>" ^
          --python tools/blender_export_knight.py -- ["<出力先フォルダ>"] [--textures "<Knight.zip を展開したフォルダ>"]
  出力先を省略するとリポジトリの assets/model/Knight に書き出す。
  --textures を付けると、ゲーム用のテクスチャ(マテリアル名.png)も作る(make_game_textures)。

書き出すもの:
  - Knight.fbx         : メッシュと骨。休止姿勢(rest)で書き出す。剣は右手の骨へスキンした部品として含める。
  - anim/Knight_<名前>.fbx : アクションごとに骨だけ。リグの制御用の骨(IK など)の動きを
                          変形用の骨へベイクする(use_armature_deform_only)。
  - anim/Knight_death.fbx  : 騎士には力尽きるモーションが無いので、リポジトリの Mixamo のクリップ
                          (assets/motion/sword and shield death.fbx)を騎士の骨格へ移し替えて作る(RETARGETS)。

注意: 書き出した FBX は購入した有料アセットなので、公開リポジトリへコミットしない(.gitignore 済み)。
"""

import math
import os
import re
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

ARMATURE_NAME = "Armature"
SWORD_NAME = "bastard sword.002"
# 剣を持つ手首の骨(剣の骨 handr.002 の親)。移し替えたクリップで剣の向きを決めるのに使う。
WEAPON_WRIST = "wristr"

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
TEXTURE_DIR = None
positional = []
i = 0
while i < len(argv):
    if argv[i] == "--textures" and i + 1 < len(argv):
        TEXTURE_DIR = os.path.abspath(argv[i + 1])
        i += 2
        continue
    if argv[i] in ("--preview", "--only") and i + 1 < len(argv):
        i += 2  # 下で読む(確認画像の置き場 / 書き出す動きの絞り込み)
        continue
    positional.append(argv[i])
    i += 1
if positional:
    OUTPUT_DIR = os.path.abspath(positional[0])
else:
    OUTPUT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "assets", "model", "Knight"))
ANIM_DIR = os.path.join(OUTPUT_DIR, "anim")
os.makedirs(ANIM_DIR, exist_ok=True)
REPO_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

# Mixamo のクリップを騎士の骨格へ移し替えて、騎士のアクションとして足す(名前 → クリップのファイル)。
_MOTION_DIR = os.path.join(REPO_DIR, "assets", "motion")
RETARGETS = {
    "death": os.path.join(_MOTION_DIR, "sword and shield death.fbx"),
    # 前のモデルの技(ダッシュ攻撃と強攻撃1〜3)。騎士の元のクリップは攻撃が3本で、どれもその場で振るだけなので、
    # 踏み込み・回転のある前のモデルの技を移して使う(CombatAttackTable.h の DefaultKnightAttack)。
    "mx_attack": os.path.join(_MOTION_DIR, "sword and shield attack.fbx"),        # ダッシュ攻撃(跳んで斬り下ろす)
    "mx_slash": os.path.join(_MOTION_DIR, "sword and shield slash.fbx"),          # 強1(振りかぶって踏み込む振り下ろし)
    "mx_attack_3": os.path.join(_MOTION_DIR, "sword and shield attack (3).fbx"),  # 強2(前進しながらの回転斬り)
    "mx_slash_4": os.path.join(_MOTION_DIR, "sword and shield slash (4).fbx"),    # 強3(沈み込んで振り抜く回転斬り)
    # 前のモデルの歩き・走り・横歩き(CharacterModelProfile.h の MakeKnightPlayerProfile)。
    # 騎士の元の歩きは腰を落としたすり足(約1.1m/秒)、走りは剣を前へ構えた小走り(約1.3m/秒)で、
    # ゲームの移動の速さ(歩き約2.4m/秒・ダッシュ約5.5m/秒)に合わせると歩きが約2.2倍速、
    # 走りが上限の4倍速で回り、足がばたついて見えた。前のモデルの移動はこの速さに合わせて選んだもの。
    "mx_walk": os.path.join(_MOTION_DIR, "sword and shield walk.fbx"),            # 前へ歩く
    "mx_run": os.path.join(_MOTION_DIR, "sword and shield run.fbx"),              # 前へ走る
    "mx_walk_2": os.path.join(_MOTION_DIR, "sword and shield walk (2).fbx"),      # 後ろへ歩く
    "mx_run_2": os.path.join(_MOTION_DIR, "sword and shield run (2).fbx"),        # 後ろへ走る
    "mx_strafe": os.path.join(_MOTION_DIR, "sword and shield strafe.fbx"),        # 右へ歩く
    "mx_strafe_2": os.path.join(_MOTION_DIR, "sword and shield strafe (2).fbx"),  # 左へ歩く
    "mx_strafe_3": os.path.join(_MOTION_DIR, "sword and shield strafe (3).fbx"),  # 左へ走る
    "mx_strafe_4": os.path.join(_MOTION_DIR, "sword and shield strafe (4).fbx"),  # 右へ走る
    # 前のモデルの待機。騎士の battle_idle は腰を深く落とした構え(腰の高さが脚の長さの0.68倍・膝126度)で、
    # 移し替えた歩き・走りと並べるとへっぴり腰に見えたので、こちらを使う。
    # idle (4) は構えたまま呼吸するだけの待機。idle・idle (2)・idle (3) と騎士の idle / idle2 は
    # 首を50〜180度振って周りを見回す動きが入っていて、戦闘中の待機に合わなかった。
    "mx_idle": os.path.join(_MOTION_DIR, "sword and shield idle (4).fbx"),
    # 回避の前転。騎士にも Sword and Shield Pack にも前転が無いので、Unity の無料アセット
    # 「RPG Character Mecanim Animation Pack FREE」(ExplosiveLLC)の Unarmed-DiveRoll-Forward1 を使う。
    # 骨が Empty の階層で入っているので、tools/blender_rpg_to_mixamo.py で Mixamo の骨の名前のアーマチュアへ
    # 焼き直したもの(その場で回る形)をここへ置く。配布元のあるアセットなのでリポジトリには入れない。
    "roll": os.path.join(REPO_DIR, "assets", "model", "Knight", "source", "rpg_dive_roll_mx.fbx"),
}
# 元のクリップの使う区間(先頭からの秒数)。前転は最初の約0.1秒が構えたままの溜め、1.13秒より後は
# 立ち上がった後の揺れなので切る(ゲームでは押した瞬間に跳び出し、移動の0.5秒に近い長さで回り切りたい)。
RETARGET_WINDOWS = {"roll": (0.10, 1.13)}
# 腰が休止姿勢より上がる分に掛ける倍率。元の前転は腰が身長の約2割も上がる大きな跳び込みで、
# 重い鎧の騎士だと宙に浮いて見えたので、少し低い跳び込みにする(下がる分はそのまま)。
# 0.4 まで下げると、着地の手前で剣が床へ0.23(騎士の身長2.16の1割)刺さった。0.6 で0.15
# (転がる間に頭・背中が床へ入る0.11と同じくらい。ゲームでは床へ入った分だけ体が持ち上がる)。
RETARGET_HIP_RISE_SCALE = {"roll": 0.6}
# --only 名前,名前 で、移し替えと書き出しをその動きだけにする(確認を速く回すため)。
ONLY = None
if "--only" in argv:
    ONLY = set(argv[argv.index("--only") + 1].split(","))
# 移し替えで休止姿勢の向きをそろえない骨(胴と頭)。Mixamo の回転の変化を騎士の休止姿勢へそのまま重ねる。
NO_ALIGN_BONES = {"pelvis", "spine", "spine1", "CHEST", "neck", "head"}
# 上体を起こすクリップと、起こす割合。Sword and Shield Pack の移動・待機は盾を構えて前かがみになる作りで
# (上体の前傾が歩き約10度・走り約34度)、騎士に当てると「へっぴり腰」に見えた。
# クリップ全体の平均の前傾のこの割合だけ、背骨(spine)から上をまとめて後ろへ起こす(揺れはそのまま残る)。
# 技は振りの軌道が変わるので起こさない。
UPRIGHT_FRACTION = 0.6
UPRIGHT_CLIPS = {"mx_walk", "mx_run", "mx_walk_2", "mx_run_2",
                 "mx_strafe", "mx_strafe_2", "mx_strafe_3", "mx_strafe_4", "mx_idle"}
# Mixamo の剣の刃の向き(剣の骨 mixamorig:Sword_joint のローカル)。
# Sword and Shield Pack の glb で Sword_joint にウェイトのある頂点の一番遠い点(剣先)から測った(T ポーズで正面 -Y を向く)。
MIXAMO_BLADE_LOCAL = Vector((-0.247, -0.062, 0.967))
# Mixamo の骨 → 騎士の骨。ここに無い騎士の骨(hip / chest1 / hipl / handr / 指など)は休止姿勢のまま親に付いていく。
MIXAMO_TO_KNIGHT = {
    "mixamorig:Hips": "pelvis",
    "mixamorig:Spine": "spine",
    "mixamorig:Spine1": "spine1",
    "mixamorig:Spine2": "CHEST",
    "mixamorig:Neck": "neck",
    "mixamorig:Head": "head",
    "mixamorig:LeftShoulder": "shoulderl",
    "mixamorig:LeftArm": "armupperl",
    "mixamorig:LeftForeArm": "armlowerl",
    # 手は手首の骨へ当てる。騎士の剣の骨(handr.002)は手首(wristr)の子で、手(handr)とは兄弟なので、
    # 手(handr)へ当てると剣が手の回転についていかず、前腕に付いたまま動いた。
    "mixamorig:LeftHand": "wristl",
    "mixamorig:RightShoulder": "shoulderr",
    "mixamorig:RightArm": "armupperr",
    "mixamorig:RightForeArm": "armlowerr",
    "mixamorig:RightHand": "wristr",
    "mixamorig:LeftUpLeg": "thighl",
    "mixamorig:LeftLeg": "calfl",
    "mixamorig:LeftFoot": "footl",
    "mixamorig:LeftToeBase": "toesL",
    "mixamorig:RightUpLeg": "thighr",
    "mixamorig:RightLeg": "calfr",
    "mixamorig:RightFoot": "footr",
    "mixamorig:RightToeBase": "toesR",
}

# メッシュとアニメーションで骨の組み合わせを揃えるため、共通の設定にする。
COMMON = dict(
    use_selection=True,
    apply_unit_scale=True,
    apply_scale_options="FBX_SCALE_NONE",
    axis_forward="-Z",
    axis_up="Y",
    add_leaf_bones=False,
    use_armature_deform_only=True,
    primary_bone_axis="Y",
    secondary_bone_axis="X",
)


def action_fcurves(action):
    """Blender 4.4 以降の層付きアクションと、それより前の形の両方から F カーブを集める。"""
    if hasattr(action, "layers") and action.layers:
        curves = []
        for layer in action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    curves.extend(bag.fcurves)
        return curves
    return list(action.fcurves)


def action_moves(action):
    for fc in action_fcurves(action):
        values = [k.co[1] for k in fc.keyframe_points]
        if values and max(values) - min(values) > 1e-4:
            return True
    return False


def clip_name(action_name):
    # .blend の中では歩きの名前の頭に "1" が付いている(1walk_back など)。外して揃える。
    name = re.sub(r"^\d+", "", action_name)
    return re.sub(r"[^A-Za-z0-9_]+", "_", name)


def select_only(objects):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects:
        o.hide_set(False)
        o.hide_viewport = False
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]


scene = bpy.context.scene
armature = bpy.data.objects[ARMATURE_NAME]
sword = bpy.data.objects.get(SWORD_NAME)
meshes = [o for o in bpy.data.objects if o.type == "MESH" and o.parent == armature and o is not sword]

print("=== MATERIALS (material -> base color image)")
for mat in bpy.data.materials:
    images = []
    if mat.use_nodes and mat.node_tree:
        for node in mat.node_tree.nodes:
            if node.type == "TEX_IMAGE" and node.image:
                images.append(os.path.basename(node.image.filepath))
    print("  %-30s %s" % (mat.name, images))

# --- モデル本体(休止姿勢) ---
armature.data.pose_position = "REST"
bpy.context.view_layer.update()

# 剣は右手の骨(handr.002)の子として置かれている。骨の子のオブジェクトは FBX では骨の下の別ノードになり、
# このゲームの読み込み(スキンのメッシュだけを骨で動かす)では手に付いてこない。
# そこで休止姿勢の位置のまま、その骨へウェイト1でスキンした部品に作り替えて本体に含める。
# ゲームはメッシュ名に "sword" を含む部品を「内蔵の剣」として描画・攻撃判定に使う(CharacterModelProfile)。
sword_grip_bone = None
if sword and sword.parent_bone:
    grip_bone = sword.parent_bone
    sword_grip_bone = grip_bone
    # 休止姿勢での剣の位置を、骨の休止姿勢の行列から計算する。
    # (matrix_world は最後に評価したポーズ(待機の途中)の位置のままのことがあり、そのまま使うと
    #  剣が手から離れた場所(足の横)にスキンされた。骨の子は骨の「先端」を基準に置かれる)
    bone = armature.data.bones[grip_bone]
    world = (armature.matrix_world @ bone.matrix_local @ Matrix.Translation((0.0, bone.length, 0.0))
             @ sword.matrix_parent_inverse @ sword.matrix_basis)
    sword.parent = armature
    sword.parent_type = "OBJECT"
    sword.parent_bone = ""
    # 位置・向きは頂点へ焼き込み、オブジェクトの変換はほかの部品と同じ(アーマチュアと一致)にする。
    # 剣だけがオブジェクトに回転・移動を持っていると、ゲームの読み込みでは手から離れた場所に出た。
    sword.data = sword.data.copy()
    sword.data.transform(armature.matrix_world.inverted() @ world)
    sword.matrix_world = armature.matrix_world.copy()
    group = sword.vertex_groups.new(name=grip_bone)
    group.add(range(len(sword.data.vertices)), 1.0, "REPLACE")
    modifier = sword.modifiers.new("Armature", "ARMATURE")
    modifier.object = armature
    meshes.append(sword)
    print("SWORD skinned to bone", grip_bone)


def normalize_skin_weights(obj, deform_bones, max_influences=4):
    """変形用の骨の重みを、多い順に max_influences 本まで残して合計1にそろえる。

    このモデルは重みの合計が1未満の頂点が多い(腕・胸・マント・脚など数千頂点)。Blender は描画のときに
    合計で割り直すので崩れないが、ゲームのシェーダーは重みをそのまま掛けるため、足りない分だけ
    頂点が原点(足元)へ引き寄せられ、鎧やマントがトゲのように伸びた。
    また5本以上の骨に重みがある頂点もあり、ゲームは4本までしか持てない。
    """
    groups = {g.index: g for g in obj.vertex_groups if g.name in deform_bones}
    fixed = 0
    for v in obj.data.vertices:
        weights = [(g.group, g.weight) for g in v.groups if g.group in groups and g.weight > 0.0]
        if not weights:
            continue
        weights.sort(key=lambda item: item[1], reverse=True)
        kept = weights[:max_influences]
        total = sum(w for _, w in kept)
        if len(weights) == len(kept) and abs(total - 1.0) < 1e-4:
            continue
        fixed += 1
        for index, _ in weights[max_influences:]:
            groups[index].remove([v.index])
        for index, weight in kept:
            groups[index].add([v.index], weight / total, "REPLACE")
    return fixed


deform_bones = {b.name for b in armature.data.bones if b.use_deform}
for mesh in meshes:
    count = normalize_skin_weights(mesh, deform_bones)
    if count:
        print("WEIGHTS normalized %-16s %d vertices" % (mesh.name, count))

select_only([armature] + meshes)
bpy.ops.export_scene.fbx(
    filepath=os.path.join(OUTPUT_DIR, "Knight.fbx"),
    object_types={"ARMATURE", "MESH"},
    use_mesh_modifiers=False,  # Armature の変形を焼き込まない(スキンとして残す)
    mesh_smooth_type="FACE",
    bake_anim=False,
    path_mode="STRIP",
    **COMMON,
)
print("EXPORT Knight.fbx meshes=%d" % len(meshes))

def retarget_mixamo_clip(name, path):
    """Mixamo のクリップを読み込み、騎士の骨格へ移し替えたアクション name を作る。

    やり方: Mixamo の骨の「休止姿勢からの回転の変化」(ワールドの向き)を、対応する騎士の骨へ重ねる。
      騎士の骨の向き(ワールド) = (Mixamoの骨の今の向き × Mixamoの骨の休止の向きの逆) × そろえ × 騎士の骨の休止の向き
      そろえ = 騎士の骨の休止の向き(根元→先端)を、Mixamo の骨の休止の向きへ回す回転
    (Mixamo の休止姿勢は T ポーズ、騎士は腕を下げた形。そろえないと腕がその差だけ下がる)。
    どちらも正面が -Y・左が +X(読み込んで確かめた)なので、体全体の向きをそろえ直す必要はない。
    腰の位置は休止姿勢からの移動量を、腰の高さの比(騎士/Mixamo)で縮めて足す。
    """
    if not os.path.exists(path):
        print("RETARGET skip %s (not found: %s)" % (name, path))
        return None
    scene = bpy.context.scene
    knight_fps = scene.render.fps / scene.render.fps_base
    saved_fps = (scene.render.fps, scene.render.fps_base)
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    # FBX の読み込みはシーンのフレームレートをファイルの値(Mixamo は30fps)へ書き換える。
    # 騎士のほかのアクションは25fpsのフレーム番号で作られているので、すぐ戻す(戻さないと全クリップが速くなる)。
    source_fps = scene.render.fps / scene.render.fps_base
    scene.render.fps, scene.render.fps_base = saved_fps
    imported = [o for o in bpy.data.objects if o not in before]
    source = next(o for o in imported if o.type == "ARMATURE")
    source_action = source.animation_data.action
    src_start = source_action.frame_range[0]
    src_end = source_action.frame_range[1]
    if name in RETARGET_WINDOWS:
        window_start, window_end = RETARGET_WINDOWS[name]
        clip_start = src_start
        src_start = clip_start + window_start * source_fps
        src_end = min(src_end, clip_start + window_end * source_fps)
    # クリップの秒数を保つよう、騎士のフレーム(25fps)ごとに元のクリップの同じ時刻を取り出す。
    frame_start = 1
    frame_end = 1 + int((src_end - src_start) * knight_fps / source_fps + 1e-4)

    target = armature
    target.data.pose_position = "POSE"
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    target_ad = target.animation_data or target.animation_data_create()
    target_ad.use_nla = False
    target_ad.action = action

    knight_to_mixamo = {k: m for m, k in MIXAMO_TO_KNIGHT.items()}
    # 親から順に並べる(子の位置は親の姿勢から決まるため)。
    ordered = []
    def walk(bone):
        ordered.append(bone)
        for child in bone.children:
            walk(child)
    walk(target.data.bones["pelvis"])

    src_world = source.matrix_world
    src_hips = source.data.bones["mixamorig:Hips"]
    src_hips_rest = src_world @ src_hips.head_local
    knight_pelvis_rest = target.matrix_world @ target.data.bones["pelvis"].head_local
    height_ratio = knight_pelvis_rest.z / max(src_hips_rest.z, 1e-4)
    target_world_inv = target.matrix_world.inverted()

    # 上体を起こす回転(ワールド)。正面が -Y・上が +Z なので、左右の軸(X)まわりに回す。
    upright = Quaternion()
    upright_bones = set()
    if name in UPRIGHT_CLIPS:
        leans = []
        for frame in range(frame_start, frame_end + 1):
            source_frame = src_start + (frame - frame_start) * source_fps / knight_fps
            scene.frame_set(int(source_frame), subframe=source_frame - int(source_frame))
            torso = (src_world @ source.pose.bones["mixamorig:Neck"].head) - (src_world @ source.pose.bones["mixamorig:Hips"].head)
            leans.append(math.atan2(-torso.y, torso.z))  # 前へ倒れているほど正
        mean_lean = sum(leans) / len(leans)
        # X 軸まわりの正の回転は上(+Z)を前(-Y)へ倒すので、起こすには負の向きへ回す。
        upright = Quaternion(Vector((1.0, 0.0, 0.0)), -UPRIGHT_FRACTION * mean_lean)
        def collect(bone):
            upright_bones.add(bone.name)
            for child in bone.children:
                collect(child)
        collect(target.data.bones["spine"])
        print("RETARGET %s upright: mean torso lean %.1f deg -> raise %.1f deg" % (
            name, math.degrees(mean_lean), math.degrees(UPRIGHT_FRACTION * mean_lean)))

    def compute_pose(frame, wrist_twist):
        """騎士のフレーム frame の姿勢(骨の名前 → アーマチュア空間の行列)。剣を持つ手首へ wrist_twist だけひねりを足す。"""
        source_frame = src_start + (frame - frame_start) * source_fps / knight_fps
        scene.frame_set(int(source_frame), subframe=source_frame - int(source_frame))
        pose = {}
        for bone in ordered:
            rest = bone.matrix_local
            if bone.parent is not None and bone.parent.name in pose:
                parent_pose = pose[bone.parent.name]
                follow = parent_pose @ (bone.parent.matrix_local.inverted() @ rest)
            else:
                follow = rest.copy()
            source_name = knight_to_mixamo.get(bone.name)
            if source_name is None:
                pose[bone.name] = follow
                continue
            sp = source.pose.bones[source_name]
            now_rot = (src_world @ sp.matrix).to_quaternion()
            rest_rot = (src_world @ sp.bone.matrix_local).to_quaternion()
            delta = now_rot @ rest_rot.inverted()
            knight_rest_rot = (target.matrix_world @ rest).to_quaternion()
            # 休止姿勢の骨の向き(根元→先端)の違いを先にそろえる。Mixamo は T ポーズ、騎士は腕を下げた形なので、
            # そろえないと腕が十数度下がったまま動き、仰向けに倒れたときに腕が床へめり込んで体ごと浮いた。
            # 胴と頭はそろえない(NO_ALIGN_BONES)。騎士の背骨は腰で約10度後ろへ反り、胸から上で前へ戻る S 字で、
            # Mixamo の向きへそろえると反りが消えて上体が約7度前へ倒れ、へっぴり腰に見えた。
            if bone.name in NO_ALIGN_BONES:
                align = Quaternion()
            else:
                src_dir = (rest_rot.to_matrix() @ Vector((0.0, 1.0, 0.0))).normalized()
                knight_dir = (knight_rest_rot.to_matrix() @ Vector((0.0, 1.0, 0.0))).normalized()
                align = knight_dir.rotation_difference(src_dir)
            world_rot = delta @ align @ knight_rest_rot
            if bone.name in upright_bones:
                world_rot = upright @ world_rot
            arm_rot = (target_world_inv.to_quaternion() @ world_rot).to_matrix().to_4x4()
            if bone.name == "pelvis":
                moved = (src_world @ sp.head) - src_hips_rest
                if moved.z > 0.0:
                    moved.z *= RETARGET_HIP_RISE_SCALE.get(name, 1.0)
                head = target_world_inv @ (knight_pelvis_rest + moved * height_ratio)
            else:
                head = follow.translation
            pose[bone.name] = Matrix.Translation(head) @ arm_rot
            if bone.name == WEAPON_WRIST and wrist_twist:
                # 骨の軸(根元→先端)まわりにひねる。
                pose[bone.name] = pose[bone.name] @ Matrix.Rotation(wrist_twist, 4, "Y")
        return pose

    # --- 剣を持つ手首のひねりを決める ---
    # Mixamo の手と騎士の手は握り方(手の中で剣がどちらを向くか)が違い、そのまま移すと
    # 倒れたときに剣が床や体へ突き刺さった。手首を骨の軸まわりに 10 度刻みで回してみて、
    # クリップ全体で剣が床・胴へめり込む量が一番少ない角度を使う。
    blade_local = []
    if sword is not None and WEAPON_WRIST in target.data.bones:
        wrist_rest_inv = target.data.bones[WEAPON_WRIST].matrix_local.inverted()
        grip_head = target.data.bones[sword_grip_bone].head_local if sword_grip_bone else None
        points = [target.matrix_world.inverted() @ (sword.matrix_world @ v.co) for v in sword.data.vertices]
        if grip_head is not None and points:
            grip = min(points, key=lambda p: (p - grip_head).length)
            tip = max(points, key=lambda p: (p - grip).length)
            blade_local = [wrist_rest_inv @ (grip + (tip - grip) * s) for s in (0.35, 0.6, 0.8, 1.0)]

    def penetration(poses, twist):
        total = 0.0
        twist_matrix = Matrix.Rotation(twist, 4, "Y")
        for pose in poses:
            wrist = pose[WEAPON_WRIST] @ twist_matrix
            base = pose["pelvis"].translation
            top = pose["head"].translation
            axis = top - base
            for local in blade_local:
                point = wrist @ local
                total += max(0.0, 0.02 - point.z)  # 床(高さ0)より下
                t = max(0.0, min(1.0, (point - base).dot(axis) / max(axis.length_squared, 1e-6)))
                total += max(0.0, 0.22 - (point - (base + axis * t)).length)  # 胴(腰→頭の太さ0.22)の中
        return total

    def misalignment(poses, blade_dirs, twist):
        """騎士の剣の向きと、元のクリップの剣の向きのずれ(1 - 内積)の合計。"""
        if not blade_dirs:
            return 0.0
        twist_matrix = Matrix.Rotation(twist, 4, "Y")
        knight_blade = (blade_local[-1] - blade_local[0]).normalized()
        total = 0.0
        for pose, source_dir in zip(poses, blade_dirs):
            direction = ((pose[WEAPON_WRIST] @ twist_matrix).to_3x3() @ knight_blade).normalized()
            total += 1.0 - direction.dot(source_dir)
        return total

    best_twist = 0.0
    if blade_local:
        untwisted = []
        blade_dirs = []
        sword_joint = source.pose.bones.get("mixamorig:Sword_joint")
        for frame in range(frame_start, frame_end + 1):
            untwisted.append(compute_pose(frame, 0.0))
            if sword_joint is not None:
                rotation = (src_world @ sword_joint.matrix).to_quaternion()
                # 上体を起こしたクリップでは、剣も同じだけ起きた向きを目標にする。
                blade_dirs.append((target_world_inv.to_quaternion() @ upright @ rotation @ MIXAMO_BLADE_LOCAL).normalized())
        candidates = [math.radians(degrees) for degrees in range(0, 360, 10)]
        # 剣の向きを元のクリップにそろえるのを優先し(振りの軌道が変わらないように)、めり込みを減らすのも見る。
        scores = {twist: misalignment(untwisted, blade_dirs, twist) + 0.5 * penetration(untwisted, twist)
                  + 0.001 * min(twist, 2 * math.pi - twist) for twist in candidates}
        best_twist = min(scores, key=scores.get)
        frames_count = max(len(untwisted), 1)
        print("RETARGET %s wrist twist %d deg (blade misalignment avg %.3f -> %.3f, penetration %.3f -> %.3f)" % (
            name, round(math.degrees(best_twist)),
            misalignment(untwisted, blade_dirs, 0.0) / frames_count, misalignment(untwisted, blade_dirs, best_twist) / frames_count,
            penetration(untwisted, 0.0), penetration(untwisted, best_twist)))

    for frame in range(frame_start, frame_end + 1):
        pose = compute_pose(frame, best_twist)
        # 計算した姿勢を、骨ごとのローカルの値(親と休止姿勢から見た値)にしてキーを打つ。
        for bone in ordered:
            pb = target.pose.bones[bone.name]
            if bone.parent is not None and bone.parent.name in pose:
                base = pose[bone.parent.name] @ (bone.parent.matrix_local.inverted() @ bone.matrix_local)
            else:
                base = bone.matrix_local
            local = base.inverted() @ pose[bone.name]
            if bone.name == "pelvis":
                pb.location = local.translation
                pb.keyframe_insert("location", frame=frame, group=bone.name)
            else:
                pb.location = (0.0, 0.0, 0.0)
            rotation = local.to_quaternion()
            if pb.rotation_mode == "QUATERNION":
                pb.rotation_quaternion = rotation
                pb.keyframe_insert("rotation_quaternion", frame=frame, group=bone.name)
            elif pb.rotation_mode == "AXIS_ANGLE":
                axis, angle = rotation.to_axis_angle()
                pb.rotation_axis_angle = (angle, axis.x, axis.y, axis.z)
                pb.keyframe_insert("rotation_axis_angle", frame=frame, group=bone.name)
            else:
                pb.rotation_euler = rotation.to_euler(pb.rotation_mode)
                pb.keyframe_insert("rotation_euler", frame=frame, group=bone.name)

    for obj in imported:
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.data.actions.remove(source_action)
    print("RETARGET %s <- %s frames %d-%d (source %.0ffps %g-%g) height_ratio=%.3f" % (
        name, os.path.basename(path), frame_start, frame_end, source_fps, src_start, src_end, height_ratio))
    return action


def report_retarget(action, preview_dir):
    """移し替えたアクションの確認用。最後のフレームで部品ごとの一番低い点を出し、横から描いた画像を置く。"""
    scene = bpy.context.scene
    ad = armature.animation_data
    ad.action = action
    if hasattr(ad, "action_slot") and action.slots:
        ad.action_slot = action.slots[0]
    last = int(action.frame_range[1])
    scene.frame_set(last)
    depsgraph = bpy.context.evaluated_depsgraph_get()
    lows = []
    for mesh in meshes:
        evaluated = mesh.evaluated_get(depsgraph)
        data = evaluated.to_mesh()
        low = min((evaluated.matrix_world @ v.co).z for v in data.vertices)
        evaluated.to_mesh_clear()
        lows.append((low, mesh.name))
    lows.sort()
    print("CHECK %s frame %d lowest parts: %s" % (action.name, last,
          ", ".join("%s=%.3f" % (name, low) for low, name in lows[:6])))
    if not preview_dir:
        return
    os.makedirs(preview_dir, exist_ok=True)
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.color_type = "MATERIAL"
    scene.render.resolution_x = 640
    scene.render.resolution_y = 400
    camera = bpy.data.objects.new("preview_camera", bpy.data.cameras.new("preview_camera"))
    scene.collection.objects.link(camera)
    scene.camera = camera
    for frame in sorted({1 + round((last - 1) * k / 7) for k in range(8)}):
        scene.frame_set(frame)
        for view, location in (("side", Vector((-7.0, -1.0, 1.4))), ("front", Vector((0.5, -7.0, 1.4)))):
            camera.location = location
            camera.rotation_euler = (Vector((0.0, 0.0, 0.9)) - location).to_track_quat("-Z", "Y").to_euler()
            scene.render.filepath = os.path.join(preview_dir, "%s_%02d_%s.png" % (action.name, frame, view))
            bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(camera, do_unlink=True)


PREVIEW_DIR = None
if "--preview" in argv:
    PREVIEW_DIR = os.path.abspath(argv[argv.index("--preview") + 1])
for retarget_name, retarget_path in RETARGETS.items():
    if ONLY is not None and retarget_name not in ONLY:
        continue
    retargeted = retarget_mixamo_clip(retarget_name, retarget_path)
    if retargeted is not None:
        report_retarget(retargeted, PREVIEW_DIR)

# --- アニメーション(1アクション1ファイル) ---
armature.data.pose_position = "POSE"
ad = armature.animation_data or armature.animation_data_create()
ad.use_nla = False  # NLA トラックの動きを混ぜない(アクション単体で評価する)
for track in ad.nla_tracks:
    track.mute = True

select_only([armature])
exported = []
for action in bpy.data.actions:
    start, end = action.frame_range
    if end - start < 1.0 or not action_moves(action):
        print("SKIP %s (動きなし)" % action.name)
        continue
    if ONLY is not None and clip_name(action.name) not in ONLY:
        continue
    ad.action = action
    if hasattr(ad, "action_slot") and hasattr(action, "slots") and action.slots:
        ad.action_slot = action.slots[0]
    scene.frame_start = int(round(start))
    scene.frame_end = int(round(end))
    name = clip_name(action.name)
    path = os.path.join(ANIM_DIR, "Knight_%s.fbx" % name)
    bpy.ops.export_scene.fbx(
        filepath=path,
        object_types={"ARMATURE"},
        bake_anim=True,
        bake_anim_use_all_bones=True,
        bake_anim_use_nla_strips=False,
        bake_anim_use_all_actions=False,
        bake_anim_force_startend_keying=True,
        bake_anim_step=1.0,
        bake_anim_simplify_factor=0.0,
        **COMMON,
    )
    seconds = (end - start) / (scene.render.fps / scene.render.fps_base)
    exported.append((name, action.name, start, end, seconds))
    print("EXPORT anim %-20s <- %-18s frames %g-%g (%.2fs)" % (name, action.name, start, end, seconds))

# --- ゲーム用のテクスチャ ---
# マテリアル名 → 販売元のテクスチャ(Knight.zip の中のパス。末尾の _BaseColor.png / _Metallic.png を除いた部分)。
# ゲームはマテリアル名と同じ名前の画像をテクスチャ置き場から探す(.blend のマテリアルに画像がつながっていないため)。
TEXTURE_SOURCES = {
    "arms": "textures/knight_arms__arms",
    "chest": "textures/knight_chest__chest",
    "cloak": "textures/knight_cloak__cloak_001",
    "helmet": "textures/knight_helmet__helmet",
    "leather": "textures/knight_leather__leather",
    "legs": "textures/knight_legs__legs_001",
    "tabart": "textures/knight_tabart__tabart",
    "dagger": "dagger/dagger_dagger",
    "bastard sword": "sword/sword_bastard_sword",
}
TEXTURE_SIZE = 2048
# 金属の部分をどれだけ暗くするか(1 で真っ黒、0 で元のまま)。
METAL_DARKEN = 0.6


def make_game_textures(source_dir):
    """PBR 用のベースカラーを、このゲームのシェーダー(拡散光だけ)向けに直して置く。

    販売元のテクスチャは PBR(ベースカラー+金属度)で、鎧の金属部分のベースカラーは明るい灰色(平均0.63)。
    PBR では金属は拡散光を返さず映り込みで暗く見えるが、このゲームはベースカラーをそのまま拡散光に使うので、
    鎧が白っぽく見えた。金属度に応じて暗くする: 色 × (1 - METAL_DARKEN × 金属度)。
    4K のままだと読み込みが重いので 2K へ縮める。
    """
    import numpy as np

    for material, prefix in TEXTURE_SOURCES.items():
        base_path = os.path.join(source_dir, prefix + "_BaseColor.png")
        metal_path = os.path.join(source_dir, prefix + "_Metallic.png")
        if not os.path.exists(base_path):
            print("TEXTURE skip %s (not found: %s)" % (material, base_path))
            continue
        base = bpy.data.images.load(base_path)
        base.scale(TEXTURE_SIZE, TEXTURE_SIZE)
        pixels = np.empty(TEXTURE_SIZE * TEXTURE_SIZE * 4, dtype=np.float32)
        base.pixels.foreach_get(pixels)
        pixels = pixels.reshape(-1, 4)
        mean_before = float(pixels[:, :3].mean())
        if os.path.exists(metal_path):
            metal = bpy.data.images.load(metal_path)
            metal.scale(TEXTURE_SIZE, TEXTURE_SIZE)
            metal_pixels = np.empty(TEXTURE_SIZE * TEXTURE_SIZE * 4, dtype=np.float32)
            metal.pixels.foreach_get(metal_pixels)
            metallic = metal_pixels.reshape(-1, 4)[:, 0]
            pixels[:, :3] *= (1.0 - METAL_DARKEN * metallic)[:, None]
            bpy.data.images.remove(metal)
        pixels[:, 3] = 1.0
        out = bpy.data.images.new("game_" + material, TEXTURE_SIZE, TEXTURE_SIZE, alpha=False)
        out.pixels.foreach_set(pixels.ravel())
        out.filepath_raw = os.path.join(OUTPUT_DIR, material + ".png")
        out.file_format = "PNG"
        out.save()
        print("TEXTURE %-14s mean %.2f -> %.2f" % (material, mean_before, float(pixels[:, :3].mean())))
        bpy.data.images.remove(out)
        bpy.data.images.remove(base)


if TEXTURE_DIR:
    make_game_textures(TEXTURE_DIR)

with open(os.path.join(OUTPUT_DIR, "export_list.txt"), "w", encoding="utf-8") as f:
    f.write("fps=%g\n" % (scene.render.fps / scene.render.fps_base))
    for name, src, start, end, seconds in exported:
        f.write("anim/Knight_%s.fbx\t%s\tframes %g-%g\t%.2fs\n" % (name, src, start, end, seconds))
print("DONE %d clips -> %s" % (len(exported), OUTPUT_DIR))
