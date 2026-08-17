from __future__ import annotations

import json
import csv
from pathlib import Path

import unreal


AREA_NAME = "SanJoaquin_Lifelike_500m"
RAW_MAP_NAME = "SanJoaquin_RawPointCloud_500m"
WORLD_DIR = Path("/home/justin/Documents/pointCloudToUnreal/data/exports/GeneratedWorlds/SanJoaquin_Lifelike_500m")
POINTCLOUD_SOURCE = WORLD_DIR / "pointcloud" / "SanJoaquin_Lifelike_500m_satellite_colored.pts"
POINTCLOUD_ASSET_FOLDER = f"/Game/GeneratedWorlds/{AREA_NAME}/PointCloud"
POINTCLOUD_ASSET_PATH = f"{POINTCLOUD_ASSET_FOLDER}/PC_SatelliteColored_Lidar"
SATELLITE_TEXTURE_SOURCE = WORLD_DIR / "satellite" / "satellite_reference_mirror_x.png"
SATELLITE_TEXTURE_FOLDER = f"/Game/GeneratedWorlds/{AREA_NAME}/Satellite"
SATELLITE_TEXTURE_PATH = f"{SATELLITE_TEXTURE_FOLDER}/T_RWG_SatelliteReference"
SATELLITE_MATERIAL_FOLDER = f"/Game/GeneratedWorlds/{AREA_NAME}/Materials"
SATELLITE_MATERIAL_PATH = f"{SATELLITE_MATERIAL_FOLDER}/M_RWG_SatelliteReference"
GROUND_MESH = "/Engine/BasicShapes/Plane.Plane"
FEATURE_MESH_FOLDER = f"/Game/GeneratedWorlds/{AREA_NAME}/Meshes"
TERRAIN_MESH_ASSET_PATH = f"{FEATURE_MESH_FOLDER}/SM_RWG_TerrainMesh"
BUILDING_MESH = "/Engine/BasicShapes/Cube.Cube"
ROAD_MESH = "/Engine/BasicShapes/Plane.Plane"
TREE_TRUNK_MESH = "/Engine/BasicShapes/Cube.Cube"
TREE_CANOPY_MESH = "/Engine/BasicShapes/Sphere.Sphere"
TREE_ACTOR_LIMIT = 2500
SPAWN_FLAT_SATELLITE_REFERENCE = False


def log(message: str) -> None:
    unreal.log(f"[RawPointCloudWorld] {message}")


def warn(message: str) -> None:
    unreal.log_warning(f"[RawPointCloudWorld] {message}")


def load_metadata() -> dict:
    with (WORLD_DIR / "metadata.json").open("r", encoding="utf-8") as handle:
        return json.load(handle)


