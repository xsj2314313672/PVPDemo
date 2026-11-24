"""Build the PVP assault rifle weapon assets and place a pickable instance.

Run headless (editor closed, PVPDemo / PVPDemoEditor compiled):
    UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript \
        -Script="<project>/Content/Python/setup_pvp_weapon.py" -unattended -nosplash -nullrhi

The AnimGraph node creation for the weapon AnimBlueprint lives in the editor C++ builder
(UPVPAnimBlueprintBuilder::BuildWeaponAnimBlueprint); this script owns the asset lifecycle:

1. validate the migrated AR skeleton / mesh / reference pose and the paired montages;
2. create AM_PVP_FP_AR_Fire_Aimed from A_PVP_FP_AR_Fire_Aimed (single DefaultSlot track), used while
   the character is aiming;
3. delete and rebuild /Game/PVP/Animation/AnimBP/ABP_PVP_FP_Weapon_AR
   (SequencePlayer(A_PVP_WEP_AR_Reference) -> DefaultSlot -> Root);
4. insert DefaultSlot into the existing ABP_PVP_FP_Locomotion in place (never delete it, so the
   BP_PVPBaseCharacter Mesh AnimClass override survives);
5. create / update /Game/PVP/Actors/BP_PVP_Weapon_AR and set its class defaults (mesh, anim class,
   sockets, pickup radius, paired Equip / Fire / AimedFire / Reload montages);
6. place exactly one tagged pickup near the PlayerStart in /Game/PVP/Maps/L_PVP_Showcase;
7. read everything back and write Saved/PVPWeaponReport/report.{json,md}.

The weapon blueprint uses actor-level configuration properties applied by APVPWeaponActor's
OnConstruction; the script never edits the inherited Mesh component templates of BP_PVPBaseCharacter.
"""

import json
import math
import os
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(os.path.dirname(SCRIPT_DIR))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from pvp_logic_layout import EXPECTED_ASSETS, target_dir_for_asset_name  # noqa: E402

WEAPON_BP_NAME = 'BP_PVP_Weapon_AR'
WEAPON_BP_DIR = '/Game/PVP/Actors'
WEAPON_BP_PATH = '%s/%s' % (WEAPON_BP_DIR, WEAPON_BP_NAME)

WEAPON_ABP_NAME = 'ABP_PVP_FP_Weapon_AR'
WEAPON_ABP_DIR = target_dir_for_asset_name(WEAPON_ABP_NAME)
WEAPON_ABP_PATH = '%s/%s' % (WEAPON_ABP_DIR, WEAPON_ABP_NAME)
LOCOMOTION_ABP_PATH = '/Game/PVP/Animation/AnimBP/ABP_PVP_FP_Locomotion'
CHARACTER_BP_PATH = '/Game/PVP/Characters/BP_PVPBaseCharacter'
MAP_PATH = '/Game/PVP/Maps/L_PVP_Showcase'

WEAPON_SKELETON_PATH = '/Game/PVPDemo/Weapons/AssaultRifle/Meshes/SKEL_PVP_AR'
WEAPON_MESH_PATH = '/Game/PVPDemo/Weapons/AssaultRifle/Meshes/SK_PVP_AR'
WEAPON_REFERENCE_PATH = \
    '/Game/PVPDemo/Weapons/AssaultRifle/Animations/Weapon/Common/A_PVP_WEP_AR_Reference'

CHARACTER_SKELETON_PATH = '/Game/PVPDemo/Characters/Mannequins/Meshes/SKEL_PVP_Mannequin'
CHARACTER_MESH_PATH = '/Game/PVPDemo/Characters/Mannequins/Meshes/SKM_PVP_FP_Manny_Simple'

CHARACTER_MONTAGE_DIR = '/Game/PVPDemo/Weapons/AssaultRifle/Animations/Character/FP'
WEAPON_MONTAGE_DIR = '/Game/PVPDemo/Weapons/AssaultRifle/Animations/Weapon/FP'
CHARACTER_EQUIP_PATH = CHARACTER_MONTAGE_DIR + '/Transitions/AM_PVP_FP_AR_Equip'
CHARACTER_FIRE_PATH = CHARACTER_MONTAGE_DIR + '/Combat/AM_PVP_FP_AR_Fire_Auto'
CHARACTER_AIMED_FIRE_SEQUENCE_PATH = CHARACTER_MONTAGE_DIR + '/Combat/A_PVP_FP_AR_Fire_Aimed'
CHARACTER_AIMED_FIRE_PATH = CHARACTER_MONTAGE_DIR + '/Combat/AM_PVP_FP_AR_Fire_Aimed'
CHARACTER_RELOAD_PATH = CHARACTER_MONTAGE_DIR + '/Combat/AM_PVP_FP_AR_Reload'
WEAPON_EQUIP_PATH = WEAPON_MONTAGE_DIR + '/AM_PVP_FP_WEP_AR_Equip'
WEAPON_FIRE_PATH = WEAPON_MONTAGE_DIR + '/AM_PVP_FP_WEP_AR_Fire'
WEAPON_RELOAD_PATH = WEAPON_MONTAGE_DIR + '/AM_PVP_FP_WEP_AR_Reload'

