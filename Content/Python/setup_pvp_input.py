"""Validate the PVP input assets and build the input configuration data asset.

Run headless (editor closed, PVPDemo module compiled):
    UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript \
        -Script="<project>/Content/Python/setup_pvp_input.py" -unattended -nosplash -nullrhi

Phases (idempotent, safe to re-run):
  1. validate the four required IA assets (Move/Look = Axis2D, Jump/Run = Boolean), fixing the
     value type of the project-local copy when it does not match the Lyra convention
  2. dump IMC_PVP_Default, assert every required action has a key mapping, and require the
     Negate(Y) modifier on IA_PVP_Look (vertical look polarity)
  3. create / update /Game/PVP/Input/DA_PVP_InputConfig (InputTag.* -> InputAction)
  4. assign the config to the BP_PVPBaseCharacter class defaults
  5. write Saved/PVPInputReport/report.json|md and fail loudly when a check did not pass

The IA / IMC / BP assets are never recreated here: they already exist in Content/PVP/Input and
Content/PVP/Characters. Only DA_PVP_InputConfig is produced by this script.
"""

import json
import os

import unreal

# --------------------------------------------------------------------------------------------------
# Constants
# --------------------------------------------------------------------------------------------------
INPUT_DIR = '/Game/PVP/Input'
MAPPING_CONTEXT = INPUT_DIR + '/IMC_PVP_Default'
INPUT_CONFIG = INPUT_DIR + '/DA_PVP_InputConfig'
INPUT_CONFIG_NAME = 'DA_PVP_InputConfig'
CHARACTER_BP = '/Game/PVP/Characters/BP_PVPBaseCharacter'

REPORT_DIR = os.path.join(unreal.Paths.project_saved_dir().replace('\\', '/'), 'PVPInputReport')
REPORT_JSON = os.path.join(REPORT_DIR, 'report.json')
REPORT_MD = os.path.join(REPORT_DIR, 'report.md')

# The four actions bound by APVPBaseCharacter::SetupPlayerInputComponent.
REQUIRED_ACTIONS = [
    {'tag': 'InputTag.Move', 'asset': 'IA_PVP_Move', 'value_type': 'AXIS2D'},
    {'tag': 'InputTag.Look', 'asset': 'IA_PVP_Look', 'value_type': 'AXIS2D'},
    {'tag': 'InputTag.Jump', 'asset': 'IA_PVP_Jump', 'value_type': 'BOOLEAN'},
    {'tag': 'InputTag.Run', 'asset': 'IA_PVP_Run', 'value_type': 'BOOLEAN'},
]

state = {
    'mapping_context': {'path': MAPPING_CONTEXT, 'action_keys': {}, 'mapping_count': 0, 'look_y_negate': None},
    'actions': [],
    'input_config': {'path': INPUT_CONFIG, 'created': False, 'entries': []},
    'character_blueprint': {'path': CHARACTER_BP, 'input_config': None},
    'fixed_assets': [],
    'errors': [],
    'warnings': [],
}


def log(message):
    unreal.log('[PVP-INPUT] %s' % message)


def warn(message):
    state['warnings'].append(message)
    unreal.log_warning('[PVP-INPUT] %s' % message)


def error(message):
    state['errors'].append(message)
    unreal.log_error('[PVP-INPUT] %s' % message)


# --------------------------------------------------------------------------------------------------
# Helpers
# --------------------------------------------------------------------------------------------------
def asset_path(name):
    return '%s/%s' % (INPUT_DIR, name)


def asset_exists(path):
    return unreal.EditorAssetLibrary.does_asset_exist(path)


def load_asset(path):
    return unreal.EditorAssetLibrary.load_asset(path)


def save_asset(path):
    if asset_exists(path):
        unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)


def value_type_name(value):
    """Readable name of an InputActionValueType (PyUE enum repr is '<Type.X: n>')."""
    text = str(value).upper()
    for name in ('BOOLEAN', 'AXIS1D', 'AXIS2D', 'AXIS3D'):
        if name in text:
            return name
    return text


def key_name_of(mapping):
    try:
        key = mapping.get_editor_property('key')
        return str(key.get_editor_property('key_name'))
    except Exception:  # noqa: BLE001
        return str(mapping.get_editor_property('key'))


def read_bool_property(obj, names, default=False):
    for name in names:
        try:
            return bool(obj.get_editor_property(name))
        except Exception:  # noqa: BLE001
            continue
    return default


def has_look_y_negate(mappings):
    """IA_PVP_Look must contain an InputModifierNegate with only Y enabled.

    After CameraFP switched to bUsePawnControlRotation, AddControllerPitchInput(Y) produced an
    inverted vertical view. The single negation lives in the IMC (Y only); the C++ handler keeps
    AddControllerPitchInput(LookAxisVector.Y) unsigned. Negating in both places would invert again.
    """
    for mapping in mappings:
        action = mapping.get_editor_property('action')
        if action is None or action.get_name() != 'IA_PVP_Look':
            continue
        for modifier in mapping.get_editor_property('modifiers'):
            if modifier.get_class().get_name() != 'InputModifierNegate':
                continue
            if (read_bool_property(modifier, ('b_y', 'y'))
                    and not read_bool_property(modifier, ('b_x', 'x'))
                    and not read_bool_property(modifier, ('b_z', 'z'))):
                return True
    return False


