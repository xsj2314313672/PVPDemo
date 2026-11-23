"""Verify the PVP game framework content produced by setup_pvp_game_framework.py.

Run headless (editor closed):
    UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript \
        -Script="<project>/Content/Python/verify_pvp_game_framework.py" -unattended -nosplash -nullrhi

Every assertion is hard: the first failure raises and the commandlet exits non zero. The report is
always written to Saved/PVPGameFrameworkReport/verify.{json,md}.
"""

import hashlib
import json
import os
import re

import unreal

CONTENT_DIR = unreal.Paths.project_content_dir().replace('\\', '/')
SAVED_DIR = unreal.Paths.project_saved_dir().replace('\\', '/')

SOURCE_MAP = '/Game/InfimaGames/TacticalFPSAnimations/Weapons/AssaultRifle/Demo/Maps/L_TFA_AR_Showcase'
TARGET_MAP = '/Game/PVP/Maps/L_PVP_Showcase'
CHARACTER_BP = '/Game/PVP/Characters/BP_PVPBaseCharacter'
LEGACY_CHARACTER_BP = '/Game/PVPDemo/Characters/BP_PVPBaseCharacter'
GAMEMODE_BP = '/Game/PVP/Game/BP_PVPGameMode'
PLAYERCONTROLLER_BP = '/Game/PVP/Game/BP_PVPPlayerController'
WEAPON_PICKUP_LABEL = 'PVP_WeaponPickup_AR'

REPORT_DIR = os.path.join(SAVED_DIR, 'PVPGameFrameworkReport')
HASH_FILE = os.path.join(REPORT_DIR, 'source_hashes.json')
REPORT_JSON = os.path.join(REPORT_DIR, 'verify.json')
REPORT_MD = os.path.join(REPORT_DIR, 'verify.md')

THIRD_PARTY_PREFIX = '/Game/InfimaGames/'
EXPECTED_DANGLING = {
    # Missing from the third-party pack itself (dangling soft reference inside SKEL_PVP_Mannequin).
    '/Game/InfimaGames/TacticalFPSAnimations/_Core/Mannequins/Meshes/SKM_Quinn',
}

checks = []


def log(message):
    unreal.log('[PVP-VERIFY] %s' % message)


def check(label, condition, detail=''):
    entry = {'label': label, 'ok': bool(condition), 'detail': str(detail)}
    checks.append(entry)
    log('%s %s%s' % ('PASS' if condition else 'FAIL', label, (' | ' + detail) if detail else ''))
    return bool(condition)


def sha256_of(path):
    if not os.path.exists(path):
        return None
    digest = hashlib.sha256()
    with open(path, 'rb') as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def scan_refs(roots):
    refs = set()
    for root in roots:
        for dirpath, _dirnames, filenames in os.walk(root):
            for filename in filenames:
                if not filename.endswith(('.uasset', '.umap')):
                    continue
                with open(os.path.join(dirpath, filename), 'rb') as handle:
                    data = handle.read()
                for match in re.finditer(rb'/Game/InfimaGames/[A-Za-z0-9_/]+', data):
                    refs.add(match.group(0).decode('ascii'))
    return refs


def count_actors(map_path):
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    if world is None:
        raise RuntimeError('cannot load %s' % map_path)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    return world, list(subsystem.get_all_level_actors())


