# Creates the locomotion blend space used for strafe / backward movement.
#
# Axes (X = Direction, Y = Speed) match UMALAnimInstance::LocomotionBlendInput, so the animation
# graph can drive the asset with values C++ already publishes. Samples are left empty on purpose:
# which animations go on the grid is an art decision.
import unreal

ASSET_DIR = "/Game/Blueprint/Characters/MALCharacter/Locomotion"
ASSET_NAME = "BS_Locomotion"
ASSET_PATH = "%s/%s" % (ASSET_DIR, ASSET_NAME)


def log(message):
    unreal.log("[MALBS] " + str(message))


def find_kwang_skeleton():
    for package in unreal.EditorAssetLibrary.list_assets("/Game/Character", recursive=True, include_folder=False):
        data = unreal.EditorAssetLibrary.find_asset_data(package)
        try:
            class_name = str(data.asset_class_path.asset_name)
        except Exception:
            class_name = ""
        if class_name == "Skeleton" and "Kwang" in package:
            loaded = unreal.EditorAssetLibrary.load_asset(package)
            if loaded:
                log("skeleton: %s" % package)
                return loaded
    return None


def make_axis(display_name, minimum, maximum, grid):
    axis = unreal.BlendParameter()
    for name, value in (
        ("display_name", display_name),
        ("min", minimum),
        ("max", maximum),
        ("grid_num", grid),
        ("snap_to_grid", False),
    ):
        try:
            axis.set_editor_property(name, value)
        except Exception as exc:
            log("axis property %s skipped: %s" % (name, exc))
    return axis


# Remove the throwaway asset created while probing the factory.
if unreal.EditorAssetLibrary.does_asset_exist("%s/BS_LocomotionProbe" % ASSET_DIR):
    unreal.EditorAssetLibrary.delete_asset("%s/BS_LocomotionProbe" % ASSET_DIR)
    log("deleted BS_LocomotionProbe")

if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
    log("%s already exists, leaving it untouched" % ASSET_PATH)
else:
    skeleton = find_kwang_skeleton()
    if not skeleton:
        log("no Kwang skeleton found; the factory needs one to build a grid")
        raise SystemExit(1)

    factory = unreal.BlendSpaceFactoryNew()
    factory.set_editor_property("target_skeleton", skeleton)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    blend_space = asset_tools.create_asset(ASSET_NAME, ASSET_DIR, unreal.BlendSpace, factory)
    if not blend_space:
        log("create_asset returned None")
        raise SystemExit(1)

    try:
        axes = [
            make_axis("Direction", -180.0, 180.0, 8),
            make_axis("Speed", 0.0, 700.0, 3),
            make_axis("Unused", 0.0, 100.0, 1),
        ]
        blend_space.set_editor_property("BlendParameters", axes)
        log("axes configured: Direction -180..180, Speed 0..700")
    except Exception as exc:
        log("axis configuration failed, defaults kept: %s" % exc)

    unreal.EditorAssetLibrary.save_asset(ASSET_PATH, only_if_is_dirty=False)
    log("created %s" % ASSET_PATH)

log("done")
