import inspect
import unreal

for target, names in (
    (unreal, ["find_package", "load_package"]),
    (unreal.PackageTools, dir(unreal.PackageTools)),
    (unreal.AssetRegistryHelpers, dir(unreal.AssetRegistryHelpers)),
):
    for name in names:
        if "create" in name.lower() or "save" in name.lower() or "package" in name.lower() or "asset" in name.lower():
            obj = getattr(target, name, None)
            if obj:
                unreal.log(f"[RWG_PKG_API] {target}.{name}: {obj}")
                try:
                    unreal.log(f"[RWG_PKG_API] doc: {obj.__doc__}")
                except Exception:
                    pass
