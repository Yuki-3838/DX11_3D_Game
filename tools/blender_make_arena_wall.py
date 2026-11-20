"""
闘技場の壁に使う石壁を、Poly Haven の「Modular Fort 01」(CC0) から取り出してゲーム用の OBJ に書き出す。

使い方(Blender 5.x):
  blender -b --python tools/blender_make_arena_wall.py -- assets/model/ModularFort/modular_fort_01_2k.fbx

出力: assets/model/ModularFort/arena_wall_thick.obj (+ .mtl)

- 使う部品は「wall_thick_straight_01」(胸壁の付いた厚い石壁。幅14.6m・厚み4.2m・高さ8.5m)。
- 単位はメートルのまま書き出す。ゲームの大きさへは実行時に合わせる(system/ArenaWallVisual.h)。
- 向き: 部品の -Y 側(笠石の線が入った面)を闘技場の内側に使う。内側の面の下端を Y=0、
  左右の中心を X=0、床を Z=0 に置く。壁の厚みは +Y へ伸びる。
  OBJ(右手系・Y上)に書き出し、ゲームの Assimp が左手系へ変換すると、厚みは +Z(外側)へ伸びる。
- UV は石積みの写真をくり返して貼る UVMap_00 だけを残す(UVMap_01 は汚れの位置合わせ用で、使わない)。
- 元の FBX には色の画像のつながりが無い(法線と粗さだけ)ので、ここで色の画像をマテリアルに付けて書き出す。
"""
import bpy
import os
import sys
from mathutils import Vector

PIECE = "modular_fort_01_wall_thick_straight_01"


def main():
    fbx = os.path.abspath(sys.argv[-1])
    folder = os.path.dirname(fbx)
    texdir = os.path.join(folder, "textures")

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=fbx)

    piece = None
    for obj in list(bpy.context.scene.objects):
        if obj.type == 'MESH' and obj.name == PIECE:
            piece = obj
        else:
            bpy.data.objects.remove(obj)
    if piece is None:
        raise RuntimeError("piece not found: " + PIECE)

    # 位置・回転・拡大を頂点へ焼き込む。
    bpy.context.view_layer.objects.active = piece
    piece.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    # 内側の面の下端を原点へ。
    corners = [Vector(c) for c in piece.bound_box]
    min_x = min(c.x for c in corners)
    max_x = max(c.x for c in corners)
    min_y = min(c.y for c in corners)
    min_z = min(c.z for c in corners)
    offset = Vector((-(min_x + max_x) * 0.5, -min_y, -min_z))
    for v in piece.data.vertices:
        v.co += offset

    # くり返し貼り用の UV だけを残す。
    mesh = piece.data
    for uv in list(mesh.uv_layers):
        if uv.name != "UVMap_00":
            mesh.uv_layers.remove(uv)

    # 色の画像をマテリアルへ付ける(OBJ の map_Kd になる)。
    for mat in mesh.materials:
        key = next((k for k in ("wall", "trim", "plaster") if "_%s_" % k in mat.name), None)
        if key is None or not mat.use_nodes:
            continue
        tree = mat.node_tree
        bsdf = next(n for n in tree.nodes if n.type == 'BSDF_PRINCIPLED')
        image_node = tree.nodes.new('ShaderNodeTexImage')
        image_node.image = bpy.data.images.load(
            os.path.join(texdir, "modular_fort_01_%s_diff_2k.jpg" % key))
        tree.links.new(image_node.outputs['Color'], bsdf.inputs['Base Color'])

    out = os.path.join(folder, "arena_wall_thick.obj")
    bpy.ops.wm.obj_export(
        filepath=out,
        export_selected_objects=True,
        export_uv=True,
        export_normals=True,
        export_materials=True,
        export_triangulated_mesh=True,
        path_mode='STRIP',
        forward_axis='NEGATIVE_Z',
        up_axis='Y',
    )
    size = Vector((max_x - min_x, max(c.y for c in corners) - min_y, max(c.z for c in corners) - min_z))
    print("EXPORTED", out, "size(m)=", tuple(round(s, 3) for s in size),
          "tris=", sum(len(p.vertices) - 2 for p in mesh.polygons))


main()