DEFAULT_MONTAGE_SLOT = 'DefaultSlot'
PICKUP_LABEL = 'PVP_WeaponPickup_AR'
PICKUP_FORWARD_OFFSET = 200.0
PICKUP_HEIGHT_OFFSET = -80.0
PICKUP_RADIUS = 120.0

REPORT_DIR = os.path.join(PROJECT_DIR, 'Saved', 'PVPWeaponReport').replace('\\', '/')
REPORT_JSON = REPORT_DIR + '/report.json'
REPORT_MD = REPORT_DIR + '/report.md'
WEAPON_ABP_BUILDER_JSON = REPORT_DIR + '/weapon_abp_builder.json'
SLOT_BUILDER_JSON = REPORT_DIR + '/slot_builder.json'
AIMED_FIRE_BUILDER_JSON = REPORT_DIR + '/aimed_fire_montage_builder.json'

state = {
    'weapon_abp_path': WEAPON_ABP_PATH,
    'weapon_bp_path': WEAPON_BP_PATH,
    'locomotion_abp_path': LOCOMOTION_ABP_PATH,
    'aimed_fire_montage_path': CHARACTER_AIMED_FIRE_PATH,
    'map_path': MAP_PATH,
    'aimed_fire_montage': {
        'deleted_previous': False,
        'created': False,
        'saved': False,
        'builder_report': None,
    },
    'weapon_abp': {
        'deleted_previous': False,
        'created': False,
        'compiled': False,
        'saved': False,
        'builder_report': None,
    },
    'character_slot': {
        'ensured': False,
        'compiled': False,
        'saved': False,
        'builder_report': None,
    },
    'weapon_blueprint': {
        'created': False,
        'compiled': False,
        'saved': False,
        'defaults': {},
    },
    'map': {
        'instance_count': 0,
        'action': 'none',
    },
    'checks': [],
    'errors': [],
    'warnings': [],
}


def log(message):
    unreal.log('[PVPWeapon] %s' % message)


def warn(message):
    state['warnings'].append(message)
    unreal.log_warning('[PVPWeapon] %s' % message)


def error(message):
    state['errors'].append(message)
    unreal.log_error('[PVPWeapon] %s' % message)


def check(label, ok, detail=''):
    state['checks'].append({'label': label, 'ok': bool(ok), 'detail': str(detail)})
    if ok:
        log('PASS %s%s' % (label, (': %s' % detail) if detail else ''))
    else:
        error('FAIL %s%s' % (label, (': %s' % detail) if detail else ''))
    return bool(ok)


def asset_file_path(package_path):
    relative = package_path[len('/Game/'):].replace('/', os.sep)
    return os.path.join(PROJECT_DIR, 'Content', relative + '.uasset')


def load_asset(path, expected_type, label):
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        error('missing %s: %s' % (label, path))
        return None
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        error('failed to load %s: %s' % (label, path))
        return None
    if expected_type and not isinstance(asset, expected_type):
        error('%s has unexpected type %s (expected %s)' % (
            path, type(asset).__name__, expected_type.__name__))
        return None
    return asset


def read_json(path):
    if not os.path.exists(path):
        error('builder report missing: %s' % path)
        return None
    try:
        with open(path, 'r', encoding='utf-8') as handle:
            return json.load(handle)
    except Exception as exc:  # noqa: BLE001
        error('failed to read %s: %s' % (path, exc))
        return None


def set_property(obj, value, *names):
    """Set the first reflected property that exists (tolerates Python name casing)."""
    last_exc = None
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return name
        except Exception as exc:  # noqa: BLE001
            last_exc = exc
    raise RuntimeError('cannot set any of %s: %s' % (list(names), last_exc))


def get_property(obj, *names):
    for name in names:
        try:
            return obj.get_editor_property(name)
        except Exception:  # noqa: BLE001
            continue
    return None


def path_of(obj):
    return obj.get_path_name() if obj else ''


