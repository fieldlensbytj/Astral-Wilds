# Shared rig prep for a Meshy quadruped (used for Cindrel and Ripplefin,
# 2026-10-09): import the FullSize GLB, decimate, normalise (1.9m tall, feet
# on z=0, centred; the Meshy GLBs already face -Y like the rigs), save
# Rigs/<Name>_Prep.blend, render views, and trace the four leg columns.
#
#   blender -b --python astral_rig_prep.py -- <Name> <tris> <out_dir>
#
# Writes <out_dir>/survey.txt: mesh pieces, the body per y slice, and each
# leg's column (median x/y per height slice, traced up from the lowest point
# in each quadrant, as ironbur_rig_columns.py does). Views go to
# <out_dir>/view_*.png (side: screen right = +Y; front: screen right = +X).
import bpy, sys, math, numpy as np
from mathutils import Vector
NAME, TRIS, OUT = sys.argv[sys.argv.index("--") + 1:][:3]
TRIS = int(TRIS)
RIGS = r"C:/Users/camer/Astral Wilds/ArtSource/Blender/Rigs"
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=r"C:/Users/camer/Astral Wilds/ArtSource/Meshy/Astrals/%s/FullSize/%s_FullSize.glb" % (NAME, NAME))
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
bpy.ops.object.select_all(action='DESELECT')
for m in meshes:
    m.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
ob = bpy.context.view_layer.objects.active
ob.parent = None
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
for o in list(bpy.context.scene.objects):
    if o != ob:
        bpy.data.objects.remove(o, do_unlink=True)
