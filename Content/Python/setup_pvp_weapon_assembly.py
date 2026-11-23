"""Assemble the assault rifle attachments inside BP_PVP_Weapon_AR.

Pipeline (idempotent):
  1. validate the classified attachment meshes (moved by reorg_pvp_weapon_attachments.py);
  2. create / update /Game/PVP/Data/DA_PVP_WeaponAttachmentCatalog with one definition per slot
     (parent slot, socket, component name, every interchangeable mesh) and the Basic / Tactical
     loadout presets;
  3. call the editor C++ builder (UPVPWeaponAssemblyBuilder) which creates / updates the
     ATT_<Slot> StaticMeshComponents on the weapon blueprint, writes the socket, compiles and saves;
  4. point BP_PVP_Weapon_AR.AttachmentCatalog at the data asset, compile and save;
  5. read the assembled components back (name / parent / socket / mesh / tags) and assert them;
  6. write Saved/PVPAttachmentReport/report.{json,md}.

The socket itself cannot be written from Python: USceneComponent::AttachSocketName and
USCS_Node::AttachToName have no CPF_Edit, so the C++ builder owns the SCS writes.

Loadout selection (default Basic):
    set PVP_WEAPON_LOADOUT=Tactical
    or append --loadout=tactical to the script arguments.

Run headless (editor closed, PVPDemo / PVPDemoEditor compiled):
    UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript \
        -Script="<project>/Content/Python/setup_pvp_weapon_assembly.py" -unattended -nosplash -nullrhi
"""

import json
import os
import re
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__)) if '__file__' in globals() else \
    unreal.Paths.project_content_dir().replace('\\', '/') + 'Python'
PROJECT_DIR = os.path.dirname(os.path.dirname(SCRIPT_DIR))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from reorg_pvp_weapon_attachments import (  # noqa: E402
    ATTACHMENT_GROUPS,
    final_path,
)

CATALOG_NAME = 'DA_PVP_WeaponAttachmentCatalog'
CATALOG_DIR = '/Game/PVP/Data'
CATALOG_PATH = '%s/%s' % (CATALOG_DIR, CATALOG_NAME)
WEAPON_BP_PATH = '/Game/PVP/Actors/BP_PVP_Weapon_AR'

REPORT_DIR = os.path.join(PROJECT_DIR, 'Saved', 'PVPAttachmentReport').replace('\\', '/')
REPORT_JSON = REPORT_DIR + '/report.json'
REPORT_MD = REPORT_DIR + '/report.md'
ASSEMBLY_JSON = REPORT_DIR + '/assembly_builder.json'
READBACK_JSON = REPORT_DIR + '/readback_builder.json'

# Slot order matters: a parent slot must be written before its children.
SLOT_ORDER = ('Handguard', 'FrontSight', 'RearSight', 'Optic', 'ForeGrip', 'Muzzle', 'Laser', 'Magazine')

