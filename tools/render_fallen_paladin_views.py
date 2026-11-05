import bpy
import os
import sys
from mathutils import Vector

fbx_path = sys.argv[-1]
out_dir = os.path.abspath(os.path.join(os.path.dirname(fbx_path), "blender_preview"))
os.makedirs(out_dir, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx_path, automatic_bone_orientation=False)

meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
points = [o.matrix_world @ v.co for o in meshes for v in o.data.vertices]
mn = Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points)))
mx = Vector((max(p.x for p in points), max(p.y for p in points), max(p.z for p in points)))
center = (mn + mx) * 0.5
radius = max((mx - mn).length * 0.65, 1.0)

world = bpy.context.scene.world
if world is None:
    world = bpy.data.worlds.new('PreviewWorld')
    bpy.context.scene.world = world
world.color = (0.04, 0.04, 0.04)
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.015, 0.02, 0.03, 1.0)
world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.25

def add_area(name, location, energy, size):
    data = bpy.data.lights.new(name, 'AREA')
    data.energy = energy
    data.shape = 'DISK'
    data.size = size
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    obj.location = location
    direction = center - obj.location
    obj.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()

add_area('key', (2.0, -3.0, 2.2), 850.0, 4.0)
add_area('fill', (-2.0, -1.5, 1.4), 500.0, 3.0)
add_area('rim', (0.0, 2.5, 2.0), 700.0, 3.0)

cam_data = bpy.data.cameras.new('Camera')
cam = bpy.data.objects.new('Camera', cam_data)
bpy.context.collection.objects.link(cam)
bpy.context.scene.camera = cam
cam_data.type = 'ORTHO'
cam_data.ortho_scale = max(mx.z - mn.z, mx.x - mn.x, mx.y - mn.y) * 1.25

views = {
    'front_y': (center.x, mn.y - radius, center.z),
    'back_y': (center.x, mx.y + radius, center.z),
    'right_x': (mx.x + radius, center.y, center.z),
    'left_x': (mn.x - radius, center.y, center.z),
    'top_z': (center.x, center.y, mx.z + radius),
}

scene = bpy.context.scene
scene.render.engine = 'BLENDER_EEVEE_NEXT'
scene.render.resolution_x = 700
scene.render.resolution_y = 700
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.film_transparent = False

for name, location in views.items():
    cam.location = location
    cam.rotation_euler = (center - cam.location).to_track_quat('-Z', 'Y').to_euler()
    scene.render.filepath = os.path.join(out_dir, name + '.png')
    bpy.ops.render.render(write_still=True)
print(out_dir)
