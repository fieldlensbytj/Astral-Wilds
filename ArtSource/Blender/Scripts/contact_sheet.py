# Tiles numbered PNG frames (e.g. Saved/AutoPlaytest/motion from Astral.MotionCapture) into one contact sheet.
# blender -b --python contact_sheet.py -- <dir> <first> <count> <cols> <out.png> [crop_frac] [step]
import bpy, sys, numpy as np
a = sys.argv[sys.argv.index("--") + 1:]
d, first, count, cols, out = a[0], int(a[1]), int(a[2]), int(a[3]), a[4]
crop = float(a[5]) if len(a) > 5 else 1.0
tiles = []
for i in range(first, first + count):
    img = bpy.data.images.load("%s/%03d.png" % (d, i))
    w, h = img.size
    px = np.array(img.pixels[:]).reshape(h, w, 4)
    cw, ch = int(w * crop), int(h * crop)
    px = px[(h - ch) // 2:(h - ch) // 2 + ch, (w - cw) // 2:(w - cw) // 2 + cw]
    step = int(a[6]) if len(a) > 6 else 2
    tiles.append(px[::step, ::step])
    bpy.data.images.remove(img)
rows = [np.concatenate(tiles[r * cols:(r + 1) * cols], axis=1) for r in range(len(tiles) // cols)]
sheet = np.concatenate(rows[::-1], axis=0)
im = bpy.data.images.new("s", sheet.shape[1], sheet.shape[0], alpha=True)
im.pixels[:] = sheet.ravel(); im.filepath_raw = out; im.file_format = 'PNG'; im.save()
