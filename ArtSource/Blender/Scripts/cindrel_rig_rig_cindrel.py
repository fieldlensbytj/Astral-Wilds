# Rigs and animates the aligned retopo Cindrel (head toward -Y, feet on z=0).
# Landmarks were measured from the mesh (see landmarks.txt); legs are given a
# per-leg "stand straight" offset so the gaits start from a neutral stance
# rather than Meshy's stepping pose.
import bpy, math, mathutils
from mathutils import Vector, Quaternion
OUT = r"C:/Users/camer/AppData/Local/Temp/claude/C--Users-camer-Astral-Wilds-Unreal/1f45ae34-9634-4dd9-ac1b-deb9025c0437/scratchpad/rig"
TEX = r"C:/Users/camer/Astral Wilds/Assets/Art/Astrals/Cindrel/Textures/Cindrel_Albedo_v02.png"
bpy.ops.wm.open_mainfile(filepath=OUT + "/cindrel_aligned.blend")
scn = bpy.context.scene
mesh = [o for o in scn.objects if o.type == 'MESH'][0]
mesh.name = "Cindrel"
log = []

# ---------------- skeleton ----------------
CX = -0.07
B = {  # name: (head, tail, parent)
    "root":      ((CX, 0.05, 0.0), (CX, -0.15, 0.0), None),
    "pelvis":    ((CX, 0.22, 0.62), (CX, 0.02, 0.64), "root"),
    "spine_01":  ((CX, 0.02, 0.64), (CX, -0.22, 0.66), "pelvis"),
    "chest":     ((CX, -0.22, 0.66), (CX, -0.44, 0.72), "spine_01"),
    "neck":      ((CX, -0.44, 0.72), (-0.02, -0.56, 0.98), "chest"),
    "head":      ((-0.02, -0.56, 0.98), (0.0, -0.80, 1.06), "neck"),
    "tail_01":   ((CX - 0.01, 0.30, 0.62), (CX, 0.52, 0.76), "pelvis"),
    "tail_02":   ((CX, 0.52, 0.76), (-0.03, 0.62, 1.02), "tail_01"),
    "tail_03":   ((-0.03, 0.62, 1.02), (0.02, 0.56, 1.32), "tail_02"),
    "tail_04":   ((0.02, 0.56, 1.32), (0.05, 0.44, 1.62), "tail_03"),
}
# Legs traced from height slices of the mesh (legs.txt): attach (shoulder/hip),
# knee, ankle, toe - bottom of the chain sits on the measured paw. FL is the
# lifted paw in Meshy's stepping pose.
LEGS = {
    "fr": [(-0.24, -0.45, 0.62), (-0.31, -0.60, 0.30), (-0.27, -0.72, 0.12), (-0.29, -0.82, 0.01), "chest"],
    "fl": [(0.05, -0.45, 0.62), (-0.04, -0.68, 0.38), (-0.15, -0.86, 0.24), (-0.16, -0.94, 0.20), "chest"],
    "bl": [(0.07, 0.12, 0.60), (0.10, 0.05, 0.38), (0.15, 0.00, 0.12), (0.15, -0.17, 0.01), "pelvis"],
    "br": [(-0.15, 0.33, 0.58), (-0.22, 0.25, 0.36), (-0.33, 0.29, 0.12), (-0.37, 0.17, 0.01), "pelvis"],
}
for k, (a_, kn, an, toe, par) in LEGS.items():
    B["%s_upper" % k] = (a_, kn, par)
    B["%s_lower" % k] = (kn, an, "%s_upper" % k)
    B["%s_foot" % k] = (an, toe, "%s_lower" % k)

arm_data = bpy.data.armatures.new("CindrelRig")
arm = bpy.data.objects.new("CindrelRig", arm_data)
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
log.append("bones: %d" % len(arm_data.bones))

# ---------------- skinning ----------------
bpy.ops.object.select_all(action='DESELECT')
mesh.select_set(True); arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.parent_set(type='ARMATURE_AUTO')
unweighted = sum(1 for v in mesh.data.vertices if not any(g.weight > 0.01 for g in v.groups))
log.append("vertex groups: %d, unweighted verts: %d / %d" % (len(mesh.vertex_groups), unweighted, len(mesh.data.vertices)))
if unweighted > 0.05 * len(mesh.data.vertices):
    log.append("automatic weights mostly failed - falling back to envelope weights")
    mesh.parent = None
    for m in list(mesh.modifiers): mesh.modifiers.remove(m)
    mesh.vertex_groups.clear()
    bpy.ops.object.select_all(action='DESELECT'); mesh.select_set(True); arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.parent_set(type='ARMATURE_ENVELOPE')
    unweighted = sum(1 for v in mesh.data.vertices if not any(g.weight > 0.01 for g in v.groups))
    log.append("after envelope: unweighted %d" % unweighted)

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
STAND = {k: stand_offsets(k) for k in LEGS}
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
mat = bpy.data.materials.new("CindrelPreview"); mat.use_nodes = True
nt = mat.node_tree; img = nt.nodes.new("ShaderNodeTexImage"); img.image = bpy.data.images.load(TEX)
nt.links.new(img.outputs["Color"], nt.nodes["Principled BSDF"].inputs["Base Color"])
if not mesh.data.materials: mesh.data.materials.append(mat)
scn.render.engine = 'BLENDER_WORKBENCH'; scn.display.shading.color_type = 'TEXTURE'; scn.display.shading.light = 'STUDIO'
scn.render.resolution_x = 600; scn.render.resolution_y = 600
cam = scn.camera
cam.data.ortho_scale = 2.2
cam.location = Vector((5, 0, 0.9)); cam.rotation_euler = (math.radians(90), 0, math.radians(90))
for act, frames in (("Walk", [1, 8, 16, 23]), ("Run", [1, 5, 10, 14]), ("Idle", [1, 45])):
    arm.animation_data.action = bpy.data.actions[act]
    for f in frames:
        scn.frame_set(f)
        scn.render.filepath = OUT + "/anim_%s_%02d.png" % (act, f)
        bpy.ops.render.render(write_still=True)

# ---------------- export ----------------
arm.animation_data.action = bpy.data.actions["Idle"]
scn.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=OUT + "/cindrel_rigged.blend")
bpy.ops.object.select_all(action='DESELECT'); mesh.select_set(True); arm.select_set(True)
bpy.ops.export_scene.fbx(filepath=OUT + "/Cindrel_Rigged.fbx", use_selection=True,
    object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False, bake_anim=True,
    bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0.0,
    path_mode='COPY', embed_textures=True, mesh_smooth_type='FACE', apply_unit_scale=True)
open(OUT + "/rig_log.txt", "w").write("\n".join(log))
