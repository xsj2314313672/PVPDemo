"""Build the PVP game framework content: game mode / controller blueprints, the project map and the
self-contained asset graph (no /Game/InfimaGames references).

Run headless (editor closed):
    UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript \
        -Script="<project>/Content/Python/setup_pvp_game_framework.py" -unattended -nosplash -nullrhi

Phases (idempotent, safe to re-run):
  1. record the source map / built-data SHA256 (the third-party map must never change)
  2. move BP_PVPBaseCharacter from Content/PVPDemo/Characters to Content/PVP/Characters
  3. create BP_PVPGameMode (DefaultPawnClass = BP_PVPBaseCharacter) and BP_PVPPlayerController
  4. copy the three missing dependency assets (interface / post process anim BP / spare mannequin)
  5. duplicate the demo level to /Game/PVP/Maps/L_PVP_Showcase (no _BuiltData copy)
  6. in the new map: replace the third-party character instance with a PlayerStart, re-instance the
     remaining third-party actors from project-local blueprint copies (never deletes logic actors)
  7. point WorldSettings at BP_PVPGameMode and save
  8. fixpoint remap: rewrite every remaining third-party reference in /Game/PVP and /Game/PVPDemo
     to the project-local copies, copying missing targets on demand (max 6 rounds)
"""

import hashlib
import json
import os
import re
import sys

import unreal

# --------------------------------------------------------------------------------------------------
# Constants
# --------------------------------------------------------------------------------------------------
PROJECT_DIR = unreal.Paths.project_dir().replace('\\', '/')
CONTENT_DIR = unreal.Paths.project_content_dir().replace('\\', '/')
SAVED_DIR = unreal.Paths.project_saved_dir().replace('\\', '/')

SOURCE_MAP = '/Game/InfimaGames/TacticalFPSAnimations/Weapons/AssaultRifle/Demo/Maps/L_TFA_AR_Showcase'
SOURCE_MAP_FILE = os.path.join(CONTENT_DIR, 'InfimaGames/TacticalFPSAnimations/Weapons/AssaultRifle/Demo/Maps/L_TFA_AR_Showcase.umap')
SOURCE_BUILT_DATA_FILE = os.path.join(CONTENT_DIR, 'InfimaGames/TacticalFPSAnimations/Weapons/AssaultRifle/Demo/Maps/L_TFA_AR_Showcase_BuiltData.uasset')

TARGET_MAP_DIR = '/Game/PVP/Maps'
TARGET_MAP = TARGET_MAP_DIR + '/L_PVP_Showcase'

LEGACY_CHARACTER_BP = '/Game/PVPDemo/Characters/BP_PVPBaseCharacter'
CHARACTER_BP_DIR = '/Game/PVP/Characters'
CHARACTER_BP = CHARACTER_BP_DIR + '/BP_PVPBaseCharacter'

GAME_BP_DIR = '/Game/PVP/Game'
GAMEMODE_BP = GAME_BP_DIR + '/BP_PVPGameMode'
PLAYERCONTROLLER_BP = GAME_BP_DIR + '/BP_PVPPlayerController'

ACTOR_ASSET_DIR = '/Game/PVP/Actors'
ART_ROOT = '/Game/PVPDemo'
LOGIC_ROOT = '/Game/PVP'
SCAN_PREFIXES = [ART_ROOT, LOGIC_ROOT]

MANIFEST_PATH = os.path.join(SAVED_DIR, 'MigrationReports/migration_manifest.json')
REPORT_DIR = os.path.join(SAVED_DIR, 'PVPGameFrameworkReport')
HASH_FILE = os.path.join(REPORT_DIR, 'source_hashes.json')
REMAP_REPORT = os.path.join(REPORT_DIR, 'remap_last_round.json')
REPORT_JSON = os.path.join(REPORT_DIR, 'report.json')
REPORT_MD = os.path.join(REPORT_DIR, 'report.md')

THIRD_PARTY_MARKER = b'/Game/InfimaGames/'
THIRD_PARTY_PREFIX = '/Game/InfimaGames/'

# Routing rules and copy targets are shared with reorg/verify scripts so a re-run cannot move the
# animation assets back into Actors/.
sys.path.insert(0, os.path.join(CONTENT_DIR, 'Python'))
from pvp_logic_layout import project_copy_package, subfolder_for_class  # noqa: E402

