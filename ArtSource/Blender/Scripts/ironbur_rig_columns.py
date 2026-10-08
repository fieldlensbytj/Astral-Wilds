# Traces Ironbur's four leg columns upward from each paw: at every height
# slice, the median x/y of vertices within R of the previous centre (medians,
# not extremes, so armour shards and claws don't pull the line).
import bpy, numpy as np
OUT = r"C:/Users/camer/AppData/Local/Temp/claude/C--Users-camer-Astral-Wilds-Unreal/1e711230-8ff1-40b6-9ce4-b6f135d55c6f/scratchpad/rig/ironbur"
bpy.ops.wm.open_mainfile(filepath=OUT + "/ironbur_aligned.blend")
ob = bpy.data.objects["Ironbur"]
V = np.array([tuple(v.co) for v in ob.data.vertices])
SEEDS = {"fl": (0.55, -0.75), "fr": (-0.58, -0.80), "bl": (0.58, 0.65), "br": (-0.25, 0.80)}
L = []
for k, c in SEEDS.items():
    c = np.array(c); L.append("== %s" % k)
    for z in np.arange(0.02, 1.0, 0.06):
        m = (np.abs(V[:, 2] - z) < 0.03) & (np.linalg.norm(V[:, :2] - c, axis=1) < 0.22)
        if m.sum() < 5:
            L.append("z=%.2f  (none)" % z); continue
        p = V[m]; c = np.median(p[:, :2], axis=0)
        L.append("z=%.2f  med (%.2f, %.2f)  n=%d  x[%.2f,%.2f] y[%.2f,%.2f]" % (z, c[0], c[1], m.sum(), np.percentile(p[:,0],10), np.percentile(p[:,0],90), np.percentile(p[:,1],10), np.percentile(p[:,1],90)))
# lowest points per quadrant (paw contact)
for k, c in SEEDS.items():
    m = np.linalg.norm(V[:, :2] - np.array(c), axis=1) < 0.3
    L.append("%s lowest z %.3f; 5th pct z %.3f" % (k, V[m, 2].min(), np.percentile(V[m, 2], 5)))
# spine: per y slice, median x and top/bottom of the body above the legs
for y in np.arange(-1.4, 1.45, 0.1):
    m = (np.abs(V[:, 1] - y) < 0.05) & (V[:, 2] > 0.35)
    if m.sum(): p = V[m]; L.append("y=%+.1f  mid x %.2f  z %.2f..%.2f (p10 %.2f p90 %.2f)" % (y, np.median(p[:,0]), p[:,2].min(), p[:,2].max(), np.percentile(p[:,2],10), np.percentile(p[:,2],90)))
open(OUT + "/columns.txt", "w").write("\n".join(L))
