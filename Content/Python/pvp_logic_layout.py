"""Single source of truth for the /Game/PVP logic asset layout.

Pure Python on purpose: this module is imported by
  - Content/Python/reorg_pvp_logic_assets.py     (editor executor, needs `unreal`)
  - Content/Python/verify_pvp_asset_layout.py    (offline verifier, no editor)
  - Content/Python/setup_pvp_game_framework.py   (routing for normalize/copy phases)

Keeping the routing rules, the move/delete plan and the expected counts in one place prevents the
framework setup script from moving the animation assets back into Actors/ on its next run.
"""

LOGIC_ROOT = '/Game/PVP'

# Order matters: the longer prefixes must come first (ANS_ before AN_, BPI_ before BP_).
LOGIC_ROUTE_RULES = (
    ('ANS_', 'Animation/Notifies'),
    ('AN_', 'Animation/Notifies'),
    ('ABP_', 'Animation/AnimBP'),
    ('BPI_', 'Animation/Interfaces'),
    ('WBP_', 'UI'),
    ('E_', 'Types'),
    ('S_', 'Types'),
)

# Asset names that do not follow the prefix rule above.
SPECIAL_ROUTES = {
    'ABP_PVP_ConstructionLamp_Large': 'Animation/AnimBP/Environment',
    'BP_PVP_BaseCharacter': 'Characters',
}

# Asset class -> sub folder, used by setup_pvp_game_framework.py when it has no asset name prefix
# information (e.g. an asset copied straight from the third-party package).
CLASS_SUBFOLDERS = {
    'InputAction': 'Input',
    'InputMappingContext': 'Input',
    'WidgetBlueprint': 'UI',
    'AnimBlueprint': 'Animation/AnimBP',
    'BlueprintInterface': 'Animation/Interfaces',
    'UserDefinedEnum': 'Types',
    'Enum': 'Types',
    'UserDefinedStruct': 'Types',
    'Struct': 'Types',
    'DataTable': 'Data',
    'CurveTable': 'Data',
    'StringTable': 'Data',
    'DataAsset': 'Data',
}

# Explicit moves. Ordered so that referenced (leaf) assets move before the assets that reference
# them, which keeps the number of re-serialized referencers low.
MOVE_PLAN = (
    # Animation / Notifies
    ('/Game/PVP/Actors/AN_PVP_DropMagazine', '/Game/PVP/Animation/Notifies'),
    ('/Game/PVP/Actors/AN_PVP_EjectCasing', '/Game/PVP/Animation/Notifies'),
    ('/Game/PVP/Actors/AN_PVP_SpawnObjectAttached', '/Game/PVP/Animation/Notifies'),
    ('/Game/PVP/Actors/AN_PVP_ThrowPhysicsObject', '/Game/PVP/Animation/Notifies'),
    ('/Game/PVP/Actors/AN_PVP_UnlockActions', '/Game/PVP/Animation/Notifies'),
    ('/Game/PVP/Actors/ANS_PVP_BlockADS', '/Game/PVP/Animation/Notifies'),
    ('/Game/PVP/Actors/ANS_PVP_HideMainMag', '/Game/PVP/Animation/Notifies'),
    ('/Game/PVP/Actors/ANS_PVP_LeftHandGrip', '/Game/PVP/Animation/Notifies'),
    ('/Game/PVP/Actors/ANS_PVP_ShowReserveMag', '/Game/PVP/Animation/Notifies'),
    # Types
    ('/Game/PVP/Actors/E_PVP_CameraPerspectives', '/Game/PVP/Types'),
    ('/Game/PVP/Actors/E_PVP_FireMode', '/Game/PVP/Types'),
    ('/Game/PVP/Actors/E_PVP_GripAttachment', '/Game/PVP/Types'),
    ('/Game/PVP/Actors/E_PVP_Stance', '/Game/PVP/Types'),
    ('/Game/PVP/Actors/S_PVP_SpringData', '/Game/PVP/Types'),
    # Animation / Interfaces
    ('/Game/PVP/Characters/BPI_PVP_AnimationState', '/Game/PVP/Animation/Interfaces'),
    # Animation / AnimBP
    ('/Game/PVP/Actors/ABP_PVP_ConstructionLamp_Large', '/Game/PVP/Animation/AnimBP/Environment'),
    ('/Game/PVP/Characters/ABP_PVP_Manny_PostProcess', '/Game/PVP/Animation/AnimBP'),
    ('/Game/PVP/Actors/ABP_PVP_FP_BaseCharacter', '/Game/PVP/Animation/AnimBP'),
    ('/Game/PVP/Actors/ABP_PVP_TP_BaseCharacter', '/Game/PVP/Animation/AnimBP'),
    # Character blueprints
    ('/Game/PVP/Actors/BP_PVP_BaseCharacter', '/Game/PVP/Characters'),
    # Widget blueprints (the duplicated copies under /Game/PVP/UI are deleted first, see DELETE_PLAN)
    ('/Game/PVP/Actors/WBP_PVP_BodycamOverlay', '/Game/PVP/UI'),
    ('/Game/PVP/Actors/WBP_PVP_Demo', '/Game/PVP/UI'),
)

