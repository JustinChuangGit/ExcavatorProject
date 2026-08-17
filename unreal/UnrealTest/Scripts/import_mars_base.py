import os
from pathlib import Path

import unreal


PROJECT_DIRECTORY = Path(__file__).resolve().parents[1]
SOURCE_DIRECTORY = Path(
    os.environ.get(
        "MARS_BASE_EXPORT_ROOT",
        PROJECT_DIRECTORY / "SourceArt" / "MarsBase" / "unreal_export",
    )
)
SOURCE_FBX = SOURCE_DIRECTORY / "SM_MarsBase_Complete.fbx"
DESTINATION_PATH = "/Game/ExcavatorSim/Environment/MarsBase"
MESH_ASSET_PATH = (
    f"{DESTINATION_PATH}/SM_MarsBase_Complete.SM_MarsBase_Complete"
)
MARS_MAP = "/Game/ExcavatorSim/Maps/Mars_ExcavationSite"
ACTOR_LABEL = "Mars_Base_Complete_EDIT_ME"
ACTOR_TAG = unreal.Name("MarsBasePlaced")
INITIAL_LOCATION = unreal.Vector(1800.0, -1700.0, 0.0)


def log(message):
    unreal.log_warning(f"MARS_BASE_IMPORT {message}")


def safe_set(obj, property_name, value):
    try:
        obj.set_editor_property(property_name, value)
        return True
    except Exception as error:
        log(f"could not set {property_name}: {error}")
        return False


def import_textures(asset_tools):
    tasks = []
    for texture_path in sorted(SOURCE_DIRECTORY.glob("*.png")):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(texture_path))
        task.set_editor_property("destination_path", DESTINATION_PATH)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", True)
        tasks.append(task)
    if tasks:
        asset_tools.import_asset_tasks(tasks)
        log(f"processed {len(tasks)} source textures")


def import_static_mesh(asset_tools):
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", True)
    options.set_editor_property("import_textures", True)
    options.set_editor_property("create_physics_asset", False)
    options.set_editor_property(
        "mesh_type_to_import",
        unreal.FBXImportType.FBXIT_STATIC_MESH,
    )

    static_options = options.get_editor_property("static_mesh_import_data")
    safe_set(static_options, "combine_meshes", True)
    safe_set(static_options, "generate_lightmap_u_vs", True)
    safe_set(static_options, "auto_generate_collision", True)
    safe_set(static_options, "convert_scene", True)
    safe_set(static_options, "convert_scene_unit", True)
    safe_set(static_options, "import_uniform_scale", 1.0)
    safe_set(
        static_options,
        "normal_import_method",
        unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS,
    )

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(SOURCE_FBX))
    task.set_editor_property("destination_path", DESTINATION_PATH)
    task.set_editor_property("destination_name", "SM_MarsBase_Complete")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", options)
    asset_tools.import_asset_tasks([task])
    log(f"mesh import returned {len(task.get_editor_property('imported_object_paths'))} assets")


if not SOURCE_FBX.is_file():
    raise RuntimeError(f"Mars base source is missing: {SOURCE_FBX}")

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
import_textures(asset_tools)
import_static_mesh(asset_tools)

mesh = unreal.load_asset(MESH_ASSET_PATH)
if mesh is None:
    raise RuntimeError(f"Imported static mesh was not found at {MESH_ASSET_PATH}")

body_setup = mesh.get_editor_property("body_setup")
if body_setup is not None:
    body_setup.set_editor_property(
        "collision_trace_flag",
        unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE,
    )
else:
    log("mesh has no body setup; leaving generated collision unchanged")
unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)

world = unreal.EditorLoadingAndSavingUtils.load_map(MARS_MAP)
if world is None:
    raise RuntimeError(f"Could not load {MARS_MAP}")

placed_actor = None
for actor in unreal.GameplayStatics.get_all_actors_of_class(
    world,
    unreal.StaticMeshActor,
):
    tags = actor.get_editor_property("tags")
    if ACTOR_TAG in tags or actor.get_actor_label() == ACTOR_LABEL:
        placed_actor = actor
        break

if placed_actor is None:
    bounding_box = mesh.get_bounding_box()
    location = unreal.Vector(
        INITIAL_LOCATION.x,
        INITIAL_LOCATION.y,
        12.0 - bounding_box.min.z,
    )
    placed_actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor,
        location,
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if placed_actor is None:
        raise RuntimeError("Could not place the imported Mars base")
    placed_actor.set_actor_label(ACTOR_LABEL)
    placed_actor.set_editor_property("tags", [ACTOR_TAG])
    placed_actor.set_folder_path("Mars Base")
    log(
        "placed new base at "
        f"X={location.x:.1f} Y={location.y:.1f} Z={location.z:.1f}"
    )
else:
    log("existing Mars base actor found; preserving its edited transform")

component = placed_actor.static_mesh_component
component.set_static_mesh(mesh)
component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
component.set_collision_profile_name("BlockAll")
component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Could not save the Mars map after placing the base")
unreal.EditorAssetLibrary.save_directory(DESTINATION_PATH, False, True)

bounds = mesh.get_bounding_box()
size = bounds.max - bounds.min
log(
    "complete; imported size "
    f"{size.x:.1f} x {size.y:.1f} x {size.z:.1f} cm, "
    f"actor label is {ACTOR_LABEL}"
)

unreal.SystemLibrary.quit_editor()