# Assets whose project copy cannot be derived from the migration manifest.
EXPLICIT_COPIES = {
    '/Game/InfimaGames/TacticalFPSAnimations/Common/Core/Animation/BPI_TFA_AnimationState':
        project_copy_package(
            '/Game/InfimaGames/TacticalFPSAnimations/Common/Core/Animation/BPI_TFA_AnimationState'),
    '/Game/InfimaGames/TacticalFPSAnimations/Common/Characters/Mannequins/Rigs/ABP_Manny_PostProcess':
        '/Game/PVP/Animation/AnimBP/ABP_PVP_Manny_PostProcess',
    '/Game/InfimaGames/TacticalFPSAnimations/_Core/Mannequins/Meshes/SKM_Quinn':
        '/Game/PVPDemo/Characters/Mannequins/Meshes/SKM_PVP_Quinn',
}

LOGIC_ASSET_CLASSES = {
    'Blueprint', 'AnimBlueprint', 'WidgetBlueprint', 'BlueprintInterface', 'DataAsset', 'World',
    'UserDefinedStruct', 'UserDefinedEnum', 'Enum', 'Struct',
    'InputAction', 'InputMappingContext', 'DataTable', 'CurveTable', 'StringTable',
}

MAX_REMAP_ROUNDS = 6

state = {
    'phases': [],
    'created': [],
    'reused': [],
    'moved': [],
    'copied': [],
    'reclassed': [],
    'removed_actors': [],
    'remap_rounds': [],
    'warnings': [],
}


def log(message):
    unreal.log('[PVP-FRAMEWORK] %s' % message)


def warn(message):
    state['warnings'].append(message)
    unreal.log_warning('[PVP-FRAMEWORK] %s' % message)


def phase(name):
    log('=== %s ===' % name)
    state['phases'].append(name)


# --------------------------------------------------------------------------------------------------
# Helpers
# --------------------------------------------------------------------------------------------------
def asset_exists(path):
    return unreal.EditorAssetLibrary.does_asset_exist(path)


def load_asset(path):
    return unreal.EditorAssetLibrary.load_asset(path)


def save_asset(path):
    if asset_exists(path):
        unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)


def native_uclass(class_name):
    """UClass object of a native project class (for BlueprintFactory / CDO defaults)."""
    cls = unreal.load_object(None, '/Script/PVPDemo.%s' % class_name)
    if cls is None:
        raise RuntimeError('native class %s not found, is the module compiled?' % class_name)
    return cls


def native_type(class_name):
    """Python wrapper type of a native project class (for isinstance checks)."""
    cls = getattr(unreal, class_name, None)
    if cls is None:
        raise RuntimeError('native type %s not exposed to Python' % class_name)
    return cls


def asset_class_name(path):
    asset = load_asset(path)
    return asset.get_class().get_name() if asset else ''


def is_redirector(path):
    """ObjectRedirector is not exposed to Python, so detect it by class name."""
    asset = load_asset(path)
    return asset is not None and asset.get_class().get_name() == 'ObjectRedirector'


def sha256_of(path):
    if not os.path.exists(path):
        return None
    digest = hashlib.sha256()
    with open(path, 'rb') as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def scan_third_party_refs():
    """Return the set of /Game/InfimaGames/... package paths still referenced on disk."""
    refs = set()
    for root in (os.path.join(CONTENT_DIR, 'PVP'), os.path.join(CONTENT_DIR, 'PVPDemo'), os.path.join(CONTENT_DIR, '__ExternalActors__')):
        if not os.path.isdir(root):
            continue
        for dirpath, _dirnames, filenames in os.walk(root):
            for filename in filenames:
                if not filename.endswith(('.uasset', '.umap')):
                    continue
                file_path = os.path.join(dirpath, filename)
                with open(file_path, 'rb') as handle:
                    data = handle.read()
                for match in re.finditer(rb'/Game/InfimaGames/[A-Za-z0-9_/]+', data):
                    refs.add(match.group(0).decode('ascii'))
    return refs


def load_manifest_mapping():
    """TFA package path -> PVP package path, taken from the migration manifest."""
    mapping = {}
    if not os.path.exists(MANIFEST_PATH):
        warn('migration manifest missing: %s' % MANIFEST_PATH)
        return mapping
    with open(MANIFEST_PATH, 'r', encoding='utf-8') as handle:
        manifest = json.load(handle)
    for entry in manifest.get('assets', []):
        source = entry.get('source')
        target = entry.get('target')
        if source and target:
            mapping[source] = target
    log('manifest mapping loaded: %d entries' % len(mapping))
    return mapping


