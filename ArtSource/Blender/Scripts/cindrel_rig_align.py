import bpy, math, mathutils
OUT = r"C:/Users/camer/AppData/Local/Temp/claude/C--Users-camer-Astral-Wilds-Unreal/1f45ae34-9634-4dd9-ac1b-deb9025c0437/scratchpad/rig"
bpy.ops.wm.open_mainfile(filepath=OUT + "/cindrel_canon.blend")
ob = [o for o in bpy.context.scene.objects if o.type == 'MESH'][0]
ob.rotation_euler = (0, 0, math.radians(-46.2))
bpy.context.view_layer.objects.active = ob; ob.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
vs = [v.co for v in ob.data.vertices]
mn = mathutils.Vector([min(v[i] for v in vs) for i in range(3)]); mx = mathutils.Vector([max(v[i] for v in vs) for i in range(3)])
# recentre XY so the body sits on the origin
off = mathutils.Vector((-(mn.x+mx.x)/2, -(mn.y+mx.y)/2, -mn.z))
for v in ob.data.vertices: v.co += off
bpy.context.view_layer.update()
vs = [v.co for v in ob.data.vertices]
mn = mathutils.Vector([min(v[i] for v in vs) for i in range(3)]); mx = mathutils.Vector([max(v[i] for v in vs) for i in range(3)])
scn = bpy.context.scene
cam = scn.camera
C = mathutils.Vector((0, 0, 0.9))
lines = ["aligned bounds min=%s max=%s" % (tuple(round(c,3) for c in mn), tuple(round(c,3) for c in mx)),
         "800px = 2.20m; px->world: a = centre + (px-400)*0.00275 along screen-right, z = 0.9 + (400-py)*0.00275"]
for name, d, rot, ctr, right in (("side_from_+X", (1,0,0), (math.radians(90),0,math.radians(90)), C, "+Y"),
                                  ("front_from_-Y", (0,-1,0), (math.radians(90),0,0), C, "+X"),
                                  ("top_from_+Z", (0,0,1), (0,0,0), mathutils.Vector((0,0,0)), "+X (up=+Y)")):
    cam.location = ctr + mathutils.Vector(d) * 5; cam.rotation_euler = rot
    scn.render.filepath = OUT + "/aligned_%s.png" % name
    bpy.ops.render.render(write_still=True)
    lines.append("%s: centre=%s screen-right=%s" % (name, tuple(round(c,2) for c in ctr), right))
open(OUT + "/aligned.txt", "w").write("\n".join(lines))
bpy.ops.wm.save_as_mainfile(filepath=OUT + "/cindrel_aligned.blend")
