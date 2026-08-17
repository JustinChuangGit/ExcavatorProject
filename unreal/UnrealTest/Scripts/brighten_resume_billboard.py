"""Add a restrained, non-blooming readability fill to the resume billboard."""

import unreal


MATERIAL_PATH = (
    "/Game/MissionControl/ResumeBillboard/M_JustinChuang_Resume"
)


def main():
    material = unreal.load_asset(MATERIAL_PATH)
    if material is None:
        raise RuntimeError(f"Could not load {MATERIAL_PATH}")

    sample = None
    for expression in unreal.MaterialEditingLibrary.get_material_expressions(
        material
    ):
        if isinstance(expression, unreal.MaterialExpressionTextureSample):
            sample = expression
            break
    if sample is None:
        raise RuntimeError("Resume material has no texture sample")

    level = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant,
        -320,
        360,
    )
    level.set_editor_property("r", 0.45)
    fill = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionMultiply,
        -80,
        360,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        sample, "RGB", fill, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        level, "", fill, "B"
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        fill,
        "",
        unreal.MaterialProperty.MP_EMISSIVE_COLOR,
    )
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError(f"Could not save {MATERIAL_PATH}")

    unreal.log(
        "[ResumeBrightnessFix] READY "
        "base color 0.65, emissive readability fill 0.45"
    )


main()