def art_module_for(source_package):
    mapping = (
        ('/Common/Characters/', 'Characters'),
        ('/Common/Environment/', 'Environment'),
        ('/Common/VFX/', 'VFX'),
        ('/Common/Audio/', 'Audio'),
        ('/Common/Props/', 'Props'),
        ('/Common/UI/', 'UI'),
        ('/Weapons/AssaultRifle/', 'Weapons/AssaultRifle'),
        ('/Common/Core/', 'Common'),
        ('_Core/', 'Characters'),
    )
    for needle, module in mapping:
        if needle in source_package:
            return module
    return 'Common'


def logic_subfolder_for(class_name):
    """Folder under /Game/PVP for a logic UClass; routing lives in pvp_logic_layout."""
    return subfolder_for_class(class_name)


def target_path_for(source_package):
    """Derive a project-local target path for an asset that has no manifest entry yet."""
    class_name = asset_class_name(source_package)
    if class_name in LOGIC_ASSET_CLASSES or 'DataAsset' in class_name:
        return project_copy_package(source_package)
    name = source_package.rsplit('/', 1)[-1]
    new_name = name.replace('TFA', 'PVP') if 'TFA' in name else 'PVP_' + name
    return '%s/%s/%s' % (ART_ROOT, art_module_for(source_package), new_name)


def package_name_of(asset_path):
    """list_assets returns '/Game/A/B.B'; strip the object part to get the package path."""
    return asset_path.split('.')[0]


def build_copy_index():
    """asset name -> project package paths, used to find copies whose folder changed."""
    index = {}
    for root in (LOGIC_ROOT, ART_ROOT):
        for asset_path in unreal.EditorAssetLibrary.list_assets(root, recursive=True, include_folder=False):
            package = package_name_of(asset_path)
            index.setdefault(package.rsplit('/', 1)[-1], []).append(package)
    return index


def derive_mapping_from_disk(refs, index=None):
    index = index or build_copy_index()
    mapping = {}
    for ref in refs:
        name = ref.rsplit('/', 1)[-1]
        new_name = name.replace('TFA', 'PVP') if 'TFA' in name else 'PVP_' + name
        candidates = index.get(new_name)
        if candidates:
            mapping[ref] = candidates[0]
    return mapping


def normalize_asset_roots():
    """Move mis-filed copies to the correct root (art -> /Game/PVPDemo, logic -> /Game/PVP)."""
    phase('normalize asset roots')
    moved = 0

    for asset_path in unreal.EditorAssetLibrary.list_assets(ART_ROOT, recursive=True, include_folder=False):
        asset_path = package_name_of(asset_path)
        class_name = asset_class_name(asset_path)
        if class_name and (class_name in LOGIC_ASSET_CLASSES or 'DataAsset' in class_name):
            name = asset_path.rsplit('/', 1)[-1]
            destination = '%s/%s/%s' % (LOGIC_ROOT, logic_subfolder_for(class_name), name)
            if not asset_exists(destination):
                unreal.EditorAssetLibrary.make_directory(destination.rsplit('/', 1)[0])
                if unreal.EditorAssetLibrary.rename_asset(asset_path, destination):
                    moved += 1
                    log('moved logic asset %s -> %s' % (asset_path, destination))

    for asset_path in unreal.EditorAssetLibrary.list_assets(LOGIC_ROOT, recursive=True, include_folder=False):
        asset_path = package_name_of(asset_path)
        class_name = asset_class_name(asset_path)
        if not class_name or class_name in LOGIC_ASSET_CLASSES or 'DataAsset' in class_name:
            continue
        name = asset_path.rsplit('/', 1)[-1]
        destination = '%s/%s/%s' % (ART_ROOT, 'Common', name)
        if not asset_exists(destination):
            unreal.EditorAssetLibrary.make_directory(destination.rsplit('/', 1)[0])
            if unreal.EditorAssetLibrary.rename_asset(asset_path, destination):
                moved += 1
                log('moved art asset %s -> %s' % (asset_path, destination))

    log('asset roots normalized, moved=%d' % moved)


def copy_asset_if_missing(source_package, target_package=None):
    """Duplicate a third-party asset into the project. Returns the target path or None."""
    target_package = target_package or target_path_for(source_package)
    if asset_exists(target_package):
        return target_package
    if not asset_exists(source_package):
        warn('source asset missing, cannot copy: %s' % source_package)
        return None
    if not unreal.EditorAssetLibrary.duplicate_asset(source_package, target_package):
        warn('duplicate_asset failed: %s -> %s' % (source_package, target_package))
        return None
    save_asset(target_package)
    state['copied'].append({'from': source_package, 'to': target_package})
    log('copied %s -> %s' % (source_package, target_package))
    return target_package


