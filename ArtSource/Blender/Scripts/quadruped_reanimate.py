# Re-authors Idle / Walk / Run on an already rigged + skinned Astral
# (Rigs/<Name>_Rigged.blend, 22-bone quadruped from <name>_rig_rig_<name>.py)
# and re-exports Rigs/<Name>_Rigged.fbx. Same skeleton and action names, so a
# re-import replaces the existing UE animation assets in place.
#
#   blender -b --python quadruped_reanimate.py -- Glacielle <out_dir>
#
# What makes it more fluid than the first pass (plain per-bone sine waves):
# - Feet are driven by IK along authored foot paths: planted and still during
#   stance (no skating), a smooth C1 arc during swing, heel lift at toe-off.
# - Every curve is smooth (no |sin| or max(0, x) kinks), sampled at 60 fps.
# - Overlapping body motion: pelvis/chest counter-rotate and roll with each
#   diagonal, the head is partly stabilised in world space (it floats rather
#   than bobbing with the body), and the tail is a travelling wave.
# - Walk/Run speeds come from the foot paths, so the engine can match cadence
#   to ground speed exactly (written to <out_dir>/speeds.txt, model metres/s).
import bpy, math, sys
from mathutils import Vector, Quaternion, Matrix

argv = sys.argv[sys.argv.index("--") + 1:]
NAME, OUT = argv[0], argv[1]
RIGS = r"C:/Users/camer/Astral Wilds/ArtSource/Blender/Rigs"
bpy.ops.wm.open_mainfile(filepath="%s/%s_Rigged.blend" % (RIGS, NAME))
scn = bpy.context.scene
arm = [o for o in scn.objects if o.type == 'ARMATURE'][0]
mesh = [o for o in scn.objects if o.type == 'MESH'][0]
assert arm.matrix_world == Matrix.Identity(4), "expects the armature at the origin"
log = []

FPS = 60
scn.render.fps = FPS
TAU = 2 * math.pi
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))

for a in list(bpy.data.actions):
    bpy.data.actions.remove(a)
bones, pb = arm.data.bones, arm.pose.bones
for p in pb:
    p.rotation_mode = 'QUATERNION'
deform = [b.name for b in bones]
rest = {b.name: b.matrix_local.to_3x3() for b in bones}
armmod = [m for m in mesh.modifiers if m.type == 'ARMATURE'][0]
armmod.show_viewport = False          # solving only needs the armature
upd = bpy.context.view_layer.update

def smooth(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3 - 2 * x)

def wrot(bone, axis, deg):
    """Rotation about an armature-space axis, expressed in the bone's rest frame."""
    return Quaternion((rest[bone].inverted() @ axis).normalized(), math.radians(deg))

# ---------------- legs ----------------
LEGS = ("fl", "fr", "bl", "br")
leg = {}
for k in LEGS:
    H, K = bones[k + "_upper"].head_local.copy(), bones[k + "_lower"].head_local.copy()
    A, T = bones[k + "_foot"].head_local.copy(), bones[k + "_foot"].tail_local.copy()
    L1, L2 = (K - H).length, (A - K).length
    # A rig modelled mid-stride (Ironbur's lifted right fore paw) stores where
    # that toe stands as "plant_<leg>" on the armature. Gaits and Idle plant it
    # there instead of at the rest pose.
    if arm.get("plant_" + k) is not None:
        shift = Vector(arm["plant_" + k]) - T
        T, A = T + shift, A + shift
        log.append("%s planted: toe moved %s" % (k, tuple(round(c, 3) for c in shift)))
    d = A - H
    front = k[0] == "f"
    # Anatomical bend in the sagittal plane: elbow back for front legs,
    # stifle forward for hind. (The model's own rest bend includes its
    # sideways splay, which bent knees outward.)
    pole = (Y if front else -Y).copy()
    leg[k] = dict(H=H, A=A, T=T, L1=L1, L2=L2, pole=pole, front=front, d0=d.length)
log.append("legs: " + ", ".join("%s reach %.3f rest %.3f pole %s" % (k, v["L1"] + v["L2"], v["d0"], tuple(round(c, 2) for c in v["pole"])) for k, v in leg.items()))

def max_half_stride(drop):
    """Largest half-stride every leg can reach with the body lowered by drop."""
    m = 1.0
    for v in leg.values():
        vert = (v["H"].z - drop) - v["A"].z
        reach = 0.985 * (v["L1"] + v["L2"])
        m = min(m, math.sqrt(max(reach * reach - vert * vert, 0.0)))
    return m

