"""Reorganize the /Game/PVP logic assets into the layout defined by pvp_logic_layout.py.

What it does (idempotent):
  1. pre-flight: every planned source/target is classified as pending / already / overwrite /
     conflict / missing, and duplicate widgets are checked for external referencers;
  2. backup: duplicated widgets are copied to Saved/AssetBackup/<timestamp>/ with SHA256 first;
  3. delete: the stale widget copies under /Game/PVP/UI are removed;
  4. move: assets are renamed with EditorAssetLibrary.rename_asset (UE fixes up hard references);
  5. cleanup: ObjectRedirector leftovers are deleted and the touched packages are saved;
  6. verify: expected folders/counts and the absence of legacy paths are asserted;
  7. report: Saved/PVPAssetReorgReport/report.{json,md}.

Default mode is a dry run. Apply with either:
    UnrealEditor-Cmd.exe "<project>.uproject" -run=PythonScript \
        -Script="<project>/Content/Python/reorg_pvp_logic_assets.py" -unattended -nosplash -nullrhi
    (with the environment variable PVP_REORG_APPLY=1)

or by appending -Apply to the Python script arguments (sys.argv is also inspected for --apply).
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
REPORT_DIR = os.path.join(SAVED_DIR, 'PVPAssetReorgReport')
REPORT_JSON = os.path.join(REPORT_DIR, 'report.json')
REPORT_MD = os.path.join(REPORT_DIR, 'report.md')
DRY_RUN_REPORT_JSON = os.path.join(REPORT_DIR, 'dry_run.json')
DRY_RUN_REPORT_MD = os.path.join(REPORT_DIR, 'dry_run.md')
LOGIC_ROOT = '/Game/PVP'
ART_PACKAGES_TO_RESAVE = (
    '/Game/PVPDemo/Characters/Mannequins/Meshes/SKM_PVP_Manny',
)

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)) if '__file__' in globals()
                else os.path.join(CONTENT_DIR, 'Python'))
from pvp_logic_layout import (  # noqa: E402
    DELETE_PLAN,
    EXPECTED_ASSETS,
    EXPECTED_COUNTS,
    MOVE_PLAN,
    assets_in,
)

state = {
    'mode': 'dry_run',
    'timestamp': '',
    'moves': [],
    'deletes': [],
    'backups': [],
    'redirectors_removed': [],
    'saved': 0,
    'verification': [],
    'warnings': [],
    'errors': [],
}


def log(message):
    unreal.log('[PVP-REORG] %s' % message)


def warn(message):
    state['warnings'].append(message)
    unreal.log_warning('[PVP-REORG] %s' % message)


def fail(message):
    state['errors'].append(message)
    unreal.log_error('[PVP-REORG] %s' % message)


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
    """Absolute file path of a package under /Game (searches .uasset then .umap)."""
    rel = package.split('/Game/', 1)[1]
    for extension in ('.uasset', '.umap'):
        candidate = os.path.join(CONTENT_DIR, rel.replace('/', os.sep) + extension)
        if os.path.exists(candidate):
            return candidate
    return None


def target_package(source, target_dir):
    return '%s/%s' % (target_dir, source.rsplit('/', 1)[-1])


def hard_referencers(package):
    """Packages with a hard reference to `package`, excluding itself."""
    try:
        refs = unreal.EditorAssetLibrary.find_package_referencers_for_asset(package)
    except Exception as error:  # noqa: BLE001
        warn('cannot query referencers of %s: %s' % (package, error))
        return None
    result = set()
    for ref in refs or []:
        normalized = str(ref).split('.')[0]
        if normalized and normalized != package:
            result.add(normalized)
    return sorted(result)


def build_plan():
    log('=== pre-flight ===')
    for source, target_dir in MOVE_PLAN:
        target = target_package(source, target_dir)
        source_exists = asset_exists(source)
        target_exists = asset_exists(target)
        if source_exists and target_exists:
            status = 'overwrite' if target in DELETE_PLAN else 'conflict'
        elif source_exists:
            status = 'pending'
        elif target_exists:
            status = 'already'
        else:
            status = 'missing'
        state['moves'].append({
            'source': source,
            'target': target,
            'target_dir': target_dir,
            'status': status,
        })
        if status in ('conflict', 'missing'):
            fail('move %s -> %s is %s' % (source, target, status))

    moved_targets = {item['target'] for item in state['moves'] if item['status'] == 'already'}
    for package in DELETE_PLAN:
        exists = asset_exists(package)
        if not exists:
            status = 'already'
            referencers = []
        elif package in moved_targets:
            # The path is now occupied by the active copy moved here by a previous run.
            status = 'already'
            referencers = []
        else:
            referencers = hard_referencers(package)
            status = 'blocked' if referencers else 'pending'
            if referencers:
                fail('refusing to delete %s, referenced by %s' % (package, referencers))
        state['deletes'].append({
            'package': package,
            'status': status,
            'referencers': referencers,
        })

    pending_moves = [item for item in state['moves'] if item['status'] in ('pending', 'overwrite')]
    pending_deletes = [item for item in state['deletes'] if item['status'] == 'pending']
    log('moves pending=%d already=%d conflict=%d missing=%d' % (
        len(pending_moves),
        len([item for item in state['moves'] if item['status'] == 'already']),
        len([item for item in state['moves'] if item['status'] == 'conflict']),
        len([item for item in state['moves'] if item['status'] == 'missing'])))
    log('deletes pending=%d blocked=%d' % (len(pending_deletes),
                                           len([item for item in state['deletes'] if item['status'] == 'blocked'])))
    return pending_moves, pending_deletes


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


def delete_packages(pending_deletes, backup_root):
    log('=== delete stale copies ===')
    for item in pending_deletes:
        backup_package(item['package'], backup_root)
        if not unreal.EditorAssetLibrary.delete_asset(item['package']):
            fail('delete_asset failed: %s' % item['package'])
            continue
        item['status'] = 'deleted'
        log('deleted %s' % item['package'])


def move_packages(pending_moves):
    log('=== move assets ===')
    for item in pending_moves:
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
                unreal.EditorAssetLibrary.list_assets(LOGIC_ROOT, recursive=True, include_folder=False)]
    for package in sorted(set(packages)):
        if is_redirector(package):
            if unreal.EditorAssetLibrary.delete_asset(package):
                state['redirectors_removed'].append(package)
                log('deleted redirector %s' % package)
            else:
                fail('cannot delete redirector %s' % package)


def save_touched_packages():
    log('=== save packages ===')
    saved = 0
    packages = [package_name_of(path) for path in
                unreal.EditorAssetLibrary.list_assets(LOGIC_ROOT, recursive=True, include_folder=False)]
    packages.extend(ART_PACKAGES_TO_RESAVE)
    for package in sorted(set(packages)):
        file_path = package_file(package)
        if file_path and file_path.endswith('.umap'):
            # EditorAssetLibrary does not save level assets.
            continue
        try:
            if unreal.EditorAssetLibrary.save_asset(package, only_if_is_dirty=False):
                saved += 1
        except Exception as error:  # noqa: BLE001
            warn('cannot save %s: %s' % (package, error))
    state['saved'] = saved
    log('saved packages: %d' % saved)


def verify():
    log('=== verify ===')
    for item in state['moves']:
        target_ok = asset_exists(item['target'])
        source_ok = not asset_exists(item['source']) or is_redirector(item['source'])
        state['verification'].append({
            'label': 'move.%s' % item['source'].rsplit('/', 1)[-1],
            'ok': bool(target_ok and source_ok),
            'detail': 'target=%s source_clean=%s' % (target_ok, source_ok),
        })
    for item in state['deletes']:
        # The path itself is re-occupied by the moved copy, so verify the delete step instead.
        done = item['status'] in ('deleted', 'already')
        state['verification'].append({
            'label': 'delete.%s' % item['package'].rsplit('/', 1)[-1],
            'ok': done,
            'detail': 'status=%s' % item['status'],
        })
    for folder, expected_count in sorted(EXPECTED_COUNTS.items()):
        actual = len(unreal.EditorAssetLibrary.list_assets(
            '%s/%s' % (LOGIC_ROOT, folder), recursive=False, include_folder=False))
        state['verification'].append({
            'label': 'count.%s' % folder,
            'ok': actual == expected_count,
            'detail': '%d (expected %d)' % (actual, expected_count),
        })
    for folder in sorted(EXPECTED_ASSETS):
        for package in assets_in(folder):
            state['verification'].append({
                'label': 'asset.%s' % package.rsplit('/', 1)[-1],
                'ok': asset_exists(package),
                'detail': package,
            })
    failed = [item for item in state['verification'] if not item['ok']]
    if failed:
        fail('%d verification checks failed' % len(failed))
    else:
        log('verification passed: %d checks' % len(state['verification']))


def report_paths():
    """Dry runs keep their own report so the apply record is never overwritten."""
    if state['mode'] == 'dry_run':
        return DRY_RUN_REPORT_JSON, DRY_RUN_REPORT_MD
    return REPORT_JSON, REPORT_MD


def write_report():
    os.makedirs(REPORT_DIR, exist_ok=True)
    report_json, report_md = report_paths()
    state['timestamp'] = datetime.datetime.now().isoformat(timespec='seconds')
    with open(report_json, 'w', encoding='utf-8') as handle:
        json.dump(state, handle, indent=2, ensure_ascii=False, default=str)

    lines = ['# PVP logic asset reorganization report', '',
             '- mode: `%s`' % state['mode'],
             '- timestamp: `%s`' % state['timestamp'],
             '- moves: `%d` (pending %d)' % (len(state['moves']),
                                             len([item for item in state['moves'] if item['status'] == 'pending'])),
             '- deletes: `%d`' % len(state['deletes']),
             '- redirectors removed: `%d`' % len(state['redirectors_removed']),
             '- saved packages: `%d`' % state['saved'],
             '- verification failures: `%d`' % len([item for item in state['verification'] if not item['ok']]),
             '']
    lines.append('## Move plan')
    lines.append('')
    lines.append('| source | target | status |')
    lines.append('| --- | --- | --- |')
    for item in state['moves']:
        lines.append('| `%s` | `%s` | %s |' % (item['source'], item['target'], item['status']))
    lines.append('')
    lines.append('## Delete plan')
    lines.append('')
    lines.append('| package | status | referencers |')
    lines.append('| --- | --- | --- |')
    for item in state['deletes']:
        lines.append('| `%s` | %s | %s |' % (item['package'], item['status'], item['referencers']))
    if state['backups']:
        lines.append('')
        lines.append('## Backups')
        lines.append('')
        for item in state['backups']:
            lines.append('- `%s` -> `%s` (sha256 `%s`)' % (item['package'], item['backup'], item['sha256']))
    if state['warnings']:
        lines.append('')
        lines.append('## Warnings')
        lines.append('')
        for message in state['warnings']:
            lines.append('- %s' % message)
    if state['errors']:
        lines.append('')
        lines.append('## Errors')
        lines.append('')
        for message in state['errors']:
            lines.append('- %s' % message)
    with open(report_md, 'w', encoding='utf-8') as handle:
        handle.write('\n'.join(lines) + '\n')
    log('report written: %s' % report_md)


def parse_mode():
    argv = [argument.lower() for argument in sys.argv]
    if '--apply' in argv or '-apply' in argv or os.environ.get('PVP_REORG_APPLY') == '1':
        return 'apply'
    return 'dry_run'


def main():
    state['mode'] = parse_mode()
    log('=== reorg start (mode=%s) ===' % state['mode'])
    os.makedirs(REPORT_DIR, exist_ok=True)

    pending_moves, pending_deletes = build_plan()
    if state['errors']:
        warn('pre-flight reported errors, nothing was modified')

    if state['mode'] == 'apply' and not state['errors']:
        backup_root = os.path.join(SAVED_DIR, 'AssetBackup',
                                   datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
        delete_packages(pending_deletes, backup_root)
        move_packages(pending_moves)
        remove_redirectors()
        save_touched_packages()
        verify()
    elif state['mode'] == 'dry_run':
        log('dry run: no asset was modified')

    write_report()
    log('=== reorg done (errors=%d) ===' % len(state['errors']))
    if state['errors']:
        raise RuntimeError('%d errors during reorganization, see %s' % (len(state['errors']), report_paths()[1]))


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
