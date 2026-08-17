import os
from pathlib import Path

import unreal


PROJECT_DIRECTORY = Path(__file__).resolve().parents[1]
SOURCE_GLB = Path(
    os.environ.get(
        "MARS_BASE_EXPORT_ROOT",
        PROJECT_DIRECTORY / "SourceArt" / "MarsBase" / "unreal_export",
    )
) / "SM_MarsBase_Complete_Textured.glb"
DESTINATION_PATH = (
    "/Game/ExcavatorSim/Environment/MarsBase/Textured"
)
MESH_ASSET_PATH = (
    f"{DESTINATION_PATH}/"
    "SM_MarsBase_Complete_Textured.SM_MarsBase_Complete_Textured"
)
MAP_PATH = "/Game/ExcavatorSim/Maps/Mars_ExcavationSite"
ACTOR_LABEL = "Mars_Base_Complete_EDIT_ME"
EXPECTED_MATERIALS = {
    "hull2",
    "glass",
    "hull",
    "White Hull",
    "red",
    "Material.004",
    "gem",
    "silver",
    "black",
}


def log(message):
    unreal.log_warning(f"MARS_BASE_REPAIR {message}")


if not SOURCE_GLB.is_file():
    raise RuntimeError(f"Corrected GLB is missing: {SOURCE_GLB}")

task = unreal.AssetImportTask()
task.set_editor_property("filename", str(SOURCE_GLB))
task.set_editor_property("destination_path", DESTINATION_PATH)
task.set_editor_property("destination_name", "SM_MarsBase_Complete_Textured")
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
log(
    "imported objects: "
    + ", ".join(task.get_editor_property("imported_object_paths"))
)

mesh = unreal.load_asset(MESH_ASSET_PATH)
if mesh is None:
    candidates = []
    for object_path in task.get_editor_property("imported_object_paths"):
        asset = unreal.load_asset(object_path)
        if isinstance(asset, unreal.StaticMesh):
            candidates.append(asset)
    if len(candidates) != 1:
        raise RuntimeError(
            f"Expected one imported static mesh, found {len(candidates)}"
        )
    mesh = candidates[0]
    log(f"resolved mesh from import result: {mesh.get_path_name()}")

slots = mesh.get_editor_property("static_materials")
slot_names = {
    str(slot.get_editor_property("imported_material_slot_name"))
    for slot in slots
}
section_count = mesh.get_num_sections(0)
log(
    f"mesh has {len(slots)} slots, {section_count} sections: "
    + ", ".join(sorted(slot_names))
)
if len(slots) != 9 or section_count != 9:
    raise RuntimeError(
        "Corrected import did not preserve nine material regions: "
        f"slots={len(slots)} sections={section_count}"
    )

# Interchange may normalize whitespace in display names. The slot/section
# count is authoritative, but report any unexpected naming for inspection.
normalized_names = {name.replace("_", " ") for name in slot_names}
missing_names = {
    name for name in EXPECTED_MATERIALS
    if name not in slot_names and name not in normalized_names
}
if missing_names:
    log("normalized or renamed material slots: " + ", ".join(missing_names))

body_setup = mesh.get_editor_property("body_setup")
if body_setup is not None:
    body_setup.set_editor_property(
        "collision_trace_flag",
        unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE,
    )
unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)

world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
if world is None:
    raise RuntimeError(f"Could not load map: {MAP_PATH}")

base_actor = None
for actor in unreal.GameplayStatics.get_all_actors_of_class(
    world,
    unreal.StaticMeshActor,
):
    if actor.get_actor_label() == ACTOR_LABEL:
        base_actor = actor
        break
if base_actor is None:
    raise RuntimeError(f"Could not find placed actor: {ACTOR_LABEL}")

base_actor.static_mesh_component.set_static_mesh(mesh)
base_actor.static_mesh_component.set_collision_profile_name("BlockAll")
base_actor.static_mesh_component.set_collision_enabled(
    unreal.CollisionEnabled.QUERY_AND_PHYSICS
)
if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Could not save the Mars map")
unreal.EditorAssetLibrary.save_directory(DESTINATION_PATH, False, True)
log(
    f"updated {ACTOR_LABEL} in place with "
    f"{len(slots)} textured material regions"
)

unreal.SystemLibrary.quit_editor()
