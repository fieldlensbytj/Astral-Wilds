# Stormrook rig, step 1: import the Meshy GLB, decimate to ~100k tris, normalise
# (1.9m tall, feet at z=0, centred, facing -Y like the quadruped rigs; the GLB
# already faces -Y), and save Rigs/Stormrook_Prep.blend. Prints a geometry
# survey (width per height, wing extents) for placing the bones.
#
#   blender -b --python stormrook_rig_prep.py
import bpy
from mathutils import Vector
RIGS = r"C:/Users/camer/Astral Wilds/ArtSource/Blender/Rigs"
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=r"C:/Users/camer/Astral Wilds/ArtSource/Meshy/Astrals/Stormrook/FullSize/Stormrook_FullSize.glb")
ob = [o for o in bpy.context.scene.objects if o.type == 'MESH'][0]
ob.parent = None
for o in list(bpy.context.scene.objects):
    if o != ob:
        bpy.data.objects.remove(o, do_unlink=True)
bpy.context.view_layer.objects.active = ob; ob.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
ob.name = "Stormrook"
tris0 = sum(len(p.vertices) - 2 for p in ob.data.polygons)
mod = ob.modifiers.new("Dec", 'DECIMATE'); mod.decimate_type = 'COLLAPSE'; # 100k, not 24k: at ~5 tris per feather the 4900 feathers shrank apart (TJ: "you can see through to the sky").
mod.ratio = min(1.0, 100000 / tris0); mod.use_collapse_triangulate = True
bpy.ops.object.modifier_apply(modifier="Dec")
vs = [v.co for v in ob.data.vertices]
mn = Vector([min(v[i] for v in vs) for i in range(3)]); mx = Vector([max(v[i] for v in vs) for i in range(3)])
s = 1.9 / (mx.z - mn.z)
for v in ob.data.vertices:
    v.co = (v.co - Vector(((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, mn.z))) * s
print("SURVEY tris %d -> %d" % (tris0, sum(len(p.vertices) - 2 for p in ob.data.polygons)))
vs = [v.co.copy() for v in ob.data.vertices]
for z0 in [i * 0.1 for i in range(19)]:
    sl = [v for v in vs if z0 <= v.z < z0 + 0.1]
    if not sl: continue
    core = [v for v in sl if abs(v.x) < 0.25]
    print("SURVEY z %.1f: n %4d x %+.2f..%+.2f y %+.2f..%+.2f | core(|x|<.25) y %+.2f..%+.2f" % (z0, len(sl),
        min(v.x for v in sl), max(v.x for v in sl), min(v.y for v in sl), max(v.y for v in sl),
        min((v.y for v in core), default=0), max((v.y for v in core), default=0)))
for x0 in [0.2 + i * 0.1 for i in range(8)]:
    for side in (1, -1):
        b = [v for v in vs if x0 <= side * v.x < x0 + 0.1]
        if b:
            print("SURVEY wing side %+d |x| %.1f: n %4d z %.2f..%.2f (mean %.2f) y %+.2f..%+.2f (mean %+.2f)" % (side, x0, len(b),
                min(v.z for v in b), max(v.z for v in b), sum(v.z for v in b) / len(b), min(v.y for v in b), max(v.y for v in b), sum(v.y for v in b) / len(b)))
bpy.ops.wm.save_as_mainfile(filepath=RIGS + "/Stormrook_Prep.blend")
