"""Reorganize the assault rifle attachment meshes into per-slot sub folders.

The assault rifle ships as a bare receiver skeleton mesh (SK_PVP_AR) plus loose attachment
meshes. This script classifies those loose meshes by attachment slot and moves them under
/Game/PVPDemo/Weapons/AssaultRifle/Attachments/<Group>/:

    Handguard  3   SM_PVP_AR_Handguard_Default / _Extended / _Short
    Sight      4   SM_PVP_AR_ATT_Sight_Front / _Front_Folded / _Rear / _Rear_Folded
    Optic      1   SM_PVP_AR_ATT_Scope
    Grip       2   SM_PVP_AR_ATT_Grip_Angled / _Vertical
    Muzzle     1   SM_PVP_AR_ATT_Silencer
    Laser      1   SM_PVP_AR_ATT_Laser
    Magazine   8   SK / SKEL / PHYS_PVP_AR_Magazine(+Transparent) and 4 static variants

Non-assembly assets are classified but stay in Meshes/: SM_PVP_AR_Bullet and
SM_PVP_AR_Bullet_Casing (ammunition) plus SM_PVP_AR_GunCase / SM_PVP_AR_GunCase_Lid (prop).
The casing and the gun case are referenced by L_PVP_Showcase, so moving them would force a level
re-save; keeping them in place keeps this reorganization limited to the weapon folder.

The receiver keeps its path in Meshes/ so every existing reference (SKEL_PVP_AR, the weapon
AnimBlueprint, ABP_PVP_FP_Weapon_AR, physics assets) stays valid.

What it does (idempotent):
  1. pre-flight: every planned source/target is classified as pending / already / overwrite /
     conflict / missing, and referencers outside the moved set are reported;
  2. backup: every pending source package is copied to Saved/AssetBackup/<timestamp>/ with SHA256;
  3. move: EditorAssetLibrary.rename_asset (UE fixes up hard references);
  4. cleanup: ObjectRedirector leftovers under the weapon root are deleted;
  5. save: touched packages are saved;
  6. verify: per-group counts, receiver-only Meshes/ folder, no legacy asset left behind;
  7. report: Saved/PVPAttachmentReport/reorg.{json,md}.

Default mode is a dry run. Apply with either the environment variable PVP_ATTACHMENT_REORG_APPLY=1
or by appending --apply to the script arguments.

Run headless (editor closed):
    UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript \
        -Script="<project>/Content/Python/reorg_pvp_weapon_attachments.py" -unattended -nosplash -nullrhi
"""

import datetime
import hashlib
import json
import os
import shutil
import sys

import unreal

CONTENT_DIR = unreal.Paths.project_content_dir().replace('\\', '/')
SAVED_DIR = unreal.Paths.project_saved_dir().replace('\\', '/')
REPORT_DIR = os.path.join(SAVED_DIR, 'PVPAttachmentReport')
REPORT_JSON = os.path.join(REPORT_DIR, 'reorg.json')
REPORT_MD = os.path.join(REPORT_DIR, 'reorg.md')
DRY_RUN_REPORT_JSON = os.path.join(REPORT_DIR, 'reorg_dry_run.json')
DRY_RUN_REPORT_MD = os.path.join(REPORT_DIR, 'reorg_dry_run.md')

WEAPON_ROOT = '/Game/PVPDemo/Weapons/AssaultRifle'
MESH_ROOT = WEAPON_ROOT + '/Meshes'
ATTACHMENTS_ROOT = WEAPON_ROOT + '/Attachments'

# Receiver meshes stay in Meshes/: the skeleton, the bare rifle mesh and its physics asset.
BASE_ASSETS = ('SK_PVP_AR', 'SKEL_PVP_AR', 'PHYS_PVP_AR')

