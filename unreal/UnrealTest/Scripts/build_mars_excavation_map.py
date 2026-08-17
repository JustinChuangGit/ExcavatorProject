import math
import random

import unreal


SOURCE_MAP = "/Game/ExcavatorSim/Maps/Excavator_TestTrack"
MARS_MAP = "/Game/ExcavatorSim/Maps/Mars_ExcavationSite"
MATERIAL_PATH = "/Game/ExcavatorSim/Materials/M_Mars_RegolithVertex"
MARS_TAG = unreal.Name("MarsGenerated")


def log(message):
    unreal.log_warning(f"MARS_BUILD {message}")


def safe_set(obj, property_name, value):
    try:
        obj.set_editor_property(property_name, value)
        return True
    except Exception as error:
        log(
            f"could not set {obj.get_name()}.{property_name}: {error}"
        )
        return False


def configure_rock_collision(mesh):
    body_setup = mesh.get_editor_property("body_setup")
    if body_setup is None:
        raise RuntimeError("SM_Rock has no body setup")
    body_setup.set_editor_property(
        "collision_trace_flag",
        unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE,
    )
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    log("rock collision set to exact complex-as-simple geometry")


def make_soil_material():
    existing = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
    if existing is not None:
        return existing

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = asset_tools.create_asset(
        "M_Mars_RegolithVertex",
        "/Game/ExcavatorSim/Materials",
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if material is None:
        raise RuntimeError("Could not create Mars soil material")

    vertex_color = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionVertexColor,
        -360,
        -80,
    )
    base_color = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant3Vector,
        -580,
        -80,
    )
    base_color.set_editor_property(
        "constant",
        unreal.LinearColor(r=0.31, g=0.047, b=0.014, a=1.0),
    )
    color_multiply = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionMultiply,
        -120,
        -80,
    )
    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant,
        -360,
        90,
    )
    roughness.set_editor_property("r", 0.93)
    specular = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant,
        -360,
        190,
    )
    specular.set_editor_property("r", 0.16)

    unreal.MaterialEditingLibrary.connect_material_property(
        color_multiply,
        "",
        unreal.MaterialProperty.MP_BASE_COLOR,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        base_color,
        "",
        color_multiply,
        "A",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        vertex_color,
        "RGB",
        color_multiply,
        "B",
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness,
        "",
        unreal.MaterialProperty.MP_ROUGHNESS,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        specular,
        "",
        unreal.MaterialProperty.MP_SPECULAR,
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    return material


def spawn_static_mesh(world, mesh, material, label, location, rotation, scale):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor,
        location,
        rotation,
    )
    if actor is None:
        raise RuntimeError(f"Could not spawn {label}")
    actor.set_actor_label(label)
    actor.set_actor_scale3d(scale)
    safe_set(actor, "tags", [MARS_TAG])

    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    component.set_material(0, material)
    component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    component.set_collision_profile_name("BlockAll")
    component.set_collision_enabled(
        unreal.CollisionEnabled.QUERY_AND_PHYSICS
    )
    return actor


if not unreal.EditorAssetLibrary.does_asset_exist(MARS_MAP):
    if not unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP, MARS_MAP):
        raise RuntimeError("Could not duplicate the test track")
    log("duplicated working test track")

world = unreal.EditorLoadingAndSavingUtils.load_map(MARS_MAP)
if world is None:
    raise RuntimeError("Could not load Mars excavation map")

soil_material = make_soil_material()
rock_mesh = unreal.load_asset("/Game/StarterContent/Props/SM_Rock")
rock_material = unreal.load_asset(
    "/Game/StarterContent/Materials/M_Rock_Sandstone"
)
cube_mesh = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
if rock_mesh is None or rock_material is None or cube_mesh is None:
    raise RuntimeError("Required StarterContent assets are missing")
configure_rock_collision(rock_mesh)

actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
generated = []
test_scenery = []
excavator = None
sun = None
skylight = None
atmosphere = None
height_fog = None

for actor in actors:
    label = actor.get_actor_label()
    tags = actor.get_editor_property("tags")
    if MARS_TAG in tags:
        generated.append(actor)
        continue

    if actor.get_class().get_name() == "BP_ROS_Excavator_C":
        excavator = actor
    elif isinstance(actor, unreal.DirectionalLight):
        sun = actor
    elif isinstance(actor, unreal.SkyLight):
        skylight = actor
    elif isinstance(actor, unreal.SkyAtmosphere):
        atmosphere = actor
    elif isinstance(actor, unreal.ExponentialHeightFog):
        height_fog = actor
    elif isinstance(actor, unreal.StaticMeshActor):
        test_scenery.append(actor)
    elif isinstance(actor, unreal.DiggableTerrain):
        generated.append(actor)

