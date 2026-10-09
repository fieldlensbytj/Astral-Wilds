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
# 2. re-measures the eye in its own frame (along its mean surface normal,
#    passing fur shards in front of it), since side-set eyes (Ironbur) and
#    eyes half behind cheek shards (Cindrel) were missed by front rays;
# 3. lays a smooth-shaded grid lid over it, lifted 2 mm off the face, in the
#    fur colour round the eye (one UV, so it shares Material_0) with a thin
#    dark lower edge (the line of a shut eye). The lid rests open, folded
#    into a thin strip just under the face along the upper rim, with a
#    "Blink" shape key that draws it down over the eye. The engine drives
#    "Blink" 0 -> 1 -> 0. (2026-10-09 rework: the first lids copied the
#    decimated face triangles, which lit as faceted tan patches in UE, and
#    left side-facing eyes open.)
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
    # Her left eye is big, round and mostly light blue, so the dark-texel
    # measure only found its upper-right outline: size set by hand (front
    # render, 2026-10-09).
    "Ripplefin": [((-0.138, 1.039), 0.042, {"size": (0.059, 0.048)}), ((0.116, 1.060), 0.042)],
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

def texel(loc, fi):
    """UV and texture luminance at a surface point."""
    poly = me.polygons[fi]
    co = [me.vertices[v].co for v in poly.vertices]
    w = poly_3d_calc(co, loc)
    uv = sum((uvl[li].uv * wi for li, wi in zip(poly.loop_indices, w)), Vector((0, 0)))
    c = PX[min(int(uv.y * TH), TH - 1) % TH, min(int(uv.x * TW), TW - 1) % TW]
    return uv, float(c.mean())

def hit(x, z):
    """Ray from the front (-Y) at (x, z): location, normal, uv, luminance; or None."""
    ok, loc, nrm, fi = mesh.ray_cast(Vector((x, -5.0, z)), Vector((0, 1, 0)))
    if not ok:
        return None
    return (loc, nrm) + texel(loc, fi)

def surf(cen, N, H, V, u, v):
    """The face at (u, v) in an eye's own frame (centre cen, outward normal N),
    by a ray along -N that passes fur shards standing proud of the eye (and
    back faces): location, normal, uv, luminance; or None."""
    o = cen + H * u + V * v + N * 0.25
    for _ in range(10):
        ok, loc, nrm, fi = mesh.ray_cast(o, -N)
        if not ok:
            return None
        if (loc - cen).dot(N) > 0.02 or nrm.dot(N) < 0.0:
            o = loc - N * 1e-4
            continue
        if (loc - cen).dot(N) < -0.04:
            return None   # through a gap, deep into the head
        return (loc, nrm) + texel(loc, fi)
    return None