# Slot groups that are actually assembled on the weapon (see setup_pvp_weapon_assembly.py).
ATTACHMENT_GROUPS = {
    'Handguard': (
        'SM_PVP_AR_Handguard_Default',
        'SM_PVP_AR_Handguard_Extended',
        'SM_PVP_AR_Handguard_Short',
    ),
    'Sight': (
        'SM_PVP_AR_ATT_Sight_Front',
        'SM_PVP_AR_ATT_Sight_Front_Folded',
        'SM_PVP_AR_ATT_Sight_Rear',
        'SM_PVP_AR_ATT_Sight_Rear_Folded',
    ),
    'Optic': (
        'SM_PVP_AR_ATT_Scope',
    ),
    'Grip': (
        'SM_PVP_AR_ATT_Grip_Angled',
        'SM_PVP_AR_ATT_Grip_Vertical',
    ),
    'Muzzle': (
        'SM_PVP_AR_ATT_Silencer',
    ),
    'Laser': (
        'SM_PVP_AR_ATT_Laser',
    ),
    'Magazine': (
        'SK_PVP_AR_Magazine',
        'SK_PVP_AR_Magazine_Transparent',
        'SKEL_PVP_AR_Magazine',
        'PHYS_PVP_AR_Magazine',
        'SM_PVP_AR_Magazine_Full',
        'SM_PVP_AR_Magazine_Empty',
        'SM_PVP_AR_Magazine_Transparent_Full',
        'SM_PVP_AR_Magazine_Transparent_Empty',
    ),
}

# Classified as non-assembly assets: ammunition and the gun case prop.
# They stay in Meshes/ on purpose - L_PVP_Showcase references the casing and gun case meshes, and
# moving a level-referenced asset would require re-saving the level package.
SUPPORT_ASSETS = (
    'SM_PVP_AR_Bullet',
    'SM_PVP_AR_Bullet_Casing',
    'SM_PVP_AR_GunCase',
    'SM_PVP_AR_GunCase_Lid',
)

SUPPORT_NOTES = {
    'SM_PVP_AR_Bullet': '非装配件：子弹，走 SOCKET_Bullet_Chambered',
    'SM_PVP_AR_Bullet_Casing': '非装配件：弹壳，走 SOCKET_Eject_Casing（被关卡引用，保留原路径）',
    'SM_PVP_AR_GunCase': '非装配件：枪箱，关卡道具（被关卡引用，保留原路径）',
    'SM_PVP_AR_GunCase_Lid': '非装配件：枪箱盖，关卡道具（被关卡引用，保留原路径）',
}

GROUP_ORDER = ('Handguard', 'Sight', 'Optic', 'Grip', 'Muzzle', 'Laser', 'Magazine')

GROUP_NOTES = {
    'Handguard': '护木：SOCKET_Handguard，自带 Grip / Laser / Muzzle / Sight_Front 子挂点',
    'Sight': '机瞄：前准星挂护木 SOCKET_Sight_Front，后照门挂枪身 SOCKET_Sight_Rear',
    'Optic': '光学瞄具：挂枪身 SOCKET_Scope',
    'Grip': '前握把：挂护木 SOCKET_Grip',
    'Muzzle': '枪口：挂护木 SOCKET_Muzzle',
    'Laser': '激光：挂护木 SOCKET_Laser',
    'Magazine': '弹匣：挂枪身 SOCKET_Magazine（静态满/空/透明变体 + 骨骼弹匣）',
}


def slot_group_of(asset_name):
    """Return the classification group of an attachment asset, or None for receiver / support."""
    for group in GROUP_ORDER:
        if asset_name in ATTACHMENT_GROUPS[group]:
            return group
    return None


def final_path(asset_name):
    """Package path after the reorganization (used by setup_pvp_weapon_assembly.py)."""
    group = slot_group_of(asset_name)
    if group is None:
        return '%s/%s' % (MESH_ROOT, asset_name)
    return '%s/%s/%s' % (ATTACHMENTS_ROOT, group, asset_name)


def build_move_plan():
    plan = []
    for group in GROUP_ORDER:
        names = ATTACHMENT_GROUPS[group]
        target_dir = '%s/%s' % (ATTACHMENTS_ROOT, group)
        for name in names:
            plan.append({
                'asset': name,
                'group': group,
                'source': '%s/%s' % (MESH_ROOT, name),
                'target_dir': target_dir,
                'target': '%s/%s' % (target_dir, name),
            })
    return plan


MOVE_PLAN = build_move_plan()

state = {
    'mode': 'dry_run',
    'timestamp': '',
    'moves': [],
    'backups': [],
    'redirectors_removed': [],
    'referencers': [],
    'saved': 0,
    'verification': [],
    'warnings': [],
    'errors': [],
}


