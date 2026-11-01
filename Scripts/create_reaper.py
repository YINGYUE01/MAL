# M3.3: build the fast melee enemy (reaper) from existing assets.
#
# Reuses:
#   - BP_StoneGolem as the template (parent, AI controller, weapon collision box)
#   - the player's Kwang mesh + ABP_MALCharacter (same skeleton as the scythe montages)
#   - GA_Scythe_Ability, which carries MAL.Ability.Attack like the golem attack, so the
#     existing behaviour tree activates it with no changes
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="...\create_reaper.py" -unattended -nullrhi
import unreal


def log(msg):
    unreal.log("[MALREAPER] " + str(msg))


PLAYER_BP = "/Game/Blueprint/Characters/MALCharacter/BP_MALCharacter"
ENEMY_SRC_BP = "/Game/Blueprint/Characters/EnemyCharacter/BP_StoneGolem"
ENEMY_DST_BP = "/Game/Blueprint/Characters/EnemyCharacter/BP_Reaper"
CLASS_SRC = "/Game/Blueprint/AI/DA_StoneGolem"
CLASS_DST = "/Game/Blueprint/AI/DA_Reaper"
SCYTHE_GA = "/Game/Blueprint/GameAbilitySyste/Ability/Scythe/GA_Scythe_Ability"

MESH_SCALE = 0.95
WALK_SPEED = 420.0
CAPSULE_RADIUS = 42.0
CAPSULE_HALF_HEIGHT = 96.0
WEAPON_BOX_EXTENT = unreal.Vector(70.0, 70.0, 70.0)


def read_player_appearance():
    player = unreal.load_asset(PLAYER_BP)
    if not player:
        return None, None
    cdo = unreal.get_default_object(player.generated_class())
    for component in cdo.get_components_by_class(unreal.SkeletalMeshComponent):
        mesh = component.get_editor_property("skeletal_mesh_asset")
        if mesh:
            return mesh, component.get_editor_property("anim_class")
    return None, None


def compile_and_save(blueprint):
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        log("compiled %s" % blueprint.get_name())
    except Exception as exc:
        log("compile failed (%s); saving anyway" % exc)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)


mesh, anim_class = read_player_appearance()
log("player mesh: %s" % mesh)
log("player anim: %s" % anim_class)

if not mesh or not anim_class:
    log("cannot resolve the player mesh/anim; aborting")
else:
    if not unreal.load_asset(CLASS_DST):
        unreal.EditorAssetLibrary.duplicate_asset(CLASS_SRC, CLASS_DST)
        log("duplicated class definition")
    class_definition = unreal.load_asset(CLASS_DST)

    reaper = unreal.load_asset(ENEMY_DST_BP)
    if not reaper:
        reaper = unreal.EditorAssetLibrary.duplicate_asset(ENEMY_SRC_BP, ENEMY_DST_BP)
        log("duplicated BP_Reaper")

    if not reaper:
        log("could not create BP_Reaper; aborting")
    else:
        cdo = unreal.get_default_object(reaper.generated_class())

        for component in cdo.get_components_by_class(unreal.SkeletalMeshComponent):
            if not component.get_editor_property("skeletal_mesh_asset"):
                continue
            component.set_editor_property("skeletal_mesh_asset", mesh)
            component.set_editor_property("anim_class", anim_class)
            component.set_editor_property("relative_scale3d", unreal.Vector(MESH_SCALE, MESH_SCALE, MESH_SCALE))
            log("mesh swapped to Kwang")

        for capsule in cdo.get_components_by_class(unreal.CapsuleComponent):
            capsule.set_editor_property("capsule_radius", CAPSULE_RADIUS)
            capsule.set_editor_property("capsule_half_height", CAPSULE_HALF_HEIGHT)
            log("capsule resized")

        # The golem's hit box is sized for its fists; a scythe sweeps wider and lower.
        for box in cdo.get_components_by_class(unreal.BoxComponent):
            box.set_editor_property("box_extent", WEAPON_BOX_EXTENT)
            log("weapon box resized")

        for movement in cdo.get_components_by_class(unreal.CharacterMovementComponent):
            movement.set_editor_property("max_walk_speed", WALK_SPEED)
            log("walk speed set to %.0f" % WALK_SPEED)

        if class_definition:
            cdo.set_editor_property("class_definition", class_definition)
            log("class definition wired")

        scythe = unreal.load_asset(SCYTHE_GA)
        if scythe:
            try:
                cdo.set_editor_property("innate_abilities", [scythe.generated_class()])
                log("innate ability: %s" % scythe.get_name())
            except Exception as exc:
                log("innate ability write failed: %s" % exc)

        compile_and_save(reaper)

log("reaper done")
