# Rigs and skins the decimated, aligned Ironbur (head toward -Y, feet on z=0)
# and saves Rigs/Ironbur_Rigged.blend. The clips come from
# quadruped_reanimate.py, so this script authors no animation.
#
#   blender -b --python ironbur_rig_rig_ironbur.py
#
# Same 22-bone skeleton and naming as Glacielle/Mossling. Landmarks come from
# ironbur/columns.txt (ironbur_rig_columns.py: per-leg median columns traced up
# from each paw) and the side view. Ironbur was modelled mid-stride: the right
# fore paw is lifted (~14cm) and reaches forward. Its bones follow the model
# as is, and the armature's "plant_fr" property tells quadruped_reanimate.py
# where that paw stands, mirrored from the left fore paw.
#
# Skinning is the Glacielle refit recipe: distance to bone segments (4th
# power, top 4), upper leg bones fade out above the shoulder/hip, and below
# the body each mesh piece goes wholly to one leg of a pair. The Meshy mesh
# is mostly one big piece, so pieces above PIECE_MAX verts are split per
# vertex instead.
import bpy, bmesh, numpy as np
from mathutils import Vector
OUT = r"C:/Users/camer/AppData/Local/Temp/claude/C--Users-camer-Astral-Wilds-Unreal/1e711230-8ff1-40b6-9ce4-b6f135d55c6f/scratchpad/rig/ironbur"
RIG = r"C:/Users/camer/Astral Wilds/ArtSource/Blender/Rigs/Ironbur_Rigged.blend"
bpy.ops.wm.open_mainfile(filepath=OUT + "/ironbur_aligned.blend")
scn = bpy.context.scene
mesh = bpy.data.objects["Ironbur"]
for o in list(scn.objects):
    if o.type == 'ARMATURE': bpy.data.objects.remove(o, do_unlink=True)
log = []

# ---------------- skeleton ----------------
CX = 0.03
B = {  # name: (head, tail, parent)
    "root":      ((CX, 0.05, 0.0), (CX, -0.15, 0.0), None),
    "pelvis":    ((CX, 0.70, 0.90), (CX, 0.25, 0.93), "root"),
    "spine_01":  ((CX, 0.25, 0.93), (CX, -0.25, 0.96), "pelvis"),
    "chest":     ((CX, -0.25, 0.96), (CX, -0.68, 0.92), "spine_01"),
    "neck":      ((CX, -0.68, 0.92), (0.0, -0.95, 0.88), "chest"),
    "head":      ((0.0, -0.95, 0.88), (-0.03, -1.40, 0.80), "neck"),
    # The tail runs back off the rump, then curls up and over (tip at y 1.16, z 1.43).
    "tail_01":   ((CX, 0.80, 0.90), (0.04, 1.05, 0.95), "pelvis"),
    "tail_02":   ((0.04, 1.05, 0.95), (0.05, 1.28, 1.03), "tail_01"),
    "tail_03":   ((0.05, 1.28, 1.03), (0.05, 1.36, 1.20), "tail_02"),
    "tail_04":   ((0.05, 1.36, 1.20), (0.04, 1.20, 1.42), "tail_03"),
}
# shoulder/hip, elbow/stifle, wrist/hock, toe. Right is -X (facing -Y).
# Fore legs lean forward-down onto big clawed paws (elbow sits back of the
# shoulder-wrist line). Hind legs are short stubs under the haunch, hock
# behind the paw, stifle forward.
LEGS = {
    "fl": [(0.48, -0.20, 0.84), (0.52, -0.35, 0.42), (0.58, -0.56, 0.14), (0.55, -0.84, 0.02), "chest"],
    "fr": [(-0.46, -0.25, 0.84), (-0.47, -0.48, 0.46), (-0.59, -0.74, 0.19), (-0.60, -1.02, 0.14), "chest"],
    "bl": [(0.42, 0.66, 0.76), (0.56, 0.56, 0.46), (0.60, 0.72, 0.19), (0.60, 0.50, 0.02), "pelvis"],
    "br": [(-0.12, 0.80, 0.76), (-0.26, 0.70, 0.46), (-0.29, 0.89, 0.19), (-0.25, 0.68, 0.02), "pelvis"],
}
for k, (a_, kn, an, toe, par) in LEGS.items():
    B["%s_upper" % k] = (a_, kn, par)
    B["%s_lower" % k] = (kn, an, "%s_upper" % k)
    B["%s_foot" % k] = (an, toe, "%s_lower" % k)

arm_data = bpy.data.armatures.new("IronburRig")
arm = bpy.data.objects.new("IronburRig", arm_data)
scn.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='EDIT')
eb = {}
for name, (h, t, par) in B.items():
    b = arm_data.edit_bones.new(name)
    b.head, b.tail, b.roll = Vector(h), Vector(t), 0.0
    eb[name] = b
for name, (h, t, par) in B.items():
    if par:
        eb[name].parent = eb[par]
        eb[name].use_connect = (Vector(eb[par].tail) - Vector(h)).length < 1e-4
eb["root"].use_deform = False
bpy.ops.object.mode_set(mode='OBJECT')
# Where the lifted right fore paw stands: the left fore toe mirrored about the midline.
fl_toe = Vector(LEGS["fl"][3])
arm["plant_fr"] = (2 * CX - fl_toe.x, fl_toe.y, fl_toe.z)
# The fore paws stand ~0.36m ahead of the shoulders: quadruped_reanimate.py
# pulls their stance back until the stride fits the legs' reach.
arm["fit_stance"] = True
log.append("bones: %d; plant_fr %s" % (len(arm_data.bones), tuple(round(c, 3) for c in arm["plant_fr"])))

