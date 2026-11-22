"""Compare the 1P setup of APVPBaseCharacter against the reference BP_TFA_BaseCharacter.

The script creates (or reuses) the derived blueprint /Game/PVPDemo/Characters/BP_PVPBaseCharacter,
spawns both actors in the TFA demo map and diffs the 1P relevant data: component tree,
attachments, relative transforms, camera settings and character level view settings.

Run headless:
    UnrealEditor-Cmd.exe <project>.uproject -run=PythonScript \
        -Script="<project>/Content/Python/compare_tfa_character.py" -unattended -nosplash -nullrhi

A structural mismatch raises an exception, which makes the commandlet fail with a non zero exit code.
"""
import json
import math
import os

import unreal

REFERENCE_BP_PATH = '/Game/InfimaGames/TacticalFPSAnimations/Common/Core/Characters/BP_TFA_BaseCharacter'
DERIVED_BP_DIR = '/Game/PVPDemo/Characters'
DERIVED_BP_NAME = 'BP_PVPBaseCharacter'
DERIVED_BP_PATH = '%s/%s' % (DERIVED_BP_DIR, DERIVED_BP_NAME)
NATIVE_CLASS_PATH = '/Script/PVPDemo.PVPBaseCharacter'
MAP_PATH = '/Game/InfimaGames/TacticalFPSAnimations/Weapons/AssaultRifle/Demo/Maps/L_TFA_AR_Showcase'
OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), 'PVPCharacterDump')

LOCATION_TOLERANCE = 0.01
ANGLE_TOLERANCE = 0.01

REQUIRED_COMPONENTS = ('CharacterMesh0', 'CameraFP')
ACTOR_PROPS = (
    'use_controller_rotation_yaw',
    'use_controller_rotation_pitch',
    'use_controller_rotation_roll',
    'base_eye_height',
    'crouched_eye_height',
)
CAMERA_PROPS = (
    'field_of_view',
    'aspect_ratio',
    'constrain_aspect_ratio',
    'use_pawn_control_rotation',
    'post_process_blend_weight',
)
EXPECTED_DIFFERENCES = {
    'CameraFP.auto_activate': 'reference toggles cameras per perspective, the 1P only class activates it by default',
}

lines = []
failures = []


def log(msg):
    unreal.log('[PARITY] %s' % msg)
    lines.append(str(msg))


def vec(value):
    return [float(value.x), float(value.y), float(value.z)]


def rot(value):
    return [float(value.pitch), float(value.yaw), float(value.roll)]


def location_diff(a, b):
    return max(abs(x - y) for x, y in zip(a, b))


def angle_delta(a, b):
    return abs((a - b + 180.0) % 360.0 - 180.0)


def rotation_diff(a, b):
    return max(angle_delta(x, y) for x, y in zip(a, b))


def report(label, ok, detail):
    entry = '%s %s: %s' % ('PASS' if ok else 'FAIL', label, detail)
    log(entry)
    if not ok:
        failures.append(entry)


def check_float(label, actual, expected, tolerance):
    delta = abs(float(actual) - float(expected))
    report(label, delta <= tolerance, '%.6f vs %.6f (delta %.6f)' % (actual, expected, delta))


def check_vector(label, actual, expected, tolerance):
    delta = location_diff(actual, expected)
    report(label, delta <= tolerance, '%s vs %s (delta %.6f)' % (actual, expected, delta))


def check_rotation(label, actual, expected, tolerance):
    delta = rotation_diff(actual, expected)
    report(label, delta <= tolerance, '%s vs %s (delta %.6f)' % (actual, expected, delta))


def collect_components(actor):
    result = {}
    for comp in actor.get_components_by_class(unreal.ActorComponent):
        info = {
            'class': comp.get_class().get_name(),
            'parent': None,
            'socket': None,
            'relative_location': None,
            'relative_rotation': None,
            'relative_scale3d': None,
        }
        if isinstance(comp, unreal.SceneComponent):
            parent = comp.get_attach_parent()
            info['parent'] = parent.get_name() if parent else None
            try:
                info['socket'] = str(comp.get_attach_socket_name())
            except Exception:  # noqa: BLE001
                info['socket'] = ''
            info['relative_location'] = vec(comp.get_editor_property('relative_location'))
            info['relative_rotation'] = rot(comp.get_editor_property('relative_rotation'))
            info['relative_scale3d'] = vec(comp.get_editor_property('relative_scale3d'))
            info['world_transform'] = str(comp.get_world_transform())
        if isinstance(comp, unreal.CameraComponent):
            info['camera'] = {}
            for prop in CAMERA_PROPS:
                try:
                    info['camera'][prop] = comp.get_editor_property(prop)
                except Exception:  # noqa: BLE001
                    pass
            info['auto_activate'] = comp.get_editor_property('auto_activate')
        result[comp.get_name()] = info
    return result


def collect(actor):
    data = {'components': collect_components(actor), 'actor': {}}
    for prop in ACTOR_PROPS:
        try:
            data['actor'][prop] = actor.get_editor_property(prop)
        except Exception:  # noqa: BLE001
            pass
    capsule = actor.get_component_by_class(unreal.CapsuleComponent)
    if capsule:
        data['capsule'] = {
            'capsule_radius': capsule.get_editor_property('capsule_radius'),
            'capsule_half_height': capsule.get_editor_property('capsule_half_height'),
        }
    return data


