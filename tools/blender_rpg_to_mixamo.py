"""Blender で実行し、骨が空のオブジェクト(Empty)の階層で入った FBX の動きを、Mixamo と同じ骨の名前の
アーマチュアへ焼き直して FBX に書き出す。tools/blender_export_knight.py の移し替え(RETARGETS)へ通すための下ごしらえ。

対象: Unity の無料アセット「RPG Character Mecanim Animation Pack FREE」(ExplosiveLLC)の FBX。
  3ds Max から書き出されていて、骨(B_Pelvis など)がアーマチュアではなく Empty として読み込まれる。
  根元の Motion が前への移動(ルートモーション)を持つ。

使い方:
  blender --background --factory-startup --disable-autoexec --python tools/blender_rpg_to_mixamo.py -- <入力.fbx> <出力.fbx>

やり方:
  - 最初のフレームの姿勢を休止姿勢として、Empty の位置(根元)から子の位置(先端)へ向く骨を作る。
    移し替えは「休止姿勢からの向きの変化」を使い、骨の向き(根元→先端)は今の姿勢へそろえるので、
    休止姿勢が T ポーズでなくても骨の向きは正しく移る(骨の軸まわりのねじれだけが休止姿勢の差だけずれる)。
  - Motion の移動は取り除き、その場で動くクリップにする。ゲームでは回避の移動をプログラム側で行う(player.cpp)。

注意: 書き出した FBX は購入・配布元のあるアセットから作ったものなので、公開リポジトリへコミットしない。
"""

import sys

import bpy
from mathutils import Matrix, Vector

argv = sys.argv[sys.argv.index("--") + 1:]
SOURCE_PATH, OUTPUT_PATH = argv[0], argv[1]

# 元の骨 → Mixamo の骨の名前。
TO_MIXAMO = {
    "B_Pelvis": "Hips",
    "B_Spine": "Spine",
    "B_Spine1": "Spine1",
    "B_Spine2": "Spine2",
    "B_Neck": "Neck",
    "B_Head": "Head",
    "B_L_Clavicle": "LeftShoulder",
    "B_L_UpperArm": "LeftArm",
    "B_L_Forearm": "LeftForeArm",
    "B_L_Hand": "LeftHand",
    "B_R_Clavicle": "RightShoulder",
    "B_R_UpperArm": "RightArm",
    "B_R_Forearm": "RightForeArm",
    "B_R_Hand": "RightHand",
    "B_L_Thigh": "LeftUpLeg",
    "B_L_Calf": "LeftLeg",
    "B_L_Foot": "LeftFoot",
    "B_L_Toe0": "LeftToeBase",
    "B_R_Thigh": "RightUpLeg",
    "B_R_Calf": "RightLeg",
    "B_R_Foot": "RightFoot",
    "B_R_Toe0": "RightToeBase",
}
# 骨の先端にする子(無い骨は親から自分への向きへ少し延ばす)。
TAIL_CHILD = {
    "B_Pelvis": "B_Spine", "B_Spine": "B_Spine1", "B_Spine1": "B_Spine2", "B_Spine2": "B_Neck",
    "B_Neck": "B_Head",
    "B_L_Clavicle": "B_L_UpperArm", "B_L_UpperArm": "B_L_Forearm", "B_L_Forearm": "B_L_Hand", "B_L_Hand": "B_L_Finger2",
    "B_R_Clavicle": "B_R_UpperArm", "B_R_UpperArm": "B_R_Forearm", "B_R_Forearm": "B_R_Hand", "B_R_Hand": "B_R_Finger2",
    "B_L_Thigh": "B_L_Calf", "B_L_Calf": "B_L_Foot", "B_L_Foot": "B_L_Toe0",
    "B_R_Thigh": "B_R_Calf", "B_R_Calf": "B_R_Foot", "B_R_Foot": "B_R_Toe0",
}

for obj in list(bpy.data.objects):
    bpy.data.objects.remove(obj, do_unlink=True)
bpy.ops.import_scene.fbx(filepath=SOURCE_PATH)
scene = bpy.context.scene
objects = bpy.data.objects
motion = objects["Motion"]
frames = sorted({int(round(k.co.x)) for a in bpy.data.actions for fc in getattr(a, "fcurves", []) for k in fc.keyframe_points})
if not frames:
    # Blender 5 のスロット付きアクション: 範囲から取る。
    start = min(int(a.frame_range[0]) for a in bpy.data.actions)
    end = max(int(a.frame_range[1]) for a in bpy.data.actions)
