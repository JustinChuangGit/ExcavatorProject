"""Import Justin's resume and build a reusable 12 m Unreal billboard asset."""

from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
SOURCE_DIR = PROJECT_ROOT / "SourceArt" / "ResumeBillboard"
TEXTURE_SOURCE = SOURCE_DIR / "JustinChuang_Resume_4K.png"
MESH_SOURCE = SOURCE_DIR / "SM_JustinChuang_Resume_Billboard_12m.obj"
DESTINATION = "/Game/MissionControl/ResumeBillboard"

TEXTURE_PATH = f"{DESTINATION}/T_JustinChuang_Resume_4K"
RESUME_MATERIAL_PATH = f"{DESTINATION}/M_JustinChuang_Resume"
FRAME_MATERIAL_PATH = f"{DESTINATION}/M_ResumeBillboard_Frame"
MESH_PATH = f"{DESTINATION}/SM_JustinChuang_Resume_Billboard_12m"


def log(message):
    unreal.log(f"[ResumeBillboard] {message}")


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
    paths = list(task.get_editor_property("imported_object_paths"))
    asset = unreal.load_asset(expected_path)
    if asset is None:
        raise RuntimeError(
            f"Could not load expected asset {expected_path}; import returned {paths}"
        )
    log(f"Imported {expected_path}")
    return asset


def fresh_material(asset_name):
    path = f"{DESTINATION}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    factory = unreal.MaterialFactoryNew()
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name,
        DESTINATION,
        unreal.Material,
        factory,
    )
    if material is None:
        raise RuntimeError(f"Could not create {path}")
    return material


def create_resume_material(texture):
    material = fresh_material("M_JustinChuang_Resume")
    material.set_editor_property("two_sided", True)
    material.set_editor_property(
        "shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT
    )

    texture_sample = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSample,
        -320,
        0,
    )
    texture_sample.set_editor_property("texture", texture)
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
    readability_level = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant,
        -320,
        360,
    )
    readability_level.set_editor_property("r", 0.45)
    readability_fill = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionMultiply,
        -80,
        360,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        texture_sample, "RGB", albedo, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        albedo_level, "", albedo, "B"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        texture_sample, "RGB", readability_fill, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        readability_level, "", readability_fill, "B"
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
    unreal.MaterialEditingLibrary.connect_material_property(
        readability_fill, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    log(f"Created {RESUME_MATERIAL_PATH}")
    return material


def create_frame_material():
    material = fresh_material("M_ResumeBillboard_Frame")

    color = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant3Vector,
        -350,
        -80,
    )
    color.set_editor_property(
        "constant", unreal.LinearColor(0.008, 0.014, 0.018, 1.0)
    )
    metallic = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant,
        -350,
        40,
    )
    metallic.set_editor_property("r", 0.82)
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
    log(f"Created {FRAME_MATERIAL_PATH}")
    return material


def main():
    if not TEXTURE_SOURCE.is_file():
        raise FileNotFoundError(TEXTURE_SOURCE)
    if not MESH_SOURCE.is_file():
        raise FileNotFoundError(MESH_SOURCE)

    unreal.EditorAssetLibrary.make_directory(DESTINATION)

    texture = import_asset(
        TEXTURE_SOURCE,
        "T_JustinChuang_Resume_4K",
        TEXTURE_PATH,
    )
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)

    resume_material = create_resume_material(texture)
    frame_material = create_frame_material()

    mesh = import_asset(
        MESH_SOURCE,
        "SM_JustinChuang_Resume_Billboard_12m",
        MESH_PATH,
    )
    static_materials = list(mesh.get_editor_property("static_materials"))
    if len(static_materials) != 2:
        raise RuntimeError(
            f"Expected two billboard material slots, found {len(static_materials)}"
        )
    for index, slot in enumerate(static_materials):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        material = resume_material if "Resume" in slot_name else frame_material
        mesh.set_material(index, material)
        log(f"Assigned {material.get_name()} to slot {index} ({slot_name})")
    mesh.set_editor_property("light_map_resolution", 128)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)

    for generated_path in (
        f"{DESTINATION}/Resume",
        f"{DESTINATION}/Frame",
        f"{DESTINATION}/TEX_JustinChuang_Resume_4K",
    ):
        if unreal.EditorAssetLibrary.does_asset_exist(generated_path):
            unreal.EditorAssetLibrary.delete_asset(generated_path)

    bounds = mesh.get_bounds()
    extent = bounds.box_extent
    log(
        "READY "
        f"{MESH_PATH} "
        f"size={extent.x * 2:.1f}x{extent.y * 2:.1f}x{extent.z * 2:.1f} cm"
    )


main()