new_verts, new_faces, closed, lid_uvs = [], [], [], []
for eye in EYES[NAME]:
    (cx, cz), r = eye[:2]
    fixed = eye[2].get("size") if len(eye) > 2 else None
    # 1. Refine: sample a grid around the guess; the eye is the dark lash
    #    outline + pupil. Centre on the dark texels' bounding box.
    G, span = 72, 2.6 * r
    xs = np.linspace(cx - span, cx + span, G); zs = np.linspace(cz - span, cz + span, G)
    dark, dark_hits = [], []
    for x in xs:
        for z in zs:
            h = hit(x, z)
            if h and h[3] < 0.16 and (x - cx) ** 2 + (z - cz) ** 2 < (1.6 * r) ** 2:
                dark.append((x, z)); dark_hits.append(h)
    if len(dark) > 8:
        d = np.array(dark)
        x0, x1 = np.percentile(d[:, 0], [3, 97]); z0, z1 = np.percentile(d[:, 1], [3, 97])
        ecx, ecz = (x0 + x1) / 2, (z0 + z1) / 2
        a, b = max((x1 - x0) / 2, 0.6 * r) * 1.12, max((z1 - z0) / 2, 0.5 * r) * 1.15
    else:
        ecx, ecz, a, b = cx, cz, r * 1.1, r * 1.0
    if fixed:
        ecx, ecz, a, b = cx, cz, fixed[0] * 1.12, fixed[1] * 1.2
    log.append("eye guess (%.3f, %.3f) r %.3f -> centre (%.3f, %.3f), half-size %.3f x %.3f (%d dark samples)" % (cx, cz, r, ecx, ecz, a, b, len(dark)))

    # Fur colour: the most typical colour in a ring round the eye (one UV
    # for the whole lid). A single sample above the eye could land on a
    # marking (Cindrel's dark red brow spots).
    ring = [hit(ecx + 1.45 * a * math.cos(t), ecz + 1.45 * b * math.sin(t)) for t in np.linspace(0, 2 * math.pi, 48, endpoint=False)]
    ring = [h for h in ring if h]
    cols = np.array([PX[min(int(h[2].y * TH), TH - 1) % TH, min(int(h[2].x * TW), TW - 1) % TW] for h in ring])
    med = np.median(cols, axis=0)
    fur_uv = ring[int(np.argmin(((cols - med) ** 2).sum(1)))][2] if ring else Vector((0.5, 0.5))

    # 2. The eye's own frame: its outward normal N is the mean surface normal
    #    over the dark eye texels, H runs across it and V up it. Eyes on the
    #    side of a long head (Ironbur) or half behind cheek shards (Cindrel's
    #    right) face well away from the front view, so the lid is laid out
    #    and the eye re-measured in this frame, with rays along -N.
    Z = Vector((0, 0, 1))
    if len(dark_hits) > 8:
        N = sum((h[1] for h in dark_hits), Vector()).normalized()
        cen = sum((h[0] for h in dark_hits), Vector()) / len(dark_hits)
    else:
        h0 = hit(ecx, ecz)
        N, cen = (h0[1].copy(), h0[0].copy()) if h0 else (Vector((0, -1, 0)), Vector((ecx, 0, ecz)))
    if fixed:
        h0 = hit(ecx, ecz)
        cen = h0[0].copy() if h0 else cen
    H = Z.cross(N).normalized(); V = N.cross(H)
    # The front view foreshortens a side-facing eye; undo that, and look for
    # the eye only inside its front outline (Ripplefin has dark fur by her
    # eyes that otherwise pulled the lid off-centre).
    cos_h = max(0.4, abs(N.y) / max(1e-6, math.hypot(N.x, N.y)))
    cos_v = max(0.4, math.hypot(N.x, N.y))
    A, B = a / cos_h, b / cos_v
    span, dk = 1.4 * max(A, B), []
    for u in np.linspace(-span, span, 64):
        for v in np.linspace(-span, span, 64):
            if (u / (1.35 * A)) ** 2 + (v / (1.35 * B)) ** 2 < 1.0:
                s = surf(cen, N, H, V, u, v)
                if s and s[3] < 0.16:
                    dk.append((u, v, s[3], s[2]))
    dark_uv = min(dk, key=lambda d: d[2])[3] if dk else fur_uv
    tilt = 0.0
    if len(dk) > 8 and not fixed:
        d = np.array([(u, v) for u, v, _, _ in dk])
        # Slanted almond eyes (Ironbur): turn the frame to the eye's long
        # axis, so the lid's top edge follows the slanted upper lash.
        # Only for clearly long eyes: a round eye's (Ripplefin) axis is noise.
        ev, evec = np.linalg.eigh(np.cov(d.T))
        major = evec[:, int(np.argmax(ev))]
        if major[0] < 0:
            major = -major
        if math.sqrt(max(ev) / max(min(ev), 1e-12)) > 1.4:
            tilt = max(-30.0, min(30.0, math.degrees(math.atan2(major[1], major[0]))))
        c, s_ = math.cos(math.radians(tilt)), math.sin(math.radians(tilt))
        H, V = (H * c + V * s_).normalized(), (V * c - H * s_).normalized()
        d = np.array([(u * c + v * s_, v * c - u * s_) for u, v in d])
        u0, u1 = np.percentile(d[:, 0], [3, 97]); v0, v1 = np.percentile(d[:, 1], [3, 97])
        cen = cen + H * float((u0 + u1) / 2) + V * float((v0 + v1) / 2)
        A = max(float(u1 - u0) / 2 * 1.18, 0.6 * r * 1.18, a / cos_h)
        B = max(float(v1 - v0) / 2 * 1.35, 0.5 * r * 1.35, b / cos_v)
    log.append("   eye frame N (%.2f, %.2f, %.2f), tilt %.0f deg, half-size %.3f x %.3f (%d dark samples)" % (N.x, N.y, N.z, tilt, A, B, len(dk)))

    # 3. The lid: a smooth grid over the eye's ellipse, each point on the face
    #    (shards passed) and lifted 2 mm along its normal, smooth shaded, so
    #    it lights like one soft lid rather than the decimated facets under
    #    it. Fur coloured, with a thin dark band along the lower edge: the
    #    line of a shut eye. Open, each column is folded into a thin strip
    #    just under the face along the upper rim (not collapsed flat: a
    #    degenerate base mesh gets its triangles stripped on import).
    NU, NV = 15, 9
    ts = list(np.linspace(0.0, 0.93, NV - 1)) + [1.0]
    base = len(new_verts)
    for u in np.linspace(-A, A, NU):
        hh = B * math.sqrt(max(1 - (u / (A * 1.08)) ** 2, 0.0))
        for j, t in enumerate(ts):
            v = hh * (1 - 2 * t)
            s = surf(cen, N, H, V, u, v)
            closed.append(s[0] + s[1] * 0.002 if s else cen + H * u + V * v + N * 0.002)
            vo = hh * (1 - 2 * 0.07 * t)
            so = surf(cen, N, H, V, u, vo)
            new_verts.append(so[0] - so[1] * 0.002 if so else cen + H * u + V * vo - N * 0.002)
            lid_uvs.append(dark_uv if j == NV - 1 else fur_uv)
    for i in range(NU - 1):
        for j in range(NV - 1):
            k = lambda i, j: base + i * NV + j
            new_faces.append([k(i, j), k(i, j + 1), k(i + 1, j + 1), k(i + 1, j)])
    log.append("   lid grid %d x %d" % (NU, NV))
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
    face.smooth = True
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
