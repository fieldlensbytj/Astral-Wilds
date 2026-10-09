# Rigs and animates Stormrook, the flying Astral (TJ, 2026-10-08: "the wings
# need to flap"). Run stormrook_rig_prep.py first, then:
#
#   blender -b --python stormrook_rig_rig_stormrook.py -- <out_dir>
#
# Writes Rigs/Stormrook_Rigged.blend/.fbx with Idle, Walk, Run (on the
# ground) and Fly (flapping) / Glide (wings held spread) for the air, plus
# <out_dir>/speeds.txt and contact sheets.
#
# Skeleton: the usual root/pelvis/spine_01/chest/neck/head/tail_0x (so the
# engine's turn bend and head lead work as on the quadrupeds), bird legs
# (thigh, shin, foot per side) and a three-bone wing per side (upper arm,
# forearm, hand) on the chest. Landmarks were traced from height and width
# slices of the decimated mesh (see the survey printed by the prep script).
#
# Motion references (art repo Docs/Design/TurnReference.md, "Flight"): eagle
# landing at 1000fps, raven take-off, Harris hawk. The flight clips hold the
# body level (the model stands upright, so they pitch it forward), tuck the
# legs back and stretch the head forward. Flapping: a powered downstroke with
# the wing fully spread, sweeping from high above the back to below the body,
# the tip lagging the shoulder; then a quicker upstroke with the wing folded
# at the wrist (hand swept back) to cut drag. Gliding: wings out, a slight
# dihedral, primaries flexing a little.
import bpy, math, sys
import numpy as np
from mathutils import Vector, Quaternion, Matrix

argv = sys.argv[sys.argv.index("--") + 1:]
OUT = argv[0]
RIGS = r"C:/Users/camer/Astral Wilds/ArtSource/Blender/Rigs"
bpy.ops.wm.open_mainfile(filepath=RIGS + "/Stormrook_Prep.blend")
scn = bpy.context.scene
mesh = bpy.data.objects["Stormrook"]
log = []
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))

# ---------------- skeleton (head toward -Y, feet on z=0; left wing is +X) ----------------
B = {  # name: (head, tail, parent)
    "root":     ((0, 0.05, 0.0), (0, -0.15, 0.0), None),
    "pelvis":   ((0, -0.18, 0.56), (0, -0.30, 0.80), "root"),
    "spine_01": ((0, -0.30, 0.80), (0, -0.36, 1.04), "pelvis"),
    "chest":    ((0, -0.36, 1.04), (0, -0.40, 1.30), "spine_01"),
    "neck":     ((0, -0.40, 1.30), (0, -0.42, 1.50), "chest"),
    "head":     ((0, -0.42, 1.50), (0, -0.64, 1.62), "neck"),
    "tail_01":  ((0, -0.08, 0.56), (0, 0.15, 0.38), "pelvis"),
    "tail_02":  ((0, 0.15, 0.38), (0, 0.40, 0.27), "tail_01"),
    "tail_03":  ((0, 0.40, 0.27), (0, 0.62, 0.21), "tail_02"),
    "tail_04":  ((0, 0.62, 0.21), (0, 0.72, 0.19), "tail_03"),
}
for s, side in ((1, "l"), (-1, "r")):
    # Bird legs: the hidden knee points forward, the visible "backward knee"
    # is the ankle; toes reach forward on the ground.
    B["leg_%s_thigh" % side] = ((s * 0.11, -0.22, 0.60), (s * 0.12, -0.32, 0.42), "pelvis")
    B["leg_%s_shin" % side] = ((s * 0.12, -0.32, 0.42), (s * 0.11, -0.18, 0.24), "leg_%s_thigh" % side)
    B["leg_%s_foot" % side] = ((s * 0.11, -0.18, 0.24), (s * 0.11, -0.50, 0.06), "leg_%s_shin" % side)
# Wings traced per side (the model isn't symmetric: its right wing sits a
# little higher and further back).
WING = {"l": [(0.20, -0.26, 1.02), (0.47, -0.30, 1.00), (0.70, -0.28, 0.86), (0.86, -0.21, 0.50)],
        "r": [(-0.20, -0.18, 1.08), (-0.47, -0.21, 1.06), (-0.68, -0.18, 0.92), (-0.85, -0.05, 0.55)]}