def log(message):
    unreal.log('[PVP-ATTACH-REORG] %s' % message)


def warn(message):
    state['warnings'].append(message)
    unreal.log_warning('[PVP-ATTACH-REORG] %s' % message)


def fail(message):
    state['errors'].append(message)
    unreal.log_error('[PVP-ATTACH-REORG] %s' % message)


def asset_exists(path):
    return unreal.EditorAssetLibrary.does_asset_exist(path)


def package_name_of(asset_path):
    return asset_path.split('.')[0]


def is_redirector(package):
    """ObjectRedirector is not exposed to Python, so detect it by class name."""
    asset = unreal.EditorAssetLibrary.load_asset(package)
    return asset is not None and asset.get_class().get_name() == 'ObjectRedirector'


def sha256_of(path):
    if not os.path.exists(path):
        return None
    digest = hashlib.sha256()
    with open(path, 'rb') as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def package_file(package):
    rel = package.split('/Game/', 1)[1]
    for extension in ('.uasset', '.umap'):
        candidate = os.path.join(CONTENT_DIR, rel.replace('/', os.sep) + extension)
        if os.path.exists(candidate):
            return candidate
    return None


def hard_referencers(package):
    try:
        refs = unreal.EditorAssetLibrary.find_package_referencers_for_asset(package)
    except Exception as error:  # noqa: BLE001
        warn('cannot query referencers of %s: %s' % (package, error))
        return None
    return sorted({
        str(ref).split('.')[0] for ref in (refs or [])
        if str(ref).split('.')[0] and str(ref).split('.')[0] != package
    })


def build_plan():
    log('=== pre-flight ===')
    moved_sources = {plan['source'] for plan in MOVE_PLAN}
    for item in state['moves']:
        source_exists = asset_exists(item['source'])
        target_exists = asset_exists(item['target'])
        if source_exists and target_exists:
            status = 'conflict'
        elif source_exists:
            status = 'pending'
        elif target_exists:
            status = 'already'
        else:
            status = 'missing'
        item['status'] = status
        if status == 'pending':
            referencers = hard_referencers(item['source'])
            # Referencers inside the moved set are fine: UE rewrites them during the rename.
            external = [ref for ref in (referencers or []) if ref not in moved_sources]
            item['external_referencers'] = external
            if external:
                warn('%s is referenced from outside the moved set: %s' % (item['source'], external))
                for referencer in external:
                    referencer_file = package_file(referencer)
                    if referencer_file and referencer_file.endswith('.umap'):
                        # Level packages cannot be saved by EditorAssetLibrary, so the old path would
                        # survive on disk after the redirector is removed. Keep support assets in place.
                        fail('%s is referenced by level %s; move it out of the plan or re-save the level manually'
                             % (item['source'], referencer))
                    elif referencer not in state['referencers']:
                        state['referencers'].append(referencer)
        if status in ('conflict', 'missing'):
            fail('move %s -> %s is %s' % (item['source'], item['target'], status))

    pending = [item for item in state['moves'] if item['status'] == 'pending']
    log('moves pending=%d already=%d conflict=%d missing=%d' % (
        len(pending),
        len([item for item in state['moves'] if item['status'] == 'already']),
        len([item for item in state['moves'] if item['status'] == 'conflict']),
        len([item for item in state['moves'] if item['status'] == 'missing'])))
    return pending


def backup_package(package, backup_root):
    file_path = package_file(package)
    if not file_path:
        warn('backup skipped, file not found: %s' % package)
        return None
    rel = os.path.relpath(file_path, CONTENT_DIR)
    backup_path = os.path.join(backup_root, rel)
    os.makedirs(os.path.dirname(backup_path), exist_ok=True)
    shutil.copy2(file_path, backup_path)
    entry = {
        'package': package,
        'file': file_path.replace('\\', '/'),
        'backup': backup_path.replace('\\', '/'),
        'sha256': sha256_of(file_path),
    }
    state['backups'].append(entry)
    log('backed up %s -> %s' % (package, entry['backup']))
    return entry


