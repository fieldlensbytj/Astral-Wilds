# Side-view strip of one action from a rigged Astral, for reviewing a gait
# frame by frame. Read-only: never saves the .blend.
#
#   blender -b --python gait_strip.py -- <Rig.blend> <Action> <frames> <out.png> [rows]
import bpy, math, sys
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
BLEND, ACT, COUNT, OUT = argv[0], argv[1], int(argv[2]), argv[3]
ROWS = int(argv[4]) if len(argv) > 4 else 1
bpy.ops.wm.open_mainfile(filepath=BLEND)
scn = bpy.context.scene
arm = [o for o in scn.objects if o.type == 'ARMATURE'][0]
arm.animation_data.action = bpy.data.actions[ACT]
start, end = (int(v) for v in arm.animation_data.action.frame_range)
N = end - start

scn.render.engine = 'BLENDER_WORKBENCH'
scn.display.shading.color_type = 'TEXTURE'
scn.display.shading.light = 'STUDIO'
R = 420
scn.render.resolution_x = scn.render.resolution_y = R
cam = scn.camera
if cam is None:
    cd = bpy.data.cameras.new("c")
    cam = bpy.data.objects.new("c", cd); scn.collection.objects.link(cam); scn.camera = cam
mesh = [o for o in scn.objects if o.type == 'MESH'][0]
cam.data.type = 'ORTHO'; cam.data.ortho_scale = max(2.4, mesh.dimensions.y + 0.4)   # Ironbur is ~2.8m long
cam.location = Vector((5, 0, 0.95)); cam.rotation_euler = (math.radians(90), 0, math.radians(90))

def px(p):
    """Armature-space point -> (row from bottom, col) in a tile. Screen right is +Y, up is +Z."""
    s = R / cam.data.ortho_scale
    return R / 2 + (p.z - cam.location.z) * s, R / 2 + (p.y - cam.location.y) * s

def line(tile, a, b, rgb):
    (r0, c0), (r1, c1) = px(a), px(b)
    for t in np.linspace(0, 1, 200):
        r, c = int(r0 + (r1 - r0) * t), int(c0 + (c1 - c0) * t)
        tile[max(r - 1, 0):r + 2, max(c - 1, 0):c + 2, :3] = rgb

# Leg bones drawn over the render (hind red, front green; near side brighter),
# plus a ground line, so joint angles read through fur and shards.
LEGS = {"bl": (1, .2, .2), "br": (.55, .1, .1), "fl": (.2, 1, .2), "fr": (.1, .5, .1)}
tiles = []
for i in range(COUNT):
    scn.frame_set(start + round(i * N / COUNT))
    scn.render.filepath = OUT + "_tmp.png"
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.load(OUT + "_tmp.png")
    tile = np.array(img.pixels[:]).reshape(R, R, 4)
    bpy.data.images.remove(img)
    tile[int(px(Vector((0, 0, 0)))[0]), :, :3] = .5
    for k, rgb in LEGS.items():
        for bn in ("_upper", "_lower", "_foot"):
            p = arm.pose.bones[k + bn]
            line(tile, arm.matrix_world @ p.head, arm.matrix_world @ p.tail, rgb)
    tiles.append(tile)
cols = math.ceil(COUNT / ROWS)
blank = np.ones((R, R, 4))
rows = []
for r in range(ROWS):
    row = tiles[r * cols:(r + 1) * cols]
    row += [blank] * (cols - len(row))
    rows.append(np.concatenate(row, axis=1))
sheet = np.concatenate(rows[::-1], axis=0)              # Blender pixels are bottom-up
out = bpy.data.images.new("sheet", sheet.shape[1], sheet.shape[0], alpha=True)
out.pixels[:] = sheet.ravel()
out.filepath_raw = OUT
out.file_format = 'PNG'
out.save()
