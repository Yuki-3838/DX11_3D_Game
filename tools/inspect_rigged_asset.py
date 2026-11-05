import bpy
import json
import sys

bpy.ops.wm.open_mainfile(filepath=sys.argv[-1])
result = []
for obj in bpy.context.scene.objects:
    if obj.type == 'MESH':
        influence_counts = [len(v.groups) for v in obj.data.vertices]
        result.append({
            'name': obj.name,
            'parent': obj.parent.name if obj.parent else None,
            'hide_render': obj.hide_render,
            'hide_viewport': obj.hide_viewport,
            'hide_get': obj.hide_get(),
            'parent_type': obj.parent_type,
            'modifiers': [(m.name, m.type, getattr(m, 'object', None).name if getattr(m, 'object', None) else None) for m in obj.modifiers],
            'vertex_groups': len(obj.vertex_groups),
            'vertices': len(obj.data.vertices),
            'max_vertex_influences': max(influence_counts, default=0),
            'average_vertex_influences': sum(influence_counts) / max(len(influence_counts), 1),
        })
    elif obj.type == 'ARMATURE':
        result.append({'name': obj.name, 'type': 'ARMATURE', 'bones': [b.name for b in obj.data.bones]})
print(json.dumps(result, ensure_ascii=False))
