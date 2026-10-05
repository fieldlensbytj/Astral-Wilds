import bpy
import bmesh
import random
import math
import os
import hashlib

PROJECT_ROOT = r"C:\Users\camer\Astral Wilds"
REPORT_PATH = os.path.join(PROJECT_ROOT, "ArtSource", "Blender", "Scripts", "_retopo_report.txt")
NEW_BLEND_PATH = os.path.join(PROJECT_ROOT, "ArtSource", "Blender", "Astral_Cindrel_Retopo_v01.blend")
NEW_FBX_PATH = os.path.join(PROJECT_ROOT, "Assets", "Art", "Astrals", "Cindrel", "Models", "Cindrel_Retopo_v01.fbx")
NEW_PREVIEW_PATH = os.path.join(PROJECT_ROOT, "ArtSource", "Blender", "Previews", "Cindrel_Retopo_v01_ThreeQuarter.png")

log_lines = []
def log(msg):
    print(msg)
    log_lines.append(str(msg))

def flush_log():
    with open(REPORT_PATH, "w") as f:
        f.write("\n".join(log_lines))

try:
    log("=== Cindrel retopology + procedural identity material pass ===")

    # Refuse to overwrite existing production outputs.
    for p in (NEW_BLEND_PATH, NEW_FBX_PATH, NEW_PREVIEW_PATH):
        if os.path.exists(p):
            raise RuntimeError("Refusing to overwrite existing output: %s" % p)

    src_name = "Cindrel_LOD0"
    if src_name not in bpy.data.objects:
        raise RuntimeError("Source object %r not found. Objects present: %s" % (src_name, [o.name for o in bpy.data.objects]))

    src = bpy.data.objects[src_name]
    bpy.context.view_layer.objects.active = src
    if bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.select_all(action='DESELECT')
    src.select_set(True)
    bpy.context.view_layer.objects.active = src

    src_tris = len(src.data.polygons)
    src_verts = len(src.data.vertices)
    log("Source %s: verts=%d polys=%d dims=%s" % (src_name, src_verts, src_tris, tuple(src.dimensions)))

    # 1. Duplicate -- never touch the original production mesh.
    bpy.ops.object.duplicate()
    new_obj = bpy.context.active_object
    new_obj.name = "Cindrel_Retopo_v01"
    new_obj.data.name = "Cindrel_Retopo_v01_mesh"
    log("Duplicated to new object: %s" % new_obj.name)

    # 2. Quad retopology via QuadriFlow.
    target_faces = 5000
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

    quad_count = len(new_obj.data.polygons)
    quad_verts = len(new_obj.data.vertices)
    n_quads = sum(1 for p in new_obj.data.polygons if len(p.vertices) == 4)
    n_tris = sum(1 for p in new_obj.data.polygons if len(p.vertices) == 3)
    n_other = quad_count - n_quads - n_tris
    log("Retopo result: verts=%d polys=%d (quads=%d tris=%d other=%d) dims=%s" % (
        quad_verts, quad_count, n_quads, n_tris, n_other, tuple(new_obj.dimensions)))

    # 3. Recalculate normals + shade smooth.
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.shade_smooth()

    # Manifold check.
    bm = bmesh.new()
    bm.from_mesh(new_obj.data)
    bm.edges.ensure_lookup_table()
    non_manifold_edges = sum(1 for e in bm.edges if not e.is_manifold)
    loose_verts = sum(1 for v in bm.verts if len(v.link_edges) == 0)
    bm.free()
    log("Manifold check: non_manifold_edges=%d loose_verts=%d" % (non_manifold_edges, loose_verts))

    # 4. Fresh UV unwrap (old UVs are invalid after retopology).
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
    bpy.ops.object.mode_set(mode='OBJECT')
    log("UV layers after unwrap: %s" % [uv.name for uv in new_obj.data.uv_layers])

    # 5. Procedural identity vertex-color pass, approximating the approved
    #    reference (Assets/Art/References/Astrals/MainAstrals/01_Main_Astrals_Cindrel_Mossling.png):
    #    warm ember-orange body/mane, cream chest/belly, dark maroon ear+tail tips.
    #    This is an algorithmic first pass, NOT the final hand/AI-authored texture --
    #    flagged as such in the report and to the user.
    me = new_obj.data
    verts_local = [v.co.copy() for v in me.vertices]
    zs = [v.z for v in verts_local]
    min_z, max_z = min(zs), max(zs)
    height = max(max_z - min_z, 1e-6)
    center = (0.0, 0.0, min_z + height * 0.42)  # rough torso-core height, not geometric centroid
    max_dist = 0.0
    dists = []
    for v in verts_local:
        d = math.sqrt((v.x - center[0]) ** 2 + (v.y - center[1]) ** 2 + (v.z - center[2]) ** 2)
        dists.append(d)
        if d > max_dist:
            max_dist = d
    max_dist = max(max_dist, 1e-6)

    def smoothstep(edge0, edge1, x):
        if edge1 <= edge0:
            return 0.0 if x < edge0 else 1.0
        t = max(0.0, min(1.0, (x - edge0) / (edge1 - edge0)))
        return t * t * (3.0 - 2.0 * t)

    def lerp(a, b, t):
        return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))

    ORANGE_BASE = (0.72, 0.27, 0.09)
    ORANGE_HI = (0.92, 0.47, 0.14)
    CREAM = (0.94, 0.87, 0.72)
    DARK_TIP = (0.14, 0.06, 0.05)

    rng = random.Random(1234)
    color_attr = me.color_attributes.new(name="Col", type='FLOAT_COLOR', domain='POINT')
    for i, v in enumerate(verts_local):
        nz = me.vertices[i].normal.z
        d_norm = dists[i] / max_dist

        # Belly/chest cream where the surface faces downward.
        belly = smoothstep(0.15, 0.75, -nz)
        base = lerp(ORANGE_BASE, ORANGE_HI, smoothstep(0.0, 1.0, (v.z - min_z) / height))
        base = lerp(base, CREAM, belly)

        # Dark tips on the extremities (ear tips, tail tip, paw tips).
        tip = smoothstep(0.62, 0.92, d_norm)
        final = lerp(base, DARK_TIP, tip)

        jitter = (rng.uniform(-0.025, 0.025))
        final = tuple(max(0.0, min(1.0, c + jitter)) for c in final)

        color_attr.data[i].color = (final[0], final[1], final[2], 1.0)

    log("Vertex color attribute 'Col' written for %d verts." % len(verts_local))

    # 6. Material reading the vertex-color attribute.
    mat = bpy.data.materials.new(name="Cindrel_Identity_v01")
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    out.location = (300, 0)
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.location = (0, 0)
    bsdf.inputs["Roughness"].default_value = 0.65
    attr = nt.nodes.new("ShaderNodeVertexColor")
    attr.layer_name = "Col"
    attr.location = (-300, 0)
    nt.links.new(attr.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])

    new_obj.data.materials.clear()
    new_obj.data.materials.append(mat)
    log("Material 'Cindrel_Identity_v01' created and assigned (vertex-color driven).")

    # 7. Save as a NEW versioned file -- original production blend is untouched.
    bpy.ops.wm.save_as_mainfile(filepath=NEW_BLEND_PATH, copy=False)
    log("Saved new blend: %s" % NEW_BLEND_PATH)

    # 8. Export FBX (new filename, does not touch the existing production FBX).
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
    )
    log("Exported FBX: %s" % NEW_FBX_PATH)

    # 9. Render a three-quarter preview using the existing REVIEW camera/lights,
    #    same as the original production preview, so it's a fair visual comparison.
    scene = bpy.context.scene
    if "REVIEW_Camera" in bpy.data.objects:
        scene.camera = bpy.data.objects["REVIEW_Camera"]
        for obj_name in ("Cindrel_LOD0",):
            if obj_name in bpy.data.objects:
                bpy.data.objects[obj_name].hide_render = True
        scene.render.filepath = NEW_PREVIEW_PATH
        scene.render.image_settings.file_format = 'PNG'
        scene.render.resolution_x = 1024
        scene.render.resolution_y = 1024
        bpy.ops.render.render(write_still=True)
        if "Cindrel_LOD0" in bpy.data.objects:
            bpy.data.objects["Cindrel_LOD0"].hide_render = False
        log("Rendered preview: %s" % NEW_PREVIEW_PATH)
    else:
        log("No REVIEW_Camera found -- skipped preview render.")

    # Re-save after render-visibility toggles / render settings changes.
    bpy.ops.wm.save_as_mainfile(filepath=NEW_BLEND_PATH, copy=False)

    log("=== DONE OK ===")

except Exception as e:
    import traceback
    log("=== FAILED ===")
    log(str(e))
    log(traceback.format_exc())

finally:
    flush_log()
