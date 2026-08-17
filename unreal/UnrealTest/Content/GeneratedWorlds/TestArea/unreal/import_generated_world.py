from __future__ import annotations

import csv
import json
from collections import defaultdict
from pathlib import Path

import unreal


# Update these paths to project-specific assets. Missing assets fall back to Engine basic shapes.
TREE_ASSETS = {
    "small_tree": "/Game/Environment/Trees/SM_Tree_Small",
    "medium_tree": "/Game/Environment/Trees/SM_Tree_Medium",
    "large_tree": "/Game/Environment/Trees/SM_Tree_Large",
    "shrub": "/Game/Environment/Trees/SM_Shrub",
}

BUILDING_ASSETS = {
    "box": "/Game/Environment/Buildings/SM_ProceduralBox",
}

FALLBACK_TREE_MESH = "/Engine/BasicShapes/Cone.Cone"
FALLBACK_BUILDING_MESH = "/Engine/BasicShapes/Cube.Cube"

# Leave empty when this script is inside Content/GeneratedWorlds/<Area>/unreal.
# For plugin use, set this to an absolute path or keep one generated world under Content/GeneratedWorlds.
GENERATED_WORLD_DIR = ""


def log(message: str) -> None:
    unreal.log(f"[RealWorldGenerator] {message}")


def warn(message: str) -> None:
    unreal.log_warning(f"[RealWorldGenerator] {message}")


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
    mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
    if mesh:
        return mesh
    warn(f"Missing mesh {asset_path}; using fallback {fallback_path}")
    mesh = unreal.EditorAssetLibrary.load_asset(fallback_path)
    if not mesh:
        warn(f"Fallback mesh is unavailable: {fallback_path}")
        return None
    return mesh


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


def create_actor(label: str) -> unreal.Actor:
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.Actor,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    actor.set_actor_label(label)
    root = unreal.SceneComponent(actor)
    actor.add_instance_component(root)
    actor.set_root_component(root)
    root.register_component()
    return actor


def make_transform(x_m: float, y_m: float, z_m: float, yaw_degrees: float, scale: float, units_per_meter: float) -> unreal.Transform:
    transform = unreal.Transform()
    transform.set_editor_property("translation", unreal.Vector(x_m * units_per_meter, y_m * units_per_meter, z_m * units_per_meter))
    transform.set_editor_property("rotation", unreal.Rotator(0.0, yaw_degrees, 0.0).quaternion())
    transform.set_editor_property("scale3d", unreal.Vector(scale, scale, scale))
    return transform


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
    for asset_type, instances in grouped.items():
        mesh = load_mesh(TREE_ASSETS.get(asset_type, TREE_ASSETS.get("medium_tree", "")), FALLBACK_TREE_MESH)
        if mesh is None:
            continue
        actor = create_actor(f"RWG_Trees_{asset_type}")
        component = unreal.HierarchicalInstancedStaticMeshComponent(actor)
        component.set_editor_property("static_mesh", mesh)
        actor.add_instance_component(component)
        component.register_component()
        for row in instances:
            transform = make_transform(
                float(row["x_m_local"]),
                float(row["y_m_local"]),
                float(row["z_m_terrain"]),
                float(row.get("yaw_degrees", 0.0)),
                float(row.get("scale", 1.0)),
                units_per_meter,
            )
            component.add_instance(transform)
            total += 1
        log(f"Spawned {len(instances)} instanced trees for asset_type={asset_type}")
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
    world_dir = find_generated_world_dir()
    metadata = load_json(world_dir / "metadata.json")
    area_name = metadata["area_name"]
    ensure_level(area_name)
    clear_generated_actors()
    print_landscape_import_instructions(world_dir, metadata)
    tree_count = spawn_trees(world_dir, metadata)
    building_count = spawn_buildings(world_dir, metadata)
    unreal.EditorLevelLibrary.save_current_level()
    log(f"Import complete for {area_name}: {tree_count} trees, {building_count} buildings.")


if __name__ == "__main__":
    main()