# --------------------------------------------------------------------------------------------------
# Phase 1/2: validate the existing IA assets and the default mapping context
# --------------------------------------------------------------------------------------------------
def validate_actions():
    for required in REQUIRED_ACTIONS:
        ia_path = asset_path(required['asset'])
        record = {
            'tag': required['tag'],
            'asset': ia_path,
            'expected_value_type': required['value_type'],
            'value_type': None,
            'mapped_keys': [],
            'value_type_fixed': False,
        }

        if not asset_exists(ia_path):
            error('missing input action %s (required for %s)' % (ia_path, required['tag']))
            state['actions'].append(record)
            continue

        input_action = load_asset(ia_path)
        expected_value_type = getattr(unreal.InputActionValueType, required['value_type'])
        actual_value_type = input_action.get_editor_property('value_type')
        record['value_type'] = value_type_name(actual_value_type)

        if actual_value_type != expected_value_type:
            warn('%s value_type=%s, expected %s; fixing the project-local copy'
                 % (ia_path, record['value_type'], required['value_type']))
            input_action.set_editor_property('value_type', expected_value_type)
            unreal.EditorAssetLibrary.save_loaded_asset(input_action)
            actual_value_type = input_action.get_editor_property('value_type')
            record['value_type'] = value_type_name(actual_value_type)
            record['value_type_fixed'] = True
            state['fixed_assets'].append(ia_path)

        if actual_value_type != expected_value_type:
            error('%s value_type stays %s after fix (expected %s)'
                  % (ia_path, record['value_type'], required['value_type']))

        state['actions'].append(record)


def dump_mapping_context():
    if not asset_exists(MAPPING_CONTEXT):
        error('missing input mapping context %s' % MAPPING_CONTEXT)
        return

    mapping_context = load_asset(MAPPING_CONTEXT)
    # UE 5.7+ stores the default mappings inside FInputMappingContextMappingData and deprecates the
    # old top level 'mappings' property. Fall back to it only if the new struct is unavailable.
    try:
        mapping_data = mapping_context.get_editor_property('default_key_mappings')
        mappings = mapping_data.get_editor_property('mappings')
    except Exception:  # noqa: BLE001
        warn('%s has no default_key_mappings, reading the deprecated mappings property'
             % MAPPING_CONTEXT)
        mappings = mapping_context.get_editor_property('mappings')
    state['mapping_context']['mapping_count'] = len(mappings)

    action_keys = state['mapping_context']['action_keys']
    for mapping in mappings:
        action = mapping.get_editor_property('action')
        if action is None:
            continue
        action_keys.setdefault(action.get_name(), []).append(key_name_of(mapping))

    look_y_negate = has_look_y_negate(mappings)
    state['mapping_context']['look_y_negate'] = look_y_negate
    if look_y_negate:
        log('IA_PVP_Look has the required Negate(Y) modifier')
    else:
        error('IA_PVP_Look has no InputModifierNegate with only Y enabled in %s; vertical look '
              'will be inverted. Add the modifier in the IMC (Y only), then re-run.' % MAPPING_CONTEXT)

    for record in state['actions']:
        action_name = record['asset'].rsplit('/', 1)[-1]
        keys = sorted(set(action_keys.get(action_name, [])))
        record['mapped_keys'] = keys
        if not keys:
            error('%s has no key mapping in %s (map it in the editor, then re-run)'
                  % (action_name, MAPPING_CONTEXT))
            continue
        log('%s -> %s' % (action_name, ', '.join(keys)))


