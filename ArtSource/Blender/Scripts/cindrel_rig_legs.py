import bpy, mathutils, random
OUT = r"C:/Users/camer/AppData/Local/Temp/claude/C--Users-camer-Astral-Wilds-Unreal/1f45ae34-9634-4dd9-ac1b-deb9025c0437/scratchpad/rig"
bpy.ops.wm.open_mainfile(filepath=OUT + "/cindrel_aligned.blend")
ob = [o for o in bpy.context.scene.objects if o.type == 'MESH'][0]
vs = [v.co.copy() for v in ob.data.vertices]
V = mathutils.Vector
def clusters2d(pts, r=0.07):
    # single-linkage clustering by radius (grid accelerated)
    pts = list(pts); lab = [-1] * len(pts); cur = 0
    grid = {}
    for i, p in enumerate(pts): grid.setdefault((int(p.x // r), int(p.y // r)), []).append(i)
    for i in range(len(pts)):
        if lab[i] >= 0: continue
        stack = [i]; lab[i] = cur
        while stack:
            j = stack.pop(); p = pts[j]
            gx, gy = int(p.x // r), int(p.y // r)
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    for k in grid.get((gx + dx, gy + dy), []):
                        if lab[k] < 0 and (pts[k] - p).length < r: lab[k] = cur; stack.append(k)
        cur += 1
    out = []
    for c in range(cur):
        m = [pts[i] for i in range(len(pts)) if lab[i] == c]
        if len(m) >= 8: out.append((sum(m, V((0, 0))) / len(m), len(m), min(p.x for p in m), max(p.x for p in m), min(p.y for p in m), max(p.y for p in m)))
    return sorted(out, key=lambda c: (c[0].y, c[0].x))
L = []
for h in [0.03, 0.10, 0.18, 0.26, 0.34, 0.42, 0.50]:
    sl = [v.xy.copy() for v in vs if abs(v.z - h) < 0.025 and v.y < 0.45]
    cs = clusters2d(sl)
    L.append("z=%.2f: " % h + " | ".join("(%.2f,%.2f) n=%d x[%.2f,%.2f] y[%.2f,%.2f]" % (c[0].x, c[0].y, c[1], c[2], c[3], c[4], c[5]) for c in cs))
open(OUT + "/legs.txt", "w").write("\n".join(L))
