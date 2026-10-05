# Rigs and animates the decimated, aligned Mossling (head toward -Y, feet on z=0).
# Same pipeline as glacielle_rig_rig_glacielle.py. Landmarks were measured from
# the mesh (mossling/legs.txt) and the side view; Mossling already stands
# naturally, so there are no stance corrections. Its antlers and their trailing
# leaves sweep back over the spine, so everything above HEAD_Z is pinned to the
# head bone rather than weighted by distance.
import bpy, math, mathutils
from mathutils import Vector, Quaternion
OUT = r"C:/Users/camer/AppData/Local/Temp/claude/C--Users-camer-Astral-Wilds-Unreal/441c051f-c68c-43b1-95d3-9b5092b9c38c/scratchpad/rig/mossling"
bpy.ops.wm.open_mainfile(filepath=OUT + "/mossling_aligned.blend")
scn = bpy.context.scene
mesh = bpy.data.objects["Mossling"]
log = []

# ---------------- skeleton ----------------
CX = -0.08
HEAD_Z = 1.05
B = {  # name: (head, tail, parent)
    "root":      ((CX, 0.05, 0.0), (CX, -0.15, 0.0), None),
    "pelvis":    ((CX, 0.28, 0.66), (CX, 0.02, 0.68), "root"),
    "spine_01":  ((CX, 0.02, 0.68), (CX, -0.25, 0.70), "pelvis"),
    "chest":     ((CX, -0.25, 0.70), (CX, -0.45, 0.78), "spine_01"),
    "neck":      ((CX, -0.45, 0.78), (-0.10, -0.52, 1.02), "chest"),
    "head":      ((-0.10, -0.52, 1.02), (-0.12, -0.80, 1.10), "neck"),
    "tail_01":   ((CX, 0.30, 0.62), (-0.14, 0.45, 0.52), "pelvis"),
    "tail_02":   ((-0.14, 0.45, 0.52), (-0.21, 0.58, 0.50), "tail_01"),
    "tail_03":   ((-0.21, 0.58, 0.50), (-0.28, 0.68, 0.58), "tail_02"),
    "tail_04":   ((-0.28, 0.68, 0.58), (-0.34, 0.80, 0.62), "tail_03"),
}
# Legs traced from height slices (mossling/legs.txt); right is -X (facing -Y).
# Front legs are near-vertical columns; back legs are digitigrade (hock at
# z~0.22 sits behind the paw). Front toe/wrist are refined below from the paws.
LEGS = {
    "fr": [(-0.20, -0.40, 0.62), (-0.26, -0.43, 0.34), (-0.24, -0.47, 0.15), (-0.24, -0.65, 0.01), "chest"],
    "fl": [(0.04, -0.36, 0.62), (0.06, -0.38, 0.34), (0.06, -0.42, 0.15), (0.09, -0.56, 0.01), "chest"],
    "bl": [(0.08, 0.15, 0.62), (0.11, 0.13, 0.42), (0.13, 0.32, 0.22), (0.18, 0.13, 0.01), "pelvis"],
    "br": [(-0.22, 0.15, 0.62), (-0.28, 0.18, 0.42), (-0.31, 0.33, 0.22), (-0.36, 0.16, 0.01), "pelvis"],
}
_low = [v.co.copy() for v in mesh.data.vertices if v.co.z < 0.06 and v.co.y < -0.2]
for k, side in (("fr", lambda v: v.x < CX), ("fl", lambda v: v.x >= CX)):
    pts = [v for v in _low if side(v)]
    if pts:
        c = sum(pts, Vector((0, 0, 0))) / len(pts)
        LEGS[k][2] = (c.x, c.y + 0.04, 0.15)        # wrist above the paw centre
        LEGS[k][3] = (c.x, min(p.y for p in pts) + 0.03, 0.01)  # toe at the paw front
for k, (a_, kn, an, toe, par) in LEGS.items():
    B["%s_upper" % k] = (a_, kn, par)
    B["%s_lower" % k] = (kn, an, "%s_upper" % k)
    B["%s_foot" % k] = (an, toe, "%s_lower" % k)

arm_data = bpy.data.armatures.new("MosslingRig")
arm = bpy.data.objects.new("MosslingRig", arm_data)
scn.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='EDIT')
eb = {}
for name, (h, t, par) in B.items():
    b = arm_data.edit_bones.new(name)
    b.head, b.tail = Vector(h), Vector(t)
    b.roll = 0.0
    eb[name] = b
for name, (h, t, par) in B.items():
    if par:
        eb[name].parent = eb[par]
        eb[name].use_connect = (Vector(eb[par].tail) - Vector(h)).length < 1e-4
