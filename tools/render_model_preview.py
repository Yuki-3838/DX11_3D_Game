import bpy
import os
import sys
from mathutils import Vector

asset = os.path.abspath(sys.argv[-1])
out_dir = os.path.join(os.path.dirname(asset), 'preview_animation' if asset.lower().endswith('.blend') else 'preview')
os.makedirs(out_dir, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
if asset.lower().endswith('.blend'):
    bpy.ops.wm.open_mainfile(filepath=asset)
elif asset.lower().endswith('.fbx'):
    bpy.ops.import_scene.fbx(filepath=asset, automatic_bone_orientation=False)
else:
    bpy.ops.import_scene.gltf(filepath=asset)
if asset.lower().endswith('.blend'):
    bpy.context.scene.frame_set(1)
    # Use the full-resolution editable mesh when comparing skinning quality;
    # the runtime decimated copy remains the one exported to the game.
    for obj in bpy.context.scene.objects:
        if obj.type == 'MESH':
            is_runtime = obj.name == 'FallenPaladin_Runtime'
            obj.hide_set(is_runtime)
            obj.hide_render = is_runtime

meshes = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH' and not obj.hide_render]
depsgraph = bpy.context.evaluated_depsgraph_get()
points = []
for obj in meshes:
    evaluated = obj.evaluated_get(depsgraph)
    evaluated_mesh = evaluated.to_mesh()
    points.extend(evaluated.matrix_world @ vert.co for vert in evaluated_mesh.vertices)
    evaluated.to_mesh_clear()
mn = Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points)))
mx = Vector((max(p.x for p in points), max(p.y for p in points), max(p.z for p in points)))
center = (mn + mx) * 0.5
radius = max((mx - mn).length * 0.65, 1.0)
world = bpy.data.worlds.new('PreviewWorld')
bpy.context.scene.world = world
world.use_nodes = True
world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.015, 0.02, 0.03, 1.0)
world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.25

def area(name, location, energy, size):
    data = bpy.data.lights.new(name, 'AREA')
    data.energy = energy
    data.shape = 'DISK'
    data.size = size
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler = (center - obj.location).to_track_quat('-Z', 'Y').to_euler()

area('Key', (2.0, -3.0, 2.2), 850, 4)
area('Fill', (-2.0, -1.5, 1.4), 500, 3)
area('Rim', (0.0, 2.5, 2.0), 700, 3)
camera_data = bpy.data.cameras.new('Camera')
camera = bpy.data.objects.new('Camera', camera_data)
bpy.context.collection.objects.link(camera)
camera_data.type = 'ORTHO'
camera_data.ortho_scale = max(mx.x-mn.x, mx.y-mn.y, mx.z-mn.z) * 1.25
bpy.context.scene.camera = camera
bpy.context.scene.render.engine = 'BLENDER_EEVEE_NEXT'
bpy.context.scene.render.resolution_x = 700
bpy.context.scene.render.resolution_y = 700
bpy.context.scene.render.resolution_percentage = 100
bpy.context.scene.render.image_settings.file_format = 'PNG'
views = {
    'front': (center.x, mx.y+radius, center.z),
    'side': (mx.x+radius, center.y, center.z),
}
for name, location in views.items():
    camera.location = location
    camera.rotation_euler = (center-camera.location).to_track_quat('-Z','Y').to_euler()
    bpy.context.scene.render.filepath = os.path.join(out_dir, name+'.png')
    bpy.ops.render.render(write_still=True)
print(out_dir)