editor_actor_subsystem = unreal.get_editor_subsystem(
    unreal.EditorActorSubsystem
)
if generated:
    editor_actor_subsystem.destroy_actors(generated)
if test_scenery:
    editor_actor_subsystem.destroy_actors(test_scenery)

if excavator is None:
    raise RuntimeError("Working ROS excavator actor was not found")
excavator.set_actor_label("ROS_Excavator_Mars")
excavator.set_actor_location(
    unreal.Vector(-1500.0, 0.0, 38.0),
    False,
    False,
)
excavator.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)

for component in excavator.get_components_by_class(unreal.ActorComponent):
    if isinstance(component, unreal.SpringArmComponent):
        safe_set(component, "target_arm_length", 850.0)
        safe_set(component, "target_offset", unreal.Vector(0.0, 0.0, 180.0))
        safe_set(component, "do_collision_test", True)
        safe_set(
            component,
            "relative_rotation",
            unreal.Rotator(pitch=-18.0, yaw=0.0, roll=0.0),
        )
    elif isinstance(component, unreal.CameraComponent):
        safe_set(component, "field_of_view", 78.0)
        safe_set(component, "post_process_blend_weight", 0.0)
    elif component.get_class().get_name() == "ExcavatorVendorAdapterComponent":
        safe_set(component, "override_initial_camera_rotation", True)
        safe_set(
            component,
            "initial_camera_rotation_override",
            unreal.Rotator(pitch=-18.0, yaw=0.0, roll=0.0),
        )

terrain = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.DiggableTerrain,
    unreal.Vector(0.0, 0.0, 0.0),
    unreal.Rotator(0.0, 0.0, 0.0),
)
if terrain is None:
    raise RuntimeError("Could not spawn DiggableTerrain")
terrain.set_actor_label("Mars_Diggable_Regolith")
safe_set(terrain, "tags", [MARS_TAG])
safe_set(terrain, "soil_material", soil_material)
safe_set(terrain, "grid_resolution", 128)
safe_set(terrain, "cell_size_centimeters", 78.125)
safe_set(terrain, "runtime_target_cell_size_centimeters", 13.0)

# A normal Unreal Landscape sits just below the deformable excavation layer.
# It removes the floating-platform edge and remains editable with Landscape
# Mode's Sculpt, Smooth, Flatten, and Ramp tools.
editable_landscape = (
    unreal.MarsLandscapeEditorLibrary
    .create_or_update_editable_mars_landscape()
)
if editable_landscape is None:
    raise RuntimeError("Could not create editable Mars Landscape")

# A deep substrate prevents seeing an empty void beyond the finite interactive
# terrain, but remains safely below the maximum digging depth.
substrate = spawn_static_mesh(
    world,
    cube_mesh,
    soil_material,
    "Mars_Deep_Substrate",
    unreal.Vector(0.0, 0.0, -650.0),
    unreal.Rotator(0.0, 0.0, 0.0),
    unreal.Vector(220.0, 220.0, 3.0),
)

rng = random.Random(2407)

# Scatter varied sandstone rocks around the outside of the 30 m work pad.
for index in range(34):
    while True:
        angle = rng.uniform(0.0, math.tau)
        radius = rng.uniform(2650.0, 3450.0)
        location = unreal.Vector(
            math.cos(angle) * radius,
            math.sin(angle) * radius,
            rng.uniform(-12.0, 30.0),
        )
        distance_from_spawn = math.hypot(
            location.x + 1500.0,
            location.y,
        )
        if distance_from_spawn >= 1750.0:
            break
    uniform_scale = rng.uniform(0.35, 1.65)
    spawn_static_mesh(
        world,
        rock_mesh,
        rock_material,
        f"Mars_Rock_{index + 1:02d}",
        location,
        unreal.Rotator(
            rng.uniform(-18.0, 18.0),
            rng.uniform(-180.0, 180.0),
            rng.uniform(-15.0, 15.0),
        ),
        unreal.Vector(
            uniform_scale,
            uniform_scale * rng.uniform(0.72, 1.18),
            uniform_scale * rng.uniform(0.65, 1.35),
        ),
    )