def move_packages(pending_moves, backup_root):
    log('=== move assets ===')
    for item in pending_moves:
        backup_package(item['source'], backup_root)
        if not unreal.EditorAssetLibrary.make_directory(item['target_dir']):
            warn('make_directory returned False (already exists?): %s' % item['target_dir'])
        if not unreal.EditorAssetLibrary.rename_asset(item['source'], item['target']):
            fail('rename_asset failed: %s -> %s' % (item['source'], item['target']))
            continue
        item['status'] = 'moved'
        log('moved %s -> %s' % (item['source'], item['target']))


def remove_redirectors():
    log('=== remove redirectors ===')
    packages = [package_name_of(path) for path in
                unreal.EditorAssetLibrary.list_assets(WEAPON_ROOT, recursive=True, include_folder=False)]
    for package in sorted(set(packages)):
        if is_redirector(package):
            if unreal.EditorAssetLibrary.delete_asset(package):
                state['redirectors_removed'].append(package)
                log('deleted redirector %s' % package)
            else:
                fail('cannot delete redirector %s' % package)


def save_touched_packages():
    """Save only the moved assets plus the referencers whose hard references the rename rewrote.

    Saving every package under the weapon root would re-serialize hundreds of untouched animations
    and textures, polluting the diff without changing anything.
    """
    log('=== save packages ===')
    packages = [item['target'] for item in state['moves'] if item['status'] == 'moved']
    packages.extend(state['referencers'])
    saved = 0
    for package in sorted(set(packages)):
        file_path = package_file(package)
        if file_path and file_path.endswith('.umap'):
            warn('skipping level package %s (levels must be saved from the editor)' % package)
            continue
        try:
            if unreal.EditorAssetLibrary.save_asset(package, only_if_is_dirty=False):
                saved += 1
        except Exception as error:  # noqa: BLE001
            warn('cannot save %s: %s' % (package, error))
    state['saved'] = saved
    log('saved packages: %d' % saved)


def assets_in_dir(directory):
    return sorted({
        package_name_of(path).rsplit('/', 1)[-1]
        for path in unreal.EditorAssetLibrary.list_assets(directory, recursive=False, include_folder=False)
    })


def verify():
    log('=== verify ===')
    expected_mesh_root = set(BASE_ASSETS) | set(SUPPORT_ASSETS)
    actual_mesh_root = set(assets_in_dir(MESH_ROOT))
    state['verification'].append({
        'label': 'meshes.receiver_and_support_only',
        'ok': actual_mesh_root == expected_mesh_root,
        'detail': 'actual=%s expected=%s' % (sorted(actual_mesh_root), sorted(expected_mesh_root)),
    })

    total_after = len(actual_mesh_root)
    for group in GROUP_ORDER:
        names = ATTACHMENT_GROUPS[group]
        directory = '%s/%s' % (ATTACHMENTS_ROOT, group)
        actual = assets_in_dir(directory)
        ok = set(actual) == set(names)
        state['verification'].append({
            'label': 'group.%s' % group,
            'ok': ok,
            'detail': '%d (expected %d) %s' % (len(actual), len(names),
                                               '' if ok else 'actual=%s' % sorted(actual)),
        })
        total_after += len(actual)

    expected_total = len(BASE_ASSETS) + len(SUPPORT_ASSETS) + len(MOVE_PLAN)
    state['verification'].append({
        'label': 'asset_total_conserved',
        'ok': total_after == expected_total,
        'detail': '%d (expected %d)' % (total_after, expected_total),
    })

    for name in SUPPORT_ASSETS:
        state['verification'].append({
            'label': 'support.%s' % name,
            'ok': asset_exists('%s/%s' % (MESH_ROOT, name)),
            'detail': 'kept in Meshes/',
        })

    for item in state['moves']:
        target_ok = asset_exists(item['target'])
        source_gone = not asset_exists(item['source']) or is_redirector(item['source'])
        state['verification'].append({
            'label': 'move.%s' % item['asset'],
            'ok': bool(target_ok and source_gone),
            'detail': 'target=%s source_clean=%s' % (target_ok, source_gone),
        })

    failed = [item for item in state['verification'] if not item['ok']]
    if failed:
        fail('%d verification checks failed' % len(failed))
    else:
        log('verification passed: %d checks' % len(state['verification']))


def report_paths():
    if state['mode'] == 'dry_run':
        return DRY_RUN_REPORT_JSON, DRY_RUN_REPORT_MD
    return REPORT_JSON, REPORT_MD


