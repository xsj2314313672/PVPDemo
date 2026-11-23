"""Offline verifier for the /Game/PVP logic asset layout (no editor required).

Checks:
  1. every expected asset file exists under Content/PVP/<folder>, legacy paths are gone;
  2. per-folder asset counts (and the 77-asset total) match the layout definition;
  3. no /Game/PVP/Actors/<Animation|Type|UI asset> legacy path string survives inside
     Content/PVP or Content/PVPDemo packages;
  4. key references (character blueprints -> ABP, mannequin -> post process ABP) point at the
     new animation folders.

Writes Saved/PVPAssetReorgReport/verify.{json,md} and exits non-zero on failure.

Run:
    python Content/Python/verify_pvp_asset_layout.py
"""

import json
import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(os.path.dirname(SCRIPT_DIR))
CONTENT_DIR = os.path.join(PROJECT_DIR, 'Content')
REPORT_DIR = os.path.join(PROJECT_DIR, 'Saved', 'PVPAssetReorgReport')
REPORT_JSON = os.path.join(REPORT_DIR, 'verify.json')
REPORT_MD = os.path.join(REPORT_DIR, 'verify.md')

sys.path.insert(0, SCRIPT_DIR)
from pvp_logic_layout import (  # noqa: E402
    EXPECTED_ASSETS,
    EXPECTED_COUNTS,
    EXPECTED_REFERENCES,
    EXPECTED_TOTAL,
    LEGACY_PATH_PREFIXES,
    LOGIC_ROOT,
)

PACKAGE_EXTENSIONS = ('.uasset', '.umap')
SCAN_ROOTS = ('PVP', 'PVPDemo')


def relative_package_path(package):
    """'/Game/PVP/Types/E_PVP_Stance' -> 'PVP/Types/E_PVP_Stance'."""
    return package[len('/Game/'):] if package.startswith('/Game/') else package


def asset_file(package):
    rel = relative_package_path(package).replace('/', os.sep)
    for extension in PACKAGE_EXTENSIONS:
        candidate = os.path.join(CONTENT_DIR, rel + extension)
        if os.path.exists(candidate):
            return candidate
    return None


def collect_folder_assets(folder):
    folder_path = os.path.join(CONTENT_DIR, 'PVP', folder.replace('/', os.sep))
    if not os.path.isdir(folder_path):
        return []
    names = []
    for filename in os.listdir(folder_path):
        if not filename.endswith(PACKAGE_EXTENSIONS):
            continue
        names.append(os.path.splitext(filename)[0])
    return sorted(names)


def scan_legacy_paths():
    """Map legacy prefix -> [(file, sample path)] for every package under the scanned roots."""
    hits = {}
    for root in SCAN_ROOTS:
        root_path = os.path.join(CONTENT_DIR, root)
        if not os.path.isdir(root_path):
            continue
        for dirpath, _dirnames, filenames in os.walk(root_path):
            for filename in filenames:
                if not filename.endswith(PACKAGE_EXTENSIONS):
                    continue
                file_path = os.path.join(dirpath, filename)
                with open(file_path, 'rb') as handle:
                    data = handle.read()
                for prefix in LEGACY_PATH_PREFIXES:
                    if prefix.encode('ascii') in data:
                        sample = re.search(re.escape(prefix.encode('ascii')) + rb'[A-Za-z0-9_]*', data)
                        hits.setdefault(prefix, []).append({
                            'file': os.path.relpath(file_path, CONTENT_DIR).replace('\\', '/'),
                            'sample': sample.group(0).decode('ascii') if sample else prefix,
                        })
    return hits


def read_package(package):
    path = asset_file(package)
    if not path:
        return b''
    with open(path, 'rb') as handle:
        return handle.read()