def remove_previous_asset(package_path):
    file_path = asset_file_path(package_path)
    if not unreal.EditorAssetLibrary.does_asset_exist(package_path) and not os.path.exists(file_path):
        log('no previous %s' % package_path)
        return True

    log('deleting previous %s' % package_path)
    if unreal.EditorAssetLibrary.does_asset_exist(package_path):
        unreal.EditorAssetLibrary.delete_asset(package_path)

    # ForceDeleteObjects clears the UObject but the commandlet can keep the file on disk until the
    # process exits; removing it keeps the rebuild deterministic.
    if os.path.exists(file_path):
        try:
            os.remove(file_path)
        except OSError as exc:
            error('failed to remove %s: %s' % (file_path, exc))
            return False

    if os.path.exists(file_path):
        error('%s still exists after delete' % file_path)
        return False
    return True


def validate_layout():
    check('layout.weapon_bp_registered', WEAPON_BP_NAME in EXPECTED_ASSETS['Actors'], WEAPON_BP_NAME)
    check('layout.weapon_abp_registered',
          WEAPON_ABP_NAME in EXPECTED_ASSETS['Animation/AnimBP'], WEAPON_ABP_NAME)
    check('layout.weapon_bp_dir', WEAPON_BP_DIR == '/Game/PVP/Actors', WEAPON_BP_DIR)
    check('layout.weapon_abp_dir', WEAPON_ABP_DIR == '/Game/PVP/Animation/AnimBP', WEAPON_ABP_DIR)


def load_dependencies():
    deps = {
        'skeleton': load_asset(WEAPON_SKELETON_PATH, unreal.Skeleton, 'weapon skeleton'),
        'weapon_mesh': load_asset(WEAPON_MESH_PATH, unreal.SkeletalMesh, 'weapon mesh'),
        'reference_pose': load_asset(WEAPON_REFERENCE_PATH, unreal.AnimSequence, 'weapon reference pose'),
        'character_skeleton': load_asset(CHARACTER_SKELETON_PATH, unreal.Skeleton, 'character skeleton'),
        'character_mesh': load_asset(CHARACTER_MESH_PATH, unreal.SkeletalMesh, 'character mesh'),
        'character_equip': load_asset(CHARACTER_EQUIP_PATH, unreal.AnimMontage, 'character equip montage'),
        'character_fire': load_asset(CHARACTER_FIRE_PATH, unreal.AnimMontage, 'character fire montage'),
        'character_aimed_fire_sequence': load_asset(
            CHARACTER_AIMED_FIRE_SEQUENCE_PATH, unreal.AnimSequence, 'character aimed fire sequence'),
        'character_reload': load_asset(CHARACTER_RELOAD_PATH, unreal.AnimMontage, 'character reload montage'),
        'weapon_equip': load_asset(WEAPON_EQUIP_PATH, unreal.AnimMontage, 'weapon equip montage'),
        'weapon_fire': load_asset(WEAPON_FIRE_PATH, unreal.AnimMontage, 'weapon fire montage'),
        'weapon_reload': load_asset(WEAPON_RELOAD_PATH, unreal.AnimMontage, 'weapon reload montage'),
    }
    if not check('character.bp_exists',
                 unreal.EditorAssetLibrary.does_asset_exist(CHARACTER_BP_PATH), CHARACTER_BP_PATH):
        deps['character_bp'] = None
    else:
        deps['character_bp'] = unreal.EditorAssetLibrary.load_asset(CHARACTER_BP_PATH)
    return deps