def read_csv_rows(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        warn(f"Missing CSV: {path}")
        return []
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def import_file_as_asset(source: Path, destination_path: str, destination_name: str, replace: bool = True) -> unreal.Object | None:
    if not source.exists():
        warn(f"Missing source file: {source}")
        return None
    unreal.EditorAssetLibrary.make_directory(destination_path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", destination_path)
    task.set_editor_property("destination_name", destination_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", replace)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported_paths = list(task.get_editor_property("imported_object_paths") or [])
    if imported_paths:
        return unreal.EditorAssetLibrary.load_asset(imported_paths[0])
    return unreal.EditorAssetLibrary.load_asset(f"{destination_path}/{destination_name}")


def make_rotator(pitch: float = 0.0, yaw: float = 0.0, roll: float = 0.0) -> unreal.Rotator:
    rotator = unreal.Rotator()
    rotator.set_editor_property("pitch", pitch)
    rotator.set_editor_property("yaw", yaw)
    rotator.set_editor_property("roll", roll)
    return rotator


def connect_expression(expression: unreal.MaterialExpression, output_name: str, material_property: unreal.MaterialProperty) -> None:
    try:
        unreal.MaterialEditingLibrary.connect_material_property(expression, output_name, material_property)
    except Exception:
        if output_name:
            unreal.MaterialEditingLibrary.connect_material_property(expression, "", material_property)
        else:
            raise


def ensure_color_material(name: str, color: unreal.LinearColor, replace: bool = True) -> unreal.MaterialInterface | None:
    material_path = f"{SATELLITE_MATERIAL_FOLDER}/{name}"
    if replace and unreal.EditorAssetLibrary.does_asset_exist(material_path):
        unreal.EditorAssetLibrary.delete_asset(material_path)
    else:
        existing = unreal.EditorAssetLibrary.load_asset(material_path)
        if existing:
            return existing
    unreal.EditorAssetLibrary.make_directory(SATELLITE_MATERIAL_FOLDER)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name,
        SATELLITE_MATERIAL_FOLDER,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not material:
        warn(f"Could not create material {material_path}")
        return None
    try:
        material.set_editor_property("two_sided", True)
        constant = unreal.MaterialEditingLibrary.create_material_expression(
            material,
            unreal.MaterialExpressionConstant3Vector,
            -250,
            0,
        )
        constant.set_editor_property("constant", color)
        connect_expression(constant, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        connect_expression(constant, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        roughness = unreal.MaterialEditingLibrary.create_material_expression(
            material,
            unreal.MaterialExpressionConstant,
            -250,
            160,
        )
        roughness.set_editor_property("r", 0.85)
        unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material)
    except Exception as exc:
        warn(f"Could not wire material {name}: {exc}")
    return material


def set_actor_material(actor: unreal.Actor | None, material: unreal.MaterialInterface | None) -> None:
    if actor is None:
        return
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if component:
        try:
            component.set_mobility(unreal.ComponentMobility.MOVABLE)
        except Exception:
            try:
                component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
            except Exception:
                pass
        if material is not None:
            component.set_material(0, material)


def ensure_satellite_texture() -> unreal.Texture2D | None:
    source = SATELLITE_TEXTURE_SOURCE if SATELLITE_TEXTURE_SOURCE.exists() else WORLD_DIR / "satellite" / "satellite_rgb.png"
    texture = import_file_as_asset(source, SATELLITE_TEXTURE_FOLDER, "T_RWG_SatelliteReference", replace=True)
    return texture


def ensure_satellite_material(texture: unreal.Texture2D | None) -> unreal.MaterialInterface | None:
    existing = unreal.EditorAssetLibrary.load_asset(SATELLITE_MATERIAL_PATH)
    if existing:
        return existing
    if texture is None:
        return None
    unreal.EditorAssetLibrary.make_directory(SATELLITE_MATERIAL_FOLDER)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_RWG_SatelliteReference",
        SATELLITE_MATERIAL_FOLDER,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not material:
        return None
    material.set_editor_property("two_sided", True)
    texture_sample = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSample,
        -400,
        0,
    )
    texture_sample.set_editor_property("texture", texture)
    unreal.MaterialEditingLibrary.connect_material_property(texture_sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(texture_sample, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def ensure_raw_level() -> str:
    folder = f"/Game/GeneratedWorlds/{AREA_NAME}"
    map_path = f"{folder}/{RAW_MAP_NAME}"
    unreal.EditorAssetLibrary.make_directory(folder)
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        unreal.EditorLevelLibrary.load_level(map_path)
    else:
        unreal.EditorLevelLibrary.new_level(map_path)
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        try:
            label = actor.get_actor_label()
        except Exception:
            continue
        if label.startswith("RWG_") or label.startswith("RPC_") or label.startswith("RWF_"):
            unreal.EditorLevelLibrary.destroy_actor(actor)
    return map_path


def spawn_lights(center: unreal.Vector) -> None:
    sun = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.DirectionalLight, center, make_rotator(pitch=-55.0, yaw=-35.0))
    sun.set_actor_label("RPC_Sun")
    for component_class in (unreal.DirectionalLightComponent, unreal.LightComponent):
        component = sun.get_component_by_class(component_class)
        if component:
            try:
                component.set_mobility(unreal.ComponentMobility.MOVABLE)
            except Exception:
                pass
    sky = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkyLight, center, make_rotator())
    sky.set_actor_label("RPC_SkyLight")
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    if sky_component:
        try:
            sky_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        except Exception:
            pass


def spawn_satellite_plane(metadata: dict, material: unreal.MaterialInterface | None) -> None:
    units_per_meter = float(metadata["unreal"]["units_per_meter"])
    width_cm = float(metadata["world_width_m"]) * units_per_meter
    height_cm = float(metadata["world_height_m"]) * units_per_meter
    center = unreal.Vector(width_cm * 0.5, height_cm * 0.5, -25.0)
    plane_mesh = unreal.EditorAssetLibrary.load_asset(GROUND_MESH)
    plane = unreal.EditorLevelLibrary.spawn_actor_from_object(plane_mesh, center, make_rotator())
    plane.set_actor_label("RPC_SatelliteReference")
    plane.set_actor_scale3d(unreal.Vector(width_cm / 100.0, height_cm / 100.0, 1.0))
    component = plane.get_component_by_class(unreal.StaticMeshComponent)
    if component and material:
        component.set_material(0, material)


def spawn_terrain_mesh(metadata: dict, material: unreal.MaterialInterface | None) -> None:
    relative_path = metadata.get("generated_file_paths", {}).get("terrain_mesh_obj")
    if not relative_path:
        warn("No terrain_mesh_obj entry in metadata; skipping terrain mesh.")
        return
    source = WORLD_DIR / relative_path
    if not source.exists():
        warn(f"Missing terrain OBJ: {source}")
        return
    mesh = import_file_as_asset(source, FEATURE_MESH_FOLDER, "SM_RWG_TerrainMesh", replace=True)
    if not mesh:
        warn("Terrain OBJ import produced no static mesh asset.")
        return
    actor = unreal.EditorLevelLibrary.spawn_actor_from_object(mesh, unreal.Vector(0.0, 0.0, 0.0), make_rotator())
    if actor:
        actor.set_actor_label("RWG_TerrainMesh")
        set_actor_material(actor, material)
        log(f"Spawned terrain mesh from {source.name}.")


def spawn_building_boxes(metadata: dict, material: unreal.MaterialInterface | None) -> int:
    csv_rel = metadata.get("generated_file_paths", {}).get("building_instances", "instances/building_instances.csv")
    rows = read_csv_rows(WORLD_DIR / csv_rel)
    if not rows:
        log("No inferred buildings to spawn.")
        return 0
    mesh = unreal.EditorAssetLibrary.load_asset(BUILDING_MESH)
    if not mesh:
        warn(f"Missing building mesh: {BUILDING_MESH}")
        return 0
    units_per_meter = float(metadata["unreal"]["units_per_meter"])
    count = 0
    for index, row in enumerate(rows):
        width_m = float(row.get("width_m", 4.0))
        depth_m = float(row.get("depth_m", 4.0))
        height_m = float(row.get("height_m", 3.0))
        x_cm = float(row["x_m_local"]) * units_per_meter
        y_cm = float(row["y_m_local"]) * units_per_meter
        z_cm = (float(row.get("z_m_terrain", 0.0)) + height_m * 0.5) * units_per_meter
        yaw = float(row.get("yaw_degrees", 0.0))
        actor = unreal.EditorLevelLibrary.spawn_actor_from_object(mesh, unreal.Vector(x_cm, y_cm, z_cm), make_rotator(yaw=yaw))
        if not actor:
            continue
        actor.set_actor_label(f"RWG_Building_{index:04d}")
        actor.set_actor_scale3d(unreal.Vector(width_m, depth_m, height_m))
        set_actor_material(actor, material)
        count += 1
    log(f"Spawned {count} inferred building boxes.")
    return count


def spawn_road_slabs(metadata: dict, material: unreal.MaterialInterface | None) -> int:
    csv_rel = metadata.get("generated_file_paths", {}).get("road_instances", "instances/road_instances.csv")
    rows = read_csv_rows(WORLD_DIR / csv_rel)
    if not rows:
        log("No inferred road slabs to spawn.")
        return 0
    mesh = unreal.EditorAssetLibrary.load_asset(ROAD_MESH)
    if not mesh:
        warn(f"Missing road mesh: {ROAD_MESH}")
        return 0
    units_per_meter = float(metadata["unreal"]["units_per_meter"])
    count = 0
    for index, row in enumerate(rows):
        width_m = float(row.get("width_m", 4.0))
        depth_m = float(row.get("depth_m", 4.0))
        x_cm = float(row["x_m_local"]) * units_per_meter
        y_cm = float(row["y_m_local"]) * units_per_meter
        z_cm = float(row.get("z_m_terrain", 0.0)) * units_per_meter + 8.0
        yaw = float(row.get("yaw_degrees", 0.0))
        actor = unreal.EditorLevelLibrary.spawn_actor_from_object(mesh, unreal.Vector(x_cm, y_cm, z_cm), make_rotator(yaw=yaw))
        if not actor:
            continue
        actor.set_actor_label(f"RWG_Road_{index:03d}")
        actor.set_actor_scale3d(unreal.Vector(width_m, depth_m, 1.0))
        set_actor_material(actor, material)
        count += 1
    log(f"Spawned {count} inferred road slabs.")
    return count


def sample_rows(rows: list[dict[str, str]], limit: int) -> list[dict[str, str]]:
    if limit <= 0 or len(rows) <= limit:
        return rows
    stride = len(rows) / float(limit)
    return [rows[int(index * stride)] for index in range(limit)]


def spawn_procedural_trees(metadata: dict, trunk_material: unreal.MaterialInterface | None, canopy_material: unreal.MaterialInterface | None) -> int:
    csv_rel = metadata.get("generated_file_paths", {}).get("tree_instances", "instances/tree_instances.csv")
    rows = read_csv_rows(WORLD_DIR / csv_rel)
    if not rows:
        log("No inferred trees to spawn.")
        return 0
    trunk_mesh = unreal.EditorAssetLibrary.load_asset(TREE_TRUNK_MESH)
    canopy_mesh = unreal.EditorAssetLibrary.load_asset(TREE_CANOPY_MESH)
    if not trunk_mesh or not canopy_mesh:
        warn("Missing engine basic tree meshes; skipping procedural trees.")
        return 0
    selected = sample_rows(rows, TREE_ACTOR_LIMIT)
    units_per_meter = float(metadata["unreal"]["units_per_meter"])
    count = 0
    for index, row in enumerate(selected):
        height_m = max(1.5, min(float(row.get("height_m", 4.0)), 18.0))
        crown_radius_m = max(0.8, min(float(row.get("crown_radius_m", height_m * 0.25)), 5.5))
        trunk_height_m = max(0.9, min(height_m * 0.35, 4.5))
        trunk_diameter_m = max(0.18, min(crown_radius_m * 0.16, 0.75))
        crown_height_m = max(1.2, crown_radius_m * 1.35)
        x_cm = float(row["x_m_local"]) * units_per_meter
        y_cm = float(row["y_m_local"]) * units_per_meter
        terrain_z_m = float(row.get("z_m_terrain", 0.0))

        trunk_z_cm = (terrain_z_m + trunk_height_m * 0.5) * units_per_meter
        trunk = unreal.EditorLevelLibrary.spawn_actor_from_object(
            trunk_mesh,
            unreal.Vector(x_cm, y_cm, trunk_z_cm),
            make_rotator(),
        )
        if trunk:
            trunk.set_actor_label(f"RWG_TreeTrunk_{index:04d}")
            trunk.set_actor_scale3d(unreal.Vector(trunk_diameter_m, trunk_diameter_m, trunk_height_m))
            set_actor_material(trunk, trunk_material)

        canopy_z_cm = (terrain_z_m + trunk_height_m + crown_height_m * 0.45) * units_per_meter
        canopy = unreal.EditorLevelLibrary.spawn_actor_from_object(
            canopy_mesh,
            unreal.Vector(x_cm, y_cm, canopy_z_cm),
            make_rotator(),
        )
        if canopy:
            canopy.set_actor_label(f"RWG_TreeCanopy_{index:04d}")
            canopy.set_actor_scale3d(unreal.Vector(crown_radius_m * 2.0, crown_radius_m * 2.0, crown_height_m))
            set_actor_material(canopy, canopy_material)
            count += 1
    if len(selected) < len(rows):
        warn(f"Displayed {len(selected)} of {len(rows)} inferred trees to keep the editor responsive.")
    log(f"Spawned {count} procedural tree canopies.")
    return count


def import_point_cloud_asset() -> tuple[unreal.LidarPointCloud | None, unreal.Vector]:
    existing = unreal.EditorAssetLibrary.load_asset(POINTCLOUD_ASSET_PATH)
    if existing:
        try:
            original_coordinates = existing.get_editor_property("original_coordinates")
        except Exception:
            original_coordinates = unreal.Vector(0.0, 0.0, 0.0)
        log(f"Using existing point cloud asset: {POINTCLOUD_ASSET_PATH} with {existing.get_num_points()} points.")
        return existing, original_coordinates

    latent = unreal.LatentActionInfo()
    columns = unreal.LidarPointCloudImportSettings_ASCII_Columns()
    world = unreal.EditorLevelLibrary.get_editor_world()
    result = unreal.LidarPointCloudFileIO_ASCII.create_point_cloud_from_file(
        world,
        str(POINTCLOUD_SOURCE),
        False,
        unreal.Vector2D(0.0, 255.0),
        columns,
        latent,
    )
    temp_cloud = result[2]
    if not temp_cloud:
        warn("Direct LiDAR ASCII import returned no point cloud.")
        return None, unreal.Vector(0.0, 0.0, 0.0)

    try:
        original_coordinates = temp_cloud.get_editor_property("original_coordinates")
    except Exception:
        original_coordinates = unreal.Vector(0.0, 0.0, 0.0)

    point_count = temp_cloud.get_num_points()
    log(f"Loaded temporary point cloud with {point_count} points.")

    unreal.EditorAssetLibrary.make_directory(POINTCLOUD_ASSET_FOLDER)
    if unreal.EditorAssetLibrary.does_asset_exist(POINTCLOUD_ASSET_PATH):
        unreal.EditorAssetLibrary.delete_asset(POINTCLOUD_ASSET_PATH)

    saved_asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "PC_SatelliteColored_Lidar",
        POINTCLOUD_ASSET_FOLDER,
        unreal.LidarPointCloud,
        unreal.LidarPointCloudFactory(),
    )
    if not saved_asset:
        warn("Could not create persistent LiDAR point cloud asset; using transient cloud for this session.")
        return temp_cloud, original_coordinates

    points = temp_cloud.get_points_as_copies(False, 0, -1)
    if not saved_asset.set_data(points):
        warn("Could not copy points into persistent LiDAR point cloud asset; using transient cloud for this session.")
        return temp_cloud, original_coordinates
    saved_asset.set_source_path(str(POINTCLOUD_SOURCE))
    try:
        saved_asset.set_editor_property("original_coordinates", original_coordinates)
    except Exception:
        pass
    saved_asset.refresh_bounds()
    saved_asset.refresh_rendering()
    unreal.EditorAssetLibrary.save_loaded_asset(saved_asset)
    log(f"Saved point cloud asset: {POINTCLOUD_ASSET_PATH} with {saved_asset.get_num_points()} points.")
    return saved_asset, original_coordinates


def spawn_point_cloud(asset: unreal.LidarPointCloud | None, actor_location: unreal.Vector) -> None:
    if not asset:
        warn("No point cloud asset available to spawn.")
        return
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.LidarPointCloudActor,
        actor_location,
        make_rotator(),
    )
    actor.set_actor_label("RPC_SatelliteColored_Lidar")
    actor.set_point_cloud(asset)
    component = actor.get_component_by_class(unreal.LidarPointCloudComponent)
    if component:
        for prop, value in (
            ("point_size", 2.0),
            ("point_size_bias", 0.0),
            ("intensity_influence", 0.0),
        ):
            try:
                component.set_editor_property(prop, value)
            except Exception:
                pass
        try:
            component.set_editor_property("color_source", unreal.LidarPointCloudColorationMode.DATA)
        except Exception:
            pass
    try:
        log(f"Spawned point cloud with {asset.get_num_points()} points at {actor_location}.")
    except Exception:
        log("Spawned point cloud.")


def spawn_camera(metadata: dict) -> None:
    units_per_meter = float(metadata["unreal"]["units_per_meter"])
    width_cm = float(metadata["world_width_m"]) * units_per_meter
    height_cm = float(metadata["world_height_m"]) * units_per_meter
    center_x = width_cm * 0.5
    center_y = height_cm * 0.5
    height = max(width_cm, height_cm) * 0.9
    camera = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.CameraActor,
        unreal.Vector(center_x, center_y, height),
        make_rotator(pitch=-90.0),
    )
    camera.set_actor_label("RPC_TopDownCamera")
    component = camera.get_component_by_class(unreal.CameraComponent)
    if component:
        component.set_editor_property("projection_mode", unreal.CameraProjectionMode.ORTHOGRAPHIC)
        component.set_editor_property("ortho_width", max(width_cm, height_cm) * 1.08)
    if hasattr(unreal.EditorLevelLibrary, "set_level_viewport_camera_info"):
        unreal.EditorLevelLibrary.set_level_viewport_camera_info(
            unreal.Vector(center_x, center_y, height),
            make_rotator(pitch=-90.0),
        )