def main():
    checks = []

    def check(label, ok, detail=''):
        checks.append({'label': label, 'ok': bool(ok), 'detail': str(detail)})
        print('%s %s%s' % ('PASS' if ok else 'FAIL', label, (': %s' % detail) if detail else ''))
        return ok

    # 1. expected assets exist, nothing else is in the managed folders.
    for folder, names in sorted(EXPECTED_ASSETS.items()):
        actual = collect_folder_assets(folder)
        expected = sorted(names)
        missing = [name for name in expected if name not in actual]
        unexpected = [name for name in actual if name not in expected]
        check('layout.%s.assets' % folder, not missing and not unexpected,
              'missing=%s unexpected=%s' % (missing, unexpected) or '')
        check('layout.%s.count' % folder, len(actual) == EXPECTED_COUNTS[folder],
              '%d (expected %d)' % (len(actual), EXPECTED_COUNTS[folder]))

    # 2. per-package existence for every expected asset.
    missing_packages = [package for folder in EXPECTED_ASSETS for package in
                        ['%s/%s/%s' % (LOGIC_ROOT, folder, name) for name in EXPECTED_ASSETS[folder]]
                        if not asset_file(package)]
    check('layout.packages.exist', not missing_packages, missing_packages)

    # 3. total count and no legacy directory leftovers.
    total = sum(len(collect_folder_assets(folder)) for folder in EXPECTED_COUNTS)
    check('layout.total', total == EXPECTED_TOTAL, '%d (expected %d)' % (total, EXPECTED_TOTAL))
    for removed in ('Characters/ABP_PVP_Manny_PostProcess.uasset', 'Characters/BPI_PVP_AnimationState.uasset',
                    'Actors/ABP_PVP_FP_BaseCharacter.uasset', 'Actors/ABP_PVP_TP_BaseCharacter.uasset',
                    'Actors/ABP_PVP_ConstructionLamp_Large.uasset', 'Actors/BP_PVP_BaseCharacter.uasset',
                    'Actors/WBP_PVP_Demo.uasset', 'Actors/WBP_PVP_BodycamOverlay.uasset'):
        path = os.path.join(CONTENT_DIR, 'PVP', removed.replace('/', os.sep))
        check('legacy.file.absent.%s' % removed.replace('/', '.'), not os.path.exists(path), path)

    # 4. legacy path strings must be gone from every package that can reference them.
    legacy_hits = scan_legacy_paths()
    check('refs.legacy_paths.absent', not legacy_hits,
          {key: value[:3] for key, value in sorted(legacy_hits.items())})

    # 5. key references point at the new folders.
    for package_rel, expected_ref in sorted(EXPECTED_REFERENCES.items()):
        data = read_package('/Game/' + package_rel)
        if not data:
            check('refs.%s.readable' % package_rel, False, 'package not found')
            continue
        check('refs.%s.%s' % (package_rel, expected_ref.rsplit('/', 1)[-1]),
              expected_ref.encode('ascii') in data, expected_ref)

    # 6. C++ defaults must not point at the third-party package.
    cpp_file = os.path.join(PROJECT_DIR, 'Source', 'PVPDemo', 'Character', 'PVPBaseCharacter.cpp')
    cpp_text = ''
    if os.path.exists(cpp_file):
        with open(cpp_file, 'r', encoding='utf-8', errors='replace') as handle:
            cpp_text = handle.read()
    check('source.readable', bool(cpp_text), cpp_file)
    check('source.no_infimagames', cpp_text and '/Game/InfimaGames/' not in cpp_text, cpp_file)
    check('source.character_abp',
          '/Game/PVP/Animation/AnimBP/ABP_PVP_FP_BaseCharacter' in cpp_text,
          'ABP default path')
    check('source.character_mesh',
          '/Game/PVPDemo/Characters/Mannequins/Meshes/SKM_PVP_FP_Manny_Simple' in cpp_text,
          'mesh default path')

    failed = [item for item in checks if not item['ok']]
    os.makedirs(REPORT_DIR, exist_ok=True)
    payload = {'ok': not failed, 'total_checks': len(checks), 'failed': failed, 'checks': checks}
    with open(REPORT_JSON, 'w', encoding='utf-8') as handle:
        json.dump(payload, handle, indent=2, ensure_ascii=False)

    lines = ['# PVP logic asset layout verification', '',
             '- result: **%s**' % ('PASS' if not failed else 'FAIL'),
             '- checks: `%d`, failed: `%d`' % (len(checks), len(failed)), '']
    lines.append('| check | result | detail |')
    lines.append('| --- | --- | --- |')
    for item in checks:
        lines.append('| `%s` | %s | %s |' % (item['label'], 'PASS' if item['ok'] else 'FAIL',
                                             str(item['detail']).replace('|', '/')))
    with open(REPORT_MD, 'w', encoding='utf-8') as handle:
        handle.write('\n'.join(lines) + '\n')

    print('\nreport: %s' % REPORT_MD)
    return 0 if not failed else 1


if __name__ == '__main__':
    sys.exit(main())