def stance_centres(drop, hs, margin=0.985):
    """Per-leg shift (m along Y) of the stance stroke's centre from the rest toe.

    The ankle's stroke (rest ankle +/- hs) has to stay within the hip's
    horizontal reach. Zero when it already does (paws under the hips, as on
    Glacielle and Mossling). Ironbur leans onto fore paws ~0.36m ahead of the
    shoulders, so its front stance is pulled back just far enough to fit."""
    c = {}
    for k, v in leg.items():
        vert = (v["H"].z - drop) - v["A"].z
        reach = margin * (v["L1"] + v["L2"])
        horiz = math.sqrt(max(reach * reach - vert * vert, 0.0))
        lo = v["H"].y - horiz + hs - v["A"].y
        hi = v["H"].y + horiz - hs - v["A"].y
        c[k] = min(max(0.0, lo), hi) if lo <= hi else (lo + hi) / 2
    return c

def hind_stroke(drop, land, push, margin=0.93):
    """Gallop stroke for the hind legs: (stride, offset of its centre from the rest toe).

    A galloping hind leg reaches far forward under the belly and pushes off far
    behind the hip, so its stroke is longer than the front legs' and sits
    forward of where the foot stands. The limits come from how far the
    hip-to-hock pair reaches (lowered by drop), with the cannon tilted to its
    landing / push-off pitch, so every rig gets the longest stroke it can do."""
    fwd = back = 1.0
    off = []
    for k in ("bl", "br"):
        v = leg[k]
        H, T0, A0 = v["H"], v["T"], v["A"]
        reach = margin * (v["L1"] + v["L2"])
        def toe_limit(pitch, sign):
            c = Matrix.Rotation(math.radians(pitch), 3, 'X') @ (A0 - T0)   # toe -> hock
            vert = (H.z - drop) - (T0.z + c.z)
            horiz = math.sqrt(max(reach * reach - vert * vert, 0.0))
            return (H.y + sign * horiz) - c.y                               # toe y with the hock at full reach
        f, b = H.y - toe_limit(land, -1), toe_limit(push, +1) - H.y        # distances ahead of / behind the hip
        fwd, back = min(fwd, f), min(back, b)
        off.append(H.y - T0.y)
    S = fwd + back
    centre = (back - fwd) / 2                                               # relative to the hip
    return S, centre + sum(off) / len(off)                                  # relative to the rest toe

def foot_targets(k, u, g):
    """Toe and ankle targets for leg k at gait phase u (0..1)."""
    v = leg[k]
    T0, A0 = v["T"], v["A"]
    h = g.get("hind") if not v["front"] else None
    if h:
        return hind_gallop_targets(T0, A0, u, g, h)
    S, b = g["S"], g["beta"]
    if u < b:                                   # stance: planted, sliding back at constant speed relative to the body
        s = u / b
        dy, dz = S * (s - 0.5), 0.0
        pitch = g["toeoff"] * smooth((s - 0.65) / 0.35)
    else:                                       # swing: C1 Hermite back -> front, lifted
        s = (u - b) / (1 - b)
        m = S / b * (1 - b) * 0.35              # leaves and lands with a share of the stance velocity
        h00, h10, h01, h11 = 2*s**3 - 3*s**2 + 1, s**3 - 2*s**2 + s, -2*s**3 + 3*s**2, s**3 - s**2
        dy = h00 * (S / 2) + h10 * m + h01 * (-S / 2) + h11 * m
        dz = g["lift"] * math.sin(math.pi * s ** 0.8) ** 2
        pitch = g["toeoff"] * (1 - smooth(s / 0.55))
    dy += g["centre"][k]
    toe = Vector((T0.x, T0.y + dy, T0.z + dz))
    ankle = toe + Matrix.Rotation(math.radians(pitch), 3, 'X') @ (A0 - T0)
    return toe, ankle

