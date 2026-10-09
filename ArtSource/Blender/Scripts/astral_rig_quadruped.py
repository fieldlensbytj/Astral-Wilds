# Rigs a prepped Meshy quadruped (astral_rig_prep.py) with the shared 22-bone
# skeleton, skins it, and re-stances it: all four legs are posed onto a
# level, even stance - a raised, curled paw is uncurled and planted pads-down
# - and that pose is applied as the rest pose. Saves Rigs/<Name>_Rigged.blend
# for quadruped_reanimate.py (Idle/Walk/Run).
#
#   blender -b --python astral_rig_quadruped.py -- <Name> <out_dir>
#
# Written for Cindrel and Ripplefin (TJ, 2026-10-09: "they both have their
# paws in a retracted position so rigging and animating them would be very
# tedious"). Both were modelled mid-step: front-left paw raised and curled,
# hind legs one forward, one back. Re-standing them in the rig means the
# gaits start from four planted paws with no hand-fixing.
#
# The first models had a raised, curled front-left paw (uncurled by the re-
# stance below, pads set in CONFIG "pads"); the 2026-10-09 ones stand on all
# four, so the re-stance only evens out the stride. Landmarks (CONFIG) were read from astral_rig_prep.py's leg traces and the
# gridded half-body side views (head toward -Y, left side is +X).
import bpy, sys, math, numpy as np
from mathutils import Vector, Matrix, Quaternion

NAME, OUT = sys.argv[sys.argv.index("--") + 1:][:2]
RIGS = r"C:/Users/camer/Astral Wilds/ArtSource/Blender/Rigs"

# Landmarks for the 2026-10-09 models (TJ's new GLBs, all four paws down;
# the first ones, with a raised curled paw, are in FullSize/old/).
CONFIG = {
    "Cindrel": dict(
        cx=-0.07,
        spine={"pelvis": ((0.24, 0.80), (0.00, 0.82)), "spine_01": ((0.00, 0.82), (-0.28, 0.84)),
               "chest": ((-0.28, 0.84), (-0.52, 0.88)), "neck": ((-0.52, 0.88), (-0.62, 1.06)),
               "head": ((-0.62, 1.06), (-0.82, 1.04))},
        tail=[(0.24, 0.82), (0.43, 0.78), (0.77, 0.95), (0.77, 1.40), (0.58, 1.68)],
        legs={  # shoulder/hip, elbow/stifle, wrist/hock, toe
            "fl": [(0.06, -0.53, 0.76), (0.10, -0.57, 0.52), (0.12, -0.68, 0.18), (0.12, -0.80, 0.02)],
            "fr": [(-0.21, -0.48, 0.76), (-0.25, -0.50, 0.52), (-0.27, -0.60, 0.18), (-0.27, -0.74, 0.02)],
            "bl": [(0.08, -0.05, 0.76), (0.14, -0.08, 0.52), (0.19, 0.07, 0.24), (0.19, -0.06, 0.02)],
            "br": [(-0.22, 0.12, 0.76), (-0.28, 0.07, 0.52), (-0.33, 0.22, 0.24), (-0.33, 0.10, 0.02)],
        },
        pads={},   # no raised paw: all four stand
        # The model's head is turned ~45 deg to its right; turn it to face
        # forward (deg about world Z; + turns the face toward +X), pitch + = nose down.
        head_turn=(70.0, 0.0, -12.0),   # yaw, pitch, roll (roll + = tips its right ear down); its head is also tilted
        head_pin=lambda v: v[2] > 0.98 and v[1] < -0.30,
        stance=dict(front_y=-0.70, hind_y=0.06, front_half=0.19, hind_half=0.24),
    ),
    "Ripplefin": dict(
        cx=-0.02,
        spine={"pelvis": ((0.30, 0.64), (0.05, 0.66)), "spine_01": ((0.05, 0.66), (-0.20, 0.68)),
               "chest": ((-0.20, 0.68), (-0.44, 0.72)), "neck": ((-0.44, 0.72), (-0.52, 0.96)),
               "head": ((-0.52, 0.96), (-0.72, 1.04))},
        tail=[(0.30, 0.66), (0.50, 0.80), (0.70, 1.05), (0.80, 1.38), (0.62, 1.66)],
        legs={
            "fl": [(0.10, -0.44, 0.70), (0.13, -0.46, 0.50), (0.15, -0.50, 0.22), (0.16, -0.62, 0.02)],
            "fr": [(-0.15, -0.40, 0.70), (-0.19, -0.38, 0.50), (-0.21, -0.40, 0.22), (-0.21, -0.50, 0.02)],
            "bl": [(0.13, 0.12, 0.66), (0.18, 0.10, 0.46), (0.21, 0.25, 0.24), (0.21, 0.12, 0.02)],
            "br": [(-0.17, 0.24, 0.66), (-0.22, 0.22, 0.46), (-0.25, 0.37, 0.24), (-0.25, 0.26, 0.02)],
        },
        pads={},
        head_turn=(60.0, 12.0, 0.0),   # head turned ~60 deg to its right and tipped up
        head_pin=lambda v: v[2] > 0.88 and v[1] < -0.42,
        stance=dict(front_y=-0.52, hind_y=0.25, front_half=0.18, hind_half=0.23),
    ),
}
C = CONFIG[NAME]
bpy.ops.wm.open_mainfile(filepath="%s/%s_Prep.blend" % (RIGS, NAME))
scn = bpy.context.scene
mesh = bpy.data.objects[NAME]
log = []
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))

