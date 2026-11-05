import bpy
import os
import sys
import numpy as np
from mathutils import Matrix

args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
blend_path, clip_path = map(os.path.abspath, args[:2])
frame = int(args[2]) if len(args) > 2 else 11
bpy.ops.wm.open_mainfile(filepath=blend_path)
scene = bpy.context.scene
armature = next(obj for obj in scene.objects if obj.type == 'ARMATURE')
mesh = bpy.data.objects['FallenPaladin_Runtime']
existing = set(scene.objects)
bpy.ops.import_scene.fbx(filepath=clip_path, automatic_bone_orientation=False)
clip_armature = next(obj for obj in scene.objects if obj.type == 'ARMATURE' and obj not in existing)
action = clip_armature.animation_data.action
slot = clip_armature.animation_data.action_slot
armature.animation_data.action = action
armature.animation_data.action_slot = slot
scene.frame_set(frame)
bpy.context.view_layer.update()

excluded = ('finger', 'index', 'middle', 'ring', 'pinky', 'thumb', 'sword', 'weapon',
            'leg', 'thigh', 'knee', 'calf', 'foot', 'ankle', 'toe')
driven = {
    bone.name for bone in armature.data.bones
    if bone.name.lower().replace('_', '').replace('-', '').replace(' ', '')
    not in {'root', 'mixamorig:root', 'hips', 'mixamorig:hips', 'pelvis'}
    and not any(token in bone.name.lower() for token in excluded)
}
sampled = {
    name: armature.pose.bones[name].matrix_basis.copy()
    for name in driven if armature.pose.bones.get(name) is not None
}
armature.animation_data.action = None
for pose_bone in armature.pose.bones:
    pose_bone.matrix_basis = Matrix.Identity(4)
for name, basis in sampled.items():
    armature.pose.bones[name].matrix_basis = basis
bpy.context.view_layer.update()

depsgraph = bpy.context.evaluated_depsgraph_get()
evaluated = mesh.evaluated_get(depsgraph)
evaluated_mesh = evaluated.to_mesh()
original = np.asarray([tuple(mesh.matrix_world @ v.co) for v in mesh.data.vertices], dtype=np.float64)
deformed = np.asarray([tuple(evaluated.matrix_world @ v.co) for v in evaluated_mesh.vertices], dtype=np.float64)
faces = np.asarray([tuple(poly.vertices) for poly in mesh.data.polygons], dtype=np.int32)
evaluated.to_mesh_clear()
edges = np.stack((faces[:, [0, 1, 2]], faces[:, [1, 2, 0]]), axis=2)
edge_lengths = np.linalg.norm(
    original[edges[:, :, 0]] - original[edges[:, :, 1]], axis=2)
deformed_lengths = np.linalg.norm(
    deformed[edges[:, :, 0]] - deformed[edges[:, :, 1]], axis=2)
ratio = deformed_lengths / np.maximum(edge_lengths, 1e-8)
max_ratio = ratio.max(axis=1)
max_deformed_edge = deformed_lengths.max(axis=1)
selected = np.flatnonzero((max_ratio > 3.0) & (max_deformed_edge > 0.04))
selected = selected[np.argsort(max_ratio[selected])[-30:][::-1]]
groups = {group.index: group.name for group in mesh.vertex_groups}
for face_index in selected:
    vertex_ids = faces[face_index]
    weights = []
    for vertex_id in vertex_ids:
        vertex = mesh.data.vertices[int(vertex_id)]
        weights.append(sorted(
            ((groups[item.group], round(item.weight, 2)) for item in vertex.groups),
            key=lambda value: value[1], reverse=True)[:3])
    print('STRAIN', round(float(max_ratio[face_index]), 2),
          'rest_center', np.round(original[vertex_ids].mean(axis=0), 3).tolist(),
          'posed_center', np.round(deformed[vertex_ids].mean(axis=0), 3).tolist(),
          'weights', weights)
print('STRAINED_FACES:', len(selected), 'OF', len(faces), 'AT_FRAME', frame)
