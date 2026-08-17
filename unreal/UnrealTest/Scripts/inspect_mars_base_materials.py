import unreal


MESH_PATH = (
    "/Game/ExcavatorSim/Environment/MarsBase/"
    "SM_MarsBase_Complete.SM_MarsBase_Complete"
)
MAP_PATH = "/Game/ExcavatorSim/Maps/Mars_ExcavationSite"
ACTOR_LABEL = "Mars_Base_Complete_EDIT_ME"


mesh = unreal.load_asset(MESH_PATH)
if mesh is None:
    raise RuntimeError(f"Missing mesh: {MESH_PATH}")

materials = mesh.get_editor_property("static_materials")
unreal.log_warning(f"MARS_BASE_INSPECT mesh slots={len(materials)}")
for index, slot in enumerate(materials):
    interface = slot.get_editor_property("material_interface")
    unreal.log_warning(
        "MARS_BASE_INSPECT "
        f"slot={index} "
        f"name={slot.get_editor_property('material_slot_name')} "
        f"imported={slot.get_editor_property('imported_material_slot_name')} "
        f"material={interface.get_path_name() if interface else 'None'}"
    )

try:
    section_count = mesh.get_num_sections(0)
except Exception:
    section_count = -1
unreal.log_warning(f"MARS_BASE_INSPECT lod0 sections={section_count}")

world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
if world is None:
    raise RuntimeError(f"Could not load map: {MAP_PATH}")
for actor in unreal.GameplayStatics.get_all_actors_of_class(
    world,
    unreal.StaticMeshActor,
):
    if actor.get_actor_label() != ACTOR_LABEL:
        continue
    component = actor.static_mesh_component
    unreal.log_warning(
        f"MARS_BASE_INSPECT component materials={component.get_num_materials()}"
    )
    for index in range(component.get_num_materials()):
        interface = component.get_material(index)
        unreal.log_warning(
            "MARS_BASE_INSPECT "
            f"component_slot={index} "
            f"material={interface.get_path_name() if interface else 'None'}"
        )
    break

unreal.SystemLibrary.quit_editor()
