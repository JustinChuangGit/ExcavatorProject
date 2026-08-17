from __future__ import annotations

import csv
import json
from collections import defaultdict
from pathlib import Path

import unreal


# Update these paths to project-specific assets. Empty values use Engine basic-shape fallbacks.
TREE_ASSETS = {
    "small_tree": "",
    "medium_tree": "",
    "large_tree": "",
    "shrub": "",
}

BUILDING_ASSETS = {
    "box": "",
}

FALLBACK_TREE_MESH = "/Engine/BasicShapes/Cone.Cone"
FALLBACK_BUILDING_MESH = "/Engine/BasicShapes/Cube.Cube"
FALLBACK_GROUND_MESH = "/Engine/BasicShapes/Plane.Plane"
FALLBACK_TREE_ACTOR_LIMIT = 2500
FALLBACK_TREE_VISUAL_SCALE = 8.0
VIEW_PADDING_MULTIPLIER = 1.15
SATELLITE_REFERENCE_Z_CM = -25.0

# Leave empty when this script is inside Content/GeneratedWorlds/<Area>/unreal.
# For plugin use, set this to an absolute path or keep one generated world under Content/GeneratedWorlds.
GENERATED_WORLD_DIR = ""


def log(message: str) -> None:
    unreal.log(f"[RealWorldGenerator] {message}")


def warn(message: str) -> None:
    unreal.log_warning(f"[RealWorldGenerator] {message}")


def require_editor_scripting_api() -> None:
    missing = [
        name
        for name in ("EditorAssetLibrary", "EditorLevelLibrary")
        if not hasattr(unreal, name)
    ]
    if missing:
        raise RuntimeError(
            "Missing Unreal Python editor APIs: "
            + ", ".join(missing)
            + ". Enable the Editor Scripting Utilities plugin for this project, "
            "then fully restart Unreal Editor."
        )


def find_generated_world_dir() -> Path:
    if GENERATED_WORLD_DIR:
        path = Path(GENERATED_WORLD_DIR).expanduser().resolve()
        if (path / "metadata.json").exists():
            return path
        raise RuntimeError(f"GENERATED_WORLD_DIR does not contain metadata.json: {path}")

    script_dir = Path(__file__).resolve().parent
    candidates = [
        script_dir.parent,
        Path(unreal.Paths.project_content_dir()) / "GeneratedWorlds",
    ]

    if (candidates[0] / "metadata.json").exists():
        return candidates[0]

    metadata_files = list(candidates[1].glob("*/metadata.json")) if candidates[1].exists() else []
    if len(metadata_files) == 1:
        return metadata_files[0].parent
    if len(metadata_files) > 1:
        names = ", ".join(path.parent.name for path in metadata_files)
        raise RuntimeError(
            "Multiple generated worlds found under Content/GeneratedWorlds. "
            f"Set GENERATED_WORLD_DIR at the top of this script. Found: {names}"
        )
    raise RuntimeError("Could not find generated world metadata.json.")


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def read_csv_rows(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        warn(f"Missing CSV: {path}")
        return []
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def load_mesh(asset_path: str, fallback_path: str) -> unreal.StaticMesh | None:
    if not asset_path:
        return unreal.EditorAssetLibrary.load_asset(fallback_path)
    mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
    if mesh:
        return mesh
    warn(f"Missing mesh {asset_path}; using fallback {fallback_path}")
    mesh = unreal.EditorAssetLibrary.load_asset(fallback_path)
    if not mesh:
        warn(f"Fallback mesh is unavailable: {fallback_path}")
        return None
    return mesh


def asset_path_exists(asset_path: str) -> bool:
    return unreal.EditorAssetLibrary.does_asset_exist(asset_path) or unreal.EditorAssetLibrary.does_asset_exist(f"{asset_path}.{asset_path.rsplit('/', 1)[-1]}")


def load_or_import_satellite_texture(world_dir: Path, area_name: str) -> unreal.Texture2D | None:
    source_path = world_dir / "satellite" / "satellite_unreal_extent_annotated.png"
    if not source_path.exists():
        source_path = world_dir / "satellite" / "satellite_rgb.png"
    if not source_path.exists():
        warn(f"Satellite image is unavailable: {source_path}")
        return None

    destination_path = f"/Game/GeneratedWorlds/{area_name}/Satellite"
    asset_name = "T_RWG_SatelliteReference"
    texture_path = f"{destination_path}/{asset_name}"

    unreal.EditorAssetLibrary.make_directory(destination_path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_path))
    task.set_editor_property("destination_path", destination_path)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = unreal.EditorAssetLibrary.load_asset(texture_path)
    if not texture:
        warn(f"Failed to import satellite texture from {source_path}")
        return None
    log(f"Imported satellite reference texture {texture_path}")
    return texture