def hind_gallop_targets(T0, A0, u, g, h):
    """Hind leg in the gallop. Stance: lands reaching under the belly with the
    cannon sloped forward, rolls over the hoof, and pushes off far behind on
    its toe. Swing: the hock folds (hoof tucked up and back, cannon near level)
    while the leg comes through, then unfolds and reaches for the next landing."""
    S, b, off = h["S"], h["beta"], h["off"]
    if u < b:
        s = u / b
        dy, dz = off + S * (s - 0.5), 0.0
        pitch = h["land"] * (1 - smooth(s / 0.4)) + h["push"] * smooth((s - 0.55) / 0.45)
    else:
        s = (u - b) / (1 - b)
        m = S / b * (1 - b) * 0.25
        # The hoof lingers behind while the hock folds, then sweeps forward.
        w = s * s * (3 - 2 * s) * 0.6 + s * 0.4
        h00, h10, h01, h11 = 2*w**3 - 3*w**2 + 1, w**3 - 2*w**2 + w, -2*w**3 + 3*w**2, w**3 - w**2
        dy = off + h00 * (S / 2) + h10 * m + h01 * (-S / 2) + h11 * m
        dz = h["lift"] * math.sin(math.pi * s ** 0.75) ** 2
        fold = math.sin(math.pi * min(s / 0.8, 1.0)) ** 2 * h["fold"]
        pitch = (h["push"] * (1 - smooth(s / 0.3))            # off the toe ...
                 + fold                                       # ... hock folds, hoof tucked ...
                 + h["land"] * smooth((s - 0.6) / 0.4))       # ... and reaches out to land
    toe = Vector((T0.x, T0.y + dy, T0.z + dz))
    ankle = toe + Matrix.Rotation(math.radians(pitch), 3, 'X') @ (A0 - T0)
    return toe, ankle

def aim(name, target):
    """Rotate a bone (in pose space, about its head) so its tail points at target."""
    p = pb[name]
    M = p.matrix.copy()
    head = M.translation.copy()
    q = (M.to_3x3() @ Y).rotation_difference(target - head)
    p.matrix = Matrix.Translation(head) @ q.to_matrix().to_4x4() @ Matrix.Translation(-head) @ M
    p.scale = Vector((1, 1, 1))     # rotation only: the matrix setter leaks parent scale into the child
    upd()

def solve_leg(k, toe, ankle):
    v = leg[k]
    H = pb[k + "_upper"].matrix.translation.copy()
    d = ankle - H
    dl = min(max(d.length, abs(v["L1"] - v["L2"]) + 1e-4), (v["L1"] + v["L2"]) * 0.999)
    n = d.normalized()
    a = (v["L1"] ** 2 - v["L2"] ** 2 + dl * dl) / (2 * dl)
    h = math.sqrt(max(v["L1"] ** 2 - a * a, 0.0))
    perp = v["pole"] - n * v["pole"].dot(n)
    perp.normalize()
    knee = H + n * a + perp * h
    aim(k + "_upper", knee)
    aim(k + "_lower", H + n * dl)
    aim(k + "_foot", toe)

def stabilise_head(amount, look):
    """Blend the head's pose rotation toward its rest orientation (world-steady), then apply a look offset."""
    p = pb["head"]
    M = p.matrix.copy()
    cur = M.to_quaternion()
    steady = rest["head"].to_quaternion()
    q = look @ cur.slerp(steady, amount)
    p.matrix = Matrix.Translation(M.translation) @ q.to_matrix().to_4x4()
    p.scale = Vector((1, 1, 1))
    upd()

# ---------------- gaits ----------------
def tail_wave(poses, t, yaw_amp, pitch_amp, freq=1, lag=0.55):
    for i in range(1, 5):
        g = 0.55 + 0.2 * i                      # amplitude grows toward the tip
        poses["tail_0%d" % i] = (wrot("tail_0%d" % i, Z, yaw_amp * g * math.sin(TAU * freq * t - lag * i))
                                 @ wrot("tail_0%d" % i, X, pitch_amp * g * math.sin(TAU * freq * t - lag * i + 0.8)))

def body_walk(t, g):
    poses = {}
    bob = -g["drop"] - g["bob"] * math.cos(2 * TAU * (t - 0.12))         # lowest just after each diagonal lands
    sway = math.sin(TAU * t)
    poses["pelvis"] = wrot("pelvis", Y, 2.5 * sway) @ wrot("pelvis", Z, 2.0 * math.sin(TAU * t + 0.6))
    poses["spine_01"] = wrot("spine_01", Z, -1.0 * math.sin(TAU * t + 0.9))
    poses["chest"] = wrot("chest", Y, -3.0 * sway) @ wrot("chest", Z, -2.0 * math.sin(TAU * t + 1.2)) @ wrot("chest", X, 1.0 * math.sin(2 * TAU * t))
    poses["neck"] = wrot("neck", X, 2.0 * math.sin(2 * TAU * (t - 0.2)))
    tail_wave(poses, t, 7.0, 3.0)
    look = Quaternion(Z, math.radians(2.0 * math.sin(TAU * t + 1.8)))
    return poses, Vector((0, 0, bob)), 0.6, look