eb["root"].use_deform = False
bpy.ops.object.mode_set(mode='OBJECT')
log.append("bones: %d; front paws %s" % (len(arm_data.bones), {k: (tuple(round(x,2) for x in LEGS[k][2]), tuple(round(x,2) for x in LEGS[k][3])) for k in ("fr","fl")}))

# ---------------- skinning ----------------
# Heat weighting needs every bone inside a closed volume, which Mossling's
# shard-covered surface isn't, so weights come from distance to each bone
# segment instead: top-3 nearest bones, inverse 6th-power falloff, normalised.
import numpy as np
deform = [b for b in arm_data.bones if b.use_deform]
heads = np.array([tuple(b.head_local) for b in deform]); tails = np.array([tuple(b.tail_local) for b in deform])
V = np.array([tuple(v.co) for v in mesh.data.vertices])
seg = tails - heads
seglen2 = np.maximum((seg ** 2).sum(1), 1e-9)
D = np.empty((len(V), len(deform)))
for i in range(len(deform)):
    t = np.clip(((V - heads[i]) @ seg[i]) / seglen2[i], 0.0, 1.0)
    closest = heads[i] + t[:, None] * seg[i]
    D[:, i] = np.sqrt(((V - closest) ** 2).sum(1))
W = 1.0 / (np.maximum(D, 0.01) ** 6)
head_i = [b.name for b in deform].index("head")
W[V[:, 2] > HEAD_Z] = 0.0
W[V[:, 2] > HEAD_Z, head_i] = 1.0
top = np.argsort(-W, axis=1)[:, :3]
mesh.vertex_groups.clear()
groups = [mesh.vertex_groups.new(name=b.name) for b in deform]
for vi in range(len(V)):
    idx = top[vi]; w = W[vi, idx]; w = w / w.sum()
    for j, wj in zip(idx, w):
        if wj > 0.01: groups[j].add([vi], float(wj), 'REPLACE')
for m in list(mesh.modifiers): mesh.modifiers.remove(m)
am = mesh.modifiers.new("Armature", 'ARMATURE'); am.object = arm
mesh.parent = arm
unweighted = sum(1 for v in mesh.data.vertices if not any(g.weight > 0.01 for g in v.groups))
log.append("distance weights: unweighted %d / %d; verts per bone: %s" % (unweighted, len(V), {b.name: int((top[:, 0] == i).sum()) for i, b in enumerate(deform)}))

# ---------------- animation helpers ----------------
FPS = 30
scn.render.fps = FPS
pb = arm.pose.bones
for p in pb: p.rotation_mode = 'QUATERNION'
rest = {b.name: b.matrix_local.to_3x3() for b in arm_data.bones}

def world_rot(bone, axis, deg):
    """Quaternion for a rotation about an armature-space axis, expressed in the bone's rest frame."""
    ax = (rest[bone].inverted() @ Vector(axis)).normalized()
    return Quaternion(ax, math.radians(deg))

def seg_angle(p0, p1):
    d = Vector(p1) - Vector(p0)
    return math.degrees(math.atan2(d.y, -d.z))   # 0 = straight down, + = toward +Y (back)

def stand_offsets(k):
    """Per-segment rotations about X that make upper and lower vertical and the paw flat, pointing forward (-Y)."""
    a_, kn, an, toe, _ = LEGS[k]
    au, al = seg_angle(a_, kn), seg_angle(kn, an)
    d = Vector(toe) - Vector(an)
    af = math.degrees(math.atan2(d.y, -d.z))     # -90 = horizontal forward
    c1 = -au
    c2 = -(al + c1)
    c3 = -(c1 + c2)   # keep the modelled paw angle in world space
    return c1, c2, c3

def key_pose(frame, poses, locs=None):
    for name, q in poses.items():
        pb[name].rotation_quaternion = q
        pb[name].keyframe_insert("rotation_quaternion", frame=frame)
    for name, l in (locs or {}).items():
        pb[name].location = l
        pb[name].keyframe_insert("location", frame=frame)

X, Z, Yax = (1, 0, 0), (0, 0, 1), (0, 1, 0)
STAND = {k: (0.0, 0.0, 0.0) for k in LEGS}   # natural stance - no corrections
log.append("stand offsets upper/lower/foot (deg about X): %s" % {k: tuple(round(x, 1) for x in v) for k, v in STAND.items()})

