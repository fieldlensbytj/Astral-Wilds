# Adds blinking eyelids to a rigged Astral (Rigs/<Name>_Rigged.blend), then
# saves it. Run after the rig script and before quadruped_reanimate.py (which
# re-exports the FBX with the shape key):
#
#   blender -b --python astral_add_eyelids.py -- <Name> <out_dir>
#
# TJ, 2026-10-09: "what about opening and closing eyes". The Meshy eyes are
# painted on the texture; there are no lids. For each eye (a rough centre
# and size read from a gridded front render, in EYES below), this:
# 1. shoots a grid of rays at the face from the front and samples the
#    texture where each lands, then centres and sizes the eye on its dark
#    lash outline and pupil (so the rough guess only needs to be close);
# 2. builds a lid from those surface hits inside the eye's ellipse, lifted
#    2.5 mm off the face, coloured with the fur just above the eye (every lid
#    vertex takes that one UV, so it shares Material_0);
# 3. rests it open - each point tucked just under the surface at the top of
#    its column, along the upper lash line - with a "Blink" shape key that
#    draws it down over the eye. The engine drives "Blink" 0 -> 1 -> 0.
# Lid verts are weighted wholly to the head bone.
import bpy, bmesh, sys, math, numpy as np
from mathutils import Vector
from mathutils.interpolate import poly_3d_calc

NAME, OUT = sys.argv[sys.argv.index("--") + 1:][:2]
RIGS = r"C:/Users/camer/Astral Wilds/ArtSource/Blender/Rigs"

# Rough eye centres (x, z; front view, rest pose) and half-widths (m), read
# from face renders after the 2026-10-09 head straightening. Stormrook's eyes
# sit on the sides of its head and are left without lids for now.
EYES = {
    "Cindrel":   [((-0.085, 1.091), 0.050), ((0.161, 1.072), 0.045)],
    "Ripplefin": [((-0.109, 1.037), 0.042), ((0.116, 1.060), 0.042)],
    "Mossling":  [((-0.280, 1.161), 0.030), ((-0.093, 1.169), 0.034)],
    "Glacielle": [((-0.156, 1.231), 0.027), ((-0.044, 1.233), 0.027)],
    "Ironbur":   [((-0.186, 0.881), 0.045), ((0.058, 0.851), 0.050)],
}

bpy.ops.wm.open_mainfile(filepath="%s/%s_Rigged.blend" % (RIGS, NAME))
scn = bpy.context.scene
arm = [o for o in scn.objects if o.type == 'ARMATURE'][0]
mesh = [o for o in scn.objects if o.type == 'MESH'][0]
me = mesh.data
log = []

# Work on the rest pose: no action, armature modifier off.
if arm.animation_data:
    arm.animation_data.action = None
for p in arm.pose.bones:
    p.rotation_quaternion = (1, 0, 0, 0); p.location = (0, 0, 0); p.scale = (1, 1, 1)
armmod = [m for m in mesh.modifiers if m.type == 'ARMATURE'][0]
armmod.show_viewport = False
bpy.context.view_layer.update()

# Remove lids from an earlier run.
if me.shape_keys:
    mesh.shape_key_clear()
if "eyelid" in mesh.vertex_groups:
    gi = mesh.vertex_groups["eyelid"].index
    bm = bmesh.new(); bm.from_mesh(me)
    dl = bm.verts.layers.deform.verify()
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if gi in v[dl]], context='VERTS')
    bm.to_mesh(me); bm.free()
    mesh.vertex_groups.remove(mesh.vertex_groups["eyelid"])

# Texture sampling.
img = None
for nd in me.materials[0].node_tree.nodes:
    if nd.type == 'BSDF_PRINCIPLED' and nd.inputs['Base Color'].links:
        img = nd.inputs['Base Color'].links[0].from_node.image
TW, TH = img.size
PX = np.array(img.pixels[:]).reshape(TH, TW, img.channels)[:, :, :3]
uvl = me.uv_layers.active.data

