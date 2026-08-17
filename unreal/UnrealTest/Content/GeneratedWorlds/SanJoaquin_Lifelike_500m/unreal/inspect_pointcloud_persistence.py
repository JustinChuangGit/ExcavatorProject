import unreal

level = "/Game/GeneratedWorlds/SanJoaquin_Lifelike_500m/SanJoaquin_RawPointCloud_500m"
unreal.EditorLevelLibrary.load_level(level)
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    try:
        label = actor.get_actor_label()
    except Exception:
        label = str(actor)
    if "Point" in label or label.startswith("RPC_"):
        unreal.log(f"[RWG_PC_PERSIST] actor={label} class={actor.get_class().get_name()}")
        if hasattr(actor, "get_point_cloud"):
            pc = actor.get_point_cloud()
            unreal.log(f"[RWG_PC_PERSIST] point_cloud={pc} path={pc.get_path_name() if pc else None}")
            if pc:
                unreal.log(f"[RWG_PC_PERSIST] points={pc.get_num_points()}")

for name in dir(unreal.LidarPointCloud):
    if "create" in name.lower() or "file" in name.lower() or "save" in name.lower() or "path" in name.lower():
        unreal.log(f"[RWG_PC_METHOD] {name}")