def main():
    os.makedirs(REPORT_DIR, exist_ok=True)

    # 1. Zero live third-party references in the project roots.
    roots = [os.path.join(CONTENT_DIR, 'PVP'), os.path.join(CONTENT_DIR, 'PVPDemo')]
    refs = scan_refs(roots)
    live = {ref for ref in refs if unreal.EditorAssetLibrary.does_asset_exist(ref)}
    dangling = refs - live
    check('assets.zero_live_third_party_refs', not live, 'live=%d dangling=%d' % (len(live), len(dangling)))
    check('assets.dangling_is_expected', dangling == EXPECTED_DANGLING,
          'dangling=%s' % sorted(dangling))

    # 2. Character blueprint location / parent.
    check('blueprint.character.exists', unreal.EditorAssetLibrary.does_asset_exist(CHARACTER_BP), CHARACTER_BP)
    check('blueprint.character.legacy_path_gone', not unreal.EditorAssetLibrary.does_asset_exist(LEGACY_CHARACTER_BP), LEGACY_CHARACTER_BP)
    character_bp = unreal.EditorAssetLibrary.load_asset(CHARACTER_BP)
    if character_bp:
        class_default = unreal.get_default_object(character_bp.generated_class())
        check('blueprint.character.parent', isinstance(class_default, unreal.PVPBaseCharacter),
              class_default.get_class().get_name())

    # 3. Game mode class defaults.
    game_mode_bp = unreal.EditorAssetLibrary.load_asset(GAMEMODE_BP)
    check('blueprint.gamemode.exists', game_mode_bp is not None, GAMEMODE_BP)
    check('blueprint.playercontroller.exists', unreal.EditorAssetLibrary.does_asset_exist(PLAYERCONTROLLER_BP), PLAYERCONTROLLER_BP)

    game_mode_class = None
    if game_mode_bp:
        game_mode_class = game_mode_bp.generated_class()
        defaults = unreal.get_default_object(game_mode_class)
        pawn_class = defaults.get_editor_property('default_pawn_class')
        controller_class = defaults.get_editor_property('player_controller_class')
        check('gamemode.default_pawn', pawn_class and '/Game/PVP/Characters/' in pawn_class.get_path_name(),
              pawn_class.get_path_name() if pawn_class else 'None')
        check('gamemode.player_controller', controller_class and '/Game/PVP/Game/' in controller_class.get_path_name(),
              controller_class.get_path_name() if controller_class else 'None')

    # 4. Map content.
    source_hash_before = sha256_of(os.path.join(CONTENT_DIR, 'InfimaGames/TacticalFPSAnimations/Weapons/AssaultRifle/Demo/Maps/L_TFA_AR_Showcase.umap'))
    _source_world, source_actors = count_actors(SOURCE_MAP)
    target_world, target_actors = count_actors(TARGET_MAP)
    source_hash_after = sha256_of(os.path.join(CONTENT_DIR, 'InfimaGames/TacticalFPSAnimations/Weapons/AssaultRifle/Demo/Maps/L_TFA_AR_Showcase.umap'))

    check('map.source_unchanged', source_hash_before == source_hash_after,
          '%s vs %s' % (source_hash_before, source_hash_after))
    if os.path.exists(HASH_FILE):
        with open(HASH_FILE, 'r', encoding='utf-8') as handle:
            baseline = json.load(handle)
        recorded = baseline.get('map', {}).get('sha256')
        check('map.source_matches_baseline', recorded == source_hash_after, '%s vs %s' % (recorded, source_hash_after))

    # The weapon pickup is the only intentional addition on top of the migrated source map.
    weapon_pickups = [a for a in target_actors if a.get_actor_label() == WEAPON_PICKUP_LABEL]
    expected_actor_count = len(source_actors) + len(weapon_pickups)
    check('map.actor_count_conserved', len(target_actors) == expected_actor_count,
          'target=%d source=%d weapon_pickups=%d' % (
              len(target_actors), len(source_actors), len(weapon_pickups)))
    check('map.weapon_pickup_count', len(weapon_pickups) == 1,
          'count=%d' % len(weapon_pickups))

    player_starts = [a for a in target_actors if isinstance(a, unreal.PlayerStart)]
    check('map.player_start', len(player_starts) >= 1, 'count=%d' % len(player_starts))

    def class_path(actor):
        return str(actor.get_class().get_path_name())

    third_party_actors = [a for a in target_actors if class_path(a).startswith(THIRD_PARTY_PREFIX)]
    check('map.no_third_party_actors', not third_party_actors,
          ', '.join(class_path(a) for a in third_party_actors[:5]))

    legacy_actors = [a for a in target_actors if 'BP_TFA_BaseCharacter' in class_path(a) or 'BP_PVP_LegacyCharacter' in class_path(a)]
    check('map.no_legacy_character', not legacy_actors, 'count=%d' % len(legacy_actors))

    world_settings = None
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in subsystem.get_all_level_actors():
        if isinstance(actor, unreal.WorldSettings):
            world_settings = actor
            break
    if world_settings is None:
        world_settings = unreal.GameplayStatics.get_actor_of_class(target_world, unreal.WorldSettings)
    check('map.world_settings_found', world_settings is not None)

    if world_settings:
        override = world_settings.get_editor_property('default_game_mode')
        override_path = override.get_path_name() if override else 'None'
        check('map.gamemode_override', override and GAMEMODE_BP in override_path, override_path)

    # 5. Report.
    failures = [entry for entry in checks if not entry['ok']]
    with open(REPORT_JSON, 'w', encoding='utf-8') as handle:
        json.dump({'checks': checks, 'failures': len(failures)}, handle, indent=2, default=str)

    lines = ['# PVP game framework verification', '', '- failures: `%d`' % len(failures), '']
    for entry in checks:
        lines.append('- %s `%s`%s' % ('PASS' if entry['ok'] else 'FAIL', entry['label'],
                                      (' — ' + entry['detail']) if entry['detail'] else ''))
    with open(REPORT_MD, 'w', encoding='utf-8') as handle:
        handle.write('\n'.join(lines) + '\n')
    log('report written: %s' % REPORT_MD)

    if failures:
        raise RuntimeError('%d verification check(s) failed, see %s' % (len(failures), REPORT_MD))
    log('VERIFY OK')


if __name__ == '__main__':
    main()
