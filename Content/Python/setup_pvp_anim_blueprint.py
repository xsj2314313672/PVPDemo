"""Build the PVP first person locomotion Animation Blueprint.

Run headless (editor closed, PVPDemo / PVPDemoEditor compiled):
    UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript \
        -Script="<project>/Content/Python/setup_pvp_anim_blueprint.py" -unattended -nosplash -nullrhi

The AnimGraph itself is built in C++ (UPVPAnimBlueprintBuilder): UEdGraph node creation is not
reflected to Python, and the ue5-automation bridge only reads AnimBlueprints. This script owns the
asset lifecycle: validate dependencies, delete the previous generated asset, call the builder,
compile, save, read back and write Saved/PVPAnimBlueprintReport/report.{json,md}.

The target is /Game/PVP/Animation/AnimBP/ABP_PVP_FP_Locomotion (routed by pvp_logic_layout.py).
Only that generated asset is deleted on a re-run; the character and the migrated
ABP_PVP_FP_BaseCharacter are never touched.
"""

import json
import os
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(os.path.dirname(SCRIPT_DIR))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from pvp_logic_layout import EXPECTED_ASSETS, target_dir_for_asset_name  # noqa: E402

ASSET_NAME = 'ABP_PVP_FP_Locomotion'
ASSET_DIR = target_dir_for_asset_name(ASSET_NAME)
ASSET_PATH = '%s/%s' % (ASSET_DIR, ASSET_NAME)
ASSET_FILE = os.path.join(PROJECT_DIR, 'Content',
                          ASSET_PATH[len('/Game/'):].replace('/', os.sep) + '.uasset')

PARENT_CLASS = unreal.PVPAnimInstanceBase
SKELETON_PATH = '/Game/PVPDemo/Characters/Mannequins/Meshes/SKEL_PVP_Mannequin'
PREVIEW_MESH_PATH = '/Game/PVPDemo/Characters/Mannequins/Meshes/SKM_PVP_FP_Manny_Simple'
LOCOMOTION_DIR = '/Game/PVPDemo/Weapons/AssaultRifle/Animations/Character/FP/Locomotion'
GROUND_BLENDSPACE_PATH = LOCOMOTION_DIR + '/BS_PVP_FP_AR_Locomotion_Standing'
RUN_LOOP_PATH = LOCOMOTION_DIR + '/A_PVP_FP_AR_Run_F_Loop'
JUMP_START_PATH = LOCOMOTION_DIR + '/A_PVP_FP_AR_Jump_Start'
JUMP_FALLING_LOOP_PATH = LOCOMOTION_DIR + '/A_PVP_FP_AR_Jump_Falling_Loop'
JUMP_END_PATH = LOCOMOTION_DIR + '/A_PVP_FP_AR_Jump_End'

REPORT_DIR = os.path.join(PROJECT_DIR, 'Saved', 'PVPAnimBlueprintReport').replace('\\', '/')
REPORT_JSON = REPORT_DIR + '/report.json'
REPORT_MD = REPORT_DIR + '/report.md'

state = {
    'asset_path': ASSET_PATH,
    'asset_dir': ASSET_DIR,
    'parent_class': 'PVPAnimInstanceBase',
    'skeleton': SKELETON_PATH,
    'preview_mesh': PREVIEW_MESH_PATH,
    'deleted_previous': False,
    'created': False,
    'compiled': False,
    'graph_found': False,
    'checks': [],
    'errors': [],
    'warnings': [],
    'builder_report': None,
}


def log(message):
    unreal.log('[PVPAnimBlueprint] %s' % message)


def error(message):
    state['errors'].append(message)
    unreal.log_error('[PVPAnimBlueprint] %s' % message)


def check(label, ok, detail=''):
    state['checks'].append({'label': label, 'ok': bool(ok), 'detail': str(detail)})
    if ok:
        log('PASS %s%s' % (label, (': %s' % detail) if detail else ''))
    else:
        error('FAIL %s%s' % (label, (': %s' % detail) if detail else ''))
    return bool(ok)


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