# --------------------------------------------------------------------------------------------------
# Phase 3: produce DA_PVP_InputConfig
# --------------------------------------------------------------------------------------------------
def ensure_input_config():
    created = False
    input_config = load_asset(INPUT_CONFIG) if asset_exists(INPUT_CONFIG) else None
    if input_config is None:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.PVPInputConfig)
        input_config = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            INPUT_CONFIG_NAME, INPUT_DIR, unreal.PVPInputConfig, factory)
        if input_config is None:
            error('failed to create %s' % INPUT_CONFIG)
            return None
        created = True
        log('created %s' % INPUT_CONFIG)
    else:
        log('reusing %s' % INPUT_CONFIG)

    # The C++ constructor seeds one slot per native tag (Move / Look / Jump / Run). Gameplay tags
    # cannot be constructed from script, so only the concrete InputAction is filled in here.
    entries = input_config.get_editor_property('native_input_actions')
    if len(entries) != len(REQUIRED_ACTIONS):
        error('%s has %d native input action slots, expected %d; recreate the asset so the C++ '
              'constructor seeds the four tag slots again'
              % (INPUT_CONFIG, len(entries), len(REQUIRED_ACTIONS)))
        return input_config

    for index, required in enumerate(REQUIRED_ACTIONS):
        ia_path = asset_path(required['asset'])
        if not asset_exists(ia_path):
            continue
        input_action = load_asset(ia_path)
        # Setting the nested struct from Python does not persist reliably, so the native helper
        # mutates the array element directly and marks the package dirty.
        if not input_config.set_native_input_action_by_index(index, input_action):
            error('failed to bind %s to slot %d of %s' % (required['asset'], index, INPUT_CONFIG))
            continue
        state['input_config']['entries'].append({
            'tag': required['tag'],
            'asset': input_action.get_name(),
        })

    if not unreal.EditorAssetLibrary.save_loaded_asset(input_config, only_if_is_dirty=False):
        error('failed to save %s' % INPUT_CONFIG)

    # Read the actions back so a silent no-op cannot pass as success.
    saved_entries = input_config.get_editor_property('native_input_actions')
    for index, required in enumerate(REQUIRED_ACTIONS):
        saved_action = None
        if index < len(saved_entries):
            saved_action = saved_entries[index].get_editor_property('input_action')
        if saved_action is None:
            error('%s slot %d (%s) has no InputAction after save'
                  % (INPUT_CONFIG, index, required['asset']))

    state['input_config']['created'] = created
    state['input_config']['entry_count'] = len(entries)
    return input_config


# --------------------------------------------------------------------------------------------------
# Phase 4: point BP_PVPBaseCharacter at the config
# --------------------------------------------------------------------------------------------------
def assign_to_character(input_config):
    if input_config is None:
        return
    if not asset_exists(CHARACTER_BP):
        error('missing character blueprint %s' % CHARACTER_BP)
        return

    character_bp = load_asset(CHARACTER_BP)
    class_default = unreal.get_default_object(character_bp.generated_class())
    class_default.set_editor_property('input_config', input_config)
    unreal.BlueprintEditorLibrary.compile_blueprint(character_bp)
    save_asset(CHARACTER_BP)

    # The CDO is re-created by the compile above, so read the serialized value back.
    class_default = unreal.get_default_object(character_bp.generated_class())
    assigned = class_default.get_editor_property('input_config')
    assigned_path = assigned.get_path_name() if assigned else None
    state['character_blueprint']['input_config'] = assigned_path

    if assigned is None:
        error('%s class default input_config is still empty' % CHARACTER_BP)
    else:
        log('%s input_config = %s' % (CHARACTER_BP, assigned_path))


# --------------------------------------------------------------------------------------------------
# Phase 5: report
# --------------------------------------------------------------------------------------------------
def write_report():
    os.makedirs(REPORT_DIR, exist_ok=True)
    with open(REPORT_JSON, 'w', encoding='utf-8') as handle:
        json.dump(state, handle, indent=2, ensure_ascii=False)

    lines = [
        '# PVP input setup report',
        '',
        '- mapping context: `%s` (%d mappings)' % (
            MAPPING_CONTEXT, state['mapping_context']['mapping_count']),
        '- IA_PVP_Look Negate(Y): `%s`' % (
            'yes' if state['mapping_context']['look_y_negate'] else 'no'),
        '- input config: `%s` (%s)' % (
            INPUT_CONFIG, 'created' if state['input_config']['created'] else 'reused'),
        '- character blueprint: `%s`' % CHARACTER_BP,
        '- class default input_config: `%s`' % (state['character_blueprint']['input_config'] or 'None'),
        '',
        '## Required actions',
        '',
        '| tag | asset | value type | keys |',
        '| --- | --- | --- | --- |',
    ]
    for record in state['actions']:
        lines.append('| `%s` | `%s` | `%s` | %s |' % (
            record['tag'],
            record['asset'].rsplit('/', 1)[-1],
            record['value_type'],
            ', '.join(record['mapped_keys']) or '-',
        ))

    lines.extend(['', '## Input config entries', ''])
    for entry in state['input_config']['entries']:
        lines.append('- `%s` -> `%s`' % (entry['tag'], entry['asset']))
    if not state['input_config']['entries']:
        lines.append('- none')

    lines.extend(['', '## Errors', ''])
    if state['errors']:
        lines.extend('- %s' % item for item in state['errors'])
    else:
        lines.append('- none')

    lines.extend(['', '## Warnings', ''])
    if state['warnings']:
        lines.extend('- %s' % item for item in state['warnings'])
    else:
        lines.append('- none')

    with open(REPORT_MD, 'w', encoding='utf-8') as handle:
        handle.write('\n'.join(lines) + '\n')

    log('report written: %s' % REPORT_JSON)


def run():
    try:
        validate_actions()
        dump_mapping_context()

        input_config = ensure_input_config()
        assign_to_character(input_config)
    except Exception as exc:  # noqa: BLE001
        error('unhandled exception: %s' % exc)
        write_report()
        raise

    write_report()

    if state['errors']:
        raise RuntimeError('%d input setup error(s), see %s' % (len(state['errors']), REPORT_MD))
    log('input setup complete')


if __name__ == '__main__':
    run()