def create_satellite_material(texture: unreal.Texture2D, area_name: str) -> unreal.MaterialInterface | None:
    material_folder = f"/Game/GeneratedWorlds/{area_name}/Materials"
    material_path = f"{material_folder}/M_RWG_SatelliteReference"
    existing = unreal.EditorAssetLibrary.load_asset(material_path)
    if existing:
        return existing

    if not hasattr(unreal, "MaterialEditingLibrary"):
        warn("MaterialEditingLibrary is unavailable; cannot create satellite material.")
        return None

    unreal.EditorAssetLibrary.make_directory(material_folder)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_RWG_SatelliteReference",
        material_folder,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not material:
        warn("Failed to create satellite reference material.")
        return None

    try:
        material.set_editor_property("two_sided", True)
        texture_sample = unreal.MaterialEditingLibrary.create_material_expression(
            material,
            unreal.MaterialExpressionTextureSample,
            -400,
            0,
        )
        texture_sample.set_editor_property("texture", texture)
        unreal.MaterialEditingLibrary.connect_material_property(
            texture_sample,
            "RGB",
            unreal.MaterialProperty.MP_BASE_COLOR,
        )
        unreal.MaterialEditingLibrary.connect_material_property(
            texture_sample,
            "RGB",
            unreal.MaterialProperty.MP_EMISSIVE_COLOR,
        )
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material)
        log(f"Created satellite reference material {material_path}")
        return material
    except Exception as exc:
        warn(f"Failed to wire satellite material: {exc}")
        return None


def ensure_level(area_name: str) -> str:
    folder = f"/Game/GeneratedWorlds/{area_name}"
    map_path = f"{folder}/{area_name}"
    unreal.EditorAssetLibrary.make_directory(folder)
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        log(f"Opening existing level {map_path}")
        unreal.EditorLevelLibrary.load_level(map_path)
    else:
        log(f"Creating level {map_path}")
        unreal.EditorLevelLibrary.new_level(map_path)
    return map_path


def clear_generated_actors() -> None:
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        try:
            label = actor.get_actor_label()
        except Exception:
            continue
        if label.startswith("RWG_"):
            unreal.EditorLevelLibrary.destroy_actor(actor)


def spawn_actor_class(class_name: str, label: str, location: unreal.Vector, rotation: unreal.Rotator) -> unreal.Actor | None:
    actor_class = getattr(unreal, class_name, None)
    if actor_class is None:
        warn(f"Unreal class is unavailable in this engine build: {class_name}")
        return None
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, location, rotation)
    if actor:
        actor.set_actor_label(label)
    return actor


def set_light_intensity(actor: unreal.Actor | None, intensity: float) -> None:
    if actor is None or not hasattr(actor, "get_component_by_class"):
        return
    for component_class_name in ("LightComponent", "DirectionalLightComponent", "SkyLightComponent"):
        component_class = getattr(unreal, component_class_name, None)
        if component_class is None:
            continue
        component = actor.get_component_by_class(component_class)
        if component and hasattr(component, "set_editor_property"):
            try:
                component.set_editor_property("intensity", intensity)
            except Exception:
                pass


def create_scene_visibility_helpers(metadata: dict) -> None:
    units_per_meter = float(metadata["unreal"]["units_per_meter"])
    world_width_cm = float(metadata["world_width_m"]) * units_per_meter
    world_height_cm = float(metadata["world_height_m"]) * units_per_meter
    center_x = world_width_cm * 0.5
    center_y = world_height_cm * 0.5
    top_z = max(float(metadata["elevation_range_m"]) * units_per_meter, 10000.0)
    center = unreal.Vector(center_x, center_y, top_z)

    sun = spawn_actor_class("DirectionalLight", "RWG_Sun", center, unreal.Rotator(-45.0, -35.0, 0.0))
    sky = spawn_actor_class("SkyLight", "RWG_SkyLight", center, unreal.Rotator(0.0, 0.0, 0.0))
    set_light_intensity(sun, 6.0)
    set_light_intensity(sky, 2.0)

    if hasattr(unreal.EditorLevelLibrary, "set_level_viewport_camera_info"):
        camera_height = max(center_x, center_y) * VIEW_PADDING_MULTIPLIER
        unreal.EditorLevelLibrary.set_level_viewport_camera_info(
            unreal.Vector(center_x, center_y, camera_height),
            unreal.Rotator(-90.0, 0.0, 0.0),
        )


