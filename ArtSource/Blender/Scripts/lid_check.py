# Eyelid check: renders a rigged Astral's rest-pose face, lit, with the lids
# open (top row) and shut (bottom) from the front and 45/70 deg to each side.
#
#   blender -b --python lid_check.py -- <Name> <out.png> <cx> <cz> <ortho_scale>
#
# (cx, cz) is the point between the eyes, as in astral_add_eyelids.py's EYES:
# Cindrel 0.04 1.08 0.45, Ironbur -0.06 0.87 0.6, Ripplefin 0.003 1.05 0.45,
# Mossling -0.19 1.165 0.4, Glacielle -0.10 1.232 0.35.
import bpy, sys, math, numpy as np
from mathutils import Vector
a = sys.argv[sys.argv.index("--") + 1:]
NAME, OUT, CX, CZ, SC = a[0], a[1], float(a[2]), float(a[3]), float(a[4])
bpy.ops.wm.open_mainfile(filepath=r"C:/Users/camer/Astral Wilds/ArtSource/Blender/Rigs/%s_Rigged.blend" % NAME)
scn = bpy.context.scene
arm = [o for o in scn.objects if o.type == 'ARMATURE'][0]
mesh = [o for o in scn.objects if o.type == 'MESH'][0]
if arm.animation_data:
    arm.animation_data.action = None
for p in arm.pose.bones:
    p.rotation_quaternion = (1, 0, 0, 0); p.location = (0, 0, 0)
scn.render.engine = 'BLENDER_WORKBENCH'; scn.display.shading.color_type = 'TEXTURE'; scn.display.shading.light = 'STUDIO'
R = 400; scn.render.resolution_x = scn.render.resolution_y = R
cd = bpy.data.cameras.new("c"); cd.type = 'ORTHO'; cd.ortho_scale = SC
cam = bpy.data.objects.new("c", cd); scn.collection.objects.link(cam); scn.camera = cam
target = Vector((CX, 0.0, CZ))
# Put the target's y on the face surface.
ok, loc, *_ = mesh.ray_cast(Vector((CX, -5, CZ)), Vector((0, 1, 0)))
if ok:
    target.y = loc.y
rows = []
for val in (0.0, 1.0):
    mesh.data.shape_keys.key_blocks["Blink"].value = val
    tiles = []
    for ang in (-70, -45, 0, 45, 70):
        t = math.radians(ang)
        d = Vector((math.sin(t), -math.cos(t), 0.0))
        cam.location = target + d * 3.0
        cam.rotation_euler = (math.radians(90), 0, t)
        scn.render.filepath = OUT + "_tmp.png"; bpy.ops.render.render(write_still=True)
        im = bpy.data.images.load(OUT + "_tmp.png"); tiles.append(np.array(im.pixels[:]).reshape(R, R, 4)); bpy.data.images.remove(im)
    rows.append(np.concatenate(tiles, axis=1))
sheet = np.concatenate(rows[::-1], axis=0)   # image rows are bottom-up: open on top
o = bpy.data.images.new("s", sheet.shape[1], sheet.shape[0], alpha=True); o.pixels[:] = sheet.ravel()
o.filepath_raw = OUT; o.file_format = 'PNG'; o.save()
