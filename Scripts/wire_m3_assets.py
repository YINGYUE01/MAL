# M3 asset wiring - dodge input only.
#
# The dodge ability is granted from code (AMALCharacter::InnateAbilities), so this script only
# wires the input path:
#   1. create IA_DashAction (duplicated from a boolean action)
#   2. bind LeftShift to it in IMC_InputContext
#   3. DA_InputConfig still needs one manual row: MAL.Input.Dash -> IA_DashAction
#
# Run with the editor closed (SavePackage fails while the editor holds the packages):
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="...\wire_m3_assets.py" -unattended -nullrhi
#
# Every step checks for existing state first, so re-running is safe.
import unreal

INPUT_FOLDER = "/Game/Blueprint/Input"
CONFIG_PATH = INPUT_FOLDER + "/DA_InputConfig"
IMC_PATH = INPUT_FOLDER + "/IMC_InputContext"

DASH_INPUT_TAG = "MAL.Input.Dash"
DODGE_KEY = "LeftShift"


def log(msg):
    unreal.log("[MALWIRE] " + str(msg))


def warn(msg):
    unreal.log_warning("[MALWIRE] " + str(msg))


def tag(name):
    # Literal tag: commandlet Python cannot request a registered tag, and a stored tag does not
    # need to be registered here -- the native tags exist once the game module loads.
    return unreal.GameplayTagLibrary.make_literal_gameplay_tag(name)


def ensure_input_action(name):
    path = "%s/%s" % (INPUT_FOLDER, name)
    existing = unreal.load_asset(path)
    if existing:
        log("%s already exists" % name)
        return existing

    # Duplicated from a boolean action rather than built from a factory: the Enhanced Input
    # factories are not exposed to commandlet Python in this build.
    created = unreal.EditorAssetLibrary.duplicate_asset(INPUT_FOLDER + "/IA_JumpAction", path)
    if created:
        unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
        log("created %s" % name)
    return created


def route_tag_to_action(config_path, tag_name, action):
    config = unreal.load_asset(config_path)
    if not config:
        warn("%s is missing" % config_path)
        return

    mappings = config.get_editor_property("AbilityInputActions")
    for entry in mappings:
        if entry.get_editor_property("Action") == action:
            log("%s is already routed" % action.get_name())
            return

    try:
        input_tag = tag(tag_name)
    except Exception as exc:
        # Commandlet Python cannot build an FGameplayTag from a string in this build (TagName is
        # read-only and no request-by-name helper is exposed). This one row has to be added in
        # the asset editor; everything else below still runs.
        warn("cannot build %s from script (%s)" % (tag_name, exc))
        warn("add it by hand: DA_InputConfig -> AbilityInputActions -> + -> %s / %s"
             % (tag_name, action.get_name()))
        return

    entry = unreal.MALInputActionMapping()
    entry.set_editor_property("InputTag", input_tag)
    entry.set_editor_property("Action", action)

    mappings.append(entry)
    config.set_editor_property("AbilityInputActions", mappings)
    unreal.EditorAssetLibrary.save_asset(config_path, only_if_is_dirty=False)
    log("routed %s -> %s" % (tag_name, action.get_name()))


def bind_key(imc_path, action, key_name):
    imc = unreal.load_asset(imc_path)
    if not imc:
        warn("%s is missing" % imc_path)
        return

    # Mappings is the deprecated array; the editor writes DefaultKeyMappings now.
    data = imc.get_editor_property("DefaultKeyMappings")
    mappings = data.get_editor_property("Mappings")

    for entry in mappings:
        if entry.get_editor_property("Action") == action:
            log("%s is already bound" % action.get_name())
            return

    key = unreal.Key()
    key.set_editor_property("KeyName", key_name)

    mapping = unreal.EnhancedActionKeyMapping()
    mapping.set_editor_property("Action", action)
    mapping.set_editor_property("Key", key)

    mappings.append(mapping)
    data.set_editor_property("Mappings", mappings)
    imc.set_editor_property("DefaultKeyMappings", data)
    unreal.EditorAssetLibrary.save_asset(imc_path, only_if_is_dirty=False)
    log("bound %s -> %s" % (action.get_name(), key_name))


def main():
    dash_action = ensure_input_action("IA_DashAction")
    if not dash_action:
        warn("could not create IA_DashAction")
        return

    route_tag_to_action(CONFIG_PATH, DASH_INPUT_TAG, dash_action)
    bind_key(IMC_PATH, dash_action, DODGE_KEY)

    log("dodge input wiring done")


main()