# Larger silhouettes at the edge make the small simulation field read as part
# of a wider Martian basin.
for index, angle_degrees in enumerate(
    (18.0, 63.0, 111.0, 167.0, 218.0, 274.0, 326.0)
):
    angle = math.radians(angle_degrees)
    radius = 3420.0
    scale = rng.uniform(2.8, 5.4)
    spawn_static_mesh(
        world,
        rock_mesh,
        rock_material,
        f"Mars_Ridge_{index + 1:02d}",
        unreal.Vector(
            math.cos(angle) * radius,
            math.sin(angle) * radius,
            rng.uniform(-25.0, 10.0),
        ),
        unreal.Rotator(
            rng.uniform(-12.0, 12.0),
            rng.uniform(-180.0, 180.0),
            rng.uniform(-8.0, 8.0),
        ),
        unreal.Vector(
            scale,
            scale * rng.uniform(0.55, 0.9),
            scale * rng.uniform(0.8, 1.5),
        ),
    )

# Small crater rim away from the initial excavator location.
crater_center = unreal.Vector(1550.0, 1850.0, -5.0)
for index in range(13):
    angle = math.tau * index / 13.0 + rng.uniform(-0.08, 0.08)
    radius = rng.uniform(500.0, 650.0)
    scale = rng.uniform(0.32, 0.72)
    spawn_static_mesh(
        world,
        rock_mesh,
        rock_material,
        f"Mars_Crater_Rim_{index + 1:02d}",
        unreal.Vector(
            crater_center.x + math.cos(angle) * radius,
            crater_center.y + math.sin(angle) * radius,
            rng.uniform(-20.0, 12.0),
        ),
        unreal.Rotator(
            rng.uniform(-20.0, 20.0),
            math.degrees(angle) + 90.0,
            rng.uniform(-12.0, 12.0),
        ),
        unreal.Vector(scale * 1.5, scale, scale * 0.65),
    )

if sun is not None:
    sun.set_actor_rotation(unreal.Rotator(-24.0, -38.0, 0.0), False)
    component = sun.get_component_by_class(
        unreal.DirectionalLightComponent
    )
    if component is not None:
        safe_set(component, "mobility", unreal.ComponentMobility.MOVABLE)
        safe_set(component, "intensity", 3.2)
        safe_set(
            component,
            "light_color",
            unreal.Color(r=255, g=172, b=118, a=255),
        )
        safe_set(component, "atmosphere_sun_light", True)
        safe_set(component, "use_temperature", True)
        safe_set(component, "temperature", 4300.0)

if skylight is not None:
    component = skylight.get_component_by_class(unreal.SkyLightComponent)
    if component is not None:
        safe_set(component, "mobility", unreal.ComponentMobility.MOVABLE)
        safe_set(component, "intensity", 0.18)
        safe_set(
            component,
            "light_color",
            unreal.Color(r=184, g=105, b=72, a=255),
        )
        safe_set(component, "real_time_capture", True)

if atmosphere is not None:
    component = atmosphere.get_component_by_class(
        unreal.SkyAtmosphereComponent
    )
    if component is not None:
        safe_set(
            component,
            "ground_albedo",
            unreal.Color(r=94, g=20, b=8, a=255),
        )
        safe_set(component, "rayleigh_scattering_scale", 0.16)
        safe_set(
            component,
            "rayleigh_scattering",
            unreal.LinearColor(r=0.23, g=0.045, b=0.018, a=1.0),
        )
        safe_set(component, "mie_scattering_scale", 0.55)
        safe_set(
            component,
            "mie_scattering",
            unreal.LinearColor(r=0.56, g=0.12, b=0.035, a=1.0),
        )
        safe_set(component, "mie_anisotropy", 0.72)

if height_fog is not None:
    component = height_fog.get_component_by_class(
        unreal.ExponentialHeightFogComponent
    )
    if component is not None:
        safe_set(component, "fog_density", 0.001)
        safe_set(component, "fog_height_falloff", 0.24)
        safe_set(component, "directional_inscattering_exponent", 6.0)
        safe_set(component, "volumetric_fog_scattering_distribution", 0.78)

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Could not save Mars excavation map")
unreal.EditorAssetLibrary.save_loaded_asset(soil_material, False)
log("Mars excavation map saved")

unreal.SystemLibrary.quit_editor()
