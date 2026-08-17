from pathlib import Path
import unreal

SRC = Path("/home/justin/Documents/pointCloudToUnreal/data/exports/GeneratedWorlds/SanJoaquin_Lifelike_500m/pointcloud/test_small.pts")
SRC.parent.mkdir(parents=True, exist_ok=True)
SRC.write_text("x y z r g b\n0 0 0 255 0 0\n100 0 0 0 255 0\n0 100 0 0 0 255\n", encoding="utf-8")

folder = "/Game/GeneratedWorlds/SanJoaquin_Lifelike_500m/PointCloud"
path = f"{folder}/PC_TestSmall"
unreal.EditorAssetLibrary.make_directory(folder)
if unreal.EditorAssetLibrary.does_asset_exist(path):
    unreal.EditorAssetLibrary.delete_asset(path)

factory = unreal.LidarPointCloudFactory()
asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset("PC_TestSmall", folder, unreal.LidarPointCloud, factory)
unreal.log(f"[RWG_PC_TEST] created asset={asset} path={asset.get_path_name() if asset else None}")

latent = unreal.LatentActionInfo()
columns = unreal.LidarPointCloudImportSettings_ASCII_Columns()
world = unreal.EditorLevelLibrary.get_editor_world()
tmp = unreal.LidarPointCloudFileIO_ASCII.create_point_cloud_from_file(
    world, str(SRC), False, unreal.Vector2D(0.0, 255.0), columns, latent
)[2]
unreal.log(f"[RWG_PC_TEST] tmp={tmp} points={tmp.get_num_points() if tmp else None}")

for name in dir(asset):
    if "data" in name.lower() or "insert" in name.lower() or "refresh" in name.lower() or "source" in name.lower():
        unreal.log(f"[RWG_PC_TEST] asset method {name}: {getattr(asset, name)} doc={getattr(asset, name).__doc__ if hasattr(getattr(asset, name), '__doc__') else ''}")

points = tmp.get_points_as_copies(False, 0, -1)
unreal.log(f"[RWG_PC_TEST] copied points len={len(points)} first={points[0] if points else None}")

ok = False
try:
    ok = asset.set_data(points)
    unreal.log(f"[RWG_PC_TEST] set_data ok={ok} points={asset.get_num_points()}")
except Exception as exc:
    unreal.log_warning(f"[RWG_PC_TEST] set_data failed {exc}")
    try:
        asset.insert_points(points, unreal.LidarPointCloudDuplicateHandling.IGNORE, False, unreal.Vector(0.0, 0.0, 0.0))
        ok = True
        unreal.log(f"[RWG_PC_TEST] insert_points ok points={asset.get_num_points()}")
    except Exception as exc2:
        unreal.log_warning(f"[RWG_PC_TEST] insert_points failed {exc2}")

if ok:
    asset.set_source_path(str(SRC))
    asset.refresh_rendering()
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    loaded = unreal.EditorAssetLibrary.load_asset(path)
    unreal.log(f"[RWG_PC_TEST] loaded={loaded} loaded_points={loaded.get_num_points() if loaded else None}")