def remove_previous_asset():
    if not unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH) and not os.path.exists(ASSET_FILE):
        log('no previous %s' % ASSET_PATH)
        return

    log('deleting previous %s' % ASSET_PATH)
    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        unreal.EditorAssetLibrary.delete_asset(ASSET_PATH)

    # ForceDeleteObjects removes the UObject but the commandlet can leave the package file on disk
    # until the process exits; removing the generated file keeps the rebuild deterministic.
    if os.path.exists(ASSET_FILE):
        try:
            os.remove(ASSET_FILE)
        except OSError as exc:
            error('failed to remove %s: %s' % (ASSET_FILE, exc))
            return

    if os.path.exists(ASSET_FILE):
        error('%s still exists after delete' % ASSET_FILE)
        return
    state['deleted_previous'] = True


def build_asset(skeleton, preview_mesh, blend_space, run_loop, jump_start, jump_falling, jump_end):
    os.makedirs(REPORT_DIR, exist_ok=True)
    log('calling UPVPAnimBlueprintBuilder for %s' % ASSET_PATH)
    blueprint = unreal.PVPAnimBlueprintBuilder.build_locomotion_anim_blueprint(
        ASSET_PATH,
        PARENT_CLASS,
        skeleton,
        preview_mesh,
        blend_space,
        run_loop,
        jump_start,
        jump_falling,
        jump_end,
        REPORT_JSON,
    )
    if blueprint is None:
        error('UPVPAnimBlueprintBuilder returned None')
        return None
    state['created'] = True
    return blueprint


def compile_and_save(blueprint):
    if not check('asset.created', bool(blueprint), ASSET_PATH):
        return None

    compiled = unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    state['compiled'] = bool(compiled)
    check('asset.compiled', compiled, 'BlueprintEditorLibrary.compile_blueprint')

    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        error('failed to save %s' % ASSET_PATH)
        return None
    if not unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        error('%s not found on disk after save' % ASSET_PATH)
        return None
    return unreal.EditorAssetLibrary.load_asset(ASSET_PATH)


def verify_asset(blueprint):
    if blueprint is None:
        return

    # Read the C++ report before write_markdown() overwrites report.json with the Python wrapper.
    if os.path.exists(REPORT_JSON):
        try:
            with open(REPORT_JSON, 'r', encoding='utf-8') as handle:
                state['builder_report'] = json.load(handle)
        except Exception as exc:  # noqa: BLE001
            error('failed to read builder report: %s' % exc)
    else:
        error('builder report missing: %s' % REPORT_JSON)

    report = state['builder_report'] or {}
    if not report:
        check('builder.report', False, REPORT_JSON)
        return

    check('builder.asset_parent', report.get('parent_class') == 'PVPAnimInstanceBase',
          report.get('parent_class') or 'None')
    check('builder.blendspace', (report.get('ground_blendspace') or '').startswith(GROUND_BLENDSPACE_PATH),
          report.get('ground_blendspace') or 'None')

    generated_class = blueprint.generated_class()
    if generated_class is None:
        error('%s has no generated class' % ASSET_PATH)
        return

    # UClass.get_super_class() is not exposed to Python in 5.9; SuperStruct is the reflected
    # equivalent, with the C++ report (read from AnimBlueprint->ParentClass) as fallback.
    parent_name = ''
    try:
        super_struct = generated_class.get_editor_property('super_struct')
        parent_name = super_struct.get_name() if super_struct else ''
    except Exception:  # noqa: BLE001
        parent_name = report.get('parent_class') or ''
    check('asset.parent_class', parent_name == 'PVPAnimInstanceBase', parent_name or 'None')

    skeleton_name = ''
    try:
        target_skeleton = blueprint.get_editor_property('target_skeleton')
        skeleton_name = target_skeleton.get_path_name() if target_skeleton else ''
    except Exception:  # noqa: BLE001
        skeleton_name = report.get('skeleton') or ''
    check('asset.skeleton', skeleton_name.startswith(SKELETON_PATH), skeleton_name or 'None')

    anim_graph = unreal.BlueprintEditorLibrary.find_graph(blueprint, 'AnimGraph')
    state['graph_found'] = anim_graph is not None
    check('asset.anim_graph', anim_graph is not None, 'AnimGraph')
    states = report.get('states', [])
    transitions = report.get('transitions', [])
    expected_states = {'Ground', 'JumpStart', 'Falling', 'Land'}
    actual_states = {item.get('name') for item in states}
    check('graph.states', actual_states == expected_states,
          'actual=%s expected=%s' % (sorted(actual_states), sorted(expected_states)))
    check('graph.transitions', len(transitions) == 6, '%d (expected 6)' % len(transitions))
    check('graph.compiled_report', bool(report.get('compiled')),
          'compile_errors=%s' % report.get('compile_errors'))

    wiring = report.get('wiring') or {}
    for key in ('root_pose', 'state_machine_pose', 'entry_to_ground',
                'ground_blendspace_x', 'ground_blendspace_y', 'ground_run_blend',
                'ground_pose', 'jump_start_pose', 'falling_pose', 'land_pose',
                'land_auto_rule'):
        check('wiring.%s' % key, bool(wiring.get(key)), key)
    check('wiring.transition_rules', wiring.get('transition_rules') == 5,
          '%s (expected 5)' % wiring.get('transition_rules'))


