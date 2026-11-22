"""Dump BP_TFA_BaseCharacter component tree and camera settings to Saved/PVPCharacterDump.

Run headless:
    UnrealEditor-Cmd.exe <project>.uproject -run=PythonScript \
        -Script="<project>/Content/Python/dump_tfa_character.py" -unattended -nosplash -nullrhi

Why spawn instead of reading the CDO: Blueprint SCS components are not instantiated on the
class default object in a commandlet, so the assembled instance is the only reliable source
for the 1P hierarchy, attachments and camera defaults.
"""
import json
import os

import unreal

BP_PATH = '/Game/InfimaGames/TacticalFPSAnimations/Common/Core/Characters/BP_TFA_BaseCharacter'
MAP_PATH = '/Game/InfimaGames/TacticalFPSAnimations/Weapons/AssaultRifle/Demo/Maps/L_TFA_AR_Showcase'
OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), 'PVPCharacterDump')

SCENE_PROPS = (
    'relative_location',
    'relative_rotation',
    'relative_scale3d',
    'visible',
    'hidden_in_game',
    'component_tags',
)
COMMON_PROPS = (
    'auto_activate',
    'is_editor_only',
)
CAMERA_PROPS = (
    'field_of_view',
    'aspect_ratio',
    'constrain_aspect_ratio',
    'use_pawn_control_rotation',
    'post_process_blend_weight',
    'projection_mode',
    'ortho_width',
)
SPRING_ARM_PROPS = (
    'target_arm_length',
    'socket_offset',
    'target_offset',
    'use_pawn_control_rotation',
    'do_collision_test',
    'enable_camera_lag',
    'camera_lag_speed',
    'camera_rotation_lag_speed',
    'enable_camera_rotation_lag',
    'inherit_pitch',
    'inherit_yaw',
    'inherit_roll',
)
MESH_PROPS = (
    'skeletal_mesh_asset',
    'anim_class',
    'override_materials',
    'cast_shadow',
    'receive_decals',
    'owner_no_see',
    'only_owner_see',
    'bounds_scale',
    'mesh_decal_mode',
    'component_tags',
)
STATIC_MESH_PROPS = (
    'static_mesh',
    'cast_shadow',
    'hidden_in_game',
    'visible',
    'owner_no_see',
    'only_owner_see',
)
ACTOR_PROPS = (
    'use_controller_rotation_yaw',
    'use_controller_rotation_pitch',
    'use_controller_rotation_roll',
    'base_eye_height',
    'crouched_eye_height',
    'auto_possess_player',
    'tags',
)
BP_VARIABLE_HINTS = (
    'CurrentCameraPerspective',
    'bAnimateCamera',
    'InitialCameraDistance',
    'MinCameraDistance',
    'MaxCameraDistance',
    'AimedFOV',
    'FP_Mesh',
    'TP_Mesh',
    'bIsAiming',
    'bIsCrouching',
    'TriggerCamera',
)


def log(msg):
    unreal.log('[TFA-DUMP] %s' % msg)


def to_json_value(value):
    """Convert common unreal types into JSON friendly values."""
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, unreal.Vector):
        return [value.x, value.y, value.z]
    if isinstance(value, unreal.Rotator):
        return [value.pitch, value.yaw, value.roll]
    if isinstance(value, unreal.Vector2D):
        return [value.x, value.y]
    if isinstance(value, unreal.Text):
        return str(value)
    if isinstance(value, unreal.EnumBase):
        return '%s::%s' % (type(value).__name__, value.get_name())
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, (list, tuple)):
        return [to_json_value(item) for item in value]
    return str(value)


def get_prop(obj, name):
    try:
        return to_json_value(obj.get_editor_property(name))
    except Exception:  # noqa: BLE001
        return None


def get_props(obj, names):
    result = {}
    for name in names:
        value = get_prop(obj, name)
        if value is not None:
            result[name] = value
    return result


def get_overridden_post_process(settings):
    """Return only the post process fields that are explicitly overridden."""
    result = {}
    for attr in dir(settings):
        if attr.startswith('b_override_') and 'b_override_' in attr:
            try:
                if settings.get_editor_property(attr):
                    target = attr.replace('b_override_', '')
                    result[target] = to_json_value(settings.get_editor_property(target))
            except Exception:  # noqa: BLE001
                continue
    return result


