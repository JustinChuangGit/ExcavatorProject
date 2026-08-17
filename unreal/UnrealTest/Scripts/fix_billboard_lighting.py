"""Make existing billboard faces matte and disable lens flare in the open map."""

import unreal


MATERIAL_PATHS = (
    "/Game/MissionControl/ResumeBillboard/M_JustinChuang_Resume",
    "/Game/MissionControl/ControlGuideBillboard/M_Excavator_Control_Guide",
)


def log(message):
    unreal.log(f"[BillboardLightingFix] {message}")


def first_texture_sample(material):
    expressions = unreal.MaterialEditingLibrary.get_material_expressions(material)
    for expression in expressions:
        if isinstance(expression, unreal.MaterialExpressionTextureSample):
            return expression
    raise RuntimeError(f"No texture sample found in {material.get_path_name()}")


def make_matte(material_path):
    material = unreal.load_asset(material_path)
    if material is None:
        raise RuntimeError(f"Could not load {material_path}")

    sample = first_texture_sample(material)
    unreal.MaterialEditingLibrary.disconnect_material_property(
        material,
        unreal.MaterialProperty.MP_EMISSIVE_COLOR,
    )
    material.set_editor_property(
        "shading_model",
        unreal.MaterialShadingModel.MSM_DEFAULT_LIT,
    )
    material.set_editor_property("two_sided", True)

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
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError(f"Could not save {material_path}")
    log(f"UPDATED {material_path}")


def disable_level_lens_flare():
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    count = 0
    for actor in actor_subsystem.get_all_level_actors():
        if not isinstance(actor, unreal.PostProcessVolume):
            continue
        settings = actor.get_editor_property("settings")
        settings.set_editor_property("override_lens_flare_intensity", True)
        settings.set_editor_property("lens_flare_intensity", 0.0)
        actor.set_editor_property("settings", settings)
        actor.modify()
        count += 1

    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, "r.LensFlareQuality 0")
    log(f"DISABLED lens flare in {count} post-process volume(s)")


def main():
    for material_path in MATERIAL_PATHS:
        make_matte(material_path)
    disable_level_lens_flare()
    if not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True):
        raise RuntimeError("One or more dirty packages could not be saved")
    log("SAVED billboard assets and current level")
    log("READY")


main()