# ---------------- skeleton ----------------
cx = C["cx"]
B = {"root": ((cx, 0.05, 0.0), (cx, -0.15, 0.0), None)}
par = {"pelvis": "root", "spine_01": "pelvis", "chest": "spine_01", "neck": "chest", "head": "neck"}
for n, ((y0, z0), (y1, z1)) in C["spine"].items():
    B[n] = ((cx, y0, z0), (cx, y1, z1), par[n])
t = C["tail"]
for i in range(4):
    B["tail_0%d" % (i + 1)] = ((cx, t[i][0], t[i][1]), (cx, t[i + 1][0], t[i + 1][1]), "pelvis" if i == 0 else "tail_0%d" % i)
for k, (a_, kn, an, toe) in C["legs"].items():
    p = "chest" if k[0] == "f" else "pelvis"
    B[k + "_upper"] = (a_, kn, p)
    B[k + "_lower"] = (kn, an, k + "_upper")
    B[k + "_foot"] = (an, toe, k + "_lower")

arm_data = bpy.data.armatures.new(NAME + "Rig")
arm = bpy.data.objects.new(NAME + "Rig", arm_data)
scn.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='EDIT')
eb = {}
for n, (h, tl, p) in B.items():
    b = arm_data.edit_bones.new(n); b.head, b.tail, b.roll = Vector(h), Vector(tl), 0.0; eb[n] = b
for n, (h, tl, p) in B.items():
    if p:
        eb[n].parent = eb[p]
        eb[n].use_connect = (Vector(eb[p].tail) - Vector(h)).length < 1e-4
eb["root"].use_deform = False
bpy.ops.object.mode_set(mode='OBJECT')

# ---------------- skinning ----------------
# The Meshy fur is thousands of separate scales/tufts (largest piece ~140
# verts), so each piece is skinned as one unit from its centre (Stormrook's
# recipe): it bends between bones as a whole and never shears. Distance to
# bone segments, 4th-power falloff, top 4. Leg bones fade out above the
# shoulder/hip; below the belly a piece belongs to one leg of a pair only.
deform = [b for b in arm_data.bones if b.use_deform]
names = [b.name for b in deform]
H = np.array([tuple(b.head_local) for b in deform]); T = np.array([tuple(b.tail_local) for b in deform])
V = np.array([tuple(v.co) for v in mesh.data.vertices])
pr = np.arange(len(V))
def find(a):
    while pr[a] != a:
        pr[a] = pr[pr[a]]; a = pr[a]
    return a
for e in mesh.data.edges:
    a, b = find(e.vertices[0]), find(e.vertices[1])
    if a != b:
        pr[a] = b
_, inv = np.unique([find(i) for i in range(len(V))], return_inverse=True)
P = np.zeros((inv.max() + 1, 3)); np.add.at(P, inv, V); P /= np.bincount(inv)[:, None]
seg = T - H
D = np.empty((len(P), len(deform)))
for i in range(len(deform)):
    tt = np.clip(((P - H[i]) @ seg[i]) / max((seg[i] ** 2).sum(), 1e-9), 0, 1)
    D[:, i] = np.sqrt(((P - (H[i] + tt[:, None] * seg[i])) ** 2).sum(1))
W = 1.0 / np.maximum(D, 0.01) ** 4
def sstep(a, b, x):
    k = np.clip((x - a) / (b - a), 0, 1); return k * k * (3 - 2 * k)
for i, b in enumerate(deform):
    if b.name.endswith("_upper"):
        W[:, i] *= 1 - sstep(b.head_local.z - 0.02, b.head_local.z + 0.10, P[:, 2])