for side, (sh, el, wr, tip) in WING.items():
    B["wing_%s_upper" % side] = (sh, el, "chest")
    B["wing_%s_fore" % side] = (el, wr, "wing_%s_upper" % side)
    B["wing_%s_hand" % side] = (wr, tip, "wing_%s_fore" % side)

arm_data = bpy.data.armatures.new("StormrookRig")
arm = bpy.data.objects.new("StormrookRig", arm_data)
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
log.append("bones: %d" % len(arm_data.bones))

# ---------------- skinning: distance to bone segments, by region ----------------
# Glacielle's recipe (4th-power falloff, top 4). Feathers past the shoulder
# (|x| > 0.30) only take their own side's wing bones, and body feathers
# (|x| < 0.24) never take wing bones, so a flapping wing doesn't drag the
# chest or legs with it; in between, the upper arm blends with the chest.
deform = [b for b in arm_data.bones if b.use_deform]
names = [b.name for b in deform]
heads = np.array([tuple(b.head_local) for b in deform]); tails = np.array([tuple(b.tail_local) for b in deform])
V = np.array([tuple(v.co) for v in mesh.data.vertices])
# The Meshy model is ~4900 separate feathers (mesh islands of <= ~50 verts).
# Each feather is skinned as one rigid piece, from its centre: it never
# shears, and a feather is either on a wing or on the body, never split
# between them (a tail feather lying beside the right wing had been, and
# stretched 1.2m when the wing flapped).
par = np.arange(len(V))
def find(a):
    while par[a] != a:
        par[a] = par[par[a]]; a = par[a]
    return a
for e in mesh.data.edges:
    a, b = find(e.vertices[0]), find(e.vertices[1])
    if a != b:
        par[a] = b
island = np.array([find(i) for i in range(len(V))])
ids, inv = np.unique(island, return_inverse=True)
C = np.zeros((len(ids), 3)); np.add.at(C, inv, V); C /= np.bincount(inv)[:, None]
seg = tails - heads
D = np.empty((len(C), len(deform)))
for i in range(len(deform)):
    t = np.clip(((C - heads[i]) @ seg[i]) / max((seg[i] ** 2).sum(), 1e-9), 0, 1)
    D[:, i] = np.sqrt(((C - (heads[i] + t[:, None] * seg[i])) ** 2).sum(1))
W = 1.0 / np.maximum(D, 0.01) ** 4
is_wing = np.array([n.startswith("wing_") for n in names])
wing_side = np.array([1 if n.startswith("wing_l") else (-1 if n.startswith("wing_r") else 0) for n in names])
# A feather is a wing feather if its nearest bone is a wing bone and it sits
# out past the body (|x| > 0.24); it then takes only that wing's bones (and
# the chest, close to the shoulder). Body feathers take no wing bones.
nearest = np.argmin(D, axis=1)
wing_isl = is_wing[nearest] & (np.abs(C[:, 0]) > 0.24)
side_isl = np.where(wing_isl, wing_side[nearest], 0)
for i, n in enumerate(names):
    if is_wing[i]:
        W[side_isl != wing_side[i], i] = 0
    elif n == "chest":
        W[wing_isl & (np.abs(C[:, 0]) > 0.36), i] = 0
    else:
        W[wing_isl, i] = 0
# Shoulders: feathers round each shoulder joint blend from the chest to the
# upper arm with distance out along the wing, so the wing root stretches
# like covert feathers instead of tearing away from the body (TJ: "the wings
# aren't connected to the shoulders").
ci, = [i for i, n in enumerate(names) if n == "chest"]
for side, s in (("l", 1), ("r", -1)):
    ui = names.index("wing_%s_upper" % side)
    sh, el = np.array(WING[side][0]), np.array(WING[side][1])
    near = (np.sign(C[:, 0]) == s) & (np.abs(C[:, 2] - sh[2]) < 0.30) & (np.abs(C[:, 1] - sh[1]) < 0.30)
    out = (s * C[:, 0] - (sh[0] * s - 0.08)) / ((el[0] - sh[0]) * s * 0.9)   # 0 just inside the shoulder -> 1 near the elbow
    k = np.clip(out, 0, 1); k = k * k * (3 - 2 * k)
    blend = near & (out > 0) & (out < 1)
    W[blend, :] = 0
    W[blend, ci] = 1 - k[blend]
    W[blend, ui] = k[blend]
    log.append("shoulder %s: %d feathers blended chest -> upper arm" % (side, int(blend.sum())))
