import bpy
import bmesh
import math
import os
import mathutils

PROJECT_ROOT = r"C:\Users\camer\Astral Wilds"
REPORT_PATH = os.path.join(PROJECT_ROOT, "ArtSource", "Blender", "Scripts", "_bake_report_v5.txt")
NEW_BLEND_PATH = os.path.join(PROJECT_ROOT, "ArtSource", "Blender", "Astral_Cindrel_Retopo_v03.blend")
NEW_FBX_PATH = os.path.join(PROJECT_ROOT, "Assets", "Art", "Astrals", "Cindrel", "Models", "Cindrel_Retopo_v03.fbx")
NEW_PREVIEW_PATH = os.path.join(PROJECT_ROOT, "ArtSource", "Blender", "Previews", "Cindrel_Retopo_v03_ThreeQuarter.png")
BAKED_ALBEDO_PATH = os.path.join(PROJECT_ROOT, "Assets", "Art", "Astrals", "Cindrel", "Textures", "Cindrel_Albedo_v02.png")

log_lines = []
def log(msg):
    print(msg)
    log_lines.append(str(msg))

def flush_log():
    with open(REPORT_PATH, "w") as f:
        f.write("\n".join(log_lines))

try:
    log("=== Cindrel finish pass (v5): decimate-to-target (QuadriFlow persistently ===")
    log("=== cancelled on this mesh for undiagnosed reasons despite passing every ===")
    log("=== manifold/shell/degenerate-face check -- using clean triangulated LOD ===")
    log("=== instead, which is still fully usable for baking/rigging/skinning) ===")

    for p in (NEW_BLEND_PATH, NEW_FBX_PATH, NEW_PREVIEW_PATH, BAKED_ALBEDO_PATH):
        if os.path.exists(p):
            raise RuntimeError("Refusing to overwrite existing output: %s" % p)

    dec_name = "Cindrel_Decimated_v02"
    if dec_name not in bpy.data.objects:
        raise RuntimeError("Expected working object %r not found -- run the retopo attempt first." % dec_name)
    new_obj = bpy.data.objects[dec_name]

    src_name = "Meshy_Cindrel Full Size Model_Mesh_0.001"
    src = bpy.data.objects[src_name]

    bpy.context.view_layer.objects.active = new_obj
    if bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.select_all(action='DESELECT')
    new_obj.select_set(True)
    bpy.context.view_layer.objects.active = new_obj

    log("Working mesh %s before final decimate: verts=%d polys=%d dims=%s" % (
        new_obj.name, len(new_obj.data.vertices), len(new_obj.data.polygons), tuple(new_obj.dimensions)))

    # Final game-budget decimate (clean triangulated topology -- QuadriFlow's
    # quad conversion could not be gotten to finish on this particular mesh).
    target_tris = 10000
    working_tris = len(new_obj.data.polygons)
    if working_tris > target_tris:
        ratio = target_tris / float(working_tris)
        mod = new_obj.modifiers.new(name="FinalGameBudgetDecimate", type='DECIMATE')
        mod.decimate_type = 'COLLAPSE'
        mod.ratio = ratio
        bpy.ops.object.modifier_apply(modifier=mod.name)
        log("Final decimate to verts=%d polys=%d (ratio=%.4f)" % (
            len(new_obj.data.vertices), len(new_obj.data.polygons), ratio))

    new_obj.name = "Cindrel_Retopo_v03"
    new_obj.data.name = "Cindrel_Retopo_v03_mesh"

    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.shade_smooth()

    bm = bmesh.new()
    bm.from_mesh(new_obj.data)
    bm.edges.ensure_lookup_table()
    non_manifold_edges = sum(1 for e in bm.edges if not e.is_manifold)
    loose_verts = sum(1 for v in bm.verts if len(v.link_edges) == 0)
    bm.free()
    log("Final mesh: verts=%d polys=%d non_manifold_edges=%d loose_verts=%d dims=%s" % (
        len(new_obj.data.vertices), len(new_obj.data.polygons), non_manifold_edges, loose_verts, tuple(new_obj.dimensions)))

    # Fresh UV unwrap for the new topology.
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
    bpy.ops.object.mode_set(mode='OBJECT')
    log("UV layers after unwrap: %s" % [uv.name for uv in new_obj.data.uv_layers])

    # Bake target material.
    bake_size = 2048
    baked_img = bpy.data.images.new("Cindrel_Albedo_v02", width=bake_size, height=bake_size, alpha=False)

    mat = bpy.data.materials.new(name="Cindrel_Identity_v02")
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    out.location = (300, 0)
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.location = (0, 0)
    bsdf.inputs["Roughness"].default_value = 0.65
    tex_node = nt.nodes.new("ShaderNodeTexImage")
    tex_node.image = baked_img
    tex_node.location = (-400, 0)
    nt.links.new(tex_node.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    for n in nt.nodes:
        n.select = False
    tex_node.select = True
    nt.nodes.active = tex_node

    new_obj.data.materials.clear()
    new_obj.data.materials.append(mat)
    log("Created bake target material 'Cindrel_Identity_v02' with empty %dx%d image." % (bake_size, bake_size))

    prev_engine = bpy.context.scene.render.engine
    bpy.context.scene.render.engine = 'CYCLES'
    bpy.context.scene.cycles.samples = 8
    bpy.context.scene.cycles.use_denoising = False

    bpy.ops.object.select_all(action='DESELECT')
    src.select_set(True)
    new_obj.select_set(True)
    bpy.context.view_layer.objects.active = new_obj  # active = bake target

    bpy.ops.object.bake(
        type='DIFFUSE',
        pass_filter={'COLOR'},
        use_selected_to_active=True,
        cage_extrusion=0.03,
        max_ray_distance=0.15,
        margin=8,
    )
    log("Baked DIFFUSE/COLOR from %s onto %s." % (src_name, new_obj.name))

    os.makedirs(os.path.dirname(BAKED_ALBEDO_PATH), exist_ok=True)
    baked_img.filepath_raw = BAKED_ALBEDO_PATH
    baked_img.file_format = 'PNG'
    baked_img.save()
    log("Saved baked albedo: %s" % BAKED_ALBEDO_PATH)

    bpy.context.scene.render.engine = prev_engine

    bpy.ops.wm.save_as_mainfile(filepath=NEW_BLEND_PATH, copy=False)
    log("Saved new blend: %s" % NEW_BLEND_PATH)

    bpy.ops.object.select_all(action='DESELECT')
    new_obj.select_set(True)
    bpy.context.view_layer.objects.active = new_obj
    bpy.ops.export_scene.fbx(
        filepath=NEW_FBX_PATH,
        use_selection=True,
        global_scale=1.0,
        apply_unit_scale=True,
        apply_scale_options='FBX_SCALE_NONE',
        axis_forward='-Z',
        axis_up='Y',
        object_types={'MESH'},
        use_mesh_modifiers=True,
        mesh_smooth_type='FACE',
        add_leaf_bones=False,
        bake_anim=False,
        path_mode='COPY',
        embed_textures=True,
    )
    log("Exported FBX: %s" % NEW_FBX_PATH)

    scene = bpy.context.scene
    bpy.ops.object.select_all(action='DESELECT')
    new_obj.select_set(True)
    bpy.context.view_layer.objects.active = new_obj

    cam_data = bpy.data.cameras.new("Cindrel_PreviewCam_v02")
    cam_obj = bpy.data.objects.new("Cindrel_PreviewCam_v02", cam_data)
    scene.collection.objects.link(cam_obj)
    dims = new_obj.dimensions
    radius = max(dims.x, dims.y, dims.z) * 1.8 + 0.5
    cam_obj.location = (radius * 0.8, -radius * 0.8, dims.z * 0.6)
    center = mathutils.Vector((0, 0, dims.z * 0.45))
    direction = center - cam_obj.location
    cam_obj.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
    scene.camera = cam_obj

    sun_data = bpy.data.lights.new("Cindrel_PreviewSun_v02", type='SUN')
    sun_data.energy = 3.0
    sun_obj = bpy.data.objects.new("Cindrel_PreviewSun_v02", sun_data)
    sun_obj.rotation_euler = (math.radians(55), 0, math.radians(35))
    scene.collection.objects.link(sun_obj)

    try:
        scene.render.engine = 'BLENDER_EEVEE_NEXT'
    except TypeError:
        scene.render.engine = prev_engine
    scene.render.filepath = NEW_PREVIEW_PATH
    scene.render.image_settings.file_format = 'PNG'
    scene.render.resolution_x = 1024
    scene.render.resolution_y = 1024
    if scene.world is None:
        scene.world = bpy.data.worlds.new("World")
    bpy.ops.render.render(write_still=True)
    log("Rendered preview: %s" % NEW_PREVIEW_PATH)

    bpy.ops.wm.save_as_mainfile(filepath=NEW_BLEND_PATH, copy=False)

    log("=== DONE OK ===")

except Exception as e:
    import traceback
    log("=== FAILED ===")
    log(str(e))
    log(traceback.format_exc())

finally:
    flush_log()
