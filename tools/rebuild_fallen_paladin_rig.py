"""Rebind Fallen Paladin to the exact rest skeleton used by the game clips."""

import bpy
import os
import sys
import numpy as np
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index('--') + 1:]
source_fbx = os.path.abspath(args[0])
donor_motion = os.path.abspath(args[1])
runtime_dir = os.path.abspath(os.path.join(os.path.dirname(source_fbx), os.pardir, 'runtime'))
os.makedirs(runtime_dir, exist_ok=True)
out_glb = os.path.join(runtime_dir, 'fallen_paladin_rigged.glb')
out_fbx = os.path.join(runtime_dir, 'fallen_paladin_rigged.fbx')
out_blend = os.path.join(runtime_dir, 'fallen_paladin_rigged.blend')
out_runtime_blend = os.path.join(runtime_dir, 'fallen_paladin_runtime_optimized.blend')

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.import_scene.fbx(filepath=source_fbx, automatic_bone_orientation=False)
meshes = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH']
if len(meshes) != 1:
    raise RuntimeError(f'Expected one source mesh, got {len(meshes)}')
mesh = meshes[0]

# Source uses Z-up with its body width on Y and depth on X. Rotate its horizontal
# axes to the Mixamo FBX convention (width X, depth Y) while preserving Z-up.
mesh.rotation_euler.z += np.pi / 2
bpy.context.view_layer.objects.active = mesh
mesh.select_set(True)
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)

original_coords = np.asarray([tuple(v.co) for v in mesh.data.vertices], dtype=np.float32)
mn = original_coords.min(axis=0)
mx = original_coords.max(axis=0)
center = (mn + mx) * 0.5
height = float(mx[2] - mn[2])

# Import the actual animation rig from the same Mixamo clip files used by the
# game. This preserves bone names, hierarchy, local axes, roll, and rest pose.
before_import = set(bpy.context.scene.objects)
bpy.ops.import_scene.fbx(filepath=donor_motion, automatic_bone_orientation=False)
donor_arms = [o for o in bpy.context.scene.objects if o.type == 'ARMATURE' and o not in before_import]
if len(donor_arms) != 1:
    raise RuntimeError(f'Expected one donor armature, got {len(donor_arms)}')
armature = donor_arms[0]

deform_bones = [b for b in armature.data.bones if b.use_deform and 'End' not in b.name]
if not deform_bones:
    raise RuntimeError('Donor animation FBX has no deform bones')
bone_min_z = min((armature.matrix_world @ b.head_local).z for b in deform_bones)
bone_max_z = max((armature.matrix_world @ b.head_local).z for b in deform_bones)
bone_height = bone_max_z - bone_min_z
if bone_height <= 0:
    raise RuntimeError('Donor skeleton has invalid height')

# Scale and position the donor armature as an object. Its FBX rotation, unit
# scale, rest bones, and imported action stay intact, so the motion channels
# continue to use the exact local basis in which they were authored.
vertical_scale = height / bone_height
armature.scale = tuple(value * vertical_scale for value in armature.scale)
armature.location = (float(center[0]), float(center[1]), float(mn[2] - bone_min_z * vertical_scale))
bpy.context.view_layer.update()

# The sculpt is already posed with its arms lowered. Weight against the bones in
# the matching idle pose, not the donor rig's T-pose; otherwise the arm shells
# are assigned to torso/legs and tear when the game applies a sword-and-shield
# walk cycle. This still uses discrete nearest bones for hard armor, with a
# narrow blend only across adjacent joints.
bones = [b for b in armature.data.bones if b.use_deform and 'End' not in b.name]
bone_names = [b.name for b in bones]
bone_index = {name: i for i, name in enumerate(bone_names)}
action = armature.animation_data.action if armature.animation_data else None
if action is None:
    raise RuntimeError('The donor idle FBX must contain an action for pose-aware weighting')
action_slot = armature.animation_data.action_slot
first_frame = int(round(action.frame_range[0]))
bpy.context.scene.frame_set(first_frame)
bpy.context.view_layer.update()

