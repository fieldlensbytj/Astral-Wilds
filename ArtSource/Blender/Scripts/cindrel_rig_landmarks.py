import bpy, mathutils, random
OUT = r"C:/Users/camer/AppData/Local/Temp/claude/C--Users-camer-Astral-Wilds-Unreal/1f45ae34-9634-4dd9-ac1b-deb9025c0437/scratchpad/rig"
bpy.ops.wm.open_mainfile(filepath=OUT + "/cindrel_aligned.blend")
ob = [o for o in bpy.context.scene.objects if o.type == 'MESH'][0]
vs = [v.co.copy() for v in ob.data.vertices]
V = mathutils.Vector
def kmeans(pts, k, restarts=20):
    best = None
    for r in range(restarts):
        random.seed(r); c = random.sample(pts, k)
        for _ in range(50):
            g = [[] for _ in range(k)]
            for p in pts: g[min(range(k), key=lambda i: (p - c[i]).length_squared)].append(p)
            c = [sum(gg, V((0, 0))) / len(gg) if gg else c[i] for i, gg in enumerate(g)]
        sse = sum(min((p - ci).length_squared for ci in c) for p in pts)
        if best is None or sse < best[0]: best = (sse, c, [len(gg) for gg in g])
    return best
L = []
low = [v.xy.copy() for v in vs if v.z < 0.12]
sse, paws, ns = kmeans(low, 4)
paws = sorted(zip(paws, ns), key=lambda t: t[0].y)
for p, n in paws: L.append("paw (%.3f, %.3f) n=%d  %s" % (p.x, p.y, n, "FRONT" if p.y < 0 else "BACK"))
# lowest z of verts near each paw (raised paw sits higher)
for p, n in paws:
    near = [v for v in vs if (v.xy - p).length < 0.08]
    L.append("  paw near-verts min z %.3f" % min(v.z for v in near))
# torso cross-sections along Y
for y in [x / 10 for x in range(-8, 9, 2)]:
    sl = [v for v in vs if abs(v.y - y) < 0.05 and 0.25 < v.z < 1.0]
    if sl:
        L.append("slice y=%+.1f: x %.2f..%.2f (mid %.2f)  z %.2f..%.2f" % (y, min(v.x for v in sl), max(v.x for v in sl), (min(v.x for v in sl)+max(v.x for v in sl))/2, min(v.z for v in sl), max(v.z for v in sl)))
# head: verts y < -0.5 and z > 0.85
hd = [v for v in vs if v.y < -0.45 and 0.85 < v.z < 1.45]
c = sum(hd, V((0,0,0))) / len(hd)
L.append("head region centroid (%.3f, %.3f, %.3f), snout min y %.3f" % (c.x, c.y, c.z, min(v.y for v in hd)))
# tail: verts y > 0.35, z > 0.5
tl = [v for v in vs if v.y > 0.35 and v.z > 0.45]
L.append("tail region: n=%d y %.2f..%.2f z %.2f..%.2f x %.2f..%.2f" % (len(tl), min(v.y for v in tl), max(v.y for v in tl), min(v.z for v in tl), max(v.z for v in tl), min(v.x for v in tl), max(v.x for v in tl)))
top = max(tl, key=lambda v: v.z); L.append("tail top vert (%.3f, %.3f, %.3f)" % tuple(top))
open(OUT + "/landmarks.txt", "w").write("\n".join(L))
