import bpy
import numpy as np
import sys

bpy.ops.wm.open_mainfile(filepath=sys.argv[-1])
mesh = bpy.data.objects['tripo_node_3786d797']
armature = next(obj for obj in bpy.context.scene.objects if obj.type == 'ARMATURE')
action = armature.animation_data.action
frame = int(round(action.frame_range[0]))
bpy.context.scene.frame_set(frame)
bpy.context.view_layer.update()

original_local = np.asarray([tuple(v.co) for v in mesh.data.vertices], dtype=np.float64)
original_world = np.asarray([tuple(mesh.matrix_world @ v.co) for v in mesh.data.vertices], dtype=np.float64)
bone_names = [bone.name for bone in armature.data.bones if bone.use_deform and 'End' not in bone.name]
bone_index = {name: i for i, name in enumerate(bone_names)}
arm_world = np.asarray(armature.matrix_world, dtype=np.float64)
inv_arm_world = np.linalg.inv(arm_world)
skin_matrices = np.repeat(np.eye(4)[None, :, :], len(bone_names), axis=0)
for i, name in enumerate(bone_names):
    bone = armature.data.bones[name]
    pose = armature.pose.bones[name]
    skin_matrices[i] = arm_world @ np.asarray(pose.matrix) @ np.linalg.inv(np.asarray(bone.matrix_local)) @ inv_arm_world

indices = np.zeros((len(mesh.data.vertices), 4), dtype=np.int32)
weights = np.zeros((len(mesh.data.vertices), 4), dtype=np.float64)
for vi, vertex in enumerate(mesh.data.vertices):
    for slot, item in enumerate(vertex.groups):
        if slot >= 4: break
        indices[vi, slot] = item.group
        weights[vi, slot] = item.weight
blend = np.zeros((len(mesh.data.vertices), 4, 4), dtype=np.float64)
for slot in range(4):
    blend += weights[:, slot, None, None] * skin_matrices[indices[:, slot]]
corrected_world = np.einsum('nij,nj->ni', np.linalg.inv(blend), np.c_[original_world, np.ones(len(original_world))])

test = mesh.copy()
test.data = mesh.data.copy()
test.name = 'BindRetargetTest'
bpy.context.collection.objects.link(test)
test.data.vertices.foreach_set('co', (corrected_world @ np.linalg.inv(np.asarray(mesh.matrix_world)).T)[:, :3].astype(np.float32).ravel())
test.data.update()
bpy.context.view_layer.update()
evaluated = test.evaluated_get(bpy.context.evaluated_depsgraph_get())
evaluated_mesh = evaluated.to_mesh()
result_world = np.asarray([tuple(evaluated.matrix_world @ v.co) for v in evaluated_mesh.vertices], dtype=np.float64)
evaluated.to_mesh_clear()
error = np.linalg.norm(result_world - original_world, axis=1)
print('FRAME:', frame, 'RMS:', float(np.sqrt(np.mean(error**2))), 'MAX:', float(error.max()))
