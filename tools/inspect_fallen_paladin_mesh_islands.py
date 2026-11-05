import bpy
import os
import sys

args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
bpy.ops.wm.open_mainfile(filepath=os.path.abspath(args[0]))
source = bpy.data.objects['tripo_node_3786d797']
mesh = source.copy()
mesh.data = source.data.copy()
mesh.name = 'IslandInspectionCopy'
bpy.context.collection.objects.link(mesh)
bpy.ops.object.select_all(action='DESELECT')
mesh.select_set(True)
bpy.context.view_layer.objects.active = mesh
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.mesh.separate(type='LOOSE')
bpy.ops.object.mode_set(mode='OBJECT')

islands = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH' and obj != source]
rows = []
for obj in islands:
    if not obj.data.vertices:
        continue
    coords = [obj.matrix_world @ vertex.co for vertex in obj.data.vertices]
    rows.append((
        len(obj.data.vertices),
        obj.name,
        tuple(round(min(point[i] for point in coords), 3) for i in range(3)),
        tuple(round(max(point[i] for point in coords), 3) for i in range(3)),
    ))
rows.sort(reverse=True)
print('ISLAND_COUNT:', len(rows))
print('LARGEST_ISLANDS:', rows[:40])