# ---------------- skinning ----------------
deform = [b for b in arm_data.bones if b.use_deform]
names = [b.name for b in deform]
heads = np.array([tuple(b.head_local) for b in deform]); tails = np.array([tuple(b.tail_local) for b in deform])
V = np.array([tuple(v.co) for v in mesh.data.vertices])
seg = tails - heads
seglen2 = np.maximum((seg ** 2).sum(1), 1e-9)
D = np.empty((len(V), len(deform)))
for i in range(len(deform)):
    t = np.clip(((V - heads[i]) @ seg[i]) / seglen2[i], 0.0, 1.0)
    D[:, i] = np.sqrt(((V - (heads[i] + t[:, None] * seg[i])) ** 2).sum(1))
W = 1.0 / (np.maximum(D, 0.01) ** 4)

def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)
for i, b in enumerate(deform):
    if b.name.endswith("_upper"):
        W[:, i] *= 1.0 - smoothstep(b.head_local.z - 0.02, b.head_local.z + 0.08, V[:, 2])
# The snout and ears stick out ahead of the body: pin them to the head. The
# lifted right fore paw was modelled up under the jaw (its top reaches
# y -1.1, z 0.5), so verts nearer a fore leg than the head are left out of
# the pin, and low ones get no head or neck weight; otherwise the paw's
# top stays with the head and smears when the paw plants.
hi = names.index("head")
fore = [i for i, n in enumerate(names) if n[:2] in ("fl", "fr")]
fore_nearer = D[:, fore].min(1) < D[:, hi]
pin = (V[:, 1] < -1.05) & (V[:, 2] > 0.45) & ~fore_nearer
W[pin] = 0.0; W[pin, hi] = 1.0
paw = fore_nearer & (V[:, 2] < 0.55)
W[np.ix_(paw, [hi, names.index("neck")])] = 0.0
log.append("head pin %d verts; head/neck weight cleared on %d fore-leg verts" % (pin.sum(), paw.sum()))

parent = np.arange(len(V))
def find(i):
    while parent[i] != i:
        parent[i] = parent[parent[i]]
        i = parent[i]
    return i
for e in mesh.data.edges:
    a, b = find(e.vertices[0]), find(e.vertices[1])
    if a != b: parent[a] = b
roots = np.array([find(i) for i in range(len(V))])
_, island, sizes = np.unique(roots, return_inverse=True, return_counts=True)
PIECE_MAX = 400
small = sizes[island] <= PIECE_MAX
def piece_mean(x):
    s = np.zeros(len(sizes)); np.add.at(s, island, x)
    return np.where(small, (s / sizes)[island], x)       # big pieces: per vertex
for pair, ztop in (("f", 0.62), ("b", 0.55)):
    li = [i for i, n in enumerate(names) if n[:2] == pair + "l"]
    ri = [i for i, n in enumerate(names) if n[:2] == pair + "r"]
    low = piece_mean(V[:, 2]) < ztop
    left_nearer = piece_mean(D[:, li].min(1)) < piece_mean(D[:, ri].min(1))
    W[np.ix_(low & left_nearer, ri)] = 0.0
    W[np.ix_(low & ~left_nearer, li)] = 0.0
log.append("%d pieces (largest %d verts, %d verts in pieces > %d)" % (len(sizes), sizes.max(), int((~small).sum()), PIECE_MAX))

top = np.argsort(-W, axis=1)[:, :4]
mesh.vertex_groups.clear()
groups = [mesh.vertex_groups.new(name=n) for n in names]
for vi in range(len(V)):
    idx = top[vi]; w = W[vi, idx]
    if w.sum() <= 0: continue
    w = w / w.sum()
    for j, wj in zip(idx, w):
        if wj > 0.01: groups[j].add([vi], float(wj), 'REPLACE')

# Faces joining the two legs of a pair stretch into spikes when they move apart.
leg_of = {}
for vi in range(len(V)):
    n = names[top[vi, 0]]
    if n[:1] in "fb" and n[1:2] in "lr" and V[vi, 2] < 0.55:
        leg_of[vi] = n[:2]
bm = bmesh.new(); bm.from_mesh(mesh.data)
bridges = [f for f in bm.faces if len({leg_of.get(v.index) for v in f.verts} - {None}) > 1]
bmesh.ops.delete(bm, geom=bridges, context='FACES_ONLY')
loose = [v for v in bm.verts if not v.link_faces]
bmesh.ops.delete(bm, geom=loose, context='VERTS')
bm.to_mesh(mesh.data); bm.free()
log.append("deleted %d leg-bridging faces, %d loose verts" % (len(bridges), len(loose)))

for m in list(mesh.modifiers): mesh.modifiers.remove(m)
am = mesh.modifiers.new("Armature", 'ARMATURE'); am.object = arm
mesh.parent = arm
unweighted = sum(1 for v in mesh.data.vertices if not any(g.weight > 0.01 for g in v.groups))
counts = {}
for g in mesh.vertex_groups: counts[g.name] = 0
for v in mesh.data.vertices:
    if v.groups:
        counts[mesh.vertex_groups[max(v.groups, key=lambda g: g.weight).group].name] += 1
log.append("unweighted %d / %d; verts per dominant bone: %s" % (unweighted, len(mesh.data.vertices), counts))

arm.animation_data_create()
act = bpy.data.actions.new("Idle"); act.use_fake_user = True; arm.animation_data.action = act
bpy.ops.wm.save_as_mainfile(filepath=RIG)
open(OUT + "/rig_log.txt", "w").write("\n".join(log))
