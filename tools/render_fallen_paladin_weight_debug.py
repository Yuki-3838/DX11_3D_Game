import bpy
import os
import sys
import numpy as np
from mathutils import Vector

args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
blend_path = os.path.abspath(args[0])
output_dir = os.path.abspath(args[1])
bpy.ops.wm.open_mainfile(filepath=blend_path)
scene = bpy.context.scene
armature = next(obj for obj in scene.objects if obj.type == 'ARMATURE')
mesh = bpy.data.objects['tripo_node_3786d797']
mesh.hide_render = False
mesh.hide_set(False)
runtime = bpy.data.objects.get('FallenPaladin_Runtime')
if runtime:
    runtime.hide_render = True
    runtime.hide_set(True)
for obj in scene.objects:
    if obj.type == 'MESH' and obj != mesh and obj != runtime:
        obj.hide_render = True
        obj.hide_set(True)

channel_groups = {
    'left': {'mixamorig:LeftArm', 'mixamorig:LeftForeArm'},
    'right': {'mixamorig:RightArm', 'mixamorig:RightForeArm'},
    'torso': {'mixamorig:Spine', 'mixamorig:Spine1', 'mixamorig:Spine2'},
}
group_names = {group.index: group.name for group in mesh.vertex_groups}
attribute = mesh.data.color_attributes.get('RigInfluenceDebug')
if attribute:
    mesh.data.color_attributes.remove(attribute)
attribute = mesh.data.color_attributes.new(
    name='RigInfluenceDebug', type='FLOAT_COLOR', domain='POINT')
colors = np.zeros((len(mesh.data.vertices), 4), dtype=np.float32)
for vertex in mesh.data.vertices:
    influences = {key: 0.0 for key in channel_groups}
    for assignment in vertex.groups:
        group = group_names[assignment.group]
        for channel, names in channel_groups.items():
            if group in names:
                influences[channel] += assignment.weight
    colors[vertex.index, 0] = influences['left']
    colors[vertex.index, 1] = influences['right']
    colors[vertex.index, 2] = influences['torso']
    colors[vertex.index, 3] = 1.0
attribute.data.foreach_set('color', colors.ravel())

material = bpy.data.materials.new('Rig influence debug')
material.use_nodes = True
nodes = material.node_tree.nodes
nodes.clear()
output = nodes.new('ShaderNodeOutputMaterial')
emission = nodes.new('ShaderNodeEmission')
color = nodes.new('ShaderNodeVertexColor')
color.layer_name = attribute.name
material.node_tree.links.new(color.outputs['Color'], emission.inputs['Color'])
material.node_tree.links.new(emission.outputs['Emission'], output.inputs['Surface'])
mesh.data.materials.clear()
mesh.data.materials.append(material)

depsgraph = bpy.context.evaluated_depsgraph_get()
evaluated = mesh.evaluated_get(depsgraph)
evaluated_mesh = evaluated.to_mesh()
points = [evaluated.matrix_world @ vertex.co for vertex in evaluated_mesh.vertices]
evaluated.to_mesh_clear()
minimum = Vector(tuple(min(point[i] for point in points) for i in range(3)))
maximum = Vector(tuple(max(point[i] for point in points) for i in range(3)))
center = (minimum + maximum) * 0.5
radius = max((maximum - minimum).length * 0.65, 1.0)

camera_data = bpy.data.cameras.new('Rig debug camera')
camera_data.type = 'ORTHO'
camera_data.ortho_scale = max(maximum.x-minimum.x, maximum.y-minimum.y, maximum.z-minimum.z) * 1.2
camera = bpy.data.objects.new('Rig debug camera', camera_data)
scene.collection.objects.link(camera)
scene.camera = camera
scene.render.engine = 'BLENDER_EEVEE_NEXT'
scene.render.resolution_x = 800
scene.render.resolution_y = 800
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.view_settings.view_transform = 'Standard'
os.makedirs(output_dir, exist_ok=True)

for label, direction in (('front', Vector((0.0, 1.0, 0.0))), ('back', Vector((0.0, -1.0, 0.0)))):
    camera.location = center + direction * radius
    camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
    scene.render.filepath = os.path.join(output_dir, f'{label}.png')
    bpy.ops.render.render(write_still=True)
    print('OUTPUT:', scene.render.filepath)
