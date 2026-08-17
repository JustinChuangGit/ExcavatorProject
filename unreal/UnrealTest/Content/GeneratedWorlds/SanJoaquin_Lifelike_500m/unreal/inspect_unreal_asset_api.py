import unreal

for name in dir(unreal):
    if "package" in name.lower() or "asset" in name.lower() and ("registry" in name.lower() or "save" in name.lower()):
        unreal.log(f"[RWG_ASSET_API] unreal.{name}")

for cls_name in ("Object", "LidarPointCloud"):
    cls = getattr(unreal, cls_name)
    for name in dir(cls):
        if "rename" in name.lower() or "outer" in name.lower() or "package" in name.lower() or "save" in name.lower():
            unreal.log(f"[RWG_ASSET_API] {cls_name}.{name}")