hi = names.index("head")
pin = np.array([C["head_pin"](p) for p in P])
W[pin] = 0; W[pin, hi] = 1
for pair, ztop in (("f", 0.55), ("b", 0.52)):
    li = [i for i, n in enumerate(names) if n[:2] == pair + "l"]
    ri = [i for i, n in enumerate(names) if n[:2] == pair + "r"]
    low = P[:, 2] < ztop
    left = D[:, li].min(1) < D[:, ri].min(1)
    W[np.ix_(low & left, ri)] = 0
    W[np.ix_(low & ~left, li)] = 0
# Anything on the ground belongs to a planted paw, never the raised one
# (Cindrel's right fore toes sit right under its curled left paw).
ground = P[:, 2] < 0.12
planted = [k for k in C["legs"] if k not in C["pads"]]
toes = np.array([C["legs"][k][3][:2] for k in planted])
near = np.argmin(((P[:, None, :2] - toes[None]) ** 2).sum(2), axis=1)
for gi in np.where(ground)[0]:
    W[gi] = 0
    W[gi, names.index(planted[near[gi]] + "_foot")] = 1
top = np.argsort(-W, axis=1)[:, :4]
mesh.vertex_groups.clear()
groups = [mesh.vertex_groups.new(name=n) for n in names]
for vi in range(len(V)):
    pi = inv[vi]; idx = top[pi]; w = W[pi, idx]
    if w.sum() <= 0:
        continue
    w = w / w.sum()
    for j, wj in zip(idx, w):
        if wj > 0.01:
            groups[j].add([vi], float(wj), 'REPLACE')
for m in list(mesh.modifiers):
    mesh.modifiers.remove(m)
am = mesh.modifiers.new("Armature", 'ARMATURE'); am.object = arm
mesh.parent = arm
log.append("pieces %d, head pin %d; verts per bone %s" % (len(P), int(pin.sum()), {n: int((top[inv][:, 0] == i).sum()) for i, n in enumerate(names)}))

# ---------------- re-stance: four planted paws, applied as rest ----------------
pb = arm.pose.bones
for p in pb:
    p.rotation_mode = 'QUATERNION'
upd = bpy.context.view_layer.update

def set_dir(name, d, pad_from=None, pad_to=None):
    """Rotate bone about its head so it points along d; with pad_from/pad_to
    (armature-space vectors in the rest pose / wanted), also twist it so the
    pad normal ends up as close to pad_to as the direction allows."""
    p = pb[name]
    M = p.matrix.copy(); head = M.translation.copy()
    cur = (M.to_3x3() @ Y).normalized(); d = d.normalized()
    q = cur.rotation_difference(d)
    if pad_from is not None:
        n0 = (q @ (M.to_3x3() @ (arm_data.bones[name].matrix_local.to_3x3().inverted() @ Vector(pad_from).normalized())))
        a, b = n0 - d * n0.dot(d), Vector(pad_to) - d * Vector(pad_to).dot(d)
        if a.length > 1e-4 and b.length > 1e-4:
            ang = a.angle(b)
            if d.dot(a.cross(b)) < 0:
                ang = -ang
            q = Quaternion(d, ang) @ q
    p.matrix = Matrix.Translation(head) @ q.to_matrix().to_4x4() @ Matrix.Translation(-head) @ M
    p.scale = Vector((1, 1, 1)); upd()

st = C["stance"]
for k, (a_, kn, an, toe) in C["legs"].items():
    front = k[0] == "f"
    side = 1 if k[1] == "l" else -1
    hip = pb[k + "_upper"].matrix.translation.copy()
    L1 = (Vector(kn) - Vector(a_)).length; L2 = (Vector(an) - Vector(kn)).length
    toe_t = Vector((cx + side * st["front_half" if front else "hind_half"], st["front_y" if front else "hind_y"], 0.02))
    foot_len = (Vector(toe) - Vector(an)).length
    # Standing foot: the wrist/hock sits above and a little behind the toe.
    fdir = Vector((0, -0.45, -0.9)).normalized() if front else Vector((0, -0.55, -0.83)).normalized()
    ank_t = toe_t - fdir * foot_len
    d = ank_t - hip
    dl = min(max(d.length, abs(L1 - L2) + 1e-4), (L1 + L2) * 0.995)
    n = d.normalized()
    a = (L1 * L1 - L2 * L2 + dl * dl) / (2 * dl)
    h = math.sqrt(max(L1 * L1 - a * a, 0))
    pole = Y if front else -Y
    perp = pole - n * pole.dot(n); perp.normalize()
    knee = hip + n * a + perp * h
    set_dir(k + "_upper", knee - hip)
    set_dir(k + "_lower", (hip + n * dl) - pb[k + "_lower"].matrix.translation)
    pad = C["pads"].get(k, (0, 0, -1))
    set_dir(k + "_foot", toe_t - pb[k + "_foot"].matrix.translation, pad_from=pad, pad_to=(0, 0, -1))
    log.append("%s: toe %s -> %s, reach %.2f/%.2f" % (k, tuple(round(c, 2) for c in toe), tuple(round(c, 2) for c in toe_t), d.length, L1 + L2))