SLOTS = {
    'Handguard': {
        'parent': 'None',
        'socket': 'SOCKET_Handguard',
        'component': 'ATT_Handguard',
        'options': (
            ('SM_PVP_AR_Handguard_Default', '护木 标准'),
            ('SM_PVP_AR_Handguard_Extended', '护木 延长'),
            ('SM_PVP_AR_Handguard_Short', '护木 短'),
        ),
        'default': 'SM_PVP_AR_Handguard_Default',
    },
    'FrontSight': {
        'parent': 'Handguard',
        'socket': 'SOCKET_Sight_Front',
        'component': 'ATT_FrontSight',
        'options': (
            ('SM_PVP_AR_ATT_Sight_Front', '前准星 立起'),
            ('SM_PVP_AR_ATT_Sight_Front_Folded', '前准星 折叠'),
        ),
        'default': 'SM_PVP_AR_ATT_Sight_Front',
    },
    'RearSight': {
        'parent': 'None',
        'socket': 'SOCKET_Sight_Rear',
        'component': 'ATT_RearSight',
        'options': (
            ('SM_PVP_AR_ATT_Sight_Rear', '后照门 立起'),
            ('SM_PVP_AR_ATT_Sight_Rear_Folded', '后照门 折叠'),
        ),
        'default': 'SM_PVP_AR_ATT_Sight_Rear',
    },
    'Optic': {
        'parent': 'None',
        'socket': 'SOCKET_Scope',
        'component': 'ATT_Optic',
        'options': (
            ('SM_PVP_AR_ATT_Scope', '光学瞄具'),
        ),
        'default': 'SM_PVP_AR_ATT_Scope',
    },
    'ForeGrip': {
        'parent': 'Handguard',
        'socket': 'SOCKET_Grip',
        'component': 'ATT_ForeGrip',
        'options': (
            ('SM_PVP_AR_ATT_Grip_Angled', '斜角握把'),
            ('SM_PVP_AR_ATT_Grip_Vertical', '垂直握把'),
        ),
        'default': 'SM_PVP_AR_ATT_Grip_Angled',
    },
    'Muzzle': {
        'parent': 'Handguard',
        'socket': 'SOCKET_Muzzle',
        'component': 'ATT_Muzzle',
        'options': (
            ('SM_PVP_AR_ATT_Silencer', '消音器'),
        ),
        'default': 'SM_PVP_AR_ATT_Silencer',
    },
    'Laser': {
        'parent': 'Handguard',
        'socket': 'SOCKET_Laser',
        'component': 'ATT_Laser',
        'options': (
            ('SM_PVP_AR_ATT_Laser', '激光指示器'),
        ),
        'default': 'SM_PVP_AR_ATT_Laser',
    },
    'Magazine': {
        'parent': 'None',
        'socket': 'SOCKET_Magazine',
        'component': 'ATT_Magazine',
        'options': (
            ('SM_PVP_AR_Magazine_Full', '满弹匣'),
            ('SM_PVP_AR_Magazine_Empty', '空弹匣'),
            ('SM_PVP_AR_Magazine_Transparent_Full', '透明满弹匣'),
            ('SM_PVP_AR_Magazine_Transparent_Empty', '透明空弹匣'),
        ),
        'default': 'SM_PVP_AR_Magazine_Full',
    },
}

LOADOUTS = {
    'Basic': {
        'display': '基础型',
        'description': '出厂裸枪：标准护木 + 立起前后机瞄 + 满弹匣',
        'entries': {
            'Handguard': 'SM_PVP_AR_Handguard_Default',
            'FrontSight': 'SM_PVP_AR_ATT_Sight_Front',
            'RearSight': 'SM_PVP_AR_ATT_Sight_Rear',
            'Magazine': 'SM_PVP_AR_Magazine_Full',
        },
    },
    'Tactical': {
        'display': '战术型',
        'description': '延长护木 + 瞄具 + 折叠机瞄 + 垂直握把 + 激光 + 消音器',
        'entries': {
            'Handguard': 'SM_PVP_AR_Handguard_Extended',
            'FrontSight': 'SM_PVP_AR_ATT_Sight_Front_Folded',
            'RearSight': 'SM_PVP_AR_ATT_Sight_Rear_Folded',
            'Optic': 'SM_PVP_AR_ATT_Scope',
            'ForeGrip': 'SM_PVP_AR_ATT_Grip_Vertical',
            'Muzzle': 'SM_PVP_AR_ATT_Silencer',
            'Laser': 'SM_PVP_AR_ATT_Laser',
            'Magazine': 'SM_PVP_AR_Magazine_Full',
        },
    },
}

DEFAULT_LOADOUT = 'Basic'

state = {
    'loadout': DEFAULT_LOADOUT,
    'catalog': {'path': CATALOG_PATH, 'created': False, 'slots': 0, 'loadouts': 0},
    'assembly': {},
    'readback': {},
    'weapon_bp': {},
    'checks': [],
    'errors': [],
    'warnings': [],
}


def log(message):
    unreal.log('[PVPWeaponAssembly] %s' % message)


def warn(message):
    state['warnings'].append(message)
    unreal.log_warning('[PVPWeaponAssembly] %s' % message)


def error(message):
    state['errors'].append(message)
    unreal.log_error('[PVPWeaponAssembly] %s' % message)


def check(label, ok, detail=''):
    state['checks'].append({'label': label, 'ok': bool(ok), 'detail': str(detail)})
    if ok:
        log('PASS %s%s' % (label, (': %s' % detail) if detail else ''))
    else:
        error('FAIL %s%s' % (label, (': %s' % detail) if detail else ''))
    return bool(ok)


def set_property(obj, value, *names):
    """Set the first reflected property that exists (tolerates the Python b-prefix stripping)."""
    last_exc = None
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return name
        except Exception as exc:  # noqa: BLE001
            last_exc = exc
    raise RuntimeError('cannot set any of %s on %s: %s' % (list(names), type(obj).__name__, last_exc))