def ensure_derived_blueprint():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    existing = unreal.EditorAssetLibrary.load_asset(DERIVED_BP_PATH)
    if existing is not None:
        log('reusing derived blueprint: %s' % DERIVED_BP_PATH)
        return existing

    native_class = getattr(unreal, 'PVPBaseCharacter', None)
    if native_class is None:
        raise RuntimeError('native class APVPBaseCharacter not found, module not compiled?')

    unreal.EditorAssetLibrary.make_directory(DERIVED_BP_DIR)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', native_class.static_class() if hasattr(native_class, 'static_class') else native_class)
    blueprint = asset_tools.create_asset(DERIVED_BP_NAME, DERIVED_BP_DIR, unreal.Blueprint, factory)
    if blueprint is None:
        raise RuntimeError('failed to create %s' % DERIVED_BP_PATH)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_asset(DERIVED_BP_PATH, only_if_is_dirty=False)
    log('created derived blueprint: %s' % DERIVED_BP_PATH)
    return blueprint


_world = None


def get_world():
    """Load the demo map once; reloading it would destroy actors spawned earlier."""
    global _world  # noqa: PLW0603
    if _world is None:
        _world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
        if _world is None:
            raise RuntimeError('failed to load map %s' % MAP_PATH)
    return _world


def spawn(blueprint_or_class, location):
    get_world()
    actor_class = blueprint_or_class
    if isinstance(blueprint_or_class, unreal.Blueprint):
        actor_class = blueprint_or_class.generated_class()
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, location, unreal.Rotator(0.0, 0.0, 0.0))
    if actor is None:
        raise RuntimeError('failed to spawn %s' % actor_class)
    return actor


def compare(reference, candidate):
    ref_components = reference['components']
    new_components = candidate['components']

    log('reference components: %s' % sorted(ref_components.keys()))
    log('candidate components: %s' % sorted(new_components.keys()))

    for name in REQUIRED_COMPONENTS:
        report('component.%s.exists' % name, name in new_components, 'found=%s' % (name in new_components))

    for name in REQUIRED_COMPONENTS:
        if name not in ref_components or name not in new_components:
            continue
        ref_comp = ref_components[name]
        new_comp = new_components[name]
        report('component.%s.class' % name, ref_comp['class'] == new_comp['class'],
               '%s vs %s' % (ref_comp['class'], new_comp['class']))
        report('component.%s.parent' % name, ref_comp['parent'] == new_comp['parent'],
               '%s vs %s' % (ref_comp['parent'], new_comp['parent']))
        report('component.%s.socket' % name, (ref_comp['socket'] or '') == (new_comp['socket'] or ''),
               '%s vs %s' % (ref_comp['socket'], new_comp['socket']))
        if ref_comp['relative_location'] is not None:
            check_vector('component.%s.relative_location' % name, new_comp['relative_location'],
                         ref_comp['relative_location'], LOCATION_TOLERANCE)
            check_rotation('component.%s.relative_rotation' % name, new_comp['relative_rotation'],
                           ref_comp['relative_rotation'], ANGLE_TOLERANCE)
            check_vector('component.%s.relative_scale3d' % name, new_comp['relative_scale3d'],
                         ref_comp['relative_scale3d'], LOCATION_TOLERANCE)
        if 'camera' in ref_comp:
            for prop in CAMERA_PROPS:
                if prop in ref_comp['camera'] and prop in new_comp.get('camera', {}):
                    check_float('component.%s.camera.%s' % (name, prop),
                                new_comp['camera'][prop], ref_comp['camera'][prop],
                                0.01 if prop != 'aspect_ratio' else 0.0001)
            log('INFO CameraFP.auto_activate: %s vs %s (%s)' % (
                ref_comp.get('auto_activate'), new_comp.get('auto_activate'),
                EXPECTED_DIFFERENCES.get('CameraFP.auto_activate')))

    for prop, ref_value in reference['actor'].items():
        new_value = candidate['actor'].get(prop)
        report('actor.%s' % prop, ref_value == new_value, '%s vs %s' % (ref_value, new_value))

    if 'capsule' in reference and 'capsule' in candidate:
        for prop, ref_value in reference['capsule'].items():
            check_float('capsule.%s' % prop, candidate['capsule'][prop], ref_value, 0.01)

    # World transforms are informative: they depend on the animation pose of the mesh.
    for name in REQUIRED_COMPONENTS:
        if name in ref_components and name in new_components:
            log('INFO %s.world_transform ref=%s' % (name, ref_components[name].get('world_transform')))
            log('INFO %s.world_transform new=%s' % (name, new_components[name].get('world_transform')))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    blueprint = ensure_derived_blueprint()
    reference_actor = spawn(unreal.EditorAssetLibrary.load_blueprint_class(REFERENCE_BP_PATH), unreal.Vector(0, 0, 0))
    candidate_actor = spawn(blueprint, unreal.Vector(0, 0, 0))
    log('reference actor: %s' % reference_actor.get_name())
    log('candidate actor: %s' % candidate_actor.get_name())

    reference = collect(reference_actor)
    candidate = collect(candidate_actor)
    compare(reference, candidate)

    report_path = os.path.join(OUT_DIR, 'parity_report.md')
    with open(report_path, 'w', encoding='utf-8') as handle:
        handle.write('# 1P parity report\n\n')
        handle.write('- reference: `%s`\n' % REFERENCE_BP_PATH)
        handle.write('- candidate: `%s`\n' % DERIVED_BP_PATH)
        handle.write('- failures: `%d`\n\n' % len(failures))
        handle.write('```\n%s\n```\n' % '\n'.join(lines))
    with open(os.path.join(OUT_DIR, 'parity_report.json'), 'w', encoding='utf-8') as handle:
        json.dump({'reference': reference, 'candidate': candidate, 'failures': failures}, handle, indent=2, default=str)
    log('report written: %s' % report_path)

    if failures:
        raise RuntimeError('%d parity check(s) failed, see %s' % (len(failures), report_path))
    log('PARITY OK')


main()