def hit(x, z):
    """Ray from the front (-Y) at (x, z): location, normal, uv, luminance; or None."""
    ok, loc, nrm, fi = mesh.ray_cast(Vector((x, -5.0, z)), Vector((0, 1, 0)))
    if not ok:
        return None
    poly = me.polygons[fi]
    co = [me.vertices[v].co for v in poly.vertices]
    w = poly_3d_calc(co, loc)
    uv = sum((uvl[li].uv * wi for li, wi in zip(poly.loop_indices, w)), Vector((0, 0)))
    c = PX[min(int(uv.y * TH), TH - 1) % TH, min(int(uv.x * TW), TW - 1) % TW]
    return loc, nrm, uv, float(c.mean())

new_verts, new_faces, closed, lid_uvs = [], [], [], []
for (cx, cz), r in EYES[NAME]:
    # 1. Refine: sample a grid around the guess; the eye is the dark lash
    #    outline + pupil. Centre on the dark texels' bounding box.
    G, span = 72, 2.6 * r
    xs = np.linspace(cx - span, cx + span, G); zs = np.linspace(cz - span, cz + span, G)
    dark = []
    for x in xs:
        for z in zs:
            h = hit(x, z)
            if h and h[3] < 0.16 and (x - cx) ** 2 + (z - cz) ** 2 < (1.6 * r) ** 2:
                dark.append((x, z))
    if len(dark) > 8:
        d = np.array(dark)
        x0, x1 = np.percentile(d[:, 0], [3, 97]); z0, z1 = np.percentile(d[:, 1], [3, 97])
        ecx, ecz = (x0 + x1) / 2, (z0 + z1) / 2
        a, b = max((x1 - x0) / 2, 0.6 * r) * 1.12, max((z1 - z0) / 2, 0.5 * r) * 1.15
    else:
        ecx, ecz, a, b = cx, cz, r * 1.1, r * 1.0
    log.append("eye guess (%.3f, %.3f) r %.3f -> centre (%.3f, %.3f), half-size %.3f x %.3f (%d dark samples)" % (cx, cz, r, ecx, ecz, a, b, len(dark)))

    # Fur colour: the most typical colour in a ring round the eye (one UV
    # for the whole lid). A single sample above the eye could land on a
    # marking (Cindrel's dark red brow spots).
    ring = [hit(ecx + 1.45 * a * math.cos(t), ecz + 1.45 * b * math.sin(t)) for t in np.linspace(0, 2 * math.pi, 48, endpoint=False)]
    ring = [h for h in ring if h]
    cols = np.array([PX[min(int(h[2].y * TH), TH - 1) % TH, min(int(h[2].x * TW), TW - 1) % TW] for h in ring])
    med = np.median(cols, axis=0)
    fur_uv = ring[int(np.argmin(((cols - med) ** 2).sum(1)))][2] if ring else Vector((0.5, 0.5))

    # 2. The lid is a copy of the face's own triangles over the eye - every
    #    face a ray from the front lands on inside the eye's ellipse (and its
    #    neighbours there, to close gaps between fur shards) - pushed out
    #    1.5 mm along its normals and coloured like the surrounding fur. It
    #    covers the painted eye wherever it is, whatever the surface does.
    hitf = set()
    for x in np.linspace(ecx - 1.5 * a, ecx + 1.5 * a, 90):
        for z in np.linspace(ecz - 1.5 * b, ecz + 1.5 * b, 90):
            if ((x - ecx) / a) ** 2 + ((z - ecz) / b) ** 2 <= 2.0:
                ok, loc, nrm, fi = mesh.ray_cast(Vector((x, -5.0, z)), Vector((0, 1, 0)))
                if ok:
                    hitf.add(fi)
    depth = {fi: me.polygons[fi].center.y for fi in hitf}
    med_y = float(np.median(list(depth.values()))) if depth else 0.0
    faces = []
    for fi in hitf:
        c = me.polygons[fi].center
        if ((c.x - ecx) / a) ** 2 + ((c.z - ecz) / b) ** 2 <= 1.9 and abs(c.y - med_y) < 0.07:
            faces.append(fi)
    vmap = {}
    for fi in faces:
        for vi in me.polygons[fi].vertices:
            if vi not in vmap:
                v = me.vertices[vi]
                vmap[vi] = len(new_verts)
                closed.append(v.co + v.normal * 0.0015)
                new_verts.append(None)          # open position set below
                lid_uvs.append(fur_uv)
    for fi in faces:
        new_faces.append([vmap[vi] for vi in me.polygons[fi].vertices])
    # 3. Open: each lid vertex slides up to the upper rim of the eye at its
    #    x, onto the lid's own surface there (the closed position of the lid
    #    vertex nearest that rim point), so the open lid is folded away along
    #    the upper lash line and the blink draws it down.
    ids = list(vmap.values())
    C = np.array([tuple(closed[k]) for k in ids])
    for k in ids:
        p = closed[k]
        u = max(-1.0, min(1.0, (p.x - ecx) / a))
        rim_z = ecz + b * math.sqrt(max(1 - u * u, 0)) * 1.02
        rim = Vector((p.x, p.y, rim_z))
        near = C[np.argmin(((C[:, 0] - p.x) ** 2) * 4 + (C[:, 2] - rim_z) ** 2)]
        new_verts[k] = Vector((p.x, float(near[1]), max(p.z, rim_z)))
    log.append("   lid from %d face triangles (%d verts)" % (len(faces), len(vmap)))