def enum_member_name(camel):
    """Handguard -> HANDGUARD, FrontSight -> FRONT_SIGHT (unreal Python enum convention)."""
    return re.sub(r'(?<!^)(?=[A-Z])', '_', camel).upper()


def slot_enum_type():
    """Resolve the reflected UENUM regardless of the E_ prefix stripping rule."""
    for name in ('PVPWeaponAttachmentSlot', 'E_PVPWeaponAttachmentSlot'):
        enum_type = getattr(unreal, name, None)
        if enum_type is not None:
            return enum_type
    for name in dir(unreal):
        if name.endswith('PVPWeaponAttachmentSlot'):
            return getattr(unreal, name)
    raise RuntimeError('PVPWeaponAttachmentSlot is not reflected; compile PVPDemo first')


def slot_enum(camel):
    enum_type = slot_enum_type()
    for candidate in (enum_member_name(camel), camel.upper(), camel):
        member = getattr(enum_type, candidate, None)
        if member is not None:
            return member
    raise RuntimeError('unknown slot enum member: %s' % camel)


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


def mesh_path(asset_name):
    return final_path(asset_name)


def load_slot_mesh(slot_name, asset_name):
    path = mesh_path(asset_name)
    mesh = load_asset(path, unreal.StaticMesh, '%s mesh' % slot_name)
    if mesh is not None and not path.endswith('/%s' % asset_name):
        error('unexpected mesh path for %s: %s' % (slot_name, path))
    return mesh


def make_option(mesh, display_name, is_default):
    option = unreal.PVPWeaponAttachmentOption()
    set_property(option, mesh, 'mesh')
    set_property(option, display_name, 'display_name')
    set_property(option, bool(is_default), 'is_default_option', 'b_is_default_option')
    return option


def make_slot_definition(slot_name):
    spec = SLOTS[slot_name]
    definition = unreal.PVPWeaponAttachmentSlotDefinition()
    set_property(definition, slot_enum(slot_name), 'slot')
    set_property(definition, slot_enum(spec['parent']), 'parent_slot')
    set_property(definition, unreal.Name(spec['socket']), 'socket_name')
    set_property(definition, unreal.Name(spec['component']), 'component_name')

    options = []
    for asset_name, display_name in spec['options']:
        mesh = load_slot_mesh(slot_name, asset_name)
        if mesh is None:
            continue
        options.append(make_option(mesh, display_name, asset_name == spec['default']))
    set_property(definition, options, 'options')
    return definition


def make_loadout(loadout_name):
    loadout_spec = LOADOUTS[loadout_name]
    loadout = unreal.PVPWeaponAttachmentLoadout()
    set_property(loadout, unreal.Name(loadout_name), 'loadout_name')
    set_property(loadout, loadout_spec['display'], 'display_name')

    entries = []
    for slot_name in SLOT_ORDER:
        asset_name = loadout_spec['entries'].get(slot_name)
        if not asset_name:
            continue
        mesh = load_slot_mesh(slot_name, asset_name)
        if mesh is None:
            continue
        entry = unreal.PVPWeaponAttachmentLoadoutEntry()
        set_property(entry, slot_enum(slot_name), 'slot')
        set_property(entry, mesh, 'mesh')
        entries.append(entry)
    set_property(loadout, entries, 'entries')
    return loadout


