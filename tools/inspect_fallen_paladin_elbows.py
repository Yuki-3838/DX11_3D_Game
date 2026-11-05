import bpy
import os
import sys
from collections import Counter

args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
blend_path = os.path.abspath(args[0])
bpy.ops.wm.open_mainfile(filepath=blend_path)
armature = next(obj for obj in bpy.context.scene.objects if obj.type == 'ARMATURE')
if armature.animation_data and armature.animation_data.action:
    bpy.context.scene.frame_set(int(round(armature.animation_data.action.frame_range[0])))
bpy.context.view_layer.update()
meshes = [
    bpy.data.objects.get('tripo_node_3786d797'),
    bpy.data.objects.get('FallenPaladin_Runtime'),
]

for mesh in filter(None, meshes):
    z_values = [vertex.co.z for vertex in mesh.data.vertices]
    radius = max((max(z_values) - min(z_values)) * 0.06, 0.015)
    group_names = {group.index: group.name for group in mesh.vertex_groups}
    evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
    evaluated_mesh = evaluated.to_mesh()
    positions = [evaluated.matrix_world @ vertex.co for vertex in evaluated_mesh.vertices]
    for side in ('Left', 'Right'):
        upper_name = f'mixamorig:{side}Arm'
        fore_name = f'mixamorig:{side}ForeArm'
        elbow = armature.matrix_world @ armature.pose.bones[upper_name].tail
        pairs = Counter()
        totals = Counter()
        included = 0
        for vertex, world in zip(mesh.data.vertices, positions):
            if (world - elbow).length >= radius:
                continue
            weights = {group_names[item.group]: item.weight for item in vertex.groups}
            upper = weights.get(upper_name, 0.0)
            fore = weights.get(fore_name, 0.0)
            if upper > 0.0 or fore > 0.0:
                included += 1
                pair = (round(upper, 2), round(fore, 2))
                pairs[pair] += 1
                totals['upper'] += upper
                totals['forearm'] += fore
        print(mesh.name, side, 'radius', round(radius, 4), 'vertices', included,
              'pair_weights', pairs.most_common(12),
              'average', round(totals['upper'] / max(included, 1), 3),
              round(totals['forearm'] / max(included, 1), 3))
    evaluated.to_mesh_clear()
