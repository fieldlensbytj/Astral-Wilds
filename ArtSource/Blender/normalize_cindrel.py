"""Create a non-destructive, Unity-oriented Cindrel production mesh.

This script deliberately refuses to overwrite existing outputs. It is intended
to be run by Blender in background mode from the Astral Wilds repository root.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector


def parse_args() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--blend-output", type=Path, required=True)
    parser.add_argument("--fbx-output", type=Path, required=True)
    parser.add_argument("--preview-output", type=Path, required=True)
    parser.add_argument("--target-triangles", type=int, default=35_000)
    parser.add_argument("--target-height", type=float, default=1.25)
    return parser.parse_args(argv)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def require_new_output(path: Path) -> None:
    if path.exists():
        raise FileExistsError(f"Refusing to overwrite existing output: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)


def look_at(obj: bpy.types.Object, target: Vector) -> None:
    direction = target - obj.location
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def mesh_bounds(mesh: bpy.types.Mesh) -> tuple[Vector, Vector]:
    minimum = Vector((math.inf, math.inf, math.inf))
    maximum = Vector((-math.inf, -math.inf, -math.inf))
    for vertex in mesh.vertices:
        for axis in range(3):
            minimum[axis] = min(minimum[axis], vertex.co[axis])
            maximum[axis] = max(maximum[axis], vertex.co[axis])
    return minimum, maximum


def topology(mesh: bpy.types.Mesh) -> dict[str, int]:
    bm = bmesh.new()
    bm.from_mesh(mesh)
    result = {
        "vertices": len(bm.verts),
        "edges": len(bm.edges),
        "faces": len(bm.faces),
        "triangles": sum(max(0, len(face.verts) - 2) for face in bm.faces),
        "boundary_edges": sum(1 for edge in bm.edges if edge.is_boundary),
        "non_manifold_edges": sum(1 for edge in bm.edges if not edge.is_manifold),
        "loose_edges": sum(1 for edge in bm.edges if edge.is_wire),
        "loose_vertices": sum(1 for vertex in bm.verts if not vertex.link_edges),
    }
    bm.free()
    return result


def create_material() -> bpy.types.Material:
    material = bpy.data.materials.new("Cindrel_Ember_Clay")
    material.use_nodes = True
    material.diffuse_color = (0.8, 0.12, 0.025, 1.0)
    principled = material.node_tree.nodes.get("Principled BSDF")
    principled.inputs["Base Color"].default_value = (0.8, 0.12, 0.025, 1.0)
    principled.inputs["Roughness"].default_value = 0.68
    principled.inputs["Metallic"].default_value = 0.0
    return material


def add_area_light(name: str, location: tuple[float, float, float], energy: float, size: float, target: Vector) -> None:
    data = bpy.data.lights.new(name, "AREA")
    data.energy = energy
    data.shape = "DISK"
    data.size = size
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = location
    look_at(obj, target)


def create_review_setup(target: Vector) -> None:
    bpy.ops.mesh.primitive_plane_add(size=8.0, location=(0.0, 0.0, -0.004))
    plane = bpy.context.object
    plane.name = "REVIEW_Ground"
    ground = bpy.data.materials.new("REVIEW_GroundMaterial")
    ground.use_nodes = True
    ground_bsdf = ground.node_tree.nodes.get("Principled BSDF")
    ground_bsdf.inputs["Base Color"].default_value = (0.018, 0.022, 0.028, 1.0)
    ground_bsdf.inputs["Roughness"].default_value = 0.82
    plane.data.materials.append(ground)

    camera_data = bpy.data.cameras.new("REVIEW_Camera")
    camera = bpy.data.objects.new("REVIEW_Camera", camera_data)
    bpy.context.scene.collection.objects.link(camera)
    camera.location = (2.45, -3.05, 1.8)
    camera_data.lens = 58.0
    look_at(camera, target)
    bpy.context.scene.camera = camera

    add_area_light("REVIEW_Key", (3.1, -2.8, 3.7), 950.0, 3.0, target)
    add_area_light("REVIEW_Fill", (-2.5, -1.2, 2.1), 525.0, 3.2, target)
    add_area_light("REVIEW_Rim", (-0.4, 2.8, 3.0), 780.0, 2.4, target)


def main() -> None:
    args = parse_args()
    repo_root = Path.cwd().resolve()
    source = (repo_root / args.source).resolve()
    blend_output = (repo_root / args.blend_output).resolve()
    fbx_output = (repo_root / args.fbx_output).resolve()
    preview_output = (repo_root / args.preview_output).resolve()

    if not source.is_file():
        raise FileNotFoundError(f"Source GLB not found: {source}")
    if args.target_triangles < 1_000:
        raise ValueError("Target triangle count is implausibly low")
    if args.target_height <= 0:
        raise ValueError("Target height must be positive")

    for output in (blend_output, fbx_output, preview_output):
        require_new_output(output)

    source_digest = sha256(source)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(source))

    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    if not meshes:
        raise RuntimeError("The source contains no mesh objects")

    bpy.ops.object.select_all(action="DESELECT")
    for obj in meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    cindrel = bpy.context.view_layer.objects.active
    cindrel.name = "Cindrel_LOD0"
    cindrel.data.name = "Cindrel_LOD0_Mesh"
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)

    before = topology(cindrel.data)
    if before["triangles"] > args.target_triangles:
        modifier = cindrel.modifiers.new("ProductionTriangleBudget", "DECIMATE")
        modifier.decimate_type = "COLLAPSE"
        modifier.ratio = args.target_triangles / before["triangles"]
        modifier.use_collapse_triangulate = True
        bpy.ops.object.modifier_apply(modifier=modifier.name)

    # Recalculate outward normals after decimation.
    bm = bmesh.new()
    bm.from_mesh(cindrel.data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(cindrel.data)
    bm.free()
    cindrel.data.update()

    # Normalize to a grounded, centered, meter-scale character with its origin at the feet.
    minimum, maximum = mesh_bounds(cindrel.data)
    source_height = maximum.z - minimum.z
    scale = args.target_height / source_height
    center_x = (minimum.x + maximum.x) * 0.5
    center_y = (minimum.y + maximum.y) * 0.5
    for vertex in cindrel.data.vertices:
        vertex.co.x = (vertex.co.x - center_x) * scale
        vertex.co.y = (vertex.co.y - center_y) * scale
        vertex.co.z = (vertex.co.z - minimum.z) * scale
    cindrel.data.update()

    # Generate a single non-overlapping UV set for the later authored texture pass.
    bpy.ops.object.select_all(action="DESELECT")
    cindrel.select_set(True)
    bpy.context.view_layer.objects.active = cindrel
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(
        angle_limit=math.radians(66.0),
        margin_method="SCALED",
        island_margin=0.012,
        area_weight=0.0,
        correct_aspect=True,
        scale_to_bounds=True,
    )
    bpy.ops.object.mode_set(mode="OBJECT")

    for polygon in cindrel.data.polygons:
        polygon.use_smooth = True

    cindrel.data.materials.clear()
    cindrel.data.materials.append(create_material())
    for polygon in cindrel.data.polygons:
        polygon.material_index = 0

    cindrel["astral_asset_name"] = "Cindrel"
    cindrel["astral_asset_version"] = "Production_v01"
    cindrel["source_sha256"] = source_digest
    cindrel["source_provider"] = "Meshy"
    cindrel["source_prompt"] = "UNKNOWN_NOT_RECORDED"
    cindrel["license_status"] = "PENDING_USER_PROVIDER_TERMS"
    cindrel["target_triangles"] = args.target_triangles
    cindrel["target_height_m"] = args.target_height

    after = topology(cindrel.data)
    bounds_min, bounds_max = mesh_bounds(cindrel.data)
    dimensions = bounds_max - bounds_min

    create_review_setup(Vector((0.0, 0.0, args.target_height * 0.52)))
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 1024
    scene.render.resolution_y = 1024
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(preview_output)
    scene.render.film_transparent = False
    if scene.world is None:
        scene.world = bpy.data.worlds.new("Cindrel_Review_World")
    scene.world.color = (0.009, 0.012, 0.018)
    bpy.ops.render.render(write_still=True)

    bpy.ops.wm.save_as_mainfile(filepath=str(blend_output), check_existing=False)

    bpy.ops.object.select_all(action="DESELECT")
    cindrel.select_set(True)
    bpy.context.view_layer.objects.active = cindrel
    bpy.ops.export_scene.fbx(
        filepath=str(fbx_output),
        check_existing=False,
        use_selection=True,
        object_types={"MESH"},
        use_mesh_modifiers=True,
        mesh_smooth_type="FACE",
        use_tspace=True,
        use_triangles=True,
        use_custom_props=True,
        bake_anim=False,
        path_mode="AUTO",
        global_scale=1.0,
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_UNITS",
        use_space_transform=True,
        bake_space_transform=False,
        axis_forward="-Z",
        axis_up="Y",
    )

    result = {
        "source": str(source),
        "source_sha256": source_digest,
        "blend_output": str(blend_output),
        "blend_sha256": sha256(blend_output),
        "fbx_output": str(fbx_output),
        "fbx_sha256": sha256(fbx_output),
        "preview_output": str(preview_output),
        "preview_sha256": sha256(preview_output),
        "before": before,
        "after": after,
        "dimensions_m": [round(value, 6) for value in dimensions],
        "uv_layers": [layer.name for layer in cindrel.data.uv_layers],
        "materials": [material.name for material in cindrel.data.materials],
    }
    print("ASTRAL_NORMALIZE_RESULT=" + json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    main()