# Face forward (TJ, 2026-10-09: "make all of their heads forward facing ...
# mainly ripplefin and cindrel"): both models look off to their right. Turn
# the neck (45%) and head (55%) about world Z by head_turn[0], and tip by
# head_turn[1] about X; it becomes part of the rest pose below.
yaw, pitch, roll = C.get("head_turn", (0.0, 0.0, 0.0))
for bn, share in (("neck", 0.45), ("head", 0.55)):
    if yaw or pitch or roll:
        p = pb[bn]
        h = p.matrix.translation.copy()
        q = Quaternion(Z, math.radians(yaw * share)) @ Quaternion(X, math.radians(pitch * share)) @ Quaternion(Y, math.radians(roll * share))
        p.matrix = Matrix.Translation(h) @ q.to_matrix().to_4x4() @ Matrix.Translation(-h) @ p.matrix
        p.scale = Vector((1, 1, 1)); upd()
log.append("head turned %.0f deg, tipped %.0f, rolled %.0f" % (yaw, pitch, roll))

# Apply: bake the posed mesh, then make the pose the armature's rest pose.
bpy.ops.object.select_all(action='DESELECT')
mesh.select_set(True); bpy.context.view_layer.objects.active = mesh
bpy.ops.object.modifier_apply(modifier="Armature")
bpy.ops.object.select_all(action='DESELECT')
arm.select_set(True); bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='POSE')
bpy.ops.pose.armature_apply(selected=False)
bpy.ops.object.mode_set(mode='OBJECT')
am = mesh.modifiers.new("Armature", 'ARMATURE'); am.object = arm
# Pieces flung clear of the body by the re-stance (a scale on the curled
# paw whose centre sat nearer another bone): put each back where it was,
# moved with the bone that carries most of its piece's neighbours - simplest
# robust fix is to report them and snap them to their nearest paw bone's
# motion. Report first.
V2 = np.array([tuple(v.co) for v in mesh.data.vertices])
P2 = np.zeros_like(P); np.add.at(P2, inv, V2); P2 /= np.bincount(inv)[:, None]
stray = np.where(P2[:, 2] < -0.03)[0]
for s in stray:
    log.append("stray piece %d: rest centre %s -> %s, bones %s" % (s, tuple(round(c, 2) for c in P[s]), tuple(round(c, 2) for c in P2[s]), [names[j] for j in top[s][:2]]))
# Feet back on z=0 (the stance may leave them a touch above or below).
zs = np.array([v.co.z for v in mesh.data.vertices])
lowest = float(np.percentile(zs[zs < 0.3], 0.5))
for v in mesh.data.vertices:
    v.co.z -= lowest
bpy.ops.object.mode_set(mode='EDIT')
for b in arm_data.edit_bones:
    b.head.z -= lowest; b.tail.z -= lowest
bpy.ops.object.mode_set(mode='OBJECT')
log.append("re-stanced; lowered by %.3f" % lowest)

arm.animation_data_create()
act = bpy.data.actions.new("Idle"); act.use_fake_user = True; arm.animation_data.action = act

# Views of the new rest pose (side, front) for checking.
scn.render.engine = 'BLENDER_WORKBENCH'; scn.display.shading.color_type = 'TEXTURE'; scn.display.shading.light = 'STUDIO'
scn.render.resolution_x = scn.render.resolution_y = 700
cd = bpy.data.cameras.new("c"); cd.type = 'ORTHO'; cd.ortho_scale = 2.5
cam = bpy.data.objects.new("c", cd); scn.collection.objects.link(cam); scn.camera = cam
for nm, loc, rot in (("side", (5, 0, 0.9), (90, 0, 90)), ("front", (0, -5, 0.9), (90, 0, 0)), ("under", (0, 0, -5), (180, 0, 0))):
    cam.location = Vector(loc); cam.rotation_euler = [math.radians(a) for a in rot]
    scn.render.filepath = OUT + "/rest_%s.png" % nm; bpy.ops.render.render(write_still=True)
bpy.data.objects.remove(cam, do_unlink=True)
bpy.ops.wm.save_as_mainfile(filepath="%s/%s_Rigged.blend" % (RIGS, NAME))
open(OUT + "/rig_log.txt", "w").write("\n".join(log))
