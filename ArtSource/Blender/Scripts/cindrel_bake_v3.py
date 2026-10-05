import bpy
import bmesh
import math
import os
import mathutils

PROJECT_ROOT = r"C:\Users\camer\Astral Wilds"
REPORT_PATH = os.path.join(PROJECT_ROOT, "ArtSource", "Blender", "Scripts", "_bake_report_v3.txt")
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
    log("=== Cindrel: decimate -> quad retopo -> baked-albedo pass (v2) ===")

    for p in (NEW_BLEND_PATH, NEW_FBX_PATH, NEW_PREVIEW_PATH, BAKED_ALBEDO_PATH):
        if os.path.exists(p):
            raise RuntimeError("Refusing to overwrite existing output: %s" % p)

    src_name = "Meshy_Cindrel Full Size Model_Mesh_0.001"
    if src_name not in bpy.data.objects:
        raise RuntimeError("Source object %r not found. Objects: %s" % (src_name, [o.name for o in bpy.data.objects]))

    src = bpy.data.objects[src_name]
    bpy.context.view_layer.objects.active = src
    if bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.select_all(action='DESELECT')
    src.select_set(True)
    bpy.context.view_layer.objects.active = src

    src_tris = len(src.data.polygons)
    log("Source %s: verts=%d polys=%d dims=%s" % (src_name, len(src.data.vertices), src_tris, tuple(src.dimensions)))

    # 1. Duplicate -- never touch the original high-res source.
    bpy.ops.object.duplicate()
    dec_obj = bpy.context.active_object
    dec_obj.name = "Cindrel_Decimated_v02"
    dec_obj.data.name = "Cindrel_Decimated_v02_mesh"
    log("Duplicated to intermediate object: %s" % dec_obj.name)

    # 2. Decimate to a QuadriFlow-friendly working set (mirrors the project's
    #    own established 975k -> 35k precedent), preserving silhouette.
    intermediate_tris = 60000
    ratio = min(0.95, intermediate_tris / float(src_tris))
    mod = dec_obj.modifiers.new(name="PreQuadriflowDecimate", type='DECIMATE')
    mod.decimate_type = 'COLLAPSE'
    mod.ratio = ratio
    bpy.context.view_layer.objects.active = dec_obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    log("Decimated to verts=%d polys=%d (ratio=%.4f)" % (
        len(dec_obj.data.vertices), len(dec_obj.data.polygons), ratio))

    # 3. Quad retopology via QuadriFlow on the now-tractable mesh.
    target_faces = 9000
    if bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    result = bpy.ops.object.quadriflow_remesh(
        use_mesh_symmetry=False,
        use_preserve_sharp=False,
        use_preserve_boundary=False,
        preserve_attributes=False,
        smooth_normals=True,
        mode='FACES',
        target_faces=target_faces,
        seed=42,
    )
    log("QuadriFlow remesh op result: %s" % result)
    if result != {'FINISHED'}:
        raise RuntimeError("QuadriFlow remesh did not finish (result=%s) -- aborting rather than exporting an unretopologized mesh." % (result,))

    new_obj = dec_obj
    new_obj.name = "Cindrel_Retopo_v03"
    new_obj.data.name = "Cindrel_Retopo_v03_mesh"

    quad_count = len(new_obj.data.polygons)
    quad_verts = len(new_obj.data.vertices)
    n_quads = sum(1 for p in new_obj.data.polygons if len(p.vertices) == 4)
    n_tris = sum(1 for p in new_obj.data.polygons if len(p.vertices) == 3)
    log("Retopo result: verts=%d polys=%d (quads=%d tris=%d) dims=%s" % (
        quad_verts, quad_count, n_quads, n_tris, tuple(new_obj.dimensions)))
    if n_quads < quad_count * 0.9:
        log("WARNING: retopo result is not predominantly quads -- flag for review.")

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
    log("Manifold check: non_manifold_edges=%d loose_verts=%d" % (non_manifold_edges, loose_verts))

    # 4. Fresh UV unwrap for the new topology.
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
    bpy.ops.object.mode_set(mode='OBJECT')
    log("UV layers after unwrap: %s" % [uv.name for uv in new_obj.data.uv_layers])

    # 5. Build the target material with an empty bake-target image texture node.
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

    # 6. Bake the high-poly's real Meshy albedo onto the low-poly's new UVs.
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

    # 7. Save as a NEW versioned file -- originals untouched.
    bpy.ops.wm.save_as_mainfile(filepath=NEW_BLEND_PATH, copy=False)
    log("Saved new blend: %s" % NEW_BLEND_PATH)

    # 8. Export FBX.
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

    # 9. Render a preview with a fresh camera framed on the new object.
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