# Stale copies that are superseded by the moved assets above. Verified to have no external
# referencers before they are deleted; the active copies live under /Game/PVP/Actors and are
# moved into /Game/PVP/UI afterwards.
DELETE_PLAN = (
    '/Game/PVP/UI/WBP_PVP_BodycamOverlay',
    '/Game/PVP/UI/WBP_PVP_Demo',
)

# Exact asset names per folder after the reorganization (package name without path/extension).
EXPECTED_ASSETS = {
    'Actors': (
        'BP_PVP_Attachment_Laser',
        'BP_PVP_BaseConfig',
        'BP_PVP_BaseMagazine',
        'BP_PVP_BasePhysicsObject',
        'BP_PVP_BaseWeapon',
        'BP_PVP_ConstructionLamp_Base',
        'BP_PVP_DemoInformation',
        'BP_PVP_PhysicsCasing',
        'BP_PVP_PhysicsMagazine',
        'BP_PVP_PhysicsSyringe',
        'BP_PVP_PhysicsSyringeCap',
        'BP_PVP_Syringe',
        'BP_PVP_Wall_Frame_Door_Plaster',
        'BP_PVP_Wall_Frame_Door_Wood',
        'BP_PVP_Wall_Frame_Plaster',
        'BP_PVP_Wall_Frame_Studs',
        'BP_PVP_Wall_Frame_Window_Plaster',
        'BP_PVP_Wall_Frame_Window_Wood',
        'BP_PVP_Wall_Frame_Wood',
        'BP_PVP_Weapon_AR',
    ),
    'Animation/AnimBP': (
        'ABP_PVP_FP_BaseCharacter',
        'ABP_PVP_FP_Locomotion',
        'ABP_PVP_FP_Weapon_AR',
        'ABP_PVP_TP_BaseCharacter',
        'ABP_PVP_Manny_PostProcess',
    ),
    'Animation/AnimBP/Environment': (
        'ABP_PVP_ConstructionLamp_Large',
    ),
    'Animation/Interfaces': (
        'BPI_PVP_AnimationState',
    ),
    'Animation/Notifies': (
        'AN_PVP_DropMagazine',
        'AN_PVP_EjectCasing',
        'AN_PVP_SpawnObjectAttached',
        'AN_PVP_ThrowPhysicsObject',
        'AN_PVP_UnlockActions',
        'ANS_PVP_BlockADS',
        'ANS_PVP_HideMainMag',
        'ANS_PVP_LeftHandGrip',
        'ANS_PVP_ShowReserveMag',
    ),
    'Characters': (
        'BP_PVPBaseCharacter',
        'BP_PVP_BaseCharacter',
    ),
    'Data': (
        'DT_PVP_TextStyle',
    ),
    'Game': (
        'BP_PVPGameMode',
        'BP_PVPPlayerController',
    ),
    'Maps': (
        'L_PVP_Showcase',
    ),
    'Types': (
        'E_PVP_CameraPerspectives',
        'E_PVP_FireMode',
        'E_PVP_GripAttachment',
        'E_PVP_Stance',
        'S_PVP_SpringData',
    ),
    'UI': (
        'WBP_PVP_BodycamOverlay',
        'WBP_PVP_Demo',
    ),
}