def ensure_catalog():
    catalog_class = getattr(unreal, 'PVPWeaponAttachmentCatalog', None)
    if catalog_class is None:
        error('PVPWeaponAttachmentCatalog is not reflected; compile PVPDemo first')
        return None

    if unreal.EditorAssetLibrary.does_asset_exist(CATALOG_PATH):
        catalog = load_asset(CATALOG_PATH, None, 'attachment catalog')
        if catalog is None:
            return None
        if not isinstance(catalog, catalog_class):
            error('%s is not a PVPWeaponAttachmentCatalog' % CATALOG_PATH)
            return None
        log('reusing %s' % CATALOG_PATH)
    else:
        if not unreal.EditorAssetLibrary.make_directory(CATALOG_DIR):
            warn('make_directory returned False (already exists?): %s' % CATALOG_DIR)
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', catalog_class)
        catalog = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            CATALOG_NAME, CATALOG_DIR, catalog_class, factory)
        if catalog is None:
            error('failed to create %s' % CATALOG_PATH)
            return None
        state['catalog']['created'] = True
        log('created %s' % CATALOG_PATH)

    slots = []
    for slot_name in SLOT_ORDER:
        definition = make_slot_definition(slot_name)
        if definition.get_editor_property('options'):
            slots.append(definition)
        else:
            error('slot %s has no loadable option mesh' % slot_name)

    loadouts = [make_loadout(name) for name in ('Basic', 'Tactical')]
    set_property(catalog, slots, 'slots')
    set_property(catalog, loadouts, 'loadouts')

    if not unreal.EditorAssetLibrary.save_loaded_asset(catalog, only_if_is_dirty=False):
        error('failed to save %s' % CATALOG_PATH)

    state['catalog']['slots'] = len(catalog.get_editor_property('slots'))
    state['catalog']['loadouts'] = len(catalog.get_editor_property('loadouts'))
    check('catalog.slot_count', state['catalog']['slots'] == len(SLOT_ORDER), state['catalog']['slots'])
    check('catalog.loadout_count', state['catalog']['loadouts'] == 2, state['catalog']['loadouts'])
    return catalog


def build_specs(loadout_name):
    specs = []
    entries = LOADOUTS[loadout_name]['entries']
    for slot_name in SLOT_ORDER:
        asset_name = entries.get(slot_name)
        if not asset_name:
            continue
        mesh = load_slot_mesh(slot_name, asset_name)
        if mesh is None:
            continue

        slot_spec = SLOTS[slot_name]
        spec = unreal.PVPWeaponAttachmentSpec()
        set_property(spec, slot_enum(slot_name), 'slot')
        set_property(spec, slot_enum(slot_spec['parent']), 'parent_slot')
        set_property(spec, unreal.Name(slot_spec['socket']), 'socket_name')
        set_property(spec, mesh, 'mesh')
        set_property(spec, unreal.Name(slot_spec['component']), 'component_name')
        set_property(spec, True, 'enabled', 'b_enabled')
        specs.append((slot_name, asset_name, spec))
    return specs


def get_builder():
    builder = getattr(unreal, 'PVPWeaponAssemblyBuilder', None)
    if builder is None:
        error('PVPWeaponAssemblyBuilder is not reflected; compile PVPDemoEditor first')
    return builder


def read_json(path):
    if not os.path.exists(path):
        error('report missing: %s' % path)
        return None
    try:
        with open(path, 'r', encoding='utf-8') as handle:
            return json.load(handle)
    except Exception as exc:  # noqa: BLE001
        error('failed to read %s: %s' % (path, exc))
        return None


def assemble(loadout_name):
    builder = get_builder()
    if builder is None:
        return False

    if not unreal.EditorAssetLibrary.does_asset_exist(WEAPON_BP_PATH):
        error('missing weapon blueprint: %s' % WEAPON_BP_PATH)
        return False

    specs = build_specs(loadout_name)
    if not specs:
        error('loadout %s produced no specs' % loadout_name)
        return False

    os.makedirs(REPORT_DIR, exist_ok=True)
    log('calling UPVPWeaponAssemblyBuilder.assemble_attachments for %s (%d slots)' % (
        WEAPON_BP_PATH, len(specs)))
    ok = builder.assemble_attachments(
        WEAPON_BP_PATH, [item[2] for item in specs], True, ASSEMBLY_JSON)

    report = read_json(ASSEMBLY_JSON) or {}
    state['assembly'] = report
    check('assembly.builder_return', bool(ok), 'UPVPWeaponAssemblyBuilder.assemble_attachments')
    check('assembly.compiled', bool(report.get('compiled')),
          'compile_errors=%s' % report.get('compile_errors'))
    check('assembly.saved', bool(report.get('saved')), ASSEMBLY_JSON)

    expected_components = {SLOTS[item[0]]['component'] for item in specs}
    actual_components = {item.get('name') for item in report.get('components') or []}
    check('assembly.component_names', actual_components == expected_components,
          'actual=%s expected=%s' % (sorted(actual_components), sorted(expected_components)))

    for slot_name, asset_name, _spec in specs:
        component = SLOTS[slot_name]['component']
        found = next((item for item in report.get('components') or []
                      if item.get('name') == component), None)
        if found is None:
            check('assembly.%s' % component, False, 'component not found in report')
            continue
        expected_parent = SLOTS[slot_name]['parent']
        expected_parent_component = 'Mesh1P' if expected_parent == 'None' \
            else SLOTS[expected_parent]['component']
        expected_mesh_suffix = '/' + asset_name + '.' + asset_name
        check('assembly.%s.socket' % component,
              found.get('socket') == SLOTS[slot_name]['socket'], found.get('socket'))
        check('assembly.%s.parent' % component,
              found.get('parent') == expected_parent_component, found.get('parent'))
        check('assembly.%s.mesh' % component,
              str(found.get('mesh') or '').endswith(expected_mesh_suffix), found.get('mesh'))
        check('assembly.%s.tag' % component,
              ('PVP.Attachment.%s' % slot_name) in (found.get('tags') or []), found.get('tags'))

    return bool(ok)