def create_satellite_reference_plane(world_dir: Path, metadata: dict) -> None:
    units_per_meter = float(metadata["unreal"]["units_per_meter"])
    world_width_cm = float(metadata["world_width_m"]) * units_per_meter
    world_height_cm = float(metadata["world_height_m"]) * units_per_meter
    center_x = world_width_cm * 0.5
    center_y = world_height_cm * 0.5
    area_name = metadata["area_name"]

    ground_mesh = unreal.EditorAssetLibrary.load_asset(FALLBACK_GROUND_MESH)
    if not ground_mesh:
        warn(f"Fallback ground mesh is unavailable: {FALLBACK_GROUND_MESH}")
        return

    texture = load_or_import_satellite_texture(world_dir, area_name)
    material = create_satellite_material(texture, area_name) if texture else None
    ground = unreal.EditorLevelLibrary.spawn_actor_from_object(
        ground_mesh,
        unreal.Vector(center_x, center_y, SATELLITE_REFERENCE_Z_CM),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    ground.set_actor_label("RWG_SatelliteReference")
    ground.set_actor_scale3d(unreal.Vector(world_width_cm / 100.0, world_height_cm / 100.0, 1.0))
    if material and hasattr(ground, "static_mesh_component"):
        ground.static_mesh_component.set_material(0, material)
    elif material:
        component = ground.get_component_by_class(unreal.StaticMeshComponent)
        if component:
            component.set_material(0, material)
    log("Added top-down satellite reference plane.")


def create_top_down_camera(metadata: dict) -> None:
    units_per_meter = float(metadata["unreal"]["units_per_meter"])
    world_width_cm = float(metadata["world_width_m"]) * units_per_meter
    world_height_cm = float(metadata["world_height_m"]) * units_per_meter
    center_x = world_width_cm * 0.5
    center_y = world_height_cm * 0.5
    camera_height = max(world_width_cm, world_height_cm) * 0.95

    camera = spawn_actor_class(
        "CameraActor",
        "RWG_TopDownSatelliteCamera",
        unreal.Vector(center_x, center_y, camera_height),
        unreal.Rotator(-90.0, 0.0, 0.0),
    )
    if camera:
        component = camera.get_component_by_class(unreal.CameraComponent)
        if component:
            component.set_editor_property("projection_mode", unreal.CameraProjectionMode.ORTHOGRAPHIC)
            component.set_editor_property("ortho_width", max(world_width_cm, world_height_cm) * 1.08)


def make_transform(x_m: float, y_m: float, z_m: float, yaw_degrees: float, scale: float, units_per_meter: float) -> unreal.Transform:
    transform = unreal.Transform()
    transform.set_editor_property("translation", unreal.Vector(x_m * units_per_meter, y_m * units_per_meter, z_m * units_per_meter))
    transform.set_editor_property("rotation", unreal.Rotator(0.0, yaw_degrees, 0.0).quaternion())
    transform.set_editor_property("scale3d", unreal.Vector(scale, scale, scale))
    return transform


def sample_rows(rows: list[dict[str, str]], limit: int) -> list[dict[str, str]]:
    if limit <= 0:
        return []
    if len(rows) <= limit:
        return rows
    stride = len(rows) / float(limit)
    return [rows[int(index * stride)] for index in range(limit)]


def spawn_trees(world_dir: Path, metadata: dict) -> int:
    units_per_meter = float(metadata["unreal"]["units_per_meter"])
    csv_path = world_dir / metadata["generated_file_paths"]["tree_instances"]
    rows = read_csv_rows(csv_path)
    if not rows:
        log("No tree instances to spawn.")
        return 0

    grouped: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        grouped[row.get("asset_type", "medium_tree")].append(row)

    total = 0
    remaining_limit = FALLBACK_TREE_ACTOR_LIMIT
    remaining_rows = len(rows)
    for asset_type, instances in grouped.items():
        mesh = load_mesh(TREE_ASSETS.get(asset_type, TREE_ASSETS.get("medium_tree", "")), FALLBACK_TREE_MESH)
        if mesh is None:
            remaining_rows -= len(instances)
            continue
        if FALLBACK_TREE_ACTOR_LIMIT > 0:
            group_limit = max(1, round(remaining_limit * (len(instances) / float(remaining_rows))))
            group_limit = min(group_limit, remaining_limit, len(instances))
        else:
            group_limit = len(instances)
        selected_instances = sample_rows(instances, group_limit)
        for index, row in enumerate(selected_instances):
            x_cm = float(row["x_m_local"]) * units_per_meter
            y_cm = float(row["y_m_local"]) * units_per_meter
            z_cm = float(row["z_m_terrain"]) * units_per_meter
            rotation = unreal.Rotator(0.0, float(row.get("yaw_degrees", 0.0)), 0.0)
            actor = unreal.EditorLevelLibrary.spawn_actor_from_object(mesh, unreal.Vector(x_cm, y_cm, z_cm), rotation)
            actor.set_actor_label(f"RWG_Tree_{asset_type}_{index:05d}")
            scale = float(row.get("scale", 1.0)) * FALLBACK_TREE_VISUAL_SCALE
            actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
            total += 1
        remaining_limit -= len(selected_instances)
        remaining_rows -= len(instances)
        log(f"Spawned {len(selected_instances)} visible tree actors for asset_type={asset_type} from {len(instances)} generated points")
    if FALLBACK_TREE_ACTOR_LIMIT > 0 and total < len(rows):
        warn(f"Displayed {total} of {len(rows)} generated tree points to keep the editor responsive.")
    return total


def spawn_buildings(world_dir: Path, metadata: dict) -> int:
    units_per_meter = float(metadata["unreal"]["units_per_meter"])
    csv_path = world_dir / metadata["generated_file_paths"]["building_instances"]
    rows = read_csv_rows(csv_path)
    if not rows:
        log("No building instances to spawn.")
        return 0

    cube_mesh = load_mesh(BUILDING_ASSETS.get("box", ""), FALLBACK_BUILDING_MESH)
    if cube_mesh is None:
        return 0

    count = 0
    for index, row in enumerate(rows):
        width_m = float(row.get("width_m", 4.0))
        depth_m = float(row.get("depth_m", 4.0))
        height_m = float(row.get("height_m", 3.0))
        x_cm = float(row["x_m_local"]) * units_per_meter
        y_cm = float(row["y_m_local"]) * units_per_meter
        z_cm = (float(row["z_m_terrain"]) + height_m * 0.5) * units_per_meter
        rotation = unreal.Rotator(0.0, float(row.get("yaw_degrees", 0.0)), 0.0)
        actor = unreal.EditorLevelLibrary.spawn_actor_from_object(cube_mesh, unreal.Vector(x_cm, y_cm, z_cm), rotation)
        actor.set_actor_label(f"RWG_Building_{index:04d}")
        actor.set_actor_scale3d(unreal.Vector(width_m, depth_m, height_m))
        count += 1
    log(f"Spawned {count} placeholder buildings.")
    return count


def print_landscape_import_instructions(world_dir: Path, metadata: dict) -> None:
    heightmap = world_dir / metadata["generated_file_paths"]["terrain_heightmap"]
    unreal_data = metadata["unreal"]
    warn("Automatic Landscape import is not attempted because Unreal Python support varies by engine version.")
    warn("Manual Landscape import values:")
    warn(f"  Heightmap: {heightmap}")
    warn(f"  Resolution: {metadata['heightmap_size']} x {metadata['heightmap_size']}")
    warn(f"  XY scale: {unreal_data['xy_scale']}")
    warn(f"  Z scale: {unreal_data['z_scale']}")
    warn("  Format: 16-bit RAW/R16, little-endian, unsigned")
    warn("After importing the Landscape, run this script again to refresh instances.")


def main() -> None:
    require_editor_scripting_api()
    world_dir = find_generated_world_dir()
    metadata = load_json(world_dir / "metadata.json")
    area_name = metadata["area_name"]
    ensure_level(area_name)
    clear_generated_actors()
    create_scene_visibility_helpers(metadata)
    create_satellite_reference_plane(world_dir, metadata)
    create_top_down_camera(metadata)
    print_landscape_import_instructions(world_dir, metadata)
    tree_count = spawn_trees(world_dir, metadata)
    building_count = spawn_buildings(world_dir, metadata)
    unreal.EditorLevelLibrary.save_current_level()
    log(f"Import complete for {area_name}: {tree_count} trees, {building_count} buildings.")


if __name__ == "__main__":
    main()