def compile_blueprint_at(path):
    asset = load_asset(path)
    if asset and isinstance(asset, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
        save_asset(path)
    return asset


def create_blueprint(package_dir, name, parent_class):
    package_path = '%s/%s' % (package_dir, name)
    if asset_exists(package_path):
        state['reused'].append(package_path)
        log('reusing blueprint %s' % package_path)
        compile_blueprint_at(package_path)
        return load_asset(package_path)

    unreal.EditorAssetLibrary.make_directory(package_dir)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', parent_class)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, package_dir, unreal.Blueprint, factory)
    if blueprint is None:
        raise RuntimeError('failed to create blueprint %s' % package_path)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save_asset(package_path)
    state['created'].append(package_path)
    log('created blueprint %s (parent=%s)' % (package_path, parent_class.get_name()))
    return blueprint


def set_class_default(blueprint, property_name, value):
    """Set a property on the blueprint CDO and recompile so the value is serialized."""
    class_default = unreal.get_default_object(blueprint.generated_class())
    class_default.set_editor_property(property_name, value)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)


def get_actor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def class_package_path(actor):
    return str(actor.get_class().get_path_name()).split('.')[0]


def find_world_settings(world, actor_subsystem):
    """WorldSettings is not always reported by get_all_level_actors(); try the gameplay accessor too."""
    candidates = []
    try:
        candidates.extend(actor_subsystem.get_all_level_actors())
    except Exception:  # noqa: BLE001
        pass
    if hasattr(unreal, 'EditorLevelLibrary'):
        try:
            candidates.extend(unreal.EditorLevelLibrary.get_all_level_actors())
        except Exception:  # noqa: BLE001
            pass

    for actor in candidates:
        if isinstance(actor, unreal.WorldSettings):
            return actor

    try:
        return unreal.GameplayStatics.get_actor_of_class(world, unreal.WorldSettings)
    except Exception:  # noqa: BLE001
        return None


# --------------------------------------------------------------------------------------------------
# Phase 1: source map fingerprints
# --------------------------------------------------------------------------------------------------
def record_source_hashes():
    phase('record source map hashes')
    os.makedirs(REPORT_DIR, exist_ok=True)
    current = {
        'map': {'file': SOURCE_MAP_FILE, 'sha256': sha256_of(SOURCE_MAP_FILE)},
        'built_data': {'file': SOURCE_BUILT_DATA_FILE, 'sha256': sha256_of(SOURCE_BUILT_DATA_FILE)},
    }
    if os.path.exists(HASH_FILE):
        with open(HASH_FILE, 'r', encoding='utf-8') as handle:
            baseline = json.load(handle)
        log('baseline hashes already recorded')
    else:
        baseline = current
        with open(HASH_FILE, 'w', encoding='utf-8') as handle:
            json.dump(baseline, handle, indent=2)
        log('baseline hashes written: %s' % HASH_FILE)

    for key in ('map', 'built_data'):
        if baseline.get(key, {}).get('sha256') != current[key]['sha256']:
            raise RuntimeError('source %s changed since the baseline was recorded (%s)' % (key, current[key]['file']))
    return baseline


# --------------------------------------------------------------------------------------------------
# Phase 2: migrate the character blueprint into the logic root
# --------------------------------------------------------------------------------------------------
def migrate_character_blueprint():
    phase('migrate BP_PVPBaseCharacter into /Game/PVP/Characters')
    unreal.EditorAssetLibrary.make_directory(CHARACTER_BP_DIR)

    if asset_exists(LEGACY_CHARACTER_BP):
        if is_redirector(LEGACY_CHARACTER_BP):
            unreal.EditorAssetLibrary.delete_asset(LEGACY_CHARACTER_BP)
            log('deleted stale redirector %s' % LEGACY_CHARACTER_BP)
        else:
            if asset_exists(CHARACTER_BP):
                raise RuntimeError('both %s and %s exist, resolve manually' % (LEGACY_CHARACTER_BP, CHARACTER_BP))
            if not unreal.EditorAssetLibrary.rename_asset(LEGACY_CHARACTER_BP, CHARACTER_BP):
                raise RuntimeError('failed to move %s to %s' % (LEGACY_CHARACTER_BP, CHARACTER_BP))
            state['moved'].append({'from': LEGACY_CHARACTER_BP, 'to': CHARACTER_BP})
            log('moved %s -> %s' % (LEGACY_CHARACTER_BP, CHARACTER_BP))

    if not asset_exists(CHARACTER_BP):
        raise RuntimeError('%s missing, it must exist before this task' % CHARACTER_BP)

    if asset_exists(LEGACY_CHARACTER_BP) and is_redirector(LEGACY_CHARACTER_BP):
        unreal.EditorAssetLibrary.delete_asset(LEGACY_CHARACTER_BP)
        log('cleaned redirector left by the move')

    blueprint = compile_blueprint_at(CHARACTER_BP)
    # UBlueprint::ParentClass is not exposed to Python; validate through the CDO class hierarchy instead.
    class_default = unreal.get_default_object(blueprint.generated_class())
    if not isinstance(class_default, native_type('PVPBaseCharacter')):
        raise RuntimeError('%s does not derive from APVPBaseCharacter' % CHARACTER_BP)
    log('character blueprint ok: %s (parent=%s)' % (CHARACTER_BP, class_default.get_class().get_name()))
    return blueprint


