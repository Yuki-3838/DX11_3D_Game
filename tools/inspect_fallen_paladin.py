import bpy
import json
import os
import sys

fbx_path = sys.argv[-1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx_path, automatic_bone_orientation=False)

objects = []
all_points = []
for obj in bpy.context.scene.objects:
    entry = {
        "name": obj.name,
        "type": obj.type,
        "parent": obj.parent.name if obj.parent else None,
        "location": list(obj.location),
        "rotation": list(obj.rotation_euler),
        "scale": list(obj.scale),
    }
    if obj.type == 'MESH':
        corners = [obj.matrix_world @ v.co for v in obj.data.vertices]
        all_points.extend([list(v) for v in corners])
        entry["vertices"] = len(obj.data.vertices)
        entry["polygons"] = len(obj.data.polygons)
        entry["dimensions"] = list(obj.dimensions)
        entry["materials"] = [slot.material.name if slot.material else None for slot in obj.material_slots]
    if obj.type == 'ARMATURE':
        entry["bones"] = [b.name for b in obj.data.bones]
    objects.append(entry)

result = {"fbx": fbx_path, "objects": objects}
if all_points:
    xs = [p[0] for p in all_points]
    ys = [p[1] for p in all_points]
    zs = [p[2] for p in all_points]
    result["world_bbox_min"] = [min(xs), min(ys), min(zs)]
    result["world_bbox_max"] = [max(xs), max(ys), max(zs)]
    result["world_dimensions"] = [max(xs)-min(xs), max(ys)-min(ys), max(zs)-min(zs)]

out_path = os.path.splitext(fbx_path)[0] + ".inspect.json"
with open(out_path, "w", encoding="utf-8") as f:
    json.dump(result, f, ensure_ascii=False, indent=2)
print(json.dumps(result, ensure_ascii=False))
