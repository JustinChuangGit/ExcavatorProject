import unreal


MAP_PATH = "/Game/ExcavatorSim/Maps/Mars_ExcavationSite"
TERRAIN_LABEL = "Mars_Diggable_Regolith"


def log(message):
    unreal.log_warning(f"MARS_LANDSCAPE {message}")


world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
if world is None:
    raise RuntimeError(f"Could not load map: {MAP_PATH}")

terrain = None
for actor in unreal.GameplayStatics.get_all_actors_of_class(
    world,
    unreal.DiggableTerrain,
):
    if actor.get_actor_label() == TERRAIN_LABEL:
        terrain = actor
        break
if terrain is None:
    raise RuntimeError(f"Could not find {TERRAIN_LABEL}")

# 128 * 78.125 cm = a 100 m square editor mesh. At runtime this becomes
# 768 cells at roughly 13 cm spacing, preserving bucket interaction fidelity.
terrain.set_editor_property("grid_resolution", 128)
terrain.set_editor_property("cell_size_centimeters", 78.125)
terrain.set_editor_property(
    "runtime_target_cell_size_centimeters",
    13.0,
)

landscape = (
    unreal.MarsLandscapeEditorLibrary
    .create_or_update_editable_mars_landscape()
)
if landscape is None:
    raise RuntimeError("Could not create editable Mars Landscape")

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Could not save Mars excavation map")

log(
    "saved 100 m diggable pad over a 126 m editable Landscape; "
    f"landscape={landscape.get_actor_label()}"
)
unreal.SystemLibrary.quit_editor()
