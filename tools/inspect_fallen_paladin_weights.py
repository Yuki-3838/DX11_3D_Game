import bpy
import json
import sys
from collections import defaultdict

bpy.ops.wm.open_mainfile(filepath=sys.argv[-1])
result = []
for obj in bpy.context.scene.objects:
    if obj.type != 'MESH' or obj.name != 'tripo_node_3786d797':
        continue
    groups = {group.index: group.name for group in obj.vertex_groups}
    accum = defaultdict(lambda: {'count': 0, 'min': [float('inf')] * 3, 'max': [float('-inf')] * 3, 'weight_sum': 0.0})
    for vertex in obj.data.vertices:
        p = obj.matrix_world @ vertex.co
        for assignment in vertex.groups:
            entry = accum[groups[assignment.group]]
            entry['count'] += 1
            entry['weight_sum'] += assignment.weight
            for axis in range(3):
                entry['min'][axis] = min(entry['min'][axis], p[axis])
                entry['max'][axis] = max(entry['max'][axis], p[axis])
    result.append({name: {**entry, 'weight_avg': entry['weight_sum'] / max(entry['count'], 1)} for name, entry in accum.items()})
print(json.dumps(result, ensure_ascii=False))