def build_aimed_fire_montage(deps):
    """Bake A_PVP_FP_AR_Fire_Aimed into the DefaultSlot montage used while aiming."""
    sequence = deps.get('character_aimed_fire_sequence')
    skeleton = deps.get('character_skeleton')
    preview_mesh = deps.get('character_mesh')
    if sequence is None or skeleton is None or preview_mesh is None:
        error('cannot build %s: missing sequence / character skeleton / preview mesh'
              % CHARACTER_AIMED_FIRE_PATH)
        return None

    if not remove_previous_asset(CHARACTER_AIMED_FIRE_PATH):
        return None
    state['aimed_fire_montage']['deleted_previous'] = True

    os.makedirs(REPORT_DIR, exist_ok=True)
    log('calling UPVPAnimBlueprintBuilder.build_montage_from_sequence for %s'
        % CHARACTER_AIMED_FIRE_PATH)
    montage = unreal.PVPAnimBlueprintBuilder.build_montage_from_sequence(
        CHARACTER_AIMED_FIRE_PATH,
        sequence,
        skeleton,
        preview_mesh,
        DEFAULT_MONTAGE_SLOT,
        AIMED_FIRE_BUILDER_JSON,
    )
    if montage is None:
        error('build_montage_from_sequence returned None')
        return None
    state['aimed_fire_montage']['created'] = True

    if not unreal.EditorAssetLibrary.save_loaded_asset(montage, only_if_is_dirty=False):
        error('failed to save %s' % CHARACTER_AIMED_FIRE_PATH)
    state['aimed_fire_montage']['saved'] = \
        unreal.EditorAssetLibrary.does_asset_exist(CHARACTER_AIMED_FIRE_PATH)

    report = read_json(AIMED_FIRE_BUILDER_JSON) or {}
    state['aimed_fire_montage']['builder_report'] = report
    check('aimed_fire.builder_source',
          (report.get('source_sequence') or '').startswith(CHARACTER_AIMED_FIRE_SEQUENCE_PATH),
          report.get('source_sequence') or 'None')
    check('aimed_fire.builder_slot', report.get('montage_slot') == DEFAULT_MONTAGE_SLOT,
          report.get('montage_slot') or 'None')
    check('aimed_fire.slot_matches', bool(report.get('slot_matches')), 'slot_matches')
    check('aimed_fire.source_segment', bool(report.get('source_segment_matches')),
          'source_segment_matches')
    check('aimed_fire.skeleton', bool(report.get('skeleton_matches')), 'skeleton_matches')
    check('aimed_fire.segments', int(report.get('segment_count') or 0) >= 1,
          report.get('segment_count'))
    check('aimed_fire.sections', int(report.get('section_count') or 0) >= 1,
          report.get('section_count'))

    loaded = load_asset(CHARACTER_AIMED_FIRE_PATH, unreal.AnimMontage, 'aimed fire montage')
    if loaded is not None:
        slot_tracks = get_property(loaded, 'slot_anim_tracks')
        if slot_tracks:
            slot_name = str(get_property(slot_tracks[0], 'slot_name'))
            check('aimed_fire.runtime_slot', slot_name == DEFAULT_MONTAGE_SLOT, slot_name)
        else:
            check('aimed_fire.runtime_slot', False, 'no slot tracks after reload')
    return loaded


def build_weapon_abp(deps):
    if not remove_previous_asset(WEAPON_ABP_PATH):
        return None
    state['weapon_abp']['deleted_previous'] = True

    os.makedirs(REPORT_DIR, exist_ok=True)
    log('calling UPVPAnimBlueprintBuilder.build_weapon_anim_blueprint for %s' % WEAPON_ABP_PATH)
    blueprint = unreal.PVPAnimBlueprintBuilder.build_weapon_anim_blueprint(
        WEAPON_ABP_PATH,
        deps['skeleton'],
        deps['weapon_mesh'],
        deps['reference_pose'],
        DEFAULT_MONTAGE_SLOT,
        WEAPON_ABP_BUILDER_JSON,
    )
    if blueprint is None:
        error('build_weapon_anim_blueprint returned None')
        return None
    state['weapon_abp']['created'] = True

    compiled = unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    state['weapon_abp']['compiled'] = bool(compiled)
    check('weapon_abp.compiled', compiled, 'BlueprintEditorLibrary.compile_blueprint')

    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        error('failed to save %s' % WEAPON_ABP_PATH)
    state['weapon_abp']['saved'] = unreal.EditorAssetLibrary.does_asset_exist(WEAPON_ABP_PATH)

    loaded = load_asset(WEAPON_ABP_PATH, unreal.AnimBlueprint, 'weapon AnimBlueprint')
    if loaded is None:
        return None

    report = read_json(WEAPON_ABP_BUILDER_JSON) or {}
    state['weapon_abp']['builder_report'] = report
    check('weapon_abp.builder_parent', report.get('parent_class') == 'AnimInstance',
          report.get('parent_class') or 'None')
    check('weapon_abp.builder_skeleton',
          (report.get('skeleton') or '').startswith(WEAPON_SKELETON_PATH),
          report.get('skeleton') or 'None')
    check('weapon_abp.builder_reference',
          (report.get('reference_pose') or '').startswith(WEAPON_REFERENCE_PATH),
          report.get('reference_pose') or 'None')
    check('weapon_abp.builder_slot', report.get('montage_slot') == DEFAULT_MONTAGE_SLOT,
          report.get('montage_slot') or 'None')
    wiring = report.get('wiring') or {}
    for key in ('reference_pose', 'slot_source', 'slot_pose', 'root_pose'):
        check('weapon_abp.wiring.%s' % key, bool(wiring.get(key)), key)
    check('weapon_abp.builder_compiled', bool(report.get('compiled')),
          'compile_errors=%s' % report.get('compile_errors'))

    target_skeleton = get_property(loaded, 'target_skeleton')
    check('weapon_abp.asset_skeleton',
          path_of(target_skeleton).startswith(WEAPON_SKELETON_PATH), path_of(target_skeleton))

    return loaded