# Match the same partial idle pose used by CCharacterAnimator for both weight
# discovery and inverse binding: upper-body clip tracks plus the hand grip,
# while hips, legs, head and all other joints remain at their skeleton rest pose.
idle_animation_bones = {
    'mixamorig:Spine', 'mixamorig:Spine1', 'mixamorig:Spine2',
    'mixamorig:LeftArm', 'mixamorig:LeftForeArm',
    'mixamorig:RightArm', 'mixamorig:RightForeArm',
}
sampled_basis = {
    name: armature.pose.bones[name].matrix_basis.copy()
    for name in idle_animation_bones
    if armature.pose.bones.get(name) is not None
}
armature.animation_data.action = None
for pose_bone in armature.pose.bones:
    pose_bone.matrix_basis = Matrix.Identity(4)
for name, matrix_basis in sampled_basis.items():
    armature.pose.bones[name].matrix_basis = matrix_basis
for finger, angles in {
    'Index': (-0.85, -1.05, -0.80),
    'Middle': (-0.90, -1.10, -0.85),
    'Ring': (-0.90, -1.10, -0.85),
    'Pinky': (-0.95, -1.15, -0.90),
}.items():
    for segment, angle in enumerate(angles, start=1):
        pose_bone = armature.pose.bones.get(f'mixamorig:RightHand{finger}{segment}')
        if pose_bone is not None:
            pose_bone.matrix_basis = Matrix.Rotation(angle, 4, 'X')
thumb = armature.pose.bones.get('mixamorig:RightHandThumb1')
if thumb is not None:
    thumb.matrix_basis = Matrix.Rotation(0.45, 4, 'Z') @ Matrix.Rotation(-0.55, 4, 'X')
bpy.context.view_layer.update()

endpoints = np.asarray([
    [
        tuple(armature.matrix_world @ armature.pose.bones[b.name].head),
        tuple(armature.matrix_world @ armature.pose.bones[b.name].tail),
    ]
    for b in bones
], dtype=np.float32)
heads = endpoints[:, 0, :]
vectors = endpoints[:, 1, :] - heads
length_sq = np.maximum(np.sum(vectors * vectors, axis=1), 1e-10)
coords = np.asarray([
    tuple(mesh.matrix_world @ Vector(vertex)) for vertex in original_coords
], dtype=np.float32)

distances = np.empty((len(coords), len(bones)), dtype=np.float32)
for i in range(len(bones)):
    rel = coords - heads[i]
    t = np.clip((rel @ vectors[i]) / length_sq[i], 0.0, 1.0)
    closest = heads[i] + t[:, None] * vectors[i]
    delta = coords - closest
    distances[:, i] = np.sum(delta * delta, axis=1)

nearest = np.argmin(distances, axis=1)

nearest_distance = np.sqrt(distances[np.arange(len(coords)), nearest])
mesh.vertex_groups.clear()
groups = {name: mesh.vertex_groups.new(name=name) for name in bone_names}
threshold = max(height * 0.025, 0.003)

# Assign each vertex to its closest bone segment. Blend only with that bone's
# immediate parent/children near joints; unrelated distant bones never tug on
# armor plates, the cape, or the sword.
for i, bone in enumerate(bones):
    vertex_ids = np.flatnonzero(nearest == i)
    if not len(vertex_ids):
        continue
    parent_index = bone_index.get(bone.parent.name) if bone.parent else None
    child_indices = [bone_index[c.name] for c in bone.children if c.name in bone_index]
    neighbors = ([parent_index] if parent_index is not None else []) + child_indices
    if not neighbors:
        groups[bone.name].add(vertex_ids.tolist(), 1.0, 'REPLACE')
        continue
    candidate_d = distances[np.ix_(vertex_ids, neighbors)]
    candidate_slot = np.argmin(candidate_d, axis=1)
    second_distance = np.sqrt(candidate_d[np.arange(len(vertex_ids)), candidate_slot])
    first_distance = nearest_distance[vertex_ids]
    blendable = (second_distance - first_distance) < threshold
    groups[bone.name].add(vertex_ids[~blendable].tolist(), 1.0, 'REPLACE')
    if np.any(blendable):
        blend_ids = vertex_ids[blendable]
        adjacent_bone_indices = np.asarray(neighbors, dtype=np.int32)[candidate_slot[blendable]]
        d0 = np.maximum(first_distance[blendable], 1e-5)
        d1 = np.maximum(second_distance[blendable], 1e-5)
        w0 = d1 / (d0 + d1)
        w1 = 1.0 - w0
        w0_bins = np.clip(np.round(w0 * 20.0) / 20.0, 0.05, 0.95)
        for weight in np.unique(w0_bins):
            mask = w0_bins == weight
            groups[bone.name].add(blend_ids[mask].tolist(), float(weight), 'REPLACE')
        for adjacent in np.unique(adjacent_bone_indices):
            mask = adjacent_bone_indices == adjacent
            ids = blend_ids[mask]
            weights = np.clip(np.round(w1[mask] * 20.0) / 20.0, 0.05, 0.95)
            for weight in np.unique(weights):
                weight_mask = weights == weight
                groups[bone_names[int(adjacent)]].add(
                    ids[weight_mask].tolist(), float(weight), 'REPLACE')

