import bpy, os

lines = []
for o in bpy.data.objects:
    if o.type == 'MESH':
        me = o.data
        lines.append("%s: verts=%d polys=%d dims=%s materials=%s uv=%s" % (
            o.name, len(me.vertices), len(me.polygons), tuple(o.dimensions),
            [m.name if m else None for m in me.materials],
            [uv.name for uv in me.uv_layers],
        ))

lines.append("IMAGES: %s" % [(i.name, i.size[0], i.size[1], i.filepath) for i in bpy.data.images])
lines.append("MATERIALS: %s" % [m.name for m in bpy.data.materials])
for m in bpy.data.materials:
    if m and m.use_nodes:
        img_nodes = [n.image.name for n in m.node_tree.nodes if n.type == 'TEX_IMAGE' and n.image]
        lines.append("  material %s image_nodes=%s" % (m.name, img_nodes))

report = "\n".join(lines)
print(report)
with open(r"C:\Users\camer\Astral Wilds\ArtSource\Blender\Scripts\_source_inspect.txt", "w") as f:
    f.write(report)
