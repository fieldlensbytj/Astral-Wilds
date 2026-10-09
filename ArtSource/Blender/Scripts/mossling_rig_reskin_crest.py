# Re-skins Mossling's antlers and leaf crest (TJ, 2026-10-09: "mossling has
# weird stretching"). The rig pinned every vertex above HEAD_Z to the head,
# so leaves crossing that height were split between head and body and
# stretched into long sheets whenever the head turned, and the crest lying
# back over the spine swung out with the head. Now each leaf (mesh island)
# moves as one rigid piece: ones by the head follow the head, the crest
# further back follows the neck and spine, blending over ~30cm, with the
# weights blurred over neighbouring leaves so they stay together. The body
# below and the lids keep their weights. Run on Rigs/Mossling_Rigged.blend,
# then mossling_rig_gait_style.py and quadruped_reanimate.py (re-exports the FBX):
#
#   blender -b --python mossling_rig_reskin_crest.py
import bpy, math, numpy as np

RIGS = r"C:/Users/camer/Astral Wilds/ArtSource/Blender/Rigs"
HEAD_Z = 1.05   # as in mossling_rig_rig_mossling.py
bpy.ops.wm.open_mainfile(filepath=RIGS + "/Mossling_Rigged.blend")
arm = [o for o in bpy.data.objects if o.type == 'ARMATURE'][0]
mesh = [o for o in bpy.data.objects if o.type == 'MESH' and o.find_armature() == arm][0]
names = [g.name for g in mesh.vertex_groups]
gi = {n: i for i, n in enumerate(names)}
nv = len(mesh.data.vertices)
V = np.array([tuple(v.co) for v in mesh.data.vertices])
W = np.zeros((nv, len(names)))
for v in mesh.data.vertices:
    for e in v.groups:
        W[v.index, e.group] = e.weight

# Lids (moved by the Blink shape key) stay wholly on the head.
keys = mesh.data.shape_keys.key_blocks if mesh.data.shape_keys else None
lid = np.zeros(nv, bool)
if keys and "Blink" in keys:
    base = np.array([d.co[:] for d in keys[0].data]); bl = np.array([d.co[:] for d in keys["Blink"].data])
    lid = np.linalg.norm(bl - base, axis=1) > 1e-5

# Mesh islands = leaves / shards.
pr = np.arange(nv)
def find(a):
    while pr[a] != a:
        pr[a] = pr[pr[a]]; a = pr[a]
    return a
for e in mesh.data.edges:
    a, b = find(e.vertices[0]), find(e.vertices[1])
    if a != b:
        pr[a] = b
inv = np.unique([find(i) for i in range(nv)], return_inverse=True)[1]
cnt = np.bincount(inv)
P = np.zeros((inv.max() + 1, 3)); np.add.at(P, inv, V); P /= cnt[:, None]
ztop = np.zeros(len(P)) - 9; np.maximum.at(ztop, inv, V[:, 2])

def sstep(a, b, x):
    k = np.clip((x - a) / (b - a), 0, 1); return k * k * (3 - 2 * k)

head = arm.data.bones["head"]
yh = head.head_local.y                       # the back of the skull
spine = [n for n in ("neck", "chest", "spine_01", "pelvis") if n in gi]
H = np.array([tuple(arm.data.bones[n].head_local) for n in spine]); T = np.array([tuple(arm.data.bones[n].tail_local) for n in spine])

# Pieces that reach above HEAD_Z: the antlers, the crest and anything the old pin cut through.
top = (ztop > HEAD_Z - 0.02)
Wp = np.zeros((len(P), len(names)))
for pi in np.where(top)[0]:
    c = P[pi]
    h = 1.0 - sstep(yh - 0.05, yh + 0.30, c[1])          # by the head -> 1, back over the spine -> 0
    seg = T - H
    t = np.clip(((c - H) * seg).sum(1) / (seg ** 2).sum(1), 0, 1)
    d = np.linalg.norm(c - (H + t[:, None] * seg), axis=1)
    ws = 1.0 / np.maximum(d, 0.02) ** 4; ws /= ws.sum()
    Wp[pi, gi["head"]] = h
    for n, w in zip(spine, ws):
        Wp[pi, gi[n]] += (1 - h) * w

# Blur over neighbouring leaves (5cm Gaussian on a 2cm grid), then one weight
# set per leaf, so it moves rigidly and its neighbours move almost the same.
sel = top[inv] & ~lid
F = Wp[inv]
cell, sig = 0.02, 2.5
lo = V.min(0) - 4 * cell
idx = np.floor((V - lo) / cell).astype(int); shape = tuple(idx.max(0) + 1)
flat = np.ravel_multi_index(idx.T, shape)
r = int(math.ceil(3 * sig)); xk = np.arange(-r, r + 1); g = np.exp(-0.5 * (xk / sig) ** 2); g /= g.sum()
def blur(grid):
    for ax in range(3):
        grid = np.apply_along_axis(lambda m: np.convolve(m, g, mode="same"), ax, grid)
    return grid
m = np.zeros(nv); m[sel] = 1
den = blur(np.bincount(flat, weights=m, minlength=np.prod(shape)).reshape(shape)).ravel()[flat]
Fb = np.zeros_like(F)
for j in range(F.shape[1]):
    if not F[sel, j].any():
        continue
    num = blur(np.bincount(flat, weights=F[:, j] * m, minlength=np.prod(shape)).reshape(shape)).ravel()[flat]
    Fb[:, j] = num / np.maximum(den, 1e-9)
# Neck leaves (any neck weight) stretched into streaks too, being weighted
# per vertex across neck and chest: give them the same treatment, from
# their current weights blurred.
neck_p = np.zeros(len(P)); np.add.at(neck_p, inv, W[:, gi["neck"]]); neck_p /= cnt
neckp = (neck_p > 0.05) & ~top
nsel = neckp[inv] & ~lid
m2 = np.zeros(nv); m2[nsel] = 1
den2 = blur(np.bincount(flat, weights=m2, minlength=np.prod(shape)).reshape(shape)).ravel()[flat]
for j in range(W.shape[1]):
    if not W[nsel, j].any():
        continue
    num = blur(np.bincount(flat, weights=W[:, j] * m2, minlength=np.prod(shape)).reshape(shape)).ravel()[flat]
    Fb[nsel, j] = (num / np.maximum(den2, 1e-9))[nsel]
sel = sel | nsel
Fp = np.zeros_like(Wp); np.add.at(Fp, inv, Fb); Fp /= cnt[:, None]
Fp /= np.maximum(Fp.sum(1, keepdims=True), 1e-9)
new = Fp[inv]

changed = np.where(sel)[0]
for vi in changed:
    w = new[vi]
    order = np.argsort(-w)[:4]
    w4 = w[order]; w4 = w4 / max(w4.sum(), 1e-9)
    for n in names:
        mesh.vertex_groups[n].remove([int(vi)])
    for j, wj in zip(order, w4):
        if wj > 0.01:
            mesh.vertex_groups[names[j]].add([int(vi)], float(wj), 'REPLACE')
print("[reskin] %d leaves above HEAD_Z and %d neck leaves re-skinned (%d verts); %d lid verts kept on the head" % (int(top.sum()), int(neckp.sum()), len(changed), int(lid.sum())))
bpy.ops.wm.save_as_mainfile(filepath=RIGS + "/Mossling_Rigged.blend")