# Give each elbow a deliberate upper-arm/forearm transition.
# A spherical falloff mostly affects vertices near the center of the arm and
# leaves the outer armor shell rigidly attached to one side. Instead, create a
# short capsule across the joint: the blend varies along the arm axis while
# covering the full cross-section of the elbow armor.
elbow_blend_counts = {}
for side in ('Left', 'Right'):
    upper_name = f'mixamorig:{side}Arm'
    lower_name = f'mixamorig:{side}ForeArm'
    upper_index = bone_index[upper_name]
    lower_index = bone_index[lower_name]
    transition_width = max(height * 0.035, 0.01)
    cross_section_radius = max(height * 0.07, 0.02)

    upper_t = np.clip(
        ((coords - heads[upper_index]) @ vectors[upper_index]) /
        length_sq[upper_index], 0.0, 1.0)
    upper_axial_distance = (
        (1.0 - upper_t) * np.sqrt(length_sq[upper_index]))
    upper_candidates = np.flatnonzero(
        (nearest == upper_index) &
        (upper_axial_distance < transition_width) &
        (distances[:, upper_index] < cross_section_radius ** 2))

    lower_t = np.clip(
        ((coords - heads[lower_index]) @ vectors[lower_index]) /
        length_sq[lower_index], 0.0, 1.0)
    lower_axial_distance = lower_t * np.sqrt(length_sq[lower_index])
    lower_candidates = np.flatnonzero(
        (nearest == lower_index) &
        (lower_axial_distance < transition_width) &
        (distances[:, lower_index] < cross_section_radius ** 2))
    ids = np.concatenate((upper_candidates, lower_candidates))
    if not len(ids):
        elbow_blend_counts[side] = 0
        continue

    for group in groups.values():
        group.remove(ids.tolist())

    axial_distance = np.where(
        nearest[ids] == upper_index,
        upper_axial_distance[ids],
        lower_axial_distance[ids])
    fade = np.clip(1.0 - axial_distance / transition_width, 0.0, 1.0)
    smooth_fade = fade * fade * (3.0 - 2.0 * fade)
    secondary_weights = 0.5 * smooth_fade
    primary_indices = nearest[ids]
    for primary_index in (upper_index, lower_index):
        primary_mask = primary_indices == primary_index
        for weight in np.unique(np.round(secondary_weights[primary_mask], 3)):
            weight_mask = primary_mask & (np.round(secondary_weights, 3) == weight)
            selected_ids = ids[weight_mask]
            if weight <= 0.0:
                groups[bone_names[primary_index]].add(
                    selected_ids.tolist(), 1.0, 'REPLACE')
                continue
            groups[bone_names[primary_index]].add(
                selected_ids.tolist(), float(1.0 - weight), 'REPLACE')
            secondary_index = lower_index if primary_index == upper_index else upper_index
            groups[bone_names[secondary_index]].add(
                selected_ids.tolist(), float(weight), 'REPLACE')
    elbow_blend_counts[side] = len(ids)

print('ELBOW_BLEND_VERTICES:', elbow_blend_counts)