log.append("feathers: %d islands, %d on the wings" % (len(C), int(wing_isl.sum())))
top_isl = np.argsort(-W, axis=1)[:, :4]
top = top_isl[inv]
W = W[inv]
mesh.vertex_groups.clear()
groups = [mesh.vertex_groups.new(name=n) for n in names]
for vi in range(len(V)):
    idx = top[vi]; w = W[vi, idx]
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
log.append("weights: verts per bone %s" % {n: int((top[:, 0] == i).sum()) for i, n in enumerate(names)})

# ---------------- posing helpers ----------------
FPS = 30
scn.render.fps = FPS
pb = arm.pose.bones
for p in pb:
    p.rotation_mode = 'QUATERNION'
rest = {b.name: b.matrix_local.to_3x3() for b in arm_data.bones}
upd = bpy.context.view_layer.update

def wrot(bone, axis, deg):
    return Quaternion((rest[bone].inverted() @ axis).normalized(), math.radians(deg))

def aim(name, direction):
    """Rotate a bone about its head so it points along direction (armature space)."""
    p = pb[name]
    M = p.matrix.copy()
    head = M.translation.copy()
    q = (M.to_3x3() @ Y).rotation_difference(direction.normalized())
    p.matrix = Matrix.Translation(head) @ q.to_matrix().to_4x4() @ Matrix.Translation(-head) @ M
    p.scale = Vector((1, 1, 1))
    upd()

# Wing surfaces (TJ, 2026-10-08: "the wings to be flat while flapping so the
# curved part is parallel to the ground"). Aiming only sets where a wing bone
# points, not its twist, so the wings kept the model's hanging pose's twist
# and swung out edge-on. Each wing segment's surface normal is measured from
# its feathers (the least-spread axis of their points), signed to the upper
# (dorsal) side - facing outward while the wing hangs folded - and every
# flight pose twists the segment about its own axis until that normal faces
# the way a bird's wing does: up, tilted by the stroke and angle of attack.
WN = {}
for i, n in enumerate(names):
    if not n.startswith("wing_"):
        continue
    P = V[top[:, 0] == i]
    P = P - P.mean(0)
    w, vecs = np.linalg.eigh(P.T @ P)
    nrm = Vector(vecs[:, 0])
    if nrm.x * (1 if "_l_" in n else -1) < 0:
        nrm = -nrm
    WN[n] = nrm   # armature space for now
    log.append("%s surface normal (rest) %s, flatness %.2f" % (n, tuple(round(c, 2) for c in nrm), w[0] / max(w[1], 1e-9)))

# The upper arm's feathers are a bulky mix of coverts and shoulder (its
# measured normal is unreliable: least/next spread 0.3-0.4 vs 0.1 for the
# hand), and twisting it by that pinched the wing root thin. On a real wing
# the arm and forearm carry one continuous surface, so it uses the forearm's.
for side in "lr":
    WN["wing_%s_upper" % side] = WN["wing_%s_fore" % side]
WN = {n: rest[n].inverted() @ v for n, v in WN.items()}   # into each bone's rest frame

def twist_to(name, want):
    """Twist a bone about its own axis so its wing surface normal faces want (armature space)."""
    p = pb[name]
    M = p.matrix.to_3x3()
    axis = (M @ Y).normalized()
    cur = M @ WN[name]
    cp, wp = cur - axis * cur.dot(axis), want - axis * want.dot(axis)
    if cp.length < 1e-4 or wp.length < 1e-4:
        return
    ang = cp.angle(wp)
    if axis.dot(cp.cross(wp)) < 0:
        ang = -ang
    head = p.matrix.translation.copy()
    p.matrix = Matrix.Translation(head) @ Quaternion(axis, ang).to_matrix().to_4x4() @ Matrix.Translation(-head) @ p.matrix
    p.scale = Vector((1, 1, 1))
    upd()

def wing_normal(s, d, aoa):
    """Upper-surface normal for a wing segment along d: perpendicular to the
    flight direction and the segment (so the wing lies flat, parallel to the
    ground when level), then pitched leading-edge-up by aoa degrees."""
    n = (-Y).cross(d.normalized()) * s
    if n.length < 1e-4:
        n = Z.copy()
    n.normalize()
    for sgn in (1, -1):
        m = Quaternion(d.normalized(), math.radians(sgn * aoa)) @ n
        if aoa == 0 or m.y > 0:      # the upper surface tilts back as the leading edge rises
            return m
    return n