def write_report():
    os.makedirs(REPORT_DIR, exist_ok=True)
    report_json, report_md = report_paths()
    state['timestamp'] = datetime.datetime.now().isoformat(timespec='seconds')
    with open(report_json, 'w', encoding='utf-8') as handle:
        json.dump(state, handle, indent=2, ensure_ascii=False, default=str)

    lines = ['# PVP weapon attachment reorganization report', '',
             '- mode: `%s`' % state['mode'],
             '- timestamp: `%s`' % state['timestamp'],
             '- attachments root: `%s`' % ATTACHMENTS_ROOT,
             '- planned moves: `%d` (pending %d)' % (
                 len(state['moves']),
                 len([item for item in state['moves'] if item['status'] == 'pending'])),
             '- redirectors removed: `%d`' % len(state['redirectors_removed']),
             '- saved packages: `%d`' % state['saved'],
             '- verification failures: `%d`' % len([item for item in state['verification'] if not item['ok']]),
             '',
             '## Classification',
             '',
             '| group | count | note |',
             '| --- | --- | --- |']
    for group in GROUP_ORDER:
        names = ATTACHMENT_GROUPS[group]
        lines.append('| `%s` | %d | %s |' % (group, len(names), GROUP_NOTES.get(group, '')))
    lines.extend(['', '## Non-assembly assets (stay in Meshes/)', '',
                  '| asset | note |',
                  '| --- | --- |'])
    for name in SUPPORT_ASSETS:
        lines.append('| `%s` | %s |' % (name, SUPPORT_NOTES.get(name, '')))
    lines.extend(['', '## Move plan', '',
                  '| asset | group | source | target | status |',
                  '| --- | --- | --- | --- | --- |'])
    for item in state['moves']:
        lines.append('| `%s` | %s | `%s` | `%s` | %s |' % (
            item['asset'], item['group'], item['source'], item['target'], item['status']))
    if state['backups']:
        lines.extend(['', '## Backups', ''])
        for item in state['backups']:
            lines.append('- `%s` -> `%s` (sha256 `%s`)' % (item['package'], item['backup'], item['sha256']))
    lines.extend(['', '## Verification', '',
                  '| check | result | detail |',
                  '| --- | --- | --- |'])
    for item in state['verification']:
        lines.append('| `%s` | %s | %s |' % (
            item['label'], 'PASS' if item['ok'] else 'FAIL', str(item['detail']).replace('|', '/')))
    if state['warnings']:
        lines.extend(['', '## Warnings', ''])
        for message in state['warnings']:
            lines.append('- %s' % message)
    if state['errors']:
        lines.extend(['', '## Errors', ''])
        for message in state['errors']:
            lines.append('- %s' % message)
    with open(report_md, 'w', encoding='utf-8') as handle:
        handle.write('\n'.join(lines) + '\n')
    log('report written: %s' % report_md)


def parse_mode():
    argv = [argument.lower() for argument in sys.argv]
    if '--apply' in argv or '-apply' in argv or os.environ.get('PVP_ATTACHMENT_REORG_APPLY') == '1':
        return 'apply'
    return 'dry_run'


def main():
    state['mode'] = parse_mode()
    state['moves'] = [dict(item) for item in MOVE_PLAN]
    log('=== attachment reorg start (mode=%s) ===' % state['mode'])
    os.makedirs(REPORT_DIR, exist_ok=True)

    pending_moves = build_plan()
    if state['mode'] == 'apply' and not state['errors']:
        backup_root = os.path.join(SAVED_DIR, 'AssetBackup',
                                   datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
        move_packages(pending_moves, backup_root)
        remove_redirectors()
        save_touched_packages()
        verify()
    elif state['mode'] == 'dry_run':
        log('dry run: no asset was modified')

    write_report()
    log('=== attachment reorg done (errors=%d) ===' % len(state['errors']))
    if state['errors']:
        raise RuntimeError('%d errors during attachment reorganization, see %s' % (
            len(state['errors']), report_paths()[1]))


if __name__ == '__main__':
    try:
        main()
    except Exception as error:  # noqa: BLE001
        fail('FATAL: %s' % error)
        try:
            write_report()
        except Exception:  # noqa: BLE001
            pass
        raise