# Inverse-skin each weighted vertex from the exact game idle baseline pose so
# the original sculpt's relaxed silhouette is preserved without detached arms.
if action is not None:
    armature_world = np.asarray(armature.matrix_world, dtype=np.float64)
    inverse_armature_world = np.linalg.inv(armature_world)
    bone_skin_matrices = np.repeat(np.eye(4, dtype=np.float64)[None, :, :], len(bone_names), axis=0)
    for i, bone in enumerate(bones):
        pose_bone = armature.pose.bones.get(bone.name)
        if pose_bone is None:
            continue
        rest = np.asarray(bone.matrix_local, dtype=np.float64)
        pose = np.asarray(pose_bone.matrix, dtype=np.float64)
        bone_skin_matrices[i] = armature_world @ pose @ np.linalg.inv(rest) @ inverse_armature_world

    influence_indices = np.zeros((len(coords), 4), dtype=np.int32)
    influence_weights = np.zeros((len(coords), 4), dtype=np.float64)
    for vertex_index, vertex in enumerate(mesh.data.vertices):
        for slot, assignment in enumerate(vertex.groups):
            if slot >= 4:
                break
            influence_indices[vertex_index, slot] = assignment.group
            influence_weights[vertex_index, slot] = assignment.weight
    weight_sums = influence_weights.sum(axis=1)
    if np.any(weight_sums < 0.999):
        raise RuntimeError('Skin weights must sum to 1 before idle-pose rebinding')

    blended_skin = np.zeros((len(coords), 4, 4), dtype=np.float64)
    for slot in range(4):
        blended_skin += (
            influence_weights[:, slot, None, None] *
            bone_skin_matrices[influence_indices[:, slot]])
    # The per-vertex blend is normally rigid or close to rigid; reject singular
    # matrices instead of exporting NaNs from an unstable inverse.
    determinants = np.linalg.det(blended_skin)
    if np.any(np.abs(determinants) < 1e-8):
        raise RuntimeError('Idle-pose skin matrix is singular for some vertices')
    inverse_skin = np.linalg.inv(blended_skin)
    mesh_world = np.asarray(mesh.matrix_world, dtype=np.float64)
    homogeneous_world = np.concatenate(
        [original_coords.astype(np.float64), np.ones((len(coords), 1), dtype=np.float64)], axis=1)
    original_world = homogeneous_world @ mesh_world.T
    corrected_world = np.einsum('nij,nj->ni', inverse_skin, original_world)
    corrected_mesh = corrected_world @ np.linalg.inv(mesh_world).T
    mesh.data.vertices.foreach_set('co', corrected_mesh[:, :3].astype(np.float32).ravel())
    mesh.data.update()

modifier = mesh.modifiers.new('Mixamo Deformation', 'ARMATURE')
modifier.object = armature
mesh.parent = armature
mesh.matrix_parent_inverse = armature.matrix_world.inverted()
bpy.context.view_layer.update()

if action is not None:
    bpy.context.scene.frame_set(first_frame)
    bpy.context.view_layer.update()
    evaluated_mesh = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
    evaluated_data = evaluated_mesh.to_mesh()
    deformed_world = np.asarray([
        tuple(evaluated_mesh.matrix_world @ vertex.co) for vertex in evaluated_data.vertices
    ], dtype=np.float64)
    evaluated_mesh.to_mesh_clear()
    bind_error = np.linalg.norm(deformed_world - original_world[:, :3], axis=1)
    rms_error = float(np.sqrt(np.mean(bind_error * bind_error)))
    max_error = float(np.max(bind_error))
    print('GAME_IDLE_BIND_FRAME:', first_frame)
    print('GAME_IDLE_BIND_RMS_ERROR:', rms_error)
    print('GAME_IDLE_BIND_MAX_ERROR:', max_error)
    if not np.isfinite(rms_error) or rms_error > height * 0.01:
        raise RuntimeError('Idle-pose rebind failed to preserve the original silhouette')

    # Keep the complete imported idle action available in the editable project;
    # the runtime GLB itself only needs the corrected rest geometry and rig.
    armature.animation_data.action = action
    armature.animation_data.action_slot = action_slot
    bpy.context.scene.frame_set(first_frame)
    bpy.context.view_layer.update()