# --------------------------------------------------------------------------------------------------
# Phase 3: game mode / player controller blueprints
# --------------------------------------------------------------------------------------------------
def create_framework_blueprints(character_blueprint):
    phase('create BP_PVPGameMode and BP_PVPPlayerController')
    game_mode_bp = create_blueprint(GAME_BP_DIR, 'BP_PVPGameMode', native_uclass('PVPGameMode'))
    controller_bp = create_blueprint(GAME_BP_DIR, 'BP_PVPPlayerController', native_uclass('PVPPlayerController'))

    set_class_default(game_mode_bp, 'default_pawn_class', character_blueprint.generated_class())
    set_class_default(game_mode_bp, 'player_controller_class', controller_bp.generated_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(game_mode_bp)
    save_asset(GAMEMODE_BP)

    defaults = unreal.get_default_object(game_mode_bp.generated_class())
    pawn_class = defaults.get_editor_property('default_pawn_class')
    controller_class = defaults.get_editor_property('player_controller_class')
    log('BP_PVPGameMode defaults: pawn=%s controller=%s' % (pawn_class.get_name(), controller_class.get_name()))
    return game_mode_bp, controller_bp


# --------------------------------------------------------------------------------------------------
# Phase 4: copy the dependency assets that have no manifest entry
# --------------------------------------------------------------------------------------------------
def copy_required_assets():
    phase('copy missing dependency assets')
    for source, target in EXPLICIT_COPIES.items():
        copy_asset_if_missing(source, target)


# --------------------------------------------------------------------------------------------------
# Phase 5: duplicate the level
# --------------------------------------------------------------------------------------------------
def duplicate_level():
    phase('duplicate the demo level')
    unreal.EditorAssetLibrary.make_directory(TARGET_MAP_DIR)
    if asset_exists(TARGET_MAP):
        state['reused'].append(TARGET_MAP)
        log('reusing existing map %s' % TARGET_MAP)
        return
    if not asset_exists(SOURCE_MAP):
        raise RuntimeError('source map missing: %s' % SOURCE_MAP)
    if not unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP, TARGET_MAP):
        raise RuntimeError('failed to duplicate %s' % SOURCE_MAP)
    save_asset(TARGET_MAP)
    state['created'].append(TARGET_MAP)
    log('duplicated %s -> %s' % (SOURCE_MAP, TARGET_MAP))


# --------------------------------------------------------------------------------------------------
# Phase 6: rework the actors of the new level
# --------------------------------------------------------------------------------------------------
def copy_component_state(old_actor, new_actor):
    """Best effort copy of per-instance component overrides onto the re-instanced actor."""
    new_components = {}
    for component in new_actor.get_components_by_class(unreal.ActorComponent):
        new_components[component.get_name()] = component

    for old_component in old_actor.get_components_by_class(unreal.ActorComponent):
        new_component = new_components.get(old_component.get_name())
        if new_component is None:
            continue
        if isinstance(old_component, unreal.SceneComponent) and isinstance(new_component, unreal.SceneComponent):
            for prop in ('relative_location', 'relative_rotation', 'relative_scale3d'):
                try:
                    new_component.set_editor_property(prop, old_component.get_editor_property(prop))
                except Exception:  # noqa: BLE001
                    pass
        for prop in ('static_mesh', 'skeletal_mesh_asset'):
            try:
                value = old_component.get_editor_property(prop)
                if value is not None:
                    new_component.set_editor_property(prop, value)
            except Exception:  # noqa: BLE001
                pass


def reinstance_actor(actor, new_class, actor_subsystem):
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    label = actor.get_actor_label()

    new_actor = actor_subsystem.spawn_actor_from_class(new_class, location, rotation)
    if new_actor is None:
        warn('failed to spawn %s at %s' % (new_class.get_name(), location))
        return None

    try:
        new_actor.set_actor_label(label)
        new_actor.set_actor_scale3d(actor.get_actor_scale3d())
        new_actor.set_editor_property('tags', actor.get_editor_property('tags'))
    except Exception:  # noqa: BLE001
        pass

    copy_component_state(actor, new_actor)
    actor_subsystem.destroy_actor(actor)
    return new_actor