def bind_catalog(catalog):
    if catalog is None:
        return
    blueprint = load_asset(WEAPON_BP_PATH, unreal.Blueprint, 'weapon blueprint')
    if blueprint is None:
        return

    if blueprint.generated_class() is None:
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    if cdo is None:
        error('%s has no CDO' % WEAPON_BP_PATH)
        return

    cdo.set_editor_property('attachment_catalog', catalog)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        error('failed to save %s' % WEAPON_BP_PATH)

    reloaded = load_asset(WEAPON_BP_PATH, unreal.Blueprint, 'weapon blueprint')
    reloaded_cdo = unreal.get_default_object(reloaded.generated_class()) if reloaded else None
    bound = reloaded_cdo.get_editor_property('attachment_catalog') if reloaded_cdo else None
    bound_path = bound.get_path_name() if bound else ''
    state['weapon_bp'] = {
        'path': WEAPON_BP_PATH,
        'attachment_catalog': bound_path,
    }
    check('weapon_bp.attachment_catalog', bound_path.startswith(CATALOG_PATH), bound_path)


def readback():
    builder = get_builder()
    if builder is None:
        return
    ok = builder.readback_attachments(WEAPON_BP_PATH, READBACK_JSON)
    report = read_json(READBACK_JSON) or {}
    state['readback'] = report
    check('readback.builder_return', bool(ok), 'UPVPWeaponAssemblyBuilder.readback_attachments')
    check('readback.compiled', bool(report.get('compiled')), REPORT_JSON)