def main() -> None:
    metadata = load_metadata()
    map_path = ensure_raw_level()
    texture = ensure_satellite_texture()
    material = ensure_satellite_material(texture)
    building_material = ensure_color_material("M_RWG_InferredBuilding", unreal.LinearColor(0.78, 0.78, 0.74, 1.0))
    road_material = ensure_color_material("M_RWG_InferredRoad", unreal.LinearColor(0.12, 0.13, 0.14, 1.0))
    trunk_material = ensure_color_material("M_RWG_TreeTrunk", unreal.LinearColor(0.23, 0.14, 0.08, 1.0))
    canopy_material = ensure_color_material("M_RWG_TreeCanopy", unreal.LinearColor(0.08, 0.34, 0.11, 1.0))
    units_per_meter = float(metadata["unreal"]["units_per_meter"])
    center = unreal.Vector(
        float(metadata["world_width_m"]) * units_per_meter * 0.5,
        float(metadata["world_height_m"]) * units_per_meter * 0.5,
        5000.0,
    )
    spawn_lights(center)
    if SPAWN_FLAT_SATELLITE_REFERENCE:
        spawn_satellite_plane(metadata, material)
    spawn_terrain_mesh(metadata, material)
    building_count = spawn_building_boxes(metadata, building_material)
    tree_count = spawn_procedural_trees(metadata, trunk_material, canopy_material)
    road_count = spawn_road_slabs(metadata, road_material)
    point_cloud, point_cloud_location = import_point_cloud_asset()
    spawn_point_cloud(point_cloud, point_cloud_location)
    spawn_camera(metadata)
    unreal.EditorLevelLibrary.save_current_level()
    log(f"Raw point cloud validation map saved: {map_path}")
    log(f"Scene features imported: {building_count} buildings, {tree_count} trees, {road_count} road slabs.")


if __name__ == "__main__":
    main()