def rework_map(game_mode_bp):
    phase('rework the new map actors')
    world = unreal.EditorLoadingAndSavingUtils.load_map(TARGET_MAP)
    if world is None:
        raise RuntimeError('failed to load %s' % TARGET_MAP)

    actor_subsystem = get_actor_subsystem()
    actors = list(actor_subsystem.get_all_level_actors())
    log('map actors: %d' % len(actors))

    player_starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
    third_party_actors = [a for a in actors if class_package_path(a).startswith(THIRD_PARTY_PREFIX)]
    character_actors = [a for a in third_party_actors if isinstance(a, unreal.Pawn)]

    # The third-party character instance is the only actor removed: the player is spawned by the game
    # mode instead of relying on the demo character's auto possess.
    for actor in character_actors:
        location = actor.get_actor_location()
        rotation = actor.get_actor_rotation()
        if not player_starts:
            player_start = actor_subsystem.spawn_actor_from_class(unreal.PlayerStart, location, rotation)
            if player_start:
                player_start.set_actor_label('PlayerStart_PVP')
                player_starts.append(player_start)
                log('spawned PlayerStart at %s' % location)
        actor_subsystem.destroy_actor(actor)
        state['removed_actors'].append({'class': actor.get_class().get_name(), 'location': str(location), 'reason': 'third-party character instance replaced by PlayerStart'})
        log('removed third-party character instance %s' % actor.get_class().get_name())

    # Every other third-party actor stays in the level, but is re-instanced from the project copy so the
    # map no longer references the third-party package. No logic actor is deleted from the scene.
    mapping = load_manifest_mapping()
    mapping.update(EXPLICIT_COPIES)

    remaining = [a for a in third_party_actors if not isinstance(a, unreal.Pawn)]
    for actor in remaining:
        source_package = class_package_path(actor)
        target_package = mapping.get(source_package)
        if not target_package or not asset_exists(target_package):
            target_package = copy_asset_if_missing(source_package)
            if not target_package:
                warn('cannot localise %s, left untouched' % source_package)
                continue
            mapping[source_package] = target_package
        compile_blueprint_at(target_package)
        new_class = unreal.EditorAssetLibrary.load_blueprint_class(target_package)
        if new_class is None:
            warn('no generated class for %s, actor left untouched' % target_package)
            continue
        new_actor = reinstance_actor(actor, new_class, actor_subsystem)
        if new_actor:
            state['reclassed'].append({
                'from': source_package,
                'to': target_package,
                'label': new_actor.get_actor_label(),
            })
            log('re-instanced %s -> %s' % (source_package.rsplit('/', 1)[-1], target_package.rsplit('/', 1)[-1]))

    # Game mode override lives in the world settings.
    world_settings = find_world_settings(world, actor_subsystem)
    if world_settings is None:
        raise RuntimeError('WorldSettings not found in %s' % TARGET_MAP)
    world_settings.set_editor_property('default_game_mode', game_mode_bp.generated_class())
    log('WorldSettings.default_game_mode = %s' % game_mode_bp.generated_class().get_name())

    if not unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET_MAP):
        raise RuntimeError('failed to save %s' % TARGET_MAP)
    log('map saved: %s' % TARGET_MAP)


# --------------------------------------------------------------------------------------------------
# Phase 7: zero third-party references (fixpoint)
# --------------------------------------------------------------------------------------------------
def remap_once(mapping_pairs, report_path):
    froms = [pair[0] for pair in mapping_pairs]
    tos = [pair[1] for pair in mapping_pairs]
    return unreal.PVPAssetRemapLibrary.remap_asset_references(froms, tos, SCAN_PREFIXES, report_path)


def resolve_missing_refs(refs, mapping):
    unresolved = []
    for ref in sorted(refs):
        target = mapping.get(ref)
        if target and asset_exists(target):
            continue
        if ref in EXPLICIT_COPIES:
            target = copy_asset_if_missing(ref, EXPLICIT_COPIES[ref])
        else:
            target = copy_asset_if_missing(ref)
        if target:
            mapping[ref] = target
        else:
            unresolved.append(ref)
    return unresolved