def solve_leg(side, ankle, toe):
    """Two-bone IK hip -> knee -> ankle (knee forward), then the foot to the toe."""
    th, sh = arm_data.bones["leg_%s_thigh" % side], arm_data.bones["leg_%s_shin" % side]
    L1, L2 = th.length, sh.length
    H = pb["leg_%s_thigh" % side].matrix.translation.copy()
    d = ankle - H
    dl = min(max(d.length, abs(L1 - L2) + 1e-4), (L1 + L2) * 0.999)
    n = d.normalized()
    a = (L1 * L1 - L2 * L2 + dl * dl) / (2 * dl)
    h = math.sqrt(max(L1 * L1 - a * a, 0))
    perp = (-Y) - n * (-Y).dot(n); perp.normalize()
    aim("leg_%s_thigh" % side, (H + n * a + perp * h) - H)
    aim("leg_%s_shin" % side, (H + n * dl) - pb["leg_%s_shin" % side].matrix.translation)
    aim("leg_%s_foot" % side, toe - pb["leg_%s_foot" % side].matrix.translation)

def reset():
    for p in pb:
        p.rotation_quaternion = Quaternion(); p.location = Vector(); p.scale = Vector((1, 1, 1))
    upd()

def smooth(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3 - 2 * x)

REST_ANKLE = {s: Vector(B["leg_%s_foot" % s][0]) for s in "lr"}
REST_TOE = {s: Vector(B["leg_%s_foot" % s][1]) for s in "lr"}
SIDE = {"l": 1, "r": -1}

# ---------------- ground clips ----------------
def ground_pose(t, g):
    """Upright raptor on its feet: Idle (g None) or a walking stride."""
    if g is None:
        breath = math.sin(2 * math.pi * 2 * t)
        pb["pelvis"].location = rest["pelvis"].inverted() @ Vector((0, 0, 0.005 * breath))
        pb["chest"].rotation_quaternion = wrot("chest", X, 1.5 * breath)
        # Raptors look round in quick turns and hold still between them.
        keys = [(0.0, 0, 0), (0.12, 0, 0), (0.16, 35, 5), (0.38, 35, 5), (0.42, -20, -8), (0.62, -20, -8), (0.66, 10, 12), (0.86, 10, 12), (0.9, 0, 0), (1.0, 0, 0)]
        for (t0, y0, p0), (t1, y1, p1) in zip(keys, keys[1:]):
            if t0 <= t <= t1:
                k = smooth((t - t0) / max(t1 - t0, 1e-6))
                yaw, pitch = y0 + (y1 - y0) * k, p0 + (p1 - p0) * k
        pb["neck"].rotation_quaternion = wrot("neck", Z, 0.4 * yaw)
        pb["head"].rotation_quaternion = wrot("head", Z, 0.6 * yaw) @ wrot("head", X, pitch)
        # A wing settle (rouse) once a loop: wings lift a touch and fold back.
        rouse = math.sin(math.pi * smooth((t - 0.5) / 0.12)) ** 2 if 0.5 <= t <= 0.62 else 0.0
        for side, s in SIDE.items():
            pb["wing_%s_upper" % side].rotation_quaternion = wrot("wing_%s_upper" % side, Y, -s * 12 * rouse)
        for i in range(1, 5):
            pb["tail_0%d" % i].rotation_quaternion = wrot("tail_0%d" % i, Z, 3 * math.sin(2 * math.pi * (t * 3 - 0.1 * i)) * (0.5 + 0.2 * i))
        upd()
        for side in "lr":
            solve_leg(side, REST_ANKLE[side], REST_TOE[side])
        return
    S, beta, lift = g["S"], g["beta"], g["lift"]
    sway = math.sin(2 * math.pi * t)
    pb["pelvis"].location = rest["pelvis"].inverted() @ Vector((0.02 * sway, 0, -0.03 - g["bob"] * math.cos(4 * math.pi * t)))
    pb["pelvis"].rotation_quaternion = wrot("pelvis", Y, 4 * sway) @ wrot("pelvis", X, -g["lean"])
    # Head-bob: the head thrusts forward then holds still while the body catches up.
    thrust = (math.sin(4 * math.pi * t) + 1) / 2
    pb["neck"].rotation_quaternion = wrot("neck", X, g["lean"] * 0.6 - 6 * thrust)
    pb["head"].rotation_quaternion = wrot("head", X, 4 * thrust)
    for side, s in SIDE.items():
        pb["wing_%s_upper" % side].rotation_quaternion = wrot("wing_%s_upper" % side, Y, -s * g["wing_out"] * (1 + 0.3 * math.sin(2 * math.pi * t * 2)))
    for i in range(1, 5):
        pb["tail_0%d" % i].rotation_quaternion = wrot("tail_0%d" % i, Z, 5 * math.sin(2 * math.pi * t - 0.5 * i))
    upd()
    for side, ph in (("l", 0.0), ("r", 0.5)):
        u = (t + ph) % 1.0
        if u < beta:
            dy, dz = S * (u / beta - 0.5), 0.0
        else:
            k = (u - beta) / (1 - beta)
            dy, dz = S * (0.5 - smooth(k)), lift * math.sin(math.pi * k)
        off = Vector((0, dy, dz))
        solve_leg(side, REST_ANKLE[side] + off + Vector((0, 0, 0.6 * dz)), REST_TOE[side] + off)