def ensure_character_slot():
    abp = load_asset(LOCOMOTION_ABP_PATH, unreal.AnimBlueprint, 'locomotion AnimBlueprint')
    if abp is None:
        return None

    os.makedirs(REPORT_DIR, exist_ok=True)
    log('calling UPVPAnimBlueprintBuilder.ensure_montage_slot for %s' % LOCOMOTION_ABP_PATH)
    ensured = unreal.PVPAnimBlueprintBuilder.ensure_montage_slot(
        LOCOMOTION_ABP_PATH, DEFAULT_MONTAGE_SLOT, SLOT_BUILDER_JSON)
    state['character_slot']['ensured'] = bool(ensured)
    check('character_slot.ensured', ensured, DEFAULT_MONTAGE_SLOT)

    compiled = unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    state['character_slot']['compiled'] = bool(compiled)
    check('character_slot.compiled', compiled, 'BlueprintEditorLibrary.compile_blueprint')

    if not unreal.EditorAssetLibrary.save_loaded_asset(abp, only_if_is_dirty=False):
        error('failed to save %s' % LOCOMOTION_ABP_PATH)
    state['character_slot']['saved'] = unreal.EditorAssetLibrary.does_asset_exist(LOCOMOTION_ABP_PATH)

    report = read_json(SLOT_BUILDER_JSON) or {}
    state['character_slot']['builder_report'] = report
    check('character_slot.report_slot', report.get('montage_slot') == DEFAULT_MONTAGE_SLOT,
          report.get('montage_slot') or 'None')
    check('character_slot.report_wired', bool(report.get('wired')), 'wired')
    check('character_slot.report_compiled', bool(report.get('compiled')),
          'compile_errors=%s' % report.get('compile_errors'))
    check('character_slot.was_existing_or_created',
          bool(report.get('found_existing')) or bool(report.get('created')),
          'found_existing=%s created=%s' % (report.get('found_existing'), report.get('created')))

    return abp


def make_montage_pair(character_montage, weapon_montage, play_rate=1.0):
    pair = unreal.PVPWeaponMontagePair()
    set_property(pair, character_montage, 'character_montage')
    set_property(pair, weapon_montage, 'weapon_montage')
    set_property(pair, play_rate, 'play_rate')
    return pair


def get_weapon_actor_class():
    weapon_class = getattr(unreal, 'PVPWeaponActor', None)
    if weapon_class is None:
        weapon_class = unreal.load_object(None, '/Script/PVPDemo.PVPWeaponActor')
    if weapon_class is None:
        raise RuntimeError('PVPWeaponActor is not reflected; is the PVPDemo module compiled?')
    return weapon_class


def is_weapon_blueprint_default(cdo, weapon_class):
    try:
        return isinstance(cdo, weapon_class)
    except TypeError:
        current = cdo.get_class()
        while current:
            if current.get_name() == 'PVPWeaponActor':
                return True
            current = get_property(current, 'super_struct')
        return False


