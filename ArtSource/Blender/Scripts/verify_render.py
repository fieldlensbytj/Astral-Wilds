import bpy
import os

PROJECT_ROOT = r"C:\Users\camer\Astral Wilds"
VERIFY_PATH = os.path.join(PROJECT_ROOT, "ArtSource", "Blender", "Previews", "Cindrel_Retopo_v03_ISOLATED.png")

log_lines = []
def log(m):
    print(m)
    log_lines.append(str(m))

new_obj = bpy.data.objects["Cindrel_Retopo_v03"]
log("Cindrel_Retopo_v03: verts=%d polys=%d materials=%s" % (
    len(new_obj.data.vertices), len(new_obj.data.polygons), [m.name for m in new_obj.data.materials]))

# Hide everything else from render.
for o in bpy.data.objects:
    if o.type in ('MESH',) and o.name != "Cindrel_Retopo_v03":
        o.hide_render = True
        log("Hid from render: %s" % o.name)
    elif o.name == "Cindrel_Retopo_v03":
        o.hide_render = False

scene = bpy.context.scene
scene.render.filepath = VERIFY_PATH
scene.render.image_settings.file_format = 'PNG'
scene.render.resolution_x = 1024
scene.render.resolution_y = 1024
bpy.ops.render.render(write_still=True)
log("Rendered isolated verification: %s" % VERIFY_PATH)

with open(os.path.join(PROJECT_ROOT, "ArtSource", "Blender", "Scripts", "_verify_report.txt"), "w") as f:
    f.write("\n".join(log_lines))