def scan_third_party_host_packages():
    """Package paths of project assets whose serialized data still contains the third-party prefix."""
    hosts = []
    roots = (
        (os.path.join(CONTENT_DIR, 'PVP'), '/Game/PVP'),
        (os.path.join(CONTENT_DIR, 'PVPDemo'), '/Game/PVPDemo'),
    )
    for root_dir, url_root in roots:
        if not os.path.isdir(root_dir):
            continue
        for dirpath, _dirnames, filenames in os.walk(root_dir):
            for filename in filenames:
                if not filename.endswith(('.uasset', '.umap')):
                    continue
                file_path = os.path.join(dirpath, filename)
                with open(file_path, 'rb') as handle:
                    if THIRD_PARTY_MARKER not in handle.read():
                        continue
                relative = os.path.relpath(file_path, root_dir).replace('\\', '/')
                hosts.append('%s/%s' % (url_root, relative.rsplit('.', 1)[0]))
    return hosts


def fix_soft_references(mapping):
    """ForceReplaceReferences only rewrites hard object references; fix soft paths (preview meshes etc.)."""
    fixed = 0
    for package in scan_third_party_host_packages():
        asset = load_asset(package)
        if asset is None:
            continue
        touched = False
        for attribute in dir(asset):
            if attribute.startswith('_'):
                continue
            try:
                value = asset.get_editor_property(attribute)
            except Exception:  # noqa: BLE001
                continue
            if isinstance(value, unreal.SoftObjectPath):
                text = str(value)
                if THIRD_PARTY_PREFIX not in text:
                    continue
                target = mapping.get(text.split('.')[0])
                if target and asset_exists(target):
                    try:
                        asset.set_editor_property(attribute, unreal.SoftObjectPath(target))
                        touched = True
                        fixed += 1
                        log('soft ref %s.%s -> %s' % (package, attribute, target))
                    except Exception:  # noqa: BLE001
                        warn('cannot rewrite soft ref %s.%s' % (package, attribute))
            elif isinstance(value, unreal.Object):
                text = str(value.get_path_name())
                if THIRD_PARTY_PREFIX not in text:
                    continue
                target = mapping.get(text.split('.')[0])
                if target and asset_exists(target):
                    replacement = load_asset(target)
                    if replacement is None:
                        continue
                    try:
                        asset.set_editor_property(attribute, replacement)
                        touched = True
                        fixed += 1
                        log('hard ref %s.%s -> %s' % (package, attribute, target))
                    except Exception:  # noqa: BLE001
                        warn('cannot rewrite hard ref %s.%s' % (package, attribute))
        if touched:
            save_asset(package)
    log('soft references rewritten: %d' % fixed)
    return fixed


def fix_map_references(mapping):
    """Placed actors keep per-instance mesh overrides; rewrite those directly on the map components."""
    world = unreal.EditorLoadingAndSavingUtils.load_map(TARGET_MAP)
    if world is None:
        return 0

    fixed = 0
    subsystem = get_actor_subsystem()
    for actor in subsystem.get_all_level_actors():
        for component in actor.get_components_by_class(unreal.ActorComponent):
            for prop in ('skeletal_mesh_asset', 'static_mesh', 'skeleton', 'anim_class'):
                try:
                    value = component.get_editor_property(prop)
                except Exception:  # noqa: BLE001
                    continue
                if not isinstance(value, unreal.Object):
                    continue
                text = str(value.get_path_name())
                if THIRD_PARTY_PREFIX not in text:
                    continue
                target = mapping.get(text.split('.')[0])
                if not (target and asset_exists(target)):
                    continue
                try:
                    component.set_editor_property(prop, load_asset(target))
                    fixed += 1
                    log('map ref %s.%s -> %s' % (component.get_name(), prop, target))
                except Exception:  # noqa: BLE001
                    warn('cannot rewrite map ref %s.%s' % (component.get_name(), prop))

    if fixed:
        unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET_MAP)
    return fixed


def resave_third_party_hosts():
    """Persist host packages so stale import/name table entries (already unused) are dropped on save."""
    saved = 0
    for package in scan_third_party_host_packages():
        if package == TARGET_MAP:
            world = unreal.EditorLoadingAndSavingUtils.load_map(TARGET_MAP)
            if world and unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET_MAP):
                saved += 1
            continue
        try:
            if unreal.EditorAssetLibrary.save_asset(package, only_if_is_dirty=False):
                saved += 1
        except Exception:  # noqa: BLE001
            warn('cannot resave %s' % package)
    log('resaved third-party host packages: %d' % saved)
    return saved


def split_dangling(refs):
    """Split refs into live targets (package exists, so it is a real dependency) and dangling ones."""
    live = set()
    dangling = set()
    for ref in refs:
        if asset_exists(ref):
            live.add(ref)
        else:
            dangling.add(ref)
    return live, dangling


