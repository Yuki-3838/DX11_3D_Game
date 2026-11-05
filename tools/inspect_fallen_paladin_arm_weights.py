import bpy
import os
import sys
from collections import defaultdict

args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
bpy.ops.wm.open_mainfile(filepath=os.path.abspath(args[0]))
mesh = bpy.data.objects['tripo_node_3786d797']
group_names = {group.index: group.name for group in mesh.vertex_groups}
categories = defaultdict(lambda: {'count': 0, 'position': [0.0] * 3, 'normal': [0.0] * 3,
                                  'arm': 0.0, 'torso': 0.0, 'hip': 0.0})

for vertex in mesh.data.vertices:
    weights = {group_names[item.group]: item.weight for item in vertex.groups}
    left = weights.get('mixamorig:LeftArm', 0.0) + weights.get('mixamorig:LeftForeArm', 0.0)
    right = weights.get('mixamorig:RightArm', 0.0) + weights.get('mixamorig:RightForeArm', 0.0)
    torso = sum(weights.get(f'mixamorig:{name}', 0.0) for name in ('Spine', 'Spine1', 'Spine2'))
    hip = weights.get('mixamorig:Hips', 0.0)
    if max(left, right) < 0.35:
        continue
    position = mesh.matrix_world @ vertex.co
    normal = mesh.matrix_world.to_3x3() @ vertex.normal
    normal.normalize()
    face = 'back-facing' if normal.y < -0.55 else 'front-facing' if normal.y > 0.55 else 'side-facing'
    side = 'left-arm' if left >= right else 'right-arm'
    height = 'upper' if position.z > 0.12 else 'lower'
    entry = categories[(side, face, height)]
    entry['count'] += 1
    for axis in range(3):
        entry['position'][axis] += position[axis]
        entry['normal'][axis] += normal[axis]
    entry['arm'] += max(left, right)
    entry['torso'] += torso
    entry['hip'] += hip

for key, entry in sorted(categories.items()):
    count = entry['count']
    print(key, {
        'vertices': count,
        'mean_position': [round(value / count, 3) for value in entry['position']],
        'mean_normal': [round(value / count, 3) for value in entry['normal']],
        'avg_arm': round(entry['arm'] / count, 3),
        'avg_torso': round(entry['torso'] / count, 3),
        'avg_hips': round(entry['hip'] / count, 3),
    })