# Check coverage and limit every vertex to no more than two influences.
unweighted = sum(1 for v in mesh.data.vertices if not v.groups)
if unweighted:
    raise RuntimeError(f'{unweighted} vertices have no skin weights')

# Reduce triangles on a copy used by the game. Preserve the editable full mesh.
runtime_mesh = mesh.copy()
runtime_mesh.data = mesh.data.copy()
runtime_mesh.name = 'FallenPaladin_Runtime'
bpy.context.collection.objects.link(runtime_mesh)
runtime_mesh.parent = armature
runtime_mesh.matrix_world = mesh.matrix_world.copy()
for src_group in mesh.vertex_groups:
    if runtime_mesh.vertex_groups.get(src_group.name) is None:
        runtime_mesh.vertex_groups.new(name=src_group.name)
decimate = runtime_mesh.modifiers.new('Runtime polygon reduction', 'DECIMATE')
decimate.decimate_type = 'COLLAPSE'
decimate.ratio = 0.20
decimate.use_collapse_triangulate = True
while runtime_mesh.modifiers.find(decimate.name) > 0:
    bpy.context.view_layer.objects.active = runtime_mesh
    bpy.ops.object.modifier_move_up(modifier=decimate.name)
bpy.context.view_layer.objects.active = runtime_mesh
bpy.ops.object.modifier_apply(modifier=decimate.name)

# Decimation can merge vertices whose neighborhoods contain different pairs of
# bones. Keep the four strongest influences supported by the game's vertex
# format, removing the weakest extra groups before the GLB is written.
remove_by_group = [[] for _ in runtime_mesh.vertex_groups]
for vertex in runtime_mesh.data.vertices:
    if len(vertex.groups) <= 4:
        continue
    ordered = sorted(vertex.groups, key=lambda item: item.weight, reverse=True)
    for influence in ordered[4:]:
        remove_by_group[influence.group].append(vertex.index)
for group_index, vertex_ids in enumerate(remove_by_group):
    if vertex_ids:
        runtime_mesh.vertex_groups[group_index].remove(vertex_ids)
max_influences = max(len(vertex.groups) for vertex in runtime_mesh.data.vertices)
if max_influences > 4:
    raise RuntimeError(f'Runtime mesh still has {max_influences} influences on a vertex')

# Keep the full rigged mesh in the editable .blend, and export the optimized
# copy with the exact motion-compatible armature for runtime.
bpy.ops.object.select_all(action='DESELECT')
armature.select_set(True)
runtime_mesh.select_set(True)
bpy.context.view_layer.objects.active = armature
bpy.ops.export_scene.gltf(
    filepath=out_glb,
    export_format='GLB',
    use_selection=True,
    export_apply=True,
    export_yup=True,
    export_animations=False,
)
bpy.ops.export_scene.fbx(
    filepath=out_fbx,
    use_selection=True,
    object_types={'ARMATURE', 'MESH'},
    add_leaf_bones=False,
    bake_anim=False,
    path_mode='COPY',
    embed_textures=False,
    axis_forward='-Z',
    axis_up='Y',
    apply_unit_scale=True,
)

# Editable source includes the full resolution skinned mesh and the optimized
# runtime variant. Hide the latter in the viewport to avoid z-fighting.
runtime_mesh.hide_set(True)
runtime_mesh.hide_render = True
bpy.ops.wm.save_as_mainfile(filepath=out_blend)
mesh.hide_set(True)
mesh.hide_render = True
runtime_mesh.hide_set(False)
runtime_mesh.hide_render = False
bpy.ops.wm.save_as_mainfile(filepath=out_runtime_blend)
print('RUNTIME_VERTICES:', len(runtime_mesh.data.vertices))
print('SOURCE_VERTICES:', len(mesh.data.vertices))
print('BONES:', len(bones))
print('UNWEIGHTED:', unweighted)
print('MAX_RUNTIME_INFLUENCES:', max_influences)
print('GLB:', out_glb)
print('FBX:', out_fbx)