def dump_component(comp):
    data = {
        'name': comp.get_name(),
        'class': comp.get_class().get_name(),
        'path': comp.get_path_name(),
    }
    data.update(get_props(comp, COMMON_PROPS))
    if isinstance(comp, unreal.SceneComponent):
        data.update(get_props(comp, SCENE_PROPS))
        parent = None
        try:
            parent = comp.get_attach_parent()
        except Exception:  # noqa: BLE001
            parent = None
        data['attach_parent'] = parent.get_name() if parent else None
        try:
            data['attach_socket'] = str(comp.get_attach_socket_name())
        except Exception:  # noqa: BLE001
            data['attach_socket'] = None
    if isinstance(comp, unreal.CameraComponent):
        data['camera'] = get_props(comp, CAMERA_PROPS)
        try:
            pp = comp.get_editor_property('post_process_settings')
            data['camera']['post_process_overrides'] = get_overridden_post_process(pp)
        except Exception as exc:  # noqa: BLE001
            data['camera']['post_process_error'] = str(exc)
    if isinstance(comp, unreal.SpringArmComponent):
        data['spring_arm'] = get_props(comp, SPRING_ARM_PROPS)
    if isinstance(comp, unreal.SkeletalMeshComponent):
        data['skeletal_mesh'] = get_props(comp, MESH_PROPS)
    if isinstance(comp, unreal.StaticMeshComponent):
        data['static_mesh'] = get_props(comp, STATIC_MESH_PROPS)
    if isinstance(comp, unreal.PostProcessComponent):
        data['post_process'] = {
            'enabled': get_prop(comp, 'enabled'),
            'blend_weight': get_prop(comp, 'blend_weight'),
            'priority': get_prop(comp, 'priority'),
            'unbound': get_prop(comp, 'unbound'),
        }
        try:
            settings = comp.get_editor_property('settings')
            data['post_process']['overrides'] = get_overridden_post_process(settings)
        except Exception as exc:  # noqa: BLE001
            data['post_process']['overrides_error'] = str(exc)
    if isinstance(comp, unreal.SpotLightComponent):
        data['spot_light'] = get_props(comp, (
            'intensity', 'attenuation_radius', 'inner_cone_angle', 'outer_cone_angle', 'light_color',
        ))
    return data


def to_snake_case(name):
    import re
    step_one = re.sub(r'(.)([A-Z][a-z]+)', r'\1_\2', name)
    return re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', step_one).lower()


def dump_bp_variables(actor):
    result = {}
    for hint in BP_VARIABLE_HINTS:
        for candidate in (hint, to_snake_case(hint)):
            value = get_prop(actor, candidate)
            if value is not None:
                result[hint] = value
                break
    return result


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    log('loaded map: %s' % world)
    if world is None:
        raise RuntimeError('failed to load %s' % MAP_PATH)

    bp_class = unreal.EditorAssetLibrary.load_blueprint_class(BP_PATH)
    log('blueprint class: %s' % bp_class)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(bp_class, unreal.Vector(0.0, 0.0, 0.0))
    log('spawned actor: %s' % actor)
    if actor is None:
        raise RuntimeError('failed to spawn %s' % BP_PATH)

    report = {
        'blueprint': BP_PATH,
        'map': MAP_PATH,
        'actor_class': bp_class.get_name(),
        'actor_props': get_props(actor, ACTOR_PROPS),
        'bp_variables': dump_bp_variables(actor),
        'components': [],
    }

    capsule = actor.get_component_by_class(unreal.CapsuleComponent)
    if capsule:
        report['capsule'] = get_props(capsule, ('capsule_radius', 'capsule_half_height', 'capsule_half_height_with_scale'))

    mesh = actor.get_component_by_class(unreal.SkeletalMeshComponent)
    if mesh:
        report['character_mesh'] = dump_component(mesh)

    for comp in actor.get_components_by_class(unreal.ActorComponent):
        report['components'].append(dump_component(comp))

    json_path = os.path.join(OUT_DIR, 'tfa_base_character.json')
    with open(json_path, 'w', encoding='utf-8') as handle:
        json.dump(report, handle, indent=2, ensure_ascii=False)
    log('json written: %s' % json_path)

    md_path = os.path.join(OUT_DIR, 'tfa_base_character.md')
    with open(md_path, 'w', encoding='utf-8') as handle:
        handle.write('# BP_TFA_BaseCharacter dump\n\n')
        handle.write('- Blueprint: `%s`\n' % BP_PATH)
        handle.write('- Actor class: `%s`\n\n' % report['actor_class'])
        handle.write('## Actor properties\n\n')
        for key, value in report['actor_props'].items():
            handle.write('- `%s` = `%s`\n' % (key, value))
        handle.write('\n## Blueprint variables\n\n')
        for key, value in report['bp_variables'].items():
            handle.write('- `%s` = `%s`\n' % (key, value))
        if 'capsule' in report:
            handle.write('\n## Capsule\n\n')
            for key, value in report['capsule'].items():
                handle.write('- `%s` = `%s`\n' % (key, value))
        handle.write('\n## Components\n\n')
        for comp in report['components']:
            handle.write('### %s (%s)\n\n' % (comp['name'], comp['class']))
            handle.write('- attach_parent: `%s`\n' % comp.get('attach_parent'))
            handle.write('- attach_socket: `%s`\n' % comp.get('attach_socket'))
            for key in ('relative_location', 'relative_rotation', 'relative_scale3d', 'visible',
                        'hidden_in_game', 'auto_activate', 'component_tags'):
                if key in comp:
                    handle.write('- %s: `%s`\n' % (key, comp[key]))
            for group in ('camera', 'spring_arm', 'skeletal_mesh', 'static_mesh', 'post_process', 'spot_light'):
                if group in comp:
                    handle.write('- %s:\n' % group)
                    for key, value in comp[group].items():
                        handle.write('  - %s: `%s`\n' % (key, value))
            handle.write('\n')
    log('markdown written: %s' % md_path)
    log('DUMP DONE')


main()