def make_action(name, frames, leg_phase, swing, lift_bend, bob, tail_amp, extra=None):
    act = bpy.data.actions.new(name)
    arm.animation_data_create(); arm.animation_data.action = act
    for f in range(frames + 1):
        t = f / frames
        poses, locs = {}, {}
        for k in LEGS:
            ph = 2 * math.pi * (t + leg_phase[k])
            # Upper leg swings about X around a vertical (stand-corrected) leg.
            # Sign: +deg about +X moves a downward-pointing paw toward +Y (backward).
            # Paw y ~ sin(ph): moving back (stance, on ground) when cos>0, forward (swing, in air) when cos<0.
            c1, c2, c3 = STAND[k]
            poses["%s_upper" % k] = world_rot("%s_upper" % k, X, c1 + swing * math.sin(ph))
            # Lift: fold the lower leg back/up only during the swing phase, toe angled down.
            fold = lift_bend * max(0.0, -math.cos(ph))
            poses["%s_lower" % k] = world_rot("%s_lower" % k, X, c2 + fold)
            poses["%s_foot" % k] = world_rot("%s_foot" % k, X, c3 - fold * 0.5)
        locs["pelvis"] = Vector((0, 0, 0)) + Vector((0, 0, 0))
        poses["pelvis"] = world_rot("pelvis", X, bob * 3 * math.sin(4 * math.pi * t))
        poses["chest"] = world_rot("chest", X, -bob * 2 * math.sin(4 * math.pi * t + 0.6))
        poses["head"] = world_rot("head", X, bob * 2.5 * math.sin(4 * math.pi * t + 1.4))
        for i in range(1, 5):
            poses["tail_0%d" % i] = world_rot("tail_0%d" % i, Z, tail_amp * math.sin(2 * math.pi * t - 0.5 * i))
        if extra: extra(t, poses)
        key_pose(f + 1, poses)
        # body bob through root translation (cm-scale in metres)
        pb["root"].location = Vector((0, 0, bob * 0.01 * abs(math.sin(2 * math.pi * t * 2))))
        pb["root"].keyframe_insert("location", frame=f + 1)
    for fc in act.fcurves if hasattr(act, "fcurves") else []:
        for kp in fc.keyframe_points: kp.interpolation = 'LINEAR'
    act.use_fake_user = True
    return act

walk_phase = {"bl": 0.0, "fl": 0.25, "br": 0.5, "fr": 0.75}       # lateral-sequence walk
run_phase = {"fl": 0.0, "fr": 0.08, "bl": 0.5, "br": 0.58}        # bound
idle_phase = {k: 0.0 for k in LEGS}

def idle_extra(t, poses):
    poses["chest"] = world_rot("chest", X, 1.5 * math.sin(2 * math.pi * t))             # breathing
    poses["head"] = world_rot("head", Z, 6 * math.sin(2 * math.pi * t)) @ world_rot("head", X, 2 * math.sin(4 * math.pi * t))

actions = [
    make_action("Idle", 90, idle_phase, swing=0.0, lift_bend=0.0, bob=0.0, tail_amp=7.0, extra=idle_extra),
    make_action("Walk", 30, walk_phase, swing=22.0, lift_bend=28.0, bob=1.0, tail_amp=6.0),
    make_action("Run", 18, run_phase, swing=34.0, lift_bend=42.0, bob=2.0, tail_amp=10.0),
]
log.append("actions: %s" % [a.name for a in actions])

# ---------------- previews ----------------
scn.render.engine = 'BLENDER_WORKBENCH'; scn.display.shading.color_type = 'TEXTURE'; scn.display.shading.light = 'STUDIO'
scn.render.resolution_x = 600; scn.render.resolution_y = 600
cam = scn.camera
cam.data.ortho_scale = 2.2
cam.location = Vector((5, 0, 0.95)); cam.rotation_euler = (math.radians(90), 0, math.radians(90))
for act, frames in (("Walk", [1, 8, 16, 23]), ("Run", [1, 5, 10, 14]), ("Idle", [1, 45])):
    arm.animation_data.action = bpy.data.actions[act]
    for f in frames:
        scn.frame_set(f)
        scn.render.filepath = OUT + "/anim_%s_%02d.png" % (act, f)
        bpy.ops.render.render(write_still=True)

# ---------------- export ----------------
arm.animation_data.action = bpy.data.actions["Idle"]
scn.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=OUT + "/mossling_rigged.blend")
bpy.ops.object.select_all(action='DESELECT'); mesh.select_set(True); arm.select_set(True)
bpy.ops.export_scene.fbx(filepath=OUT + "/Mossling_Rigged.fbx", use_selection=True,
    object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False, bake_anim=True,
    bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0.0,
    path_mode='COPY', embed_textures=True, mesh_smooth_type='FACE', apply_unit_scale=True)
open(OUT + "/rig_log.txt", "w").write("\n".join(log))
