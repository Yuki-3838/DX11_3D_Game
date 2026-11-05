import bpy
import json
import os
import sys
from mathutils import Vector

paths = sys.argv[sys.argv.index('--') + 1:]
for path in paths:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    ext = os.path.splitext(path)[1].lower()
    if ext == '.fbx':
        bpy.ops.import_scene.fbx(filepath=path, automatic_bone_orientation=False)
    elif ext in ('.glb', '.gltf'):
        bpy.ops.import_scene.gltf(filepath=path)
    else:
        bpy.ops.wm.open_mainfile(filepath=path)
    result = {'file': path, 'objects': []}
    for obj in bpy.context.scene.objects:
        if obj.type == 'ARMATURE':
            relevant = {'mixamorig:Hips','mixamorig:Spine','mixamorig:Spine1','mixamorig:Spine2','mixamorig:Neck','mixamorig:Head',
                'mixamorig:LeftShoulder','mixamorig:LeftArm','mixamorig:LeftForeArm','mixamorig:LeftHand',
                'mixamorig:RightShoulder','mixamorig:RightArm','mixamorig:RightForeArm','mixamorig:RightHand',
                'mixamorig:LeftUpLeg','mixamorig:LeftLeg','mixamorig:LeftFoot','mixamorig:RightUpLeg','mixamorig:RightLeg','mixamorig:RightFoot'}
            result['objects'].append({
                'name': obj.name,
                'loc': list(obj.location),
                'rot': list(obj.rotation_euler),
                'scale': list(obj.scale),
                'bones': [{
                    'name': b.name,
                    'parent': b.parent.name if b.parent else None,
                    'head_world': list(obj.matrix_world @ b.head_local),
                    'tail_world': list(obj.matrix_world @ b.tail_local),
                    'matrix': [list(row) for row in b.matrix_local] if b.name in {'mixamorig:Hips','mixamorig:Spine','mixamorig:Spine1','mixamorig:Spine2','mixamorig:LeftArm','mixamorig:RightArm'} else None,
                    'deform': b.use_deform,
                } for b in obj.data.bones if b.name in relevant],
            })
        elif obj.type == 'MESH':
            points = [obj.matrix_world @ v.co for v in obj.data.vertices]
            if points:
                result['objects'].append({
                    'name': obj.name, 'type': 'MESH',
                    'parent': obj.parent.name if obj.parent else None,
                    'vertices': len(obj.data.vertices),
                    'bbox': [
                        [min(p[i] for p in points) for i in range(3)],
                        [max(p[i] for p in points) for i in range(3)],
                    ],
                })
    print(json.dumps(result, ensure_ascii=False))
