# Refits Glacielle's front legs on Rigs/Glacielle_Rigged.blend, then re-skins.
# Run before quadruped_reanimate.py (which re-authors the clips on the new bones).
#
# Why (TJ, 2026-10-05: "Glacielle's front legs aren't really animating properly"):
# - The first rig put each front toe at the furthest-forward vertex on that
#   side, which on Glacielle is a fur shard ~13cm ahead of the paw. That made
#   26cm horizontal foot bones, and the paws smeared forward when they moved.
# - The two front legs touch, so distance weighting gave each leg's inner fur
#   partly to the other leg's bones, and the legs dragged together.
# Bones now follow per-side leg columns measured by height slices (median /
# 10th-percentile, not min, so shards don't count). Below the chest, leg bones
# never share a vertex between the two legs of a pair (see the skinning note).
import bpy, numpy as np
from mathutils import Vector

RIG = r"C:/Users/camer/Astral Wilds/ArtSource/Blender/Rigs/Glacielle_Rigged.blend"
bpy.ops.wm.open_mainfile(filepath=RIG)
scn = bpy.context.scene
arm = [o for o in scn.objects if o.type == 'ARMATURE'][0]
mesh = [o for o in scn.objects if o.type == 'MESH'][0]

# Measured per side (x<0 = right): shoulder, elbow, wrist, toe.
FRONT = {
    "fr": [(-0.16, -0.44, 0.64), (-0.13, -0.47, 0.40), (-0.08, -0.50, 0.13), (-0.075, -0.64, 0.02)],
    "fl": [(0.13, -0.40, 0.64), (0.10, -0.37, 0.40), (0.08, -0.35, 0.13), (0.075, -0.52, 0.02)],
}
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='EDIT')
eb = arm.data.edit_bones
for k, (sh, el, wr, toe) in FRONT.items():
    for name, h, t in (("upper", sh, el), ("lower", el, wr), ("foot", wr, toe)):
        b = eb["%s_%s" % (k, name)]
        b.use_connect = False
        b.head, b.tail, b.roll = Vector(h), Vector(t), 0.0
    eb["%s_lower" % k].use_connect = True
    eb["%s_foot" % k].use_connect = True
bpy.ops.object.mode_set(mode='OBJECT')

# ---- re-skin: same distance weighting as the rig script, plus side masks ----
bones = arm.data.bones
deform = [b for b in bones if b.use_deform]
heads = np.array([tuple(b.head_local) for b in deform]); tails = np.array([tuple(b.tail_local) for b in deform])
V = np.array([tuple(v.co) for v in mesh.data.vertices])
seg = tails - heads
seglen2 = np.maximum((seg ** 2).sum(1), 1e-9)
D = np.empty((len(V), len(deform)))
for i in range(len(deform)):
    t = np.clip(((V - heads[i]) @ seg[i]) / seglen2[i], 0.0, 1.0)
    D[:, i] = np.sqrt(((V - (heads[i] + t[:, None] * seg[i])) ** 2).sum(1))
# 4th power over the top 4 bones: softer joint blends than the first rig's
# 6th power / top 3, so elbows and wrists bend rather than crease.
W = 1.0 / (np.maximum(D, 0.01) ** 4)
names = [b.name for b in deform]

# Upper leg bones fade out from just below their shoulder/hip to 8cm above,
# so the chest and haunch fur rides the body instead of swinging with the
# leg (the first refit let fl_upper/fr_upper reach most of the chest side;
# fading from 12cm below opened a gap under the chest when the legs swung).
def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)
for i, b in enumerate(deform):
    if b.name.endswith("_upper"):
        W[:, i] *= 1.0 - smoothstep(b.head_local.z - 0.02, b.head_local.z + 0.08, V[:, 2])

# Glacielle's mesh has no continuous skin: it is ~2,400 overlapping ice-shard
# pieces (the largest is 62 vertices). Weights stay smooth per vertex, so
# neighbouring pieces move together; making each piece rigid (or copying a
# "skin" weight) was tried and separated the pieces. The one decision made
# per piece is which leg of a pair it belongs to: below the chest a piece
# goes wholly to whichever leg's bone chain is nearer on average, and gets no
# weight from the other leg. Per vertex, that split pieces between legs and
# they flickered; a midline plane cut the left paw (the paws sit 15cm apart
# in depth) and tore it into sheets.
parent = np.arange(len(V))
def find(i):
    while parent[i] != i:
        parent[i] = parent[parent[i]]
        i = parent[i]
    return i
for e in mesh.data.edges:
    a, b = find(e.vertices[0]), find(e.vertices[1])
    if a != b:
        parent[a] = b
roots = np.array([find(i) for i in range(len(V))])
_, island, sizes = np.unique(roots, return_inverse=True, return_counts=True)
def piece_mean(x):
    s = np.zeros(len(sizes)); np.add.at(s, island, x)
    return (s / sizes)[island]
for pair, ztop in (("f", 0.56), ("b", 0.50)):
    li = [i for i, n in enumerate(names) if n[:2] == pair + "l"]
    ri = [i for i, n in enumerate(names) if n[:2] == pair + "r"]
    low = piece_mean(V[:, 2]) < ztop
    left_nearer = piece_mean(D[:, li].min(1)) < piece_mean(D[:, ri].min(1))
    W[np.ix_(low & left_nearer, ri)] = 0.0
    W[np.ix_(low & ~left_nearer, li)] = 0.0
print("refit: %d pieces (largest %d verts); leg sides decided per piece" % (len(sizes), sizes.max()))

top = np.argsort(-W, axis=1)[:, :4]
mesh.vertex_groups.clear()
groups = [mesh.vertex_groups.new(name=b.name) for b in deform]
for vi in range(len(V)):
    idx = top[vi]; w = W[vi, idx]
    if w.sum() <= 0:
        continue
    w = w / w.sum()
    for j, wj in zip(idx, w):
        if wj > 0.01:
            groups[j].add([vi], float(wj), 'REPLACE')

# Decimation fused the touching front paws, so some triangles join the two
# legs and stretch into spikes whatever the weights. They sit where the legs
# press together, so delete them.
import bmesh
leg_of = {}
for vi in range(len(V)):
    n = names[top[vi, 0]]
    if n[:1] in "fb" and n[1:2] in "lr" and V[vi, 2] < 0.56:
        leg_of[vi] = n[:2]
bm = bmesh.new(); bm.from_mesh(mesh.data)
bridges = [f for f in bm.faces if len({leg_of.get(v.index) for v in f.verts} - {None}) > 1]
bmesh.ops.delete(bm, geom=bridges, context='FACES_ONLY')
loose = [v for v in bm.verts if not v.link_faces]
bmesh.ops.delete(bm, geom=loose, context='VERTS')
bm.to_mesh(mesh.data); bm.free()
print("refit: deleted %d leg-bridging faces, %d loose verts" % (len(bridges), len(loose)))
unweighted = sum(1 for v in mesh.data.vertices if not any(g.weight > 0.01 for g in v.groups))
print("refit: unweighted %d / %d; front verts %s" % (unweighted, len(mesh.data.vertices), {b.name: int((top[:, 0] == i).sum()) for i, b in enumerate(deform) if b.name[0] == "f"}))
bpy.ops.wm.save_as_mainfile(filepath=RIG)