def smoke_test_runtime():
    """Spawn the weapon in the editor world and assert the SCS applied the sockets.

    This is the closest automated stand-in for the PIE visual check: it proves that
    USCS_Node::AttachToName actually attaches the attachment components to the right socket
    and parent at construction time, not just that the fields were serialized.
    """
    blueprint = load_asset(WEAPON_BP_PATH, unreal.Blueprint, 'weapon blueprint')
    if blueprint is None or blueprint.generated_class() is None:
        check('runtime.spawn', False, 'weapon blueprint has no generated class')
        return

    try:
        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        editor_world = editor_subsystem.get_editor_world()
    except Exception as exc:  # noqa: BLE001
        warn('cannot access the editor world, runtime attachment check skipped: %s' % exc)
        return

    if editor_world is None:
        warn('no editor world, runtime attachment check skipped')
        return

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actor_subsystem.spawn_actor_from_class(
        blueprint.generated_class(), unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
    if actor is None:
        check('runtime.spawn', False, 'spawn_actor_from_class returned None')
        return

    try:
        components = {component.get_name(): component
                      for component in actor.get_components_by_class(unreal.StaticMeshComponent)}
        expected_components = [SLOTS[slot_name]['component']
                               for slot_name in SLOT_ORDER
                               if slot_name in LOADOUTS[state['loadout']]['entries']]
        check('runtime.component_count',
              all(name in components for name in expected_components),
              'actual=%s expected=%s' % (sorted(components), sorted(expected_components)))

        for slot_name in SLOT_ORDER:
            if slot_name not in LOADOUTS[state['loadout']]['entries']:
                continue
            component_name = SLOTS[slot_name]['component']
            component = components.get(component_name)
            if component is None:
                check('runtime.%s.present' % component_name, False, 'component missing on spawned actor')
                continue

            socket_name = str(component.get_attach_socket_name())
            check('runtime.%s.socket' % component_name, socket_name == SLOTS[slot_name]['socket'],
                  socket_name)

            parent_slot = SLOTS[slot_name]['parent']
            expected_parent = 'Mesh1P' if parent_slot == 'None' \
                else SLOTS[parent_slot]['component']
            parent = component.get_attach_parent()
            parent_name = parent.get_name() if parent else ''
            check('runtime.%s.parent' % component_name,
                  parent_name == expected_parent or parent_name.startswith(expected_parent),
                  parent_name)

            mesh = component.get_editor_property('static_mesh')
            check('runtime.%s.mesh' % component_name, mesh is not None, str(mesh))
    finally:
        actor_subsystem.destroy_actor(actor)


def write_markdown():
    lines = [
        '# PVP weapon attachment assembly',
        '',
        '- result: **%s**' % ('PASS' if not state['errors'] else 'FAIL'),
        '- loadout: `%s` (%s)' % (state['loadout'], LOADOUTS[state['loadout']]['display']),
        '- weapon blueprint: `%s`' % WEAPON_BP_PATH,
        '- catalog: `%s`' % CATALOG_PATH,
        '',
        '## Classification',
        '',
        '| slot | parent | socket | component | default mesh | options |',
        '| --- | --- | --- | --- | --- | --- |',
    ]
    for slot_name in SLOT_ORDER:
        spec = SLOTS[slot_name]
        lines.append('| %s | %s | `%s` | `%s` | `%s` | %s |' % (
            slot_name, spec['parent'], spec['socket'], spec['component'], spec['default'],
            '<br>'.join('`%s`' % name for name, _display in spec['options'])))

    lines.extend(['', '## Loadouts', '',
                  '| loadout | description | slots |',
                  '| --- | --- | --- |'])
    for name, loadout in LOADOUTS.items():
        lines.append('| `%s` | %s | %s |' % (
            name, loadout['description'],
            ', '.join(loadout['entries'].keys())))

    lines.extend(['', '## Assembled components', '',
                  '| component | parent | socket | mesh | tags |',
                  '| --- | --- | --- | --- | --- |'])
    for item in state['readback'].get('components') or []:
        lines.append('| `%s` | `%s` | `%s` | `%s` | %s |' % (
            item.get('name'), item.get('parent'), item.get('socket'), item.get('mesh'),
            ', '.join(item.get('tags') or [])))

    lines.extend(['', '## Checks', '',
                  '| check | result | detail |',
                  '| --- | --- | --- |'])
    for item in state['checks']:
        lines.append('| `%s` | %s | %s |' % (
            item['label'], 'PASS' if item['ok'] else 'FAIL', item['detail'].replace('|', '/')))

    if state['warnings']:
        lines.extend(['', '## Warnings', ''])
        lines.extend(['- %s' % message for message in state['warnings']])
    if state['errors']:
        lines.extend(['', '## Errors', ''])
        lines.extend(['- %s' % message for message in state['errors']])
    lines.append('')

    os.makedirs(REPORT_DIR, exist_ok=True)
    with open(REPORT_MD, 'w', encoding='utf-8') as handle:
        handle.write('\n'.join(lines))
    with open(REPORT_JSON, 'w', encoding='utf-8') as handle:
        json.dump(state, handle, indent=2, ensure_ascii=False)


def parse_loadout():
    argv = [argument.lower() for argument in sys.argv]
    for argument in argv:
        if argument.startswith('--loadout='):
            requested = argument.split('=', 1)[1]
            break
    else:
        requested = (os.environ.get('PVP_WEAPON_LOADOUT') or DEFAULT_LOADOUT).lower()

    for name in LOADOUTS:
        if name.lower() == requested:
            return name
    warn('unknown loadout %s, falling back to %s' % (requested, DEFAULT_LOADOUT))
    return DEFAULT_LOADOUT


def run():
    state['loadout'] = parse_loadout()
    log('loadout=%s blueprint=%s' % (state['loadout'], WEAPON_BP_PATH))

    for group, names in ATTACHMENT_GROUPS.items():
        for name in names:
            if not unreal.EditorAssetLibrary.does_asset_exist(mesh_path(name)):
                error('classified %s asset is missing at %s (run reorg first)' % (name, mesh_path(name)))
    if state['errors']:
        return

    catalog = ensure_catalog()
    if catalog is None:
        return

    assemble(state['loadout'])
    bind_catalog(catalog)
    readback()
    smoke_test_runtime()


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
