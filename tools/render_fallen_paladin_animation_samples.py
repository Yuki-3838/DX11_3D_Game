import bpy
import os
import sys
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
asset = os.path.abspath(args[0])
clip_path = os.path.abspath(args[1]) if len(args) > 1 else None
bpy.ops.wm.open_mainfile(filepath=asset)
scene = bpy.context.scene
armature = next(obj for obj in scene.objects if obj.type == 'ARMATURE')
action = armature.animation_data.action
action_slot = armature.animation_data.action_slot
if clip_path:
    existing_objects = set(scene.objects)
    bpy.ops.import_scene.fbx(filepath=clip_path, automatic_bone_orientation=False)
    clip_objects = {obj for obj in scene.objects if obj not in existing_objects}
    clip_armature = next(
        obj for obj in scene.objects
        if obj.type == 'ARMATURE' and obj in clip_objects)
    action = clip_armature.animation_data.action
    action_slot = clip_armature.animation_data.action_slot
    armature.animation_data.action = action
    armature.animation_data.action_slot = action_slot
    for obj in scene.objects:
        if obj != armature and obj.type in {'ARMATURE', 'MESH'}:
            obj.hide_set(True)
            obj.hide_render = True
start, end = (int(round(value)) for value in action.frame_range)
frames = sorted(set(round(start + (end - start) * fraction) for fraction in (0.0, 0.25, 0.5, 0.75, 1.0)))
idle_animation_bones = {
    'mixamorig:Spine', 'mixamorig:Spine1', 'mixamorig:Spine2',
    'mixamorig:LeftArm', 'mixamorig:LeftForeArm',
    'mixamorig:RightArm', 'mixamorig:RightForeArm',
}
driven_bones = set(idle_animation_bones)
clip_name = os.path.basename(clip_path).lower() if clip_path else ''
if 'walk' in clip_name or 'run' in clip_name:
    driven_bones.update({
        'mixamorig:LeftUpLeg', 'mixamorig:RightUpLeg',
        'mixamorig:LeftLeg', 'mixamorig:RightLeg',
        'mixamorig:LeftFoot', 'mixamorig:RightFoot',
    })
elif clip_path and any(token in clip_name for token in ('attack', 'slash', 'block')):
    excluded_tokens = (
        'finger', 'index', 'middle', 'ring', 'pinky', 'thumb',
        'sword', 'weapon', 'leg', 'thigh', 'knee', 'calf',
        'foot', 'ankle', 'toe',
    )
    driven_bones = {
        bone.name for bone in armature.data.bones
        if bone.name.lower().replace('_', '').replace('-', '').replace(' ', '')
        not in {'root', 'mixamorig:root', 'hips', 'mixamorig:hips', 'pelvis'}
        and not any(token in bone.name.lower() for token in excluded_tokens)
    }

def apply_game_pose(frame):
    # Match GameScene's clip-specific bone mask and right-hand grip pose.
    armature.animation_data.action = action
    armature.animation_data.action_slot = action_slot
    scene.frame_set(frame)
    bpy.context.view_layer.update()
    sampled = {
        name: armature.pose.bones[name].matrix_basis.copy()
        for name in driven_bones
        if armature.pose.bones.get(name) is not None
    }
    armature.animation_data.action = None
    for pose_bone in armature.pose.bones:
        pose_bone.matrix_basis = Matrix.Identity(4)
    for name, basis in sampled.items():
        armature.pose.bones[name].matrix_basis = basis
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

for obj in scene.objects:
    if obj.type == 'MESH':
        if clip_path and obj in clip_objects:
            obj.hide_set(True)
            obj.hide_render = True
            continue
        is_runtime = obj.name == 'FallenPaladin_Runtime'
        obj.hide_set(not is_runtime)
        obj.hide_render = not is_runtime

apply_game_pose(start)
meshes = [obj for obj in scene.objects if obj.type == 'MESH' and not obj.hide_render]
depsgraph = bpy.context.evaluated_depsgraph_get()
points = []
for obj in meshes:
    evaluated = obj.evaluated_get(depsgraph)
    evaluated_mesh = evaluated.to_mesh()
    points.extend(evaluated.matrix_world @ vertex.co for vertex in evaluated_mesh.vertices)
    evaluated.to_mesh_clear()
mn = Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points)))
mx = Vector((max(p.x for p in points), max(p.y for p in points), max(p.z for p in points)))
center = (mn + mx) * 0.5
radius = max((mx - mn).length * 0.65, 1.0)

world = bpy.data.worlds.new('AnimationSampleWorld')
scene.world = world
world.use_nodes = True
world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.015, 0.02, 0.03, 1.0)
world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.25

def area(name, location, energy, size):
    data = bpy.data.lights.new(name, 'AREA')
    data.energy = energy
    data.shape = 'DISK'
    data.size = size
    obj = bpy.data.objects.new(name, data)
    scene.collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler = (center - obj.location).to_track_quat('-Z', 'Y').to_euler()

area('SampleKey', (2.0, -3.0, 2.2), 850, 4)
area('SampleFill', (-2.0, -1.5, 1.4), 500, 3)
area('SampleRim', (0.0, 2.5, 2.0), 700, 3)
camera_data = bpy.data.cameras.new('AnimationSampleCamera')
camera = bpy.data.objects.new('AnimationSampleCamera', camera_data)
scene.collection.objects.link(camera)
camera_data.type = 'ORTHO'
camera_data.ortho_scale = max(mx.x - mn.x, mx.y - mn.y, mx.z - mn.z) * 1.25
scene.camera = camera
scene.render.engine = 'BLENDER_EEVEE_NEXT'
scene.render.resolution_x = 600
scene.render.resolution_y = 600
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'

clip_label = 'idle'
if clip_path:
    clip_label = os.path.splitext(os.path.basename(clip_path))[0]
    clip_label = clip_label.lower().replace('sword and shield ', '').replace(' ', '_')
out_dir = os.path.join(os.path.dirname(asset), 'preview_animation', 'samples', clip_label)
os.makedirs(out_dir, exist_ok=True)
for frame in frames:
    apply_game_pose(frame)
    camera.location = (center.x, mx.y + radius, center.z)
    camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
    scene.render.filepath = os.path.join(out_dir, f'frame_{frame:03d}.png')
    bpy.ops.render.render(write_still=True)
print('SAMPLE_FRAMES:', frames)
print('OUTPUT:', out_dir)
