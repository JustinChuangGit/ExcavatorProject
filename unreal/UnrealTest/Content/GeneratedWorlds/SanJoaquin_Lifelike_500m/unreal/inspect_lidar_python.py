import unreal

names = [name for name in dir(unreal) if "Lidar" in name or "PointCloud" in name]
for name in sorted(names):
    unreal.log(f"[RWG_LIDAR_INSPECT] {name}")
    obj = getattr(unreal, name)
    methods = [method for method in dir(obj) if "create" in method.lower() or "import" in method.lower() or "point" in method.lower()]
    for method in sorted(methods)[:40]:
        unreal.log(f"[RWG_LIDAR_INSPECT]   {method}")
