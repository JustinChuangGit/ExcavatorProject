"""Replace the control-guide artwork and mesh without spawning another actor."""

from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
SOURCE_DIR = PROJECT_ROOT / "SourceArt" / "ControlGuideBillboard"
TEXTURE_SOURCE = SOURCE_DIR / "Excavator_Control_Guide.png"
MESH_SOURCE = SOURCE_DIR / "SM_Excavator_Control_Guide_Billboard_14m.obj"
DESTINATION = "/Game/MissionControl/ControlGuideBillboard"

TEXTURE_PATH = f"{DESTINATION}/T_Excavator_Control_Guide"
MESH_PATH = f"{DESTINATION}/SM_Excavator_Control_Guide_Billboard_14m"
GUIDE_MATERIAL_PATH = f"{DESTINATION}/M_Excavator_Control_Guide"
FRAME_MATERIAL_PATH = f"{DESTINATION}/M_ControlGuideBillboard_Frame"


def import_replacement(source, destination_name, expected_path):
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
        raise RuntimeError(f"Could not load replacement asset {expected_path}")
    return asset


def main():
    if not TEXTURE_SOURCE.is_file():
        raise FileNotFoundError(TEXTURE_SOURCE)
    if not MESH_SOURCE.is_file():
        raise FileNotFoundError(MESH_SOURCE)

    texture = import_replacement(
        TEXTURE_SOURCE,
        "T_Excavator_Control_Guide",
        TEXTURE_PATH,
    )
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)

    mesh = import_replacement(
        MESH_SOURCE,
        "SM_Excavator_Control_Guide_Billboard_14m",
        MESH_PATH,
    )
    guide_material = unreal.load_asset(GUIDE_MATERIAL_PATH)
    frame_material = unreal.load_asset(FRAME_MATERIAL_PATH)
    if guide_material is None or frame_material is None:
        raise RuntimeError("Could not load existing control-guide materials")

    slots = list(mesh.get_editor_property("static_materials"))
    if len(slots) != 2:
        raise RuntimeError(f"Expected two material slots, found {len(slots)}")
    for index, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property("material_slot_name")).lower()
        target = guide_material if "guide" in slot_name else frame_material
        mesh.set_material(index, target)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)

    unreal.log(
        "[ControlGuideReplacement] READY new artwork, "
        "1400 x 934 cm mesh, existing actors preserved"
    )


main()