def body_run(t, g):
    """Fast trot: two beats per cycle, one per diagonal pair."""
    poses = {}
    bob = -g["drop"] - g["bob"] * math.cos(2 * TAU * (t - 0.2))         # lowest mid-stance of each diagonal
    sway = math.sin(TAU * t)
    poses["pelvis"] = wrot("pelvis", Y, 4.0 * sway) @ wrot("pelvis", Z, 3.0 * math.sin(TAU * t + 0.6))
    poses["spine_01"] = wrot("spine_01", Z, -1.5 * math.sin(TAU * t + 0.9))
    poses["chest"] = (wrot("chest", Y, -4.5 * sway) @ wrot("chest", Z, -3.0 * math.sin(TAU * t + 1.2))
                      @ wrot("chest", X, 2.0 * math.sin(2 * TAU * (t - 0.1))))
    poses["neck"] = wrot("neck", X, 3.0 * math.sin(2 * TAU * (t - 0.25)))
    tail_wave(poses, t, 6.0, 6.0, freq=2, lag=0.6)
    return poses, Vector((0, 0, bob)), 0.6, Quaternion()

def body_idle(t, g):
    poses = {}
    breath = math.sin(2 * TAU * t)                                       # 2 breaths per loop
    shift = math.sin(TAU * t)                                            # slow weight shift
    poses["pelvis"] = wrot("pelvis", Y, 1.5 * shift)
    poses["spine_01"] = wrot("spine_01", X, 0.6 * breath)
    poses["chest"] = wrot("chest", X, 1.2 * breath) @ wrot("chest", Y, -1.0 * shift)
    yaw = 14.0 * math.sin(TAU * t + 0.4) + 5.0 * math.sin(2 * TAU * t + 1.7)
    pitch = 4.0 * math.sin(2 * TAU * t + 0.3) + 2.0 * math.sin(3 * TAU * t + 2.2)
    poses["neck"] = wrot("neck", Z, 0.35 * yaw)                          # the neck leads, the head finishes the turn
    tail_wave(poses, t, 6.0, 2.5, lag=0.5)
    poses["tail_01"] = poses["tail_01"] @ wrot("tail_01", Z, 3.0 * math.sin(3 * TAU * t + 1.0))
    look = Quaternion(Z, math.radians(0.65 * yaw)) @ Quaternion(X, math.radians(pitch))
    loc = Vector((0.012 * shift, 0, 0.004 * breath - g["drop"]))
    return poses, loc, 0.5, look

# The Meshy models stand on near-straight legs, so stride length comes from
# lowering the body a little in each gait (drop), like a real animal's
# flexed stance.
hs_walk = max_half_stride(0.055)
hs_run = max_half_stride(0.075)
GAITS = {
    # Walk is a brisk diagonal walk/trot: wild Astrals wander at 200 cm/s,
    # a trotting pace for their size.
    "Walk": dict(frames=30, phase={"bl": 0.0, "fr": 0.06, "br": 0.5, "fl": 0.56}, beta=0.5,
                 S=2 * min(hs_walk, 0.26), lift=0.07, toeoff=28.0, drop=0.055, bob=0.012, body=body_walk),
    # Run is a fast trot: diagonal pairs alternate (bl+fr, then br+fl). The
    # front feet land a little after their hind partner so the two share a
    # mid-stance (the hind stance is longer, see below).
    "Run":  dict(frames=24, phase={"bl": 0.0, "fr": 0.91, "br": 0.5, "fl": 0.41}, beta=0.3,
                 S=2 * min(hs_run, 0.32), lift=0.12, toeoff=40.0, drop=0.075, bob=0.03, body=body_run),
    "Idle": dict(frames=360, phase=None, drop=0.0, body=body_idle),
}
# Hind legs get a longer stroke than the front (reaching under the belly,
# pushing off well behind) and fold the hock in the swing, so they visibly
# drive. They stay down for a matching share of the cycle so they slide
# back at the same speed as the front feet.
for name, g in GAITS.items():
    if g["phase"] is not None:
        # Opt-in ("fit_stance" on the armature): Glacielle/Mossling's approved
        # gaits keep their strides centred on the rest paws.
        g["centre"] = (stance_centres(g["drop"], g["S"] / 2) if arm.get("fit_stance")
                       else {k: 0.0 for k in LEGS})
        log.append("%s stance centres: %s" % (name, {k: round(c, 3) for k, c in g["centre"].items()}))
run = GAITS["Run"]
hS, hoff = hind_stroke(run["drop"], land=-12.0, push=45.0)
run["hind"] = dict(S=hS, off=hoff, beta=run["beta"] * hS / run["S"],
                   land=-12.0, push=45.0, fold=65.0, lift=0.13)
log.append("Run hind stroke %.3f m (front %.3f), centre %+.3f m from rest toe, duty %.2f"
           % (hS, run["S"], hoff, run["hind"]["beta"]))