# Add the lid geometry to the mesh (head-weighted), with the Blink key.
bm = bmesh.new(); bm.from_mesh(me)
uvlayer = bm.loops.layers.uv.active
dl = bm.verts.layers.deform.verify()
hg = mesh.vertex_groups["head"].index
eg = mesh.vertex_groups.new(name="eyelid").index
bverts = []
for co in new_verts:
    v = bm.verts.new(co); v[dl][hg] = 1.0; v[dl][eg] = 1.0; bverts.append(v)
bm.verts.ensure_lookup_table()
for f in new_faces:
    if len(set(f)) < 3:
        continue
    try:
        face = bm.faces.new([bverts[k] for k in f])
    except ValueError:
        continue   # a duplicate (two eyes sharing a face)
    face.material_index = 0
    for loop in face.loops:
        loop[uvlayer].uv = lid_uvs[f[[bverts[k] for k in f].index(loop.vert)]]
bm.normal_update()
bm.to_mesh(me); bm.free()
first = len(me.vertices) - len(new_verts)
mesh.shape_key_add(name="Basis", from_mix=False)
blink = mesh.shape_key_add(name="Blink", from_mix=False)
for k, co in enumerate(closed):
    blink.data[first + k].co = co
# The eyelid group is only a marker for re-runs; keep it out of skinning.
log.append("lids: %d verts, %d faces; Blink shape key added" % (len(new_verts), len(new_faces)))
armmod.show_viewport = True

# Check renders: front close-up, open and closed.
scn.render.engine = 'BLENDER_WORKBENCH'; scn.display.shading.color_type = 'TEXTURE'; scn.display.shading.light = 'FLAT'
R = 600; scn.render.resolution_x = scn.render.resolution_y = R
cd = bpy.data.cameras.new("lidcam"); cd.type = 'ORTHO'
xs_all = [e[0][0] for e in EYES[NAME]]; zs_all = [e[0][1] for e in EYES[NAME]]
cd.ortho_scale = max(0.3, (max(xs_all) - min(xs_all)) * 2.2)
cam = bpy.data.objects.new("lidcam", cd); scn.collection.objects.link(cam); scn.camera = cam
cam.location = Vector((sum(xs_all) / 2, -5, sum(zs_all) / 2)); cam.rotation_euler = (math.radians(90), 0, 0)
tiles = []
for val in (0.0, 1.0):
    mesh.data.shape_keys.key_blocks["Blink"].value = val
    scn.render.filepath = OUT + "/_lid.png"; bpy.ops.render.render(write_still=True)
    im = bpy.data.images.load(OUT + "/_lid.png"); tiles.append(np.array(im.pixels[:]).reshape(R, R, 4)); bpy.data.images.remove(im)
mesh.data.shape_keys.key_blocks["Blink"].value = 0.0
sheet = np.concatenate(tiles, axis=1)
o = bpy.data.images.new("lids", sheet.shape[1], sheet.shape[0], alpha=True); o.pixels[:] = sheet.ravel()
o.filepath_raw = OUT + "/lids_%s.png" % NAME; o.file_format = 'PNG'; o.save()
bpy.data.objects.remove(cam, do_unlink=True)

bpy.ops.wm.save_as_mainfile(filepath="%s/%s_Rigged.blend" % (RIGS, NAME))
open(OUT + "/lids_%s.txt" % NAME, "w").write("\n".join(log))
