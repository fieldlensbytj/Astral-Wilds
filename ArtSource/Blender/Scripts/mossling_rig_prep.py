import bpy, math, mathutils, random
from mathutils import Vector
OUT = r"C:/Users/camer/AppData/Local/Temp/claude/C--Users-camer-Astral-Wilds-Unreal/441c051f-c68c-43b1-95d3-9b5092b9c38c/scratchpad/rig/mossling"
GLB = r"C:/Users/camer/Astral Wilds/ArtSource/Meshy/Astrals/Mossling/FullSize/Mossling_FullSize.glb"
L = []
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=GLB)
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
L.append("imported meshes: %s" % [(m.name, len(m.data.polygons)) for m in meshes])
bpy.ops.object.select_all(action='DESELECT')
for m in meshes: m.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1: bpy.ops.object.join()
ob = bpy.context.view_layer.objects.active
ob.name = "Mossling"
# clear any parent/empties, apply transforms
ob.parent = None
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
for o in list(bpy.context.scene.objects):
    if o != ob: bpy.data.objects.remove(o, do_unlink=True)
tris0 = sum(len(p.vertices) - 2 for p in ob.data.polygons)
target = 20000
mod = ob.modifiers.new("Dec", 'DECIMATE'); mod.decimate_type = 'COLLAPSE'; mod.ratio = min(1.0, target / tris0); mod.use_collapse_triangulate = True
bpy.ops.object.modifier_apply(modifier="Dec")
tris1 = sum(len(p.vertices) - 2 for p in ob.data.polygons)
L.append("triangles %d -> %d, verts %d, uv layers %s" % (tris0, tris1, len(ob.data.vertices), [u.name for u in ob.data.uv_layers]))
# normalise: height 1.9m, feet at z=0, centred
vs = [v.co for v in ob.data.vertices]
mn = Vector([min(v[i] for v in vs) for i in range(3)]); mx = Vector([max(v[i] for v in vs) for i in range(3)])
s = 1.9 / (mx.z - mn.z)
for v in ob.data.vertices: v.co = (v.co - Vector(((mn.x+mx.x)/2, (mn.y+mx.y)/2, mn.z))) * s
vs = [v.co.copy() for v in ob.data.vertices]
# head vs tail: split high verts into 2 clusters, head = higher max z (antlers)
high = [v for v in vs if v.z > 1.1]
c = [max(high, key=lambda v: v.x).xy.copy(), min(high, key=lambda v: v.x).xy.copy()]
for _ in range(30):
    g = [[], []]
    for v in high: g[0 if (v.xy - c[0]).length < (v.xy - c[1]).length else 1].append(v)
    c = [sum((v.xy for v in gg), Vector((0, 0))) / max(1, len(gg)) for gg in g]
info = [(c[i], len(g[i]), max(v.z for v in g[i]) if g[i] else 0) for i in range(2)]
head = max(info, key=lambda t: t[2]); tail = min(info, key=lambda t: t[2])
L.append("high clusters: %s" % [("(%.2f,%.2f)" % (t[0].x, t[0].y), t[1], round(t[2], 2)) for t in info])
axis = head[0] - tail[0]
ang = math.atan2(axis.y, axis.x)                     # current heading of tail->head
rot = -math.pi / 2 - ang                              # rotate so head points to -Y
R = mathutils.Matrix.Rotation(rot, 3, 'Z')
for v in ob.data.vertices: v.co = R @ v.co
vs = [v.co for v in ob.data.vertices]
mn = Vector([min(v[i] for v in vs) for i in range(3)]); mx = Vector([max(v[i] for v in vs) for i in range(3)])
for v in ob.data.vertices: v.co -= Vector(((mn.x+mx.x)/2, (mn.y+mx.y)/2, 0))
L.append("rotated %.1f deg; bounds after: %s" % (math.degrees(rot), tuple(round(mx[i]-mn[i], 2) for i in range(3))))
# views
scn = bpy.context.scene
scn.render.engine = 'BLENDER_WORKBENCH'; scn.display.shading.color_type = 'TEXTURE'; scn.display.shading.light = 'STUDIO'
scn.render.resolution_x = scn.render.resolution_y = 800
cd = bpy.data.cameras.new("c"); cd.type = 'ORTHO'; cd.ortho_scale = 2.2
cam = bpy.data.objects.new("c", cd); scn.collection.objects.link(cam); scn.camera = cam
C = Vector((0, 0, 0.95))
for name, d, rot_e, ctr in (("side_from_+X", (1,0,0), (math.radians(90),0,math.radians(90)), C),
                            ("front_from_-Y", (0,-1,0), (math.radians(90),0,0), C),
                            ("top_from_+Z", (0,0,1), (0,0,0), Vector((0,0,0)))):
    cam.location = ctr + Vector(d) * 5; cam.rotation_euler = rot_e
    scn.render.filepath = OUT + "/view_%s.png" % name
    bpy.ops.render.render(write_still=True)
L.append("views: 800px = 2.2m; side: screen-right=+Y, centre z=0.95; front: screen-right=+X; top: right=+X up=+Y")
bpy.ops.wm.save_as_mainfile(filepath=OUT + "/mossling_aligned.blend")
open(OUT + "/prep.txt", "w").write("\n".join(L))