# ---------------- flight clips ----------------
FLY_PITCH = 72.0   # pitch the upright model forward to fly level

# Legs in the air keep the ankle at its modelled bend (TJ, 2026-10-09: in
# flight "i cant see its full legs it glitches out and i only see the sky").
# The leg is many small rigid scale pieces, and the skin weights switch from
# shin to foot above the ankle pivot, so any ankle fold (the old tuck swung
# the foot ~80 deg back, even 40% of that) opens a gap there and the toes
# come apart. The hip and knee are hidden in feathers, so the pose comes
# from them: foot_deg is where the foot (tarsus + toes) points, in the
# side plane, 0 = straight back, -90 = straight down, -180 = forward.
def _side_deg(v):
    return math.degrees(math.atan2(v[2], v[1]))
REST_THIGH_DEG = _side_deg(Vector(B["leg_l_thigh"][1]) - Vector(B["leg_l_thigh"][0]))
REST_KNEE = _side_deg(Vector(B["leg_l_shin"][1]) - Vector(B["leg_l_shin"][0])) - REST_THIGH_DEG
REST_ANKLE_DEG = _side_deg(Vector(B["leg_l_foot"][1]) - Vector(B["leg_l_foot"][0])) - REST_THIGH_DEG - REST_KNEE

def air_leg(side, s, foot_deg, knee_extra):
    """Aim thigh and shin so the foot, at its rest bend to the shin, points
    along foot_deg; knee_extra bends the knee past its rest angle."""
    thigh = foot_deg - REST_ANKLE_DEG - REST_KNEE - knee_extra
    shin = thigh + REST_KNEE + knee_extra
    for bone, deg in (("thigh", thigh), ("shin", shin)):
        a = math.radians(deg)
        aim("leg_%s_%s" % (side, bone), Vector((s * 0.03, math.cos(a), math.sin(a))))
    pb["leg_%s_foot" % side].rotation_quaternion = Quaternion()
    upd()

def wing_dirs(s, raise_deg, fold, sweep=0.0):
    """Wing segment directions (armature space, body level) for a wing raised
    raise_deg above horizontal (per segment), folded 0..1 at the wrist."""
    out = []
    for i, (r, back) in enumerate(zip(raise_deg, (sweep, sweep + 35 * fold, sweep + 85 * fold))):
        rr, bb = math.radians(r), math.radians(back + (10 if i == 2 else 0))
        d = Vector((s * math.cos(rr) * math.cos(bb), math.sin(bb), math.sin(rr)))
        out.append(d)
    return out