def write_markdown():
    lines = [
        '# PVP locomotion Animation Blueprint',
        '',
        '- result: **%s**' % ('PASS' if not state['errors'] else 'FAIL'),
        '- asset: `%s`' % state['asset_path'],
        '- parent class: `%s`' % state['parent_class'],
        '- skeleton: `%s`' % state['skeleton'],
        '- preview mesh: `%s`' % state['preview_mesh'],
        '- deleted previous asset: `%s`' % state['deleted_previous'],
        '- created: `%s`' % state['created'],
        '- compiled: `%s`' % state['compiled'],
        '- AnimGraph found: `%s`' % state['graph_found'],
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
    lines.extend(['', '## Builder report (C++)', '', '```json',
                  json.dumps(state['builder_report'], indent=2, ensure_ascii=False),
                  '```', '', '## Errors', ''])
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
    log('target=%s parent=%s' % (ASSET_PATH, state['parent_class']))
    if ASSET_NAME not in EXPECTED_ASSETS['Animation/AnimBP']:
        error('%s is not registered in pvp_logic_layout.EXPECTED_ASSETS[Animation/AnimBP]' % ASSET_NAME)
    if ASSET_DIR != '/Game/PVP/Animation/AnimBP':
        error('layout route changed unexpectedly: %s' % ASSET_DIR)

    skeleton = load_asset(SKELETON_PATH, unreal.Skeleton, 'skeleton')
    preview_mesh = load_asset(PREVIEW_MESH_PATH, unreal.SkeletalMesh, 'preview mesh')
    blend_space = load_asset(GROUND_BLENDSPACE_PATH, unreal.BlendSpace, 'ground blend space')
    run_loop = load_asset(RUN_LOOP_PATH, unreal.AnimSequence, 'run loop')
    jump_start = load_asset(JUMP_START_PATH, unreal.AnimSequence, 'jump start')
    jump_falling = load_asset(JUMP_FALLING_LOOP_PATH, unreal.AnimSequence, 'jump falling loop')
    jump_end = load_asset(JUMP_END_PATH, unreal.AnimSequence, 'jump end')

    if state['errors']:
        return

    remove_previous_asset()
    if state['errors']:
        return

    blueprint = build_asset(skeleton, preview_mesh, blend_space, run_loop, jump_start, jump_falling, jump_end)
    blueprint = compile_and_save(blueprint)
    verify_asset(blueprint)


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
