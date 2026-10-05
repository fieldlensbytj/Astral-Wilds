import bpy, math, mathutils
OUT = r"C:/Users/camer/AppData/Local/Temp/claude/C--Users-camer-Astral-Wilds-Unreal/1f45ae34-9634-4dd9-ac1b-deb9025c0437/scratchpad/rig"
FBX = r"C:/Users/camer/Astral Wilds/Assets/Art/Astrals/Cindrel/Models/Cindrel_Retopo_v03.fbx"
TEX = r"C:/Users/camer/Astral Wilds/Assets/Art/Astrals/Cindrel/Textures/Cindrel_Albedo_v02.png"
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=FBX)
ob = [o for o in bpy.context.scene.objects if o.type == 'MESH'][0]
ob.rotation_euler = (0, 0, 0); ob.location = (0, 0, 0)
bpy.context.view_layer.update()
vs = [ob.matrix_world @ v.co for v in ob.data.vertices]
mn = mathutils.Vector([min(v[i] for v in vs) for i in range(3)]); mx = mathutils.Vector([max(v[i] for v in vs) for i in range(3)])
# recentre: XY centre at origin, feet at z=0
off = mathutils.Vector((-(mn.x + mx.x) / 2, -(mn.y + mx.y) / 2, -mn.z))
ob.location = off
bpy.context.view_layer.update()
vs = [ob.matrix_world @ v.co for v in ob.data.vertices]
mn = mathutils.Vector([min(v[i] for v in vs) for i in range(3)]); mx = mathutils.Vector([max(v[i] for v in vs) for i in range(3)])
mat = bpy.data.materials.new("P"); mat.use_nodes = True
nt = mat.node_tree; img = nt.nodes.new("ShaderNodeTexImage"); img.image = bpy.data.images.load(TEX)
nt.links.new(img.outputs["Color"], nt.nodes["Principled BSDF"].inputs["Base Color"])
ob.data.materials.clear(); ob.data.materials.append(mat)
scn = bpy.context.scene
scn.render.engine = 'BLENDER_WORKBENCH'; scn.display.shading.color_type = 'TEXTURE'; scn.display.shading.light = 'STUDIO'
scn.render.resolution_x = scn.render.resolution_y = 800
S = 2.2  # ortho scale (m) -> 800px, so 1px = 2.75mm
cd = bpy.data.cameras.new("c"); cd.type = 'ORTHO'; cd.ortho_scale = S
cam = bpy.data.objects.new("c", cd); scn.collection.objects.link(cam); scn.camera = cam
C = mathutils.Vector((0, 0, 0.9))
lines = ["offset applied=%s  bounds min=%s max=%s" % (tuple(round(c,3) for c in off), tuple(round(c,3) for c in mn), tuple(round(c,3) for c in mx)),
         "Each view: 800px = %.2fm, centre (400,400) = world point listed. Screen right/up axes listed." % S]
views = {
  # name: camera dir from centre, euler, centre, right axis, up axis
  "side_from_+X": ((1,0,0), (math.radians(90), 0, math.radians(90)), C, "-Y", "+Z"),
  "front_from_-Y": ((0,-1,0), (math.radians(90), 0, 0), C, "+X", "+Z"),
  "top_from_+Z": ((0,0,1), (0,0,0), mathutils.Vector((0,0,0)), "+X", "+Y"),
}
for name, (d, rot, ctr, right, up) in views.items():
    cam.location = ctr + mathutils.Vector(d) * 5; cam.rotation_euler = rot
    scn.render.filepath = OUT + "/canon_%s.png" % name
    bpy.ops.render.render(write_still=True)
    lines.append("%s: centre=%s right=%s up=%s" % (name, tuple(round(c,2) for c in ctr), right, up))
open(OUT + "/canon.txt", "w").write("\n".join(lines))
bpy.ops.wm.save_as_mainfile(filepath=OUT + "/cindrel_canon.blend")
