import inspect
import unreal

for obj_name in ("LidarPointCloudFileIO_ASCII", "LidarPointCloudBlueprintLibrary"):
    obj = getattr(unreal, obj_name)
    for method_name in ("create_point_cloud_from_file", "create_point_cloud_empty"):
        if hasattr(obj, method_name):
            method = getattr(obj, method_name)
            unreal.log(f"[RWG_LIDAR_SIGNATURE] {obj_name}.{method_name}: {method}")
            try:
                unreal.log(f"[RWG_LIDAR_SIGNATURE] signature: {inspect.signature(method)}")
            except Exception as exc:
                unreal.log_warning(f"[RWG_LIDAR_SIGNATURE] no signature: {exc}")
            try:
                unreal.log(f"[RWG_LIDAR_SIGNATURE] doc: {method.__doc__}")
            except Exception as exc:
                unreal.log_warning(f"[RWG_LIDAR_SIGNATURE] no doc: {exc}")