ob.name = NAME
tris0 = sum(len(p.vertices) - 2 for p in ob.data.polygons)
mod = ob.modifiers.new("Dec", 'DECIMATE'); mod.decimate_type = 'COLLAPSE'
mod.ratio = min(1.0, TRIS / tris0); mod.use_collapse_triangulate = True
bpy.ops.object.modifier_apply(modifier="Dec")
vs = [v.co for v in ob.data.vertices]
mn = Vector([min(v[i] for v in vs) for i in range(3)]); mx = Vector([max(v[i] for v in vs) for i in range(3)])
s = 1.9 / (mx.z - mn.z)
for v in ob.data.vertices:
    v.co = (v.co - Vector(((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, mn.z))) * s
L = ["triangles %d -> %d, verts %d" % (tris0, sum(len(p.vertices) - 2 for p in ob.data.polygons), len(ob.data.vertices))]
V = np.array([tuple(v.co) for v in ob.data.vertices])

# Heading: Cindrel and Ripplefin stand turned ~45 deg in their GLBs. The body
# axis is the main spread of the torso's points (z 0.3-1.2, top view); the
# head end is the end nearer the front paws (the lowest points under the
# body's front half are the paws; the head sits over the end with the tallest
# narrow column of points - simplest robust cue here: the end the eyes face
# in Meshy's default framing, -Y, i.e. the end with the lower mean y).
mid = V[(V[:, 2] > 0.3) & (V[:, 2] < 1.2)][:, :2]
mid = mid - mid.mean(0)
w, vec = np.linalg.eigh(mid.T @ mid)
axis = vec[:, 1]                                  # main spread
if axis[1] > 0:
    axis = -axis                                  # point it toward the head end (-y side)
ang = math.atan2(axis[1], axis[0])
rot = -math.pi / 2 - ang                          # head end -> -Y
c, s_ = math.cos(rot), math.sin(rot)
xy = V[:, :2] @ np.array([[c, s_], [-s_, c]])
V[:, :2] = xy
V[:, 0] -= (V[:, 0].min() + V[:, 0].max()) / 2
V[:, 1] -= (V[:, 1].min() + V[:, 1].max()) / 2
for i, v in enumerate(ob.data.vertices):
    v.co = Vector(V[i])
L.append("heading: rotated %.1f deg about Z (body axis %s)" % (math.degrees(rot), tuple(round(a, 2) for a in axis)))

# Paws: cluster the ground-contact points (z < 0.08) into four (k-means on xy).
G = V[V[:, 2] < 0.08][:, :2]
cent = np.array([[0.2, -0.4], [-0.2, -0.4], [0.2, 0.4], [-0.2, 0.4]])
for _ in range(40):
    lab = np.argmin(((G[:, None, :] - cent[None]) ** 2).sum(2), axis=1)
    cent = np.array([G[lab == k].mean(0) if (lab == k).any() else cent[k] for k in range(4)])
# Fine heading from the paws (the torso spread is skewed by big tails and
# ears): when all four paws are down, turn so the hind-pair midpoint sits
# straight behind the front-pair midpoint, then cluster again.
if all((lab == k).sum() > 50 for k in range(4)):
    order_y = np.argsort(cent[:, 1])
    fm, hm = cent[order_y[:2]].mean(0), cent[order_y[2:]].mean(0)
    ax2 = hm - fm
    a2 = math.atan2(ax2[0], ax2[1])                    # 0 when hind is straight +Y of front
    c2, s2 = math.cos(a2), math.sin(a2)
    V[:, :2] = V[:, :2] @ np.array([[c2, -s2], [s2, c2]]).T @ np.array([[1, 0], [0, 1]])
    V[:, 0] -= (V[:, 0].min() + V[:, 0].max()) / 2
    V[:, 1] -= (V[:, 1].min() + V[:, 1].max()) / 2
    for i, v in enumerate(ob.data.vertices):
        v.co = Vector(V[i])
    G = V[V[:, 2] < 0.08][:, :2]
    for _ in range(40):
        lab = np.argmin(((G[:, None, :] - cent[None]) ** 2).sum(2), axis=1)
        cent = np.array([G[lab == k].mean(0) if (lab == k).any() else cent[k] for k in range(4)])
    L.append("paw heading: turned a further %.1f deg" % math.degrees(a2))
L.append("ground clusters (x, y, n): %s" % [(round(c[0], 2), round(c[1], 2), int((lab == k).sum())) for k, c in enumerate(cent)])
PAW_SEEDS = {}
for k, c in enumerate(cent):
    PAW_SEEDS[("f" if c[1] < 0 else "b") + ("l" if c[0] > 0 else "r")] = c

# Mesh pieces (islands).
par = np.arange(len(V))
def find(a):
    while par[a] != a:
        par[a] = par[par[a]]; a = par[a]
    return a
for e in ob.data.edges:
    a, b = find(e.vertices[0]), find(e.vertices[1])
    if a != b:
        par[a] = b
_, isl, sizes = np.unique([find(i) for i in range(len(V))], return_inverse=True, return_counts=True)
L.append("pieces %d, largest %s" % (len(sizes), sorted(sizes)[-5:]))

# Body per y slice (head is toward -Y).
for y in np.arange(-1.0, 1.0, 0.1):
    m = np.abs(V[:, 1] - y) < 0.05
    if m.sum():
        p = V[m]
        L.append("y=%+.1f  n %4d  mid x %+.2f  z %.2f..%.2f (p10 %.2f p90 %.2f)" % (y, m.sum(), np.median(p[:, 0]), p[:, 2].min(), p[:, 2].max(), np.percentile(p[:, 2], 10), np.percentile(p[:, 2], 90)))

# Leg columns: seed each quadrant at its lowest points, trace upward.
for k in ("fl", "fr", "bl", "br"):
    if k not in PAW_SEEDS:
        L.append("== %s: no ground cluster (raised paw?)" % k); continue
    c = PAW_SEEDS[k]
    L.append("== %s ground cluster at (%.2f, %.2f)" % (k, c[0], c[1]))
    for z in np.arange(0.02, 1.2, 0.06):
        m = (np.abs(V[:, 2] - z) < 0.03) & (np.linalg.norm(V[:, :2] - c, axis=1) < 0.16)
        if m.sum() < 5:
            L.append("  z=%.2f (none)" % z); continue
        p = V[m]; c = np.median(p[:, :2], axis=0)
        L.append("  z=%.2f med (%+.2f, %+.2f) n %d x[%+.2f,%+.2f] y[%+.2f,%+.2f]" % (z, c[0], c[1], m.sum(), np.percentile(p[:, 0], 10), np.percentile(p[:, 0], 90), np.percentile(p[:, 1], 10), np.percentile(p[:, 1], 90)))
open(OUT + "/survey.txt", "w").write("\n".join(L))

scn = bpy.context.scene
scn.render.engine = 'BLENDER_WORKBENCH'; scn.display.shading.color_type = 'TEXTURE'; scn.display.shading.light = 'STUDIO'
scn.render.resolution_x = scn.render.resolution_y = 800
cd = bpy.data.cameras.new("c"); cd.type = 'ORTHO'; cd.ortho_scale = 2.2
cam = bpy.data.objects.new("c", cd); scn.collection.objects.link(cam); scn.camera = cam
for name, loc, rot in (("side", (5, 0, 0.95), (90, 0, 90)), ("front", (0, -5, 0.95), (90, 0, 0)), ("top", (0, 0, 5), (0, 0, 0))):
    cam.location = Vector(loc); cam.rotation_euler = [math.radians(a) for a in rot]
    scn.render.filepath = OUT + "/view_%s.png" % name; bpy.ops.render.render(write_still=True)
bpy.data.objects.remove(cam, do_unlink=True)
bpy.ops.wm.save_as_mainfile(filepath="%s/%s_Prep.blend" % (RIGS, NAME))