def flare_pose(t):
    """Braking (TJ: "if it slows down then its wings turn forward to slow
    momentum"; eagle landing reference): the body pitches nose-up, the wings
    sweep forward, spread, and stand at a steep angle to the air like air
    brakes, beating slowly and deeply; the tail fans down and the feet reach
    forward."""
    pb["pelvis"].rotation_quaternion = wrot("pelvis", X, FLY_PITCH - 38)   # ~38 deg nose-up
    pb["pelvis"].location = rest["pelvis"].inverted() @ Vector((0, 0.1, 0.25 + 0.02 * math.sin(2 * math.pi * t)))
    upd()
    aim("spine_01", Vector((0, -0.8, 0.6)))
    aim("chest", Vector((0, -0.75, 0.65)))
    aim("neck", Vector((0, -0.6, 0.8)))
    aim("head", Vector((0, -1, -0.1)))            # eyes stay on the landing spot
    for i, dz in enumerate((-0.6, -0.5, -0.45, -0.4)):
        aim("tail_0%d" % (i + 1), Vector((0, 0.8, dz)))
    for side, s in SIDE.items():
        air_leg(side, s, -165, -10)   # feet reaching forward for the landing, knee a touch straighter
        ph = [(t - lag) % 1.0 for lag in (0.0, 0.05, 0.1)]
        raise_deg = [15 + 30 * math.cos(2 * math.pi * u) for u in ph]
        dirs = wing_dirs(s, raise_deg, 0.0, sweep=-38)   # swept forward
        for bn, d in zip(("upper", "fore", "hand"), dirs):
            aim("wing_%s_%s" % (side, bn), d)
            twist_to("wing_%s_%s" % (side, bn), wing_normal(s, d, 55))

def flight_pose(t, flap):
    # Body level, legs tucked back under the tail, head stretched forward.
    pb["pelvis"].rotation_quaternion = wrot("pelvis", X, FLY_PITCH)   # +X rotation tips the top (head) forward, toward -Y
    pb["pelvis"].location = rest["pelvis"].inverted() @ Vector((0, 0.15, 0.25))
    heave = 0.0
    if flap:
        # The body rises on the downstroke, settles on the upstroke.
        heave = 0.03 * math.sin(2 * math.pi * t - 0.4)
        pb["pelvis"].location = rest["pelvis"].inverted() @ Vector((0, 0.15, 0.25 + heave))
    upd()
    aim("spine_01", Vector((0, -1, 0.05)))
    aim("chest", Vector((0, -1, 0.10)))
    aim("neck", Vector((0, -0.8, 0.6)))
    aim("head", Vector((0, -1, -0.15 + (0.06 * math.sin(2 * math.pi * t) if flap else 0))))
    for i, dz in enumerate((-0.15, -0.08, -0.04, 0.0)):
        aim("tail_0%d" % (i + 1), Vector((0, 1, dz + (0.05 * math.sin(2 * math.pi * t - 0.6 * i) if flap else 0.02 * math.sin(2 * math.pi * t)))))
    for side, s in SIDE.items():
        air_leg(side, s, -60, 20)   # tucked: knee drawn up, feet trailing back under the tail
        if flap:
            # Downstroke over the first 55% (spread, sweeping down, tip
            # lagging), upstroke over the rest (folded at the wrist).
            ph = [(t - lag) % 1.0 for lag in (0.0, 0.04, 0.09)]
            def stroke(u):
                return 50 - 85 * smooth(u / 0.55) if u < 0.55 else -35 + 85 * smooth((u - 0.55) / 0.45)   # +50 to -35 deg (was +60 to -40: tore the shoulders)
            fold = math.sin(math.pi * smooth((t - 0.5) / 0.5)) ** 2 if t > 0.5 else 0.0
            dirs = wing_dirs(s, [stroke(u) for u in ph], fold, sweep=-5)
        else:
            flex = math.sin(2 * math.pi * t)
            dirs = wing_dirs(s, [6 + 1.5 * flex, 3 + 2.5 * flex, 8 + 4 * flex], 0.0)
        for bn, d in zip(("upper", "fore", "hand"), dirs):
            aim("wing_%s_%s" % (side, bn), d)
            # Flat, with a slight angle of attack (a little more on the
            # downstroke, where the wing pulls).
            twist_to("wing_%s_%s" % (side, bn), wing_normal(s, d, 8 if flap and t < 0.55 else 4))