def ensure_weapon_blueprint(deps, weapon_abp, aimed_fire_montage):
    weapon_class = get_weapon_actor_class()
    created = False

    if unreal.EditorAssetLibrary.does_asset_exist(WEAPON_BP_PATH):
        blueprint = unreal.EditorAssetLibrary.load_asset(WEAPON_BP_PATH)
        if not isinstance(blueprint, unreal.Blueprint):
            error('%s is not a Blueprint' % WEAPON_BP_PATH)
            return None
        if blueprint.generated_class() is None:
            unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        cdo = unreal.get_default_object(blueprint.generated_class())
        if cdo is None or not is_weapon_blueprint_default(cdo, weapon_class):
            error('%s does not derive from APVPWeaponActor; delete it and re-run' % WEAPON_BP_PATH)
            return None
        log('reusing %s' % WEAPON_BP_PATH)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', weapon_class)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            WEAPON_BP_NAME, WEAPON_BP_DIR, unreal.Blueprint, factory)
        if blueprint is None:
            error('failed to create %s' % WEAPON_BP_PATH)
            return None
        created = True
        log('created %s (parent=PVPWeaponActor)' % WEAPON_BP_PATH)

    state['weapon_blueprint']['created'] = created

    cdo = unreal.get_default_object(blueprint.generated_class())
    if cdo is None:
        error('%s has no generated class' % WEAPON_BP_PATH)
        return None

    weapon_anim_class = weapon_abp.generated_class()
    if weapon_anim_class is None:
        error('%s has no generated class' % WEAPON_ABP_PATH)
        return None

    set_property(cdo, deps['weapon_mesh'], 'mesh1p_asset', 'mesh_1p_asset')
    set_property(cdo, weapon_anim_class, 'mesh1p_anim_class', 'mesh_1p_anim_class')
    set_property(cdo, deps['weapon_mesh'], 'mesh3p_asset', 'mesh_3p_asset')
    set_property(cdo, weapon_anim_class, 'mesh3p_anim_class', 'mesh_3p_anim_class')
    set_property(cdo, 'ik_hand_gun', 'attach_socket_name')
    set_property(cdo, 'SOCKET_Muzzle', 'muzzle_socket_name')
    set_property(cdo, PICKUP_RADIUS, 'pickup_radius')
    set_property(cdo, True, 'b_auto_pickup', 'auto_pickup')
    set_property(cdo, True, 'b_play_equip_montage_on_pickup', 'play_equip_montage_on_pickup')
    set_property(cdo, make_montage_pair(deps['character_equip'], deps['weapon_equip']),
                 'equip_montage')
    set_property(cdo, make_montage_pair(deps['character_fire'], deps['weapon_fire']),
                 'fire_montage')
    # Aimed fire reuses the weapon-side fire montage and swaps only the character-side animation.
    set_property(cdo, make_montage_pair(aimed_fire_montage, deps['weapon_fire']),
                 'aimed_fire_montage')
    set_property(cdo, make_montage_pair(deps['character_reload'], deps['weapon_reload']),
                 'reload_montage')

    compiled = unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    state['weapon_blueprint']['compiled'] = bool(compiled)
    check('weapon_bp.compiled', compiled, 'BlueprintEditorLibrary.compile_blueprint')

    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        error('failed to save %s' % WEAPON_BP_PATH)
    state['weapon_blueprint']['saved'] = unreal.EditorAssetLibrary.does_asset_exist(WEAPON_BP_PATH)

    loaded = load_asset(WEAPON_BP_PATH, unreal.Blueprint, 'weapon Blueprint')
    if loaded is None:
        return None
    cdo = unreal.get_default_object(loaded.generated_class())
    if cdo is None:
        error('%s has no CDO after reload' % WEAPON_BP_PATH)
        return None

    mesh1p = get_property(cdo, 'mesh1p_asset', 'mesh_1p_asset')
    mesh3p = get_property(cdo, 'mesh3p_asset', 'mesh_3p_asset')
    anim1p = get_property(cdo, 'mesh1p_anim_class', 'mesh_1p_anim_class')
    anim3p = get_property(cdo, 'mesh3p_anim_class', 'mesh_3p_anim_class')
    attach_socket = get_property(cdo, 'attach_socket_name')
    muzzle_socket = get_property(cdo, 'muzzle_socket_name')
    pickup_radius = get_property(cdo, 'pickup_radius')
    auto_pickup = get_property(cdo, 'b_auto_pickup', 'auto_pickup')
    play_equip = get_property(cdo, 'b_play_equip_montage_on_pickup',
                              'play_equip_montage_on_pickup')

    defaults = {
        'mesh1p': path_of(mesh1p),
        'mesh3p': path_of(mesh3p),
        'anim1p': path_of(anim1p),
        'anim3p': path_of(anim3p),
        'attach_socket': str(attach_socket),
        'muzzle_socket': str(muzzle_socket),
        'pickup_radius': pickup_radius,
        'auto_pickup': bool(auto_pickup),
        'play_equip_montage': bool(play_equip),
    }
    state['weapon_blueprint']['defaults'] = defaults

    check('weapon_bp.mesh1p', defaults['mesh1p'] == path_of(deps['weapon_mesh']), defaults['mesh1p'])
    check('weapon_bp.mesh3p', defaults['mesh3p'] == path_of(deps['weapon_mesh']), defaults['mesh3p'])
    check('weapon_bp.anim1p', defaults['anim1p'].startswith(WEAPON_ABP_PATH), defaults['anim1p'])
    check('weapon_bp.anim3p', defaults['anim3p'].startswith(WEAPON_ABP_PATH), defaults['anim3p'])
    check('weapon_bp.attach_socket', defaults['attach_socket'] == 'ik_hand_gun',
          defaults['attach_socket'])
    check('weapon_bp.muzzle_socket', defaults['muzzle_socket'] == 'SOCKET_Muzzle',
          defaults['muzzle_socket'])
    check('weapon_bp.pickup_radius', pickup_radius is not None
          and abs(float(pickup_radius) - PICKUP_RADIUS) < 0.01, pickup_radius)
    check('weapon_bp.auto_pickup', bool(auto_pickup), bool(auto_pickup))
    check('weapon_bp.play_equip_montage', bool(play_equip), bool(play_equip))

    expected_pairs = (
        ('equip_montage', deps['character_equip'], deps['weapon_equip']),
        ('fire_montage', deps['character_fire'], deps['weapon_fire']),
        ('aimed_fire_montage', aimed_fire_montage, deps['weapon_fire']),
        ('reload_montage', deps['character_reload'], deps['weapon_reload']),
    )
    for prop_name, expected_character, expected_weapon in expected_pairs:
        pair = get_property(cdo, prop_name)
        character_montage = get_property(pair, 'character_montage') if pair else None
        weapon_montage = get_property(pair, 'weapon_montage') if pair else None
        pair_ok = (path_of(character_montage) == path_of(expected_character)
                   and path_of(weapon_montage) == path_of(expected_weapon))
        check('weapon_bp.%s' % prop_name, pair_ok,
              'character=%s weapon=%s' % (path_of(character_montage), path_of(weapon_montage)))

    return loaded


