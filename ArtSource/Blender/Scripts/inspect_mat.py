import bpy

lines = []
for mat_name in ("Material_0.001", "Material_0"):
    mat = bpy.data.materials.get(mat_name)
    if not mat or not mat.use_nodes:
        continue
    lines.append("=== %s ===" % mat_name)
    bsdf = None
    for n in mat.node_tree.nodes:
        if n.type == 'BSDF_PRINCIPLED':
            bsdf = n
    if bsdf:
        for inp in bsdf.inputs:
            if inp.is_linked:
                src = inp.links[0].from_node
                img = getattr(src, "image", None)
                lines.append("  BSDF input %r <- node %s (type=%s) image=%s" % (
                    inp.name, src.name, src.type, img.name if img else None))
    for n in mat.node_tree.nodes:
        if n.type == 'TEX_IMAGE' and n.image:
            lines.append("  image node %s -> image %s colorspace=%s size=%s" % (
                n.name, n.image.name, n.image.colorspace_settings.name, tuple(n.image.size)))

report = "\n".join(lines)
print(report)
with open(r"C:\Users\camer\Astral Wilds\ArtSource\Blender\Scripts\_mat_inspect.txt", "w") as f:
    f.write(report)
