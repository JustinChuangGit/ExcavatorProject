import unreal


MARS_MAP = "/Game/ExcavatorSim/Maps/Mars_ExcavationSite"
ROCK_MESH = "/Game/StarterContent/Props/SM_Rock"
ROCK_PREFIXES = ("Mars_Rock_", "Mars_Ridge_", "Mars_Crater_Rim_")


def log(message):
    unreal.log_warning(f"MARS_COLLISION {message}")


rock_mesh = unreal.load_asset(ROCK_MESH)
if rock_mesh is None:
    raise RuntimeError("SM_Rock is unavailable")

body_setup = rock_mesh.get_editor_property("body_setup")
if body_setup is None:
    raise RuntimeError("SM_Rock has no body setup")
body_setup.set_editor_property(
    "collision_trace_flag",
    unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE,
)
unreal.EditorAssetLibrary.save_loaded_asset(rock_mesh, False)
log("enabled exact complex-as-simple collision on SM_Rock")

world = unreal.EditorLoadingAndSavingUtils.load_map(MARS_MAP)
if world is None:
    raise RuntimeError("Could not load the Mars excavation map")

updated = 0
actors = unreal.GameplayStatics.get_all_actors_of_class(
    world,
    unreal.StaticMeshActor,
)
for actor in actors:
    if not actor.get_actor_label().startswith(ROCK_PREFIXES):
        continue
    component = actor.static_mesh_component
    component.set_collision_profile_name("BlockAll")
    component.set_collision_enabled(
        unreal.CollisionEnabled.QUERY_AND_PHYSICS
    )
    updated += 1

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Could not save the Mars excavation map")
log(f"updated {updated} placed rock actors")

unreal.SystemLibrary.quit_editor()