# Input/ is generated by setup_pvp_input.py; only its size is asserted here.
EXPECTED_COUNTS = {
    'Actors': 20,
    'Animation/AnimBP': 5,
    'Animation/AnimBP/Environment': 1,
    'Animation/Interfaces': 1,
    'Animation/Notifies': 9,
    'Characters': 2,
    'Data': 1,
    'Game': 2,
    'Input': 31,
    'Maps': 1,
    'Types': 5,
    'UI': 2,
}
EXPECTED_TOTAL = 80

# Legacy paths that must not survive anywhere in Content/PVP or Content/PVPDemo after the move.
LEGACY_PATH_PREFIXES = (
    '/Game/PVP/Actors/ABP_',
    '/Game/PVP/Actors/AN_',
    '/Game/PVP/Actors/ANS_',
    '/Game/PVP/Actors/E_',
    '/Game/PVP/Actors/S_',
    '/Game/PVP/Actors/WBP_',
    '/Game/PVP/Actors/BP_PVP_BaseCharacter',
    '/Game/PVP/Characters/ABP_',
    '/Game/PVP/Characters/BPI_',
)

# References that must exist after the move (package path -> expected dependency package path).
# BP_PVPBaseCharacter is intentionally absent: it inherits Mesh / AnimClass from APVPBaseCharacter,
# whose defaults are asserted separately against the C++ source (source.character_abp / _mesh).
EXPECTED_REFERENCES = {
    'PVP/Characters/BP_PVP_BaseCharacter': '/Game/PVP/Animation/AnimBP/ABP_PVP_FP_BaseCharacter',
    'PVPDemo/Characters/Mannequins/Meshes/SKM_PVP_Manny': '/Game/PVP/Animation/AnimBP/ABP_PVP_Manny_PostProcess',
}


def subfolder_for_asset_name(name):
    """Folder under /Game/PVP for a logic asset name, or None when no prefix rule matches."""
    for prefix, subfolder in LOGIC_ROUTE_RULES:
        if name.startswith(prefix):
            return subfolder
    if name.startswith('BP_'):
        return 'Actors'
    return None


def route_for_asset_name(name):
    """Folder under /Game/PVP for a logic asset name (never None)."""
    if name in SPECIAL_ROUTES:
        return SPECIAL_ROUTES[name]
    return subfolder_for_asset_name(name) or 'Actors'


def target_dir_for_asset_name(name):
    return '%s/%s' % (LOGIC_ROOT, route_for_asset_name(name))


def target_package_for(source_package):
    """Destination package of a managed asset, keeping its name."""
    name = source_package.rsplit('/', 1)[-1]
    return '%s/%s' % (target_dir_for_asset_name(name), name)


def project_copy_name(source_name):
    """Third-party asset name -> project name (TFA is replaced by PVP, others get a PVP_ prefix)."""
    return source_name.replace('TFA', 'PVP') if 'TFA' in source_name else 'PVP_' + source_name


def project_copy_package(source_package):
    """Destination package for a third-party asset copied into the project."""
    name = project_copy_name(source_package.rsplit('/', 1)[-1])
    return '%s/%s' % (target_dir_for_asset_name(name), name)


def subfolder_for_class(class_name):
    """Folder under /Game/PVP derived from the UClass name (used when the name prefix is unknown)."""
    return CLASS_SUBFOLDERS.get(class_name, 'Actors')


def assets_in(folder):
    return ['%s/%s/%s' % (LOGIC_ROOT, folder, name) for name in EXPECTED_ASSETS[folder]]