# ---------------- bake ----------------
speeds = {}
for name, g in GAITS.items():
    act = bpy.data.actions.new(name)
    arm.animation_data_create()
    arm.animation_data.action = act
    N = g["frames"]
    prev = {}
    for f in range(N + 1):
        t = (f % N) / N                              # last frame == first frame: seamless loop
        for p in pb:
            p.rotation_quaternion = Quaternion()
            p.location = Vector()
            # Scale is never animated, but setting pose matrices leaked a little
            # into it, which compounded frame to frame (and run to run, via the
            # saved .blend) until Glacielle's foot was 2.4x long. Reset it.
            p.scale = Vector((1, 1, 1))
        poses, pelvis_off, stab, look = g["body"](t, g)
        for bn, q in poses.items():
            pb[bn].rotation_quaternion = q
        pb["pelvis"].location = rest["pelvis"].inverted() @ pelvis_off
        upd()
        stabilise_head(stab, look)
        for k in LEGS:
            if g["phase"] is None:
                toe, ankle = leg[k]["T"].copy(), leg[k]["A"].copy()
            else:
                toe, ankle = foot_targets(k, (t + g["phase"][k]) % 1.0, g)
            solve_leg(k, toe, ankle)
        for p in pb:
            q = p.rotation_quaternion.copy()
            if p.name in prev and prev[p.name].dot(q) < 0:
                q.negate()                            # keep interpolation on the short path
            p.rotation_quaternion = q
            prev[p.name] = q
            p.keyframe_insert("rotation_quaternion", frame=f + 1)
            p.keyframe_insert("location", frame=f + 1)
    act.use_fake_user = True
    if g["phase"] is not None:
        speeds[name] = g["S"] / (g["beta"] * N / FPS)          # stance covers S in beta of a cycle
        log.append("%s: %d frames (%.2fs), stride %.3f m, duty %.2f -> %.2f m/s (model)" % (name, N, N / FPS, g["S"], g["beta"], speeds[name]))
open(OUT + "/speeds.txt", "w").write("\n".join("%s %.4f" % kv for kv in speeds.items()))

# ---------------- previews ----------------
armmod.show_viewport = True
scn.render.engine = 'BLENDER_WORKBENCH'; scn.display.shading.color_type = 'TEXTURE'; scn.display.shading.light = 'STUDIO'
scn.render.resolution_x = scn.render.resolution_y = 500
cam = scn.camera
if cam is None:
    cd = bpy.data.cameras.new("c"); cd.type = 'ORTHO'
    cam = bpy.data.objects.new("c", cd); scn.collection.objects.link(cam); scn.camera = cam
cam.data.type = 'ORTHO'; cam.data.ortho_scale = max(2.4, mesh.dimensions.y + 0.4)   # Ironbur is ~2.8m long
cam.location = Vector((5, 0, 0.95)); cam.rotation_euler = (math.radians(90), 0, math.radians(90))
import numpy as np
COLS, R = 6, scn.render.resolution_x
rows = []
for act in ("Walk", "Run", "Idle"):
    arm.animation_data.action = bpy.data.actions[act]
    N = GAITS[act]["frames"]
    row = []
    for i in range(COLS):
        scn.frame_set(1 + round(i * N / COLS))
        scn.render.filepath = OUT + "/_frame.png"
        bpy.ops.render.render(write_still=True)
        img = bpy.data.images.load(OUT + "/_frame.png")
        row.append(np.array(img.pixels[:]).reshape(R, R, 4))
        bpy.data.images.remove(img)
    rows.append(np.concatenate(row, axis=1))
# Contact sheet: rows Walk / Run / Idle (top to bottom), 6 evenly spaced frames each.
sheet = np.concatenate(rows[::-1], axis=0)              # Blender pixels are bottom-up
out = bpy.data.images.new("sheet", sheet.shape[1], sheet.shape[0], alpha=True)
out.pixels[:] = sheet.ravel()
out.filepath_raw = OUT + "/sheet_%s.png" % NAME
out.file_format = 'PNG'
out.save()

# ---------------- export ----------------
arm.animation_data.action = bpy.data.actions["Idle"]
scn.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath="%s/%s_Rigged.blend" % (RIGS, NAME))
bpy.ops.object.select_all(action='DESELECT'); mesh.select_set(True); arm.select_set(True)
bpy.ops.export_scene.fbx(filepath="%s/%s_Rigged.fbx" % (RIGS, NAME), use_selection=True,
    object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False, bake_anim=True,
    bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0.0,
    path_mode='COPY', embed_textures=True, mesh_smooth_type='FACE', apply_unit_scale=True)
open(OUT + "/reanimate_log.txt", "w").write("\n".join(log))