def clear_third_party_refs():
    phase('zero third-party references (fixpoint)')
    mapping = load_manifest_mapping()
    mapping.update(EXPLICIT_COPIES)
    state['dangling_refs'] = []

    for round_index in range(1, MAX_REMAP_ROUNDS + 1):
        refs = scan_third_party_refs()
        live, dangling = split_dangling(refs)
        state['dangling_refs'] = sorted(dangling)
        log('round %d: %d references (%d live, %d dangling)' % (round_index, len(refs), len(live), len(dangling)))
        if not live:
            state['remap_rounds'].append({'round': round_index, 'refs': len(refs), 'live': 0,
                                          'dangling': len(dangling), 'remapped': 0})
            return

        unresolved = resolve_missing_refs(live, mapping)
        normalize_asset_roots()
        mapping = load_manifest_mapping()
        mapping.update(EXPLICIT_COPIES)
        mapping.update(derive_mapping_from_disk(live))
        pairs = [(ref, mapping[ref]) for ref in live if ref in mapping and asset_exists(mapping[ref])]
        ok = remap_once(pairs, REMAP_REPORT) if pairs else False
        fix_soft_references(mapping)
        fix_map_references(mapping)
        after_live, after_dangling = split_dangling(scan_third_party_refs())
        if after_live and len(after_live) == len(live):
            # No reference changed in memory: the remaining strings are stale package table entries,
            # so re-save the hosts to rewrite them away.
            resave_third_party_hosts()
            after_live, after_dangling = split_dangling(scan_third_party_refs())
        state['remap_rounds'].append({
            'round': round_index,
            'refs': len(refs),
            'live': len(live),
            'dangling': len(dangling),
            'remapped': len(pairs),
            'unresolved': len(unresolved),
            'live_after': len(after_live),
            'dangling_after': len(after_dangling),
            'ok': bool(ok),
        })
        log('round %d: remapped=%d unresolved=%d live_after=%d' % (round_index, len(pairs), len(unresolved), len(after_live)))
        if not after_live:
            return

    remaining = scan_third_party_refs()
    live_remaining, _dangling = split_dangling(remaining)
    if not live_remaining:
        return
    raise RuntimeError('%d live third-party references left after %d rounds: %s' % (
        len(live_remaining), MAX_REMAP_ROUNDS, sorted(live_remaining)[:10]))


# --------------------------------------------------------------------------------------------------
# Report
# --------------------------------------------------------------------------------------------------
def write_report():
    os.makedirs(REPORT_DIR, exist_ok=True)
    remaining = scan_third_party_refs()
    live, dangling = split_dangling(remaining)
    state['refs_remaining'] = sorted(remaining)
    state['refs_remaining_live'] = sorted(live)
    state['dangling_refs'] = sorted(dangling)
    with open(REPORT_JSON, 'w', encoding='utf-8') as handle:
        json.dump(state, handle, indent=2, default=str)

    lines = ['# PVP game framework setup report', '']
    lines.append('- third-party references remaining (live): `%d`' % len(live))
    lines.append('- third-party references remaining (dangling, target does not exist): `%d`' % len(dangling))
    lines.append('- moved assets: `%d`' % len(state['moved']))
    lines.append('- created assets: `%d`' % len(state['created']))
    lines.append('- reused assets: `%d`' % len(state['reused']))
    lines.append('- copied third-party assets: `%d`' % len(state['copied']))
    lines.append('- re-instanced actors: `%d`' % len(state['reclassed']))
    lines.append('- removed actors: `%d`' % len(state['removed_actors']))
    if state['warnings']:
        lines.append('')
        lines.append('## Warnings')
        lines.append('')
        for message in state['warnings']:
            lines.append('- %s' % message)
    with open(REPORT_MD, 'w', encoding='utf-8') as handle:
        handle.write('\n'.join(lines) + '\n')
    log('report written: %s' % REPORT_MD)


def main():
    os.makedirs(REPORT_DIR, exist_ok=True)
    record_source_hashes()
    character_blueprint = migrate_character_blueprint()
    game_mode_bp, _controller_bp = create_framework_blueprints(character_blueprint)
    copy_required_assets()
    duplicate_level()
    rework_map(game_mode_bp)
    normalize_asset_roots()
    clear_third_party_refs()
    write_report()
    log('SETUP DONE')


if __name__ == '__main__':
    try:
        main()
    except Exception as error:  # noqa: BLE001
        unreal.log_error('[PVP-FRAMEWORK] FAILED: %s' % error)
        try:
            state['fatal'] = str(error)
            write_report()
        except Exception:  # noqa: BLE001
            pass
        raise