def find_pickup_actors(actors):
    return [actor for actor in actors if actor.get_actor_label() == PICKUP_LABEL]


def place_weapon_pickup(weapon_bp):
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    if world is None:
        error('failed to load %s' % MAP_PATH)
        return
    state['map']['loaded'] = True

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = list(actor_subsystem.get_all_level_actors())
    player_starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
    if not player_starts:
        error('%s has no PlayerStart' % MAP_PATH)
        return

    player_start = player_starts[0]
    start_location = player_start.get_actor_location()
    start_rotation = player_start.get_actor_rotation()
    yaw_radians = math.radians(start_rotation.yaw)
    forward = unreal.Vector(math.cos(yaw_radians), math.sin(yaw_radians), 0.0)
    spawn_location = start_location + forward * PICKUP_FORWARD_OFFSET \
        + unreal.Vector(0.0, 0.0, PICKUP_HEIGHT_OFFSET)
    spawn_rotation = unreal.Rotator(pitch=0.0, yaw=start_rotation.yaw, roll=0.0)

    weapon_class = weapon_bp.generated_class()
    if weapon_class is None:
        error('%s has no generated class' % WEAPON_BP_PATH)
        return

    existing = find_pickup_actors(actors)
    actor = None
    for candidate in existing:
        same_class = candidate.get_class() == weapon_class
        if actor is None and same_class:
            actor = candidate
        else:
            actor_subsystem.destroy_actor(candidate)

    if actor is None:
        actor = actor_subsystem.spawn_actor_from_class(weapon_class, spawn_location, spawn_rotation)
        state['map']['action'] = 'spawned'
    else:
        actor.set_actor_location(spawn_location, False, False)
        actor.set_actor_rotation(spawn_rotation, False)
        state['map']['action'] = 'updated'

    if actor is None:
        error('failed to spawn %s in %s' % (WEAPON_BP_PATH, MAP_PATH))
        return

    actor.set_actor_label(PICKUP_LABEL)
    try:
        actor.set_editor_property('tags', [unreal.Name(PICKUP_LABEL)])
    except Exception as exc:  # noqa: BLE001
        warn('cannot set pickup actor tags: %s' % exc)

    check('map.pickup_actor_class', actor.get_class() == weapon_class,
          actor.get_class().get_name())

    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
        error('failed to save %s' % MAP_PATH)
        return
    state['map']['saved'] = True

    # Reload the map so the report reflects the serialized state, not the in-memory edit.
    reloaded = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    if reloaded is None:
        error('failed to reload %s after save' % MAP_PATH)
        return
    reloaded_actors = list(actor_subsystem.get_all_level_actors())
    pickups = find_pickup_actors(reloaded_actors)
    state['map']['instance_count'] = len(pickups)
    if pickups:
        state['map']['instance_class'] = pickups[0].get_class().get_name()
        state['map']['instance_location'] = str(pickups[0].get_actor_location())
    check('map.pickup_count', len(pickups) == 1, '%d (expected 1)' % len(pickups))
    if pickups:
        check('map.pickup_class', pickups[0].get_class() == weapon_class,
              pickups[0].get_class().get_name())


def read_character_anim_class():
    """Read the Mesh AnimClass from the BP_PVPBaseCharacter CDO (the live override, not a string)."""
    try:
        character_bp = unreal.EditorAssetLibrary.load_asset(CHARACTER_BP_PATH)
        if character_bp is None:
            return None
        generated_class = character_bp.generated_class()
        if generated_class is None:
            unreal.BlueprintEditorLibrary.compile_blueprint(character_bp)
            generated_class = character_bp.generated_class()
        if generated_class is None:
            return None
        cdo = unreal.get_default_object(generated_class)
        mesh = cdo.get_editor_property('mesh') if cdo else None
        if mesh is None:
            return None
        anim_class = None
        try:
            anim_class = mesh.get_anim_instance_class()
        except Exception:  # noqa: BLE001
            anim_class = get_property(mesh, 'anim_class', 'anim_instance_class')
        return path_of(anim_class)
    except Exception as exc:  # noqa: BLE001
        warn('cannot read BP_PVPBaseCharacter CDO Mesh AnimClass: %s' % exc)
        return None


