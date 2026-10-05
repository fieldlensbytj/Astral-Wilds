import bpy, os

report_path = os.path.join(os.path.dirname(bpy.data.filepath), "..", "..", "ArtSource", "Blender", "Scripts", "_inspect_report.txt")
report_path = os.path.abspath(report_path)

lines = []
lines.append("Blend file: %s" % bpy.data.filepath)
lines.append("Objects:")
for obj in bpy.data.objects:
    lines.append("  - name=%r type=%s" % (obj.name, obj.type))
    if obj.type == 'MESH':
        me = obj.data
        lines.append("      verts=%d polys=%d uv_layers=%s materials=%s" % (
            len(me.vertices), len(me.polygons),
            [uv.name for uv in me.uv_layers],
            [m.name if m else None for m in me.data.materials] if hasattr(me, 'data') else None
        ))
        lines.append("      dims=%s location=%s" % (tuple(obj.dimensions), tuple(obj.location)))

lines.append("Materials in file: %s" % [m.name for m in bpy.data.materials])
lines.append("Collections: %s" % [c.name for c in bpy.data.collections])

with open(report_path, "w") as f:
    f.write("\n".join(lines))

print("INSPECT_DONE")
print("\n".join(lines))
