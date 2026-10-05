import bpy, math, mathutils, sys
from mathutils import Vector
OUT = r"C:/Users/camer/AppData/Local/Temp/claude/C--Users-camer-Astral-Wilds-Unreal/441c051f-c68c-43b1-95d3-9b5092b9c38c/scratchpad/rig/mossling"
DEG = float(sys.argv[sys.argv.index("--") + 1])
bpy.ops.wm.open_mainfile(filepath=OUT + "/mossling_aligned.blend")
ob = bpy.data.objects["Mossling"]
R = mathutils.Matrix.Rotation(math.radians(DEG), 3, 'Z')
for v in ob.data.vertices: v.co = R @ v.co
vs = [v.co for v in ob.data.vertices]
mn = Vector([min(v[i] for v in vs) for i in range(3)]); mx = Vector([max(v[i] for v in vs) for i in range(3)])
for v in ob.data.vertices: v.co -= Vector(((mn.x+mx.x)/2, (mn.y+mx.y)/2, 0))
scn = bpy.context.scene; cam = scn.camera
C = Vector((0, 0, 0.95))
for name, d, rot_e, ctr in (("side_from_+X", (1,0,0), (math.radians(90),0,math.radians(90)), C),
                            ("front_from_-Y", (0,-1,0), (math.radians(90),0,0), C),
                            ("top_from_+Z", (0,0,1), (0,0,0), Vector((0,0,0)))):
    cam.location = ctr + Vector(d) * 5; cam.rotation_euler = rot_e
    scn.render.filepath = OUT + "/view_%s.png" % name
    bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=OUT + "/mossling_aligned.blend")
open(OUT + "/rotate.txt", "w").write("rotated %+.1f; bounds %s" % (DEG, tuple(round(mx[i]-mn[i], 2) for i in range(3))))