else:
    start, end = frames[0], frames[-1]


def world_at(frame):
    """frame の各骨のワールド行列。Motion の移動を引いて、その場の動きにする。"""
    scene.frame_set(frame)
    root_shift = Matrix.Translation(-motion.matrix_world.translation.copy())
    return {name: root_shift @ objects[name].matrix_world.copy() for name in TO_MIXAMO}


def position(world, name):
    return world[name].translation.copy()


rest_world = world_at(start)
tail_world = {}
scene.frame_set(start)
root_shift = Matrix.Translation(-motion.matrix_world.translation.copy())
for name in TO_MIXAMO:
    child = TAIL_CHILD.get(name)
    if child is not None:
        tail_world[name] = (root_shift @ objects[child].matrix_world).translation.copy()
    elif name == "B_Head":
        tail_world[name] = position(rest_world, name) + Vector((0.0, 0.0, 0.2))
    else:  # つま先: 足からつま先への向きへ延ばす
        foot = "B_L_Foot" if "_L_" in name else "B_R_Foot"
        direction = (position(rest_world, name) - position(rest_world, foot)).normalized()
        tail_world[name] = position(rest_world, name) + direction * 0.1

# アーマチュアを作る(休止姿勢 = 最初のフレーム)。
arm_data = bpy.data.armatures.new("mixamo_rig")
arm = bpy.data.objects.new("Armature", arm_data)
scene.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
edit = {}
for name, mixamo in TO_MIXAMO.items():
    eb = arm_data.edit_bones.new("mixamorig:" + mixamo)
    eb.head = position(rest_world, name)
    eb.tail = tail_world[name]
    # 骨の軸まわりの向き(ロール)は元の Empty の Z 軸に合わせる(どれでもよいが、決まった向きにする)。
    eb.align_roll(rest_world[name].to_3x3() @ Vector((0.0, 0.0, 1.0)))
    edit[name] = eb
for name, eb in edit.items():
    parent = objects[name].parent
    while parent is not None and parent.name not in edit:
        parent = parent.parent
    if parent is not None:
        eb.parent = edit[parent.name]
bpy.ops.object.mode_set(mode="OBJECT")

bones = {name: arm_data.bones["mixamorig:" + TO_MIXAMO[name]] for name in TO_MIXAMO}
# Empty から骨への固定のずれ(休止姿勢で骨の行列 = Empty の行列 @ ずれ)。
offset = {name: rest_world[name].inverted() @ bones[name].matrix_local for name in TO_MIXAMO}

action = bpy.data.actions.new("mixamo.com")
arm.animation_data_create().action = action
ordered = []


def walk(bone):
    ordered.append(bone)
    for child in bone.children:
        walk(child)


walk(arm_data.bones["mixamorig:Hips"])
source_of = {bone.name: name for name, bone in bones.items()}
for frame in range(start, end + 1):
    world = world_at(frame)
    pose = {}
    for bone in ordered:
        name = source_of[bone.name]
        pose[bone.name] = world[name] @ offset[name]
        # 伸び縮みを落とす(回転と位置だけにする)。
        loc, rot, _ = pose[bone.name].decompose()
        pose[bone.name] = Matrix.Translation(loc) @ rot.to_matrix().to_4x4()
    for bone in ordered:
        pb = arm.pose.bones[bone.name]
        if bone.parent is not None:
            base = pose[bone.parent.name] @ (bone.parent.matrix_local.inverted() @ bone.matrix_local)
        else:
            base = bone.matrix_local
        basis = base.inverted() @ pose[bone.name]
        pb.rotation_mode = "QUATERNION"
        pb.location = basis.translation
        pb.rotation_quaternion = basis.to_quaternion()
        pb.keyframe_insert("location", frame=frame, group=bone.name)
        pb.keyframe_insert("rotation_quaternion", frame=frame, group=bone.name)

for obj in list(bpy.data.objects):
    if obj != arm:
        bpy.data.objects.remove(obj, do_unlink=True)
scene.frame_start, scene.frame_end = start, end
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.export_scene.fbx(
    filepath=OUTPUT_PATH, use_selection=True, bake_anim=True, bake_anim_use_all_actions=False,
    bake_anim_use_nla_strips=False, add_leaf_bones=False, apply_unit_scale=True,
    primary_bone_axis="Y", secondary_bone_axis="X")
print("CONVERTED %s -> %s frames %d-%d" % (SOURCE_PATH, OUTPUT_PATH, start, end))