# ---------------- bake ----------------
CLIPS = {
    "Idle":  dict(frames=180, ground=None),
    "Walk":  dict(frames=18, ground=dict(S=0.30, beta=0.6, lift=0.08, bob=0.012, lean=6, wing_out=3)),
    "Run":   dict(frames=12, ground=dict(S=0.42, beta=0.45, lift=0.12, bob=0.02, lean=14, wing_out=10)),
    "Fly":   dict(frames=21, flap=True),     # ~1.4 wingbeats a second at 1x
    "Glide": dict(frames=60, flap=False),
    "Flare": dict(frames=30, flare=True),    # braking: ~1 slow, deep beat a second, wings forward
}
for a in list(bpy.data.actions):
    bpy.data.actions.remove(a)
speeds = {}
for name, c in CLIPS.items():
    act = bpy.data.actions.new(name)
    arm.animation_data_create(); arm.animation_data.action = act
    N = c["frames"]; prev = {}
    for f in range(N + 1):
        t = (f % N) / N
        reset()
        if c.get("flare"):
            flare_pose(t)
        elif "flap" in c:
            flight_pose(t, c["flap"])
        else:
            ground_pose(t, c["ground"])
        for p in pb:
            q = p.rotation_quaternion.copy()
            if p.name in prev and prev[p.name].dot(q) < 0:
                q.negate()
            p.rotation_quaternion = q; prev[p.name] = q
            p.keyframe_insert("rotation_quaternion", frame=f + 1)
            p.keyframe_insert("location", frame=f + 1)
    act.use_fake_user = True
    g = c.get("ground")
    if g:
        speeds[name] = g["S"] / (g["beta"] * N / FPS)
        log.append("%s: %d frames, stride %.2f, duty %.2f -> %.2f m/s (model)" % (name, N, g["S"], g["beta"], speeds[name]))
open(OUT + "/speeds.txt", "w").write("\n".join("%s %.4f" % kv for kv in speeds.items()))

# ---------------- previews: rows Idle/Walk/Fly/Glide, side and front ----------------
scn.render.engine = 'BLENDER_WORKBENCH'; scn.display.shading.color_type = 'TEXTURE'; scn.display.shading.light = 'STUDIO'
R = 420
scn.render.resolution_x = scn.render.resolution_y = R
cd = bpy.data.cameras.new("c"); cd.type = 'ORTHO'; cd.ortho_scale = 2.4
cam = bpy.data.objects.new("c", cd); scn.collection.objects.link(cam); scn.camera = cam
rows = []
for act, view in (("Fly", "side"), ("Fly", "front"), ("Glide", "front"), ("Flare", "side"), ("Flare", "front")):
    arm.animation_data.action = bpy.data.actions[act]
    N = CLIPS[act]["frames"]; row = []
    if view == "side":
        cam.location = Vector((5, 0, 0.9)); cam.rotation_euler = (math.radians(90), 0, math.radians(90))
    else:
        cam.location = Vector((0, -5, 0.7)); cam.rotation_euler = (math.radians(90), 0, 0)
    for i in range(6):
        scn.frame_set(1 + round(i * N / 6))
        scn.render.filepath = OUT + "/_f.png"; bpy.ops.render.render(write_still=True)
        img = bpy.data.images.load(OUT + "/_f.png"); row.append(np.array(img.pixels[:]).reshape(R, R, 4)); bpy.data.images.remove(img)
    rows.append(np.concatenate(row, axis=1))
sheet = np.concatenate(rows[::-1], axis=0)
img = bpy.data.images.new("sheet", sheet.shape[1], sheet.shape[0], alpha=True)
img.pixels[:] = sheet.ravel(); img.filepath_raw = OUT + "/sheet_Stormrook.png"; img.file_format = 'PNG'; img.save()

# ---------------- export ----------------
arm.animation_data.action = bpy.data.actions["Idle"]; scn.frame_set(1)
for o in list(scn.objects):
    if o.type == 'CAMERA':
        bpy.data.objects.remove(o, do_unlink=True)
bpy.ops.wm.save_as_mainfile(filepath=RIGS + "/Stormrook_Rigged.blend")
bpy.ops.object.select_all(action='DESELECT'); mesh.select_set(True); arm.select_set(True)
bpy.ops.export_scene.fbx(filepath=RIGS + "/Stormrook_Rigged.fbx", use_selection=True, object_types={'ARMATURE', 'MESH'},
    add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
    bake_anim_simplify_factor=0.0, path_mode='COPY', embed_textures=True, mesh_smooth_type='FACE', apply_unit_scale=True)
open(OUT + "/rig_log.txt", "w").write("\n".join(log))
