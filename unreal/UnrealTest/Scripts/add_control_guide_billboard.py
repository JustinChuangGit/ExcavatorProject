"""Create the excavator control guide billboard and place it before the viewport."""

from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
SOURCE_DIR = PROJECT_ROOT / "SourceArt" / "ControlGuideBillboard"
TEXTURE_SOURCE = SOURCE_DIR / "Excavator_Control_Guide.png"
MESH_SOURCE = SOURCE_DIR / "SM_Excavator_Control_Guide_Billboard_14m.obj"
DESTINATION = "/Game/MissionControl/ControlGuideBillboard"

TEXTURE_PATH = f"{DESTINATION}/T_Excavator_Control_Guide"
GUIDE_MATERIAL_PATH = f"{DESTINATION}/M_Excavator_Control_Guide"
FRAME_MATERIAL_PATH = f"{DESTINATION}/M_ControlGuideBillboard_Frame"
MESH_PATH = f"{DESTINATION}/SM_Excavator_Control_Guide_Billboard_14m"
ACTOR_LABEL = "Excavator Control Guide Billboard"


def log(message):
    unreal.log(f"[ControlGuideBillboard] {message}")


def import_asset(source, destination_name, expected_path):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", DESTINATION)
    task.set_editor_property("destination_name", destination_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset = unreal.load_asset(expected_path)
    if asset is None:
        imported = list(task.get_editor_property("imported_object_paths"))
        raise RuntimeError(
            f"Could not load {expected_path}; importer returned {imported}"
        )
    log(f"Imported {expected_path}")
    return asset


def fresh_material(asset_name):
    path = f"{DESTINATION}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name,
        DESTINATION,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if material is None:
        raise RuntimeError(f"Could not create {path}")
    return material


def create_guide_material(texture):
    material = fresh_material("M_Excavator_Control_Guide")
    material.set_editor_property("two_sided", True)
    material.set_editor_property(
        "shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT
    )
    sample = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSample,
        -320,
        0,
    )
    sample.set_editor_property("texture", texture)
    albedo_level = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant,
        -320,
        180,
    )
    albedo_level.set_editor_property("r", 0.65)
    albedo = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionMultiply,
        -80,
        0,
    )
    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant,
        -80,
        160,
    )
    roughness.set_editor_property("r", 0.92)
    specular = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant,
        -80,
        260,
    )
    specular.set_editor_property("r", 0.08)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        sample, "RGB", albedo, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        albedo_level, "", albedo, "B"
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        albedo, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        specular, "", unreal.MaterialProperty.MP_SPECULAR
    )
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def create_frame_material():
    material = fresh_material("M_ControlGuideBillboard_Frame")
    color = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant3Vector,
        -350,
        -80,
    )
    color.set_editor_property(
        "constant", unreal.LinearColor(0.008, 0.012, 0.016, 1.0)
    )
    metallic = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant,
        -350,
        40,
    )
    metallic.set_editor_property("r", 0.8)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant,
        -350,
        140,
    )
    roughness.set_editor_property("r", 0.28)
    unreal.MaterialEditingLibrary.connect_material_property(
        color, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        metallic, "", unreal.MaterialProperty.MP_METALLIC
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def place_before_viewport(mesh):
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    camera_location, camera_rotation = editor.get_level_viewport_camera_info()
    forward = unreal.MathLibrary.get_forward_vector(camera_rotation)

    distance = 2500.0
    location = unreal.Vector(
        camera_location.x + forward.x * distance,
        camera_location.y + forward.y * distance,
        camera_location.z - 467.0,
    )
    rotation = unreal.Rotator(
        pitch=0.0,
        yaw=camera_rotation.yaw + 90.0,
        roll=0.0,
    )

    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actors.spawn_actor_from_object(mesh, location, rotation)
    if actor is None:
        raise RuntimeError("Unreal could not spawn the billboard actor")
    actor.set_actor_label(ACTOR_LABEL)
    actors.clear_actor_selection_set()
    actors.set_actor_selection_state(actor, True)
    log(
        f"Placed and selected {ACTOR_LABEL} at "
        f"({location.x:.1f}, {location.y:.1f}, {location.z:.1f})"
    )
    return actor


def main():
    if not TEXTURE_SOURCE.is_file():
        raise FileNotFoundError(TEXTURE_SOURCE)
    if not MESH_SOURCE.is_file():
        raise FileNotFoundError(MESH_SOURCE)

    unreal.EditorAssetLibrary.make_directory(DESTINATION)
    texture = import_asset(
        TEXTURE_SOURCE,
        "T_Excavator_Control_Guide",
        TEXTURE_PATH,
    )
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)

    guide_material = create_guide_material(texture)
    frame_material = create_frame_material()
    mesh = import_asset(
        MESH_SOURCE,
        "SM_Excavator_Control_Guide_Billboard_14m",
        MESH_PATH,
    )

    slots = list(mesh.get_editor_property("static_materials"))
    if len(slots) != 2:
        raise RuntimeError(f"Expected two material slots, found {len(slots)}")
    for index, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        material = guide_material if "Guide" in slot_name else frame_material
        mesh.set_material(index, material)
    mesh.set_editor_property("light_map_resolution", 128)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)

    for generated_path in (
        f"{DESTINATION}/Guide",
        f"{DESTINATION}/Frame",
    ):
        if unreal.EditorAssetLibrary.does_asset_exist(generated_path):
            unreal.EditorAssetLibrary.delete_asset(generated_path)

    place_before_viewport(mesh)
    log(f"READY {MESH_PATH}")


main()