def verify_character_anim_override():
    """The locomotion ABP is edited / rebuilt, so the character blueprint must still reference it."""
    file_path = asset_file_path(CHARACTER_BP_PATH)
    if not os.path.exists(file_path):
        error('character blueprint file missing: %s' % file_path)
        return

    anim_class_path = read_character_anim_class()
    if anim_class_path is None:
        warn('CDO Mesh AnimClass is unavailable; falling back to the package string scan')
    else:
        check('character.locomotion_anim_class',
              anim_class_path.startswith(LOCOMOTION_ABP_PATH), anim_class_path)

    with open(file_path, 'rb') as handle:
        data = handle.read()
    expected = b'/Game/PVP/Animation/AnimBP/ABP_PVP_FP_Locomotion'
    found = expected in data
    check('character.locomotion_reference', found,
          'BP_PVPBaseCharacter package contains ABP_PVP_FP_Locomotion' if found
          else 'override lost, set Mesh AnimClass back to ABP_PVP_FP_Locomotion')

    if not found:
        warn('BP_PVPBaseCharacter lost its ABP_PVP_FP_Locomotion Mesh AnimClass override; '
             're-assign it in the editor before testing.')


def write_markdown():
    lines = [
        '# PVP weapon setup',
        '',
        '- result: **%s**' % ('PASS' if not state['errors'] else 'FAIL'),
        '- weapon blueprint: `%s`' % WEAPON_BP_PATH,
        '- weapon AnimBlueprint: `%s`' % WEAPON_ABP_PATH,
        '- locomotion AnimBlueprint: `%s`' % LOCOMOTION_ABP_PATH,
        '- aimed fire montage: `%s`' % CHARACTER_AIMED_FIRE_PATH,
        '- map: `%s`' % MAP_PATH,
        '- pickup label: `%s`' % PICKUP_LABEL,
        '',
        '## Checks',
        '',
        '| check | result | detail |',
        '| --- | --- | --- |',
    ]
    for item in state['checks']:
        lines.append('| `%s` | %s | %s |' % (
            item['label'], 'PASS' if item['ok'] else 'FAIL',
            item['detail'].replace('|', '/')))

    lines.extend([
        '',
        '## Weapon blueprint defaults',
        '',
        '```json',
        json.dumps(state['weapon_blueprint']['defaults'], indent=2, ensure_ascii=False),
        '```',
        '',
        '## Map',
        '',
        '```json',
        json.dumps(state['map'], indent=2, ensure_ascii=False),
        '```',
        '',
        '## Builder reports',
        '',
        '### Aimed fire montage',
        '',
        '```json',
        json.dumps(state['aimed_fire_montage']['builder_report'], indent=2, ensure_ascii=False),
        '```',
        '',
        '### Weapon AnimBlueprint',
        '',
        '```json',
        json.dumps(state['weapon_abp']['builder_report'], indent=2, ensure_ascii=False),
        '```',
        '',
        '### Locomotion montage slot',
        '',
        '```json',
        json.dumps(state['character_slot']['builder_report'], indent=2, ensure_ascii=False),
        '```',
        '',
        '## Errors',
        '',
    ])
    lines.extend(['- `%s`' % item for item in state['errors']] or ['- none'])
    lines.extend(['', '## Warnings', ''])
    lines.extend(['- `%s`' % item for item in state['warnings']] or ['- none'])
    lines.append('')

    os.makedirs(REPORT_DIR, exist_ok=True)
    with open(REPORT_MD, 'w', encoding='utf-8') as handle:
        handle.write('\n'.join(lines))
    with open(REPORT_JSON, 'w', encoding='utf-8') as handle:
        json.dump(state, handle, indent=2, ensure_ascii=False)


def run():
    log('weapon blueprint=%s' % WEAPON_BP_PATH)
    log('weapon AnimBlueprint=%s slot=%s' % (WEAPON_ABP_PATH, DEFAULT_MONTAGE_SLOT))

    validate_layout()
    deps = load_dependencies()
    if state['errors']:
        return

    aimed_fire_montage = build_aimed_fire_montage(deps)
    if aimed_fire_montage is None:
        return

    weapon_abp = build_weapon_abp(deps)
    if weapon_abp is None:
        return

    ensure_character_slot()

    weapon_bp = ensure_weapon_blueprint(deps, weapon_abp, aimed_fire_montage)
    if weapon_bp is None:
        return

    place_weapon_pickup(weapon_bp)
    verify_character_anim_override()


def main():
    try:
        run()
    except Exception as exc:  # noqa: BLE001
        error('unhandled exception: %s' % exc)
        raise
    finally:
        write_markdown()
        log('report: %s' % REPORT_MD)

    failed = [item for item in state['checks'] if not item['ok']]
    if state['errors'] or failed:
        raise RuntimeError('%d error(s), %d failed check(s); see %s' % (
            len(state['errors']), len(failed), REPORT_MD))


if __name__ == '__main__':
    main()
