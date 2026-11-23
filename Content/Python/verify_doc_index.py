"""Verify PVPDemo documentation index consistency.

Runs headless with any Python 3 interpreter (no Unreal dependency):
    python Content/Python/verify_doc_index.py

Checks:
  1. Every module directory under Docs/ has the fixed three-document triad.
  2. Every Markdown path reference inside Docs/ resolves to an existing file.
  3. Source layer empty/non-empty state matches Docs/Source-Structure.md tree annotations.

Report: Saved/DocIndexReport/verify.{json,md}. Any FAIL exits non-zero.
"""

import json
import os
import re
import sys

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOCS_DIR = os.path.join(PROJECT_ROOT, 'Docs')
SOURCE_DIR = os.path.join(PROJECT_ROOT, 'Source', 'PVPDemo')
REPORT_DIR = os.path.join(PROJECT_ROOT, 'Saved', 'DocIndexReport')
REPORT_JSON = os.path.join(REPORT_DIR, 'verify.json')
REPORT_MD = os.path.join(REPORT_DIR, 'verify.md')

checks = []


def log(message):
    print('[PVP-DOC-VERIFY] %s' % message)


def check(label, condition, detail=''):
    entry = {'label': label, 'ok': bool(condition), 'detail': str(detail)}
    checks.append(entry)
    log('%s %s%s' % ('PASS' if condition else 'FAIL', label, (' | ' + detail) if detail else ''))
    return bool(condition)


def rel(path):
    return os.path.relpath(path, PROJECT_ROOT).replace('\\', '/')


def scan_triad():
    module_dirs = [name for name in sorted(os.listdir(DOCS_DIR))
                   if os.path.isdir(os.path.join(DOCS_DIR, name))]
    check('docs.module_dirs.nonempty', len(module_dirs) > 0, 'modules=%s' % module_dirs)

    for module in module_dirs:
        expected = ['README.md', '%s-Dev.md' % module, '%s-Integration.md' % module]
        missing = [name for name in expected
                   if not os.path.isfile(os.path.join(DOCS_DIR, module, name))]
        check('docs.triad.%s' % module, not missing,
              ('missing=%s' % missing) if missing
              else 'README.md + %s-Dev.md + %s-Integration.md' % (module, module))


SKIPPED_REF = object()


def is_doc_ref(ref):
    """Only treat references to source docs as checkable; skip artifacts/placeholders."""
    if '<' in ref or '>' in ref or '|' in ref or ref.startswith(('@', 'Saved/')):
        return False
    if ref == 'CODEBUDDY.md':
        return False
    if ref in ('AGENTS.md', 'README.md'):
        return True
    if ref.startswith('Docs/'):
        return True
    return ref.endswith(('-Dev.md', '-Integration.md'))


def resolve_md_ref(ref, doc_path):
    """Return an existing path, SKIPPED_REF for non-doc mentions, or None."""
    if not is_doc_ref(ref):
        return SKIPPED_REF

    candidates = []
    if ref.startswith(('Docs/', 'Content/', 'Source/')) or ref in ('AGENTS.md', 'README.md'):
        candidates.append(os.path.join(PROJECT_ROOT, ref))
    candidates.append(os.path.join(os.path.dirname(doc_path), ref))
    candidates.append(os.path.join(DOCS_DIR, ref))

    doc_rel = rel(doc_path).split('/')
    if len(doc_rel) >= 3 and doc_rel[0] == 'Docs':
        candidates.append(os.path.join(PROJECT_ROOT, 'Docs', doc_rel[1], ref))

    # Bare module-document filenames like "Input-Dev.md" may live in any module folder.
    if '/' not in ref:
        for module in os.listdir(DOCS_DIR):
            candidate = os.path.join(DOCS_DIR, module, ref)
            if os.path.isfile(candidate):
                return candidate

    for candidate in candidates:
        if os.path.isfile(candidate):
            return candidate
    return None


def scan_md_refs():
    ref_pattern = re.compile(r'`([^`]+?\.md)`')
    broken = []
    checked = 0
    skipped = 0
    for dirpath, _dirnames, filenames in os.walk(DOCS_DIR):
        for filename in filenames:
            if not filename.endswith('.md'):
                continue
            path = os.path.join(dirpath, filename)
            with open(path, 'r', encoding='utf-8') as handle:
                text = handle.read()
            for ref in ref_pattern.findall(text):
                resolved = resolve_md_ref(ref, path)
                if resolved is SKIPPED_REF:
                    skipped += 1
                    continue
                checked += 1
                if resolved is None:
                    broken.append('%s -> %s' % (rel(path), ref))

    check('docs.md_refs.resolved', not broken,
          'checked=%d skipped=%d broken=%s' % (checked, skipped, broken[:10]))


def source_layers():
    actual = {}
    for name in sorted(os.listdir(SOURCE_DIR)):
        path = os.path.join(SOURCE_DIR, name)
        if not os.path.isdir(path):
            continue
        has_code = False
        for _dirpath, _dirnames, filenames in os.walk(path):
            if any(filename.endswith(('.h', '.cpp')) for filename in filenames):
                has_code = True
                break
        actual[name] = has_code
    return actual


def declared_layers():
    path = os.path.join(DOCS_DIR, 'Source-Structure.md')
    with open(path, 'r', encoding='utf-8') as handle:
        text = handle.read()
    declared = {}
    pattern = re.compile(
        r'^│\s{3}[├└]──\s+([A-Za-z][A-Za-z0-9_]*)/\s+[^（\n]*（([^）]*)）',
        re.MULTILINE,
    )
    for match in pattern.finditer(text):
        name, note = match.group(1), match.group(2)
        if '已有' in note or '已落地' in note:
            declared[name] = True
        elif '空' in note:
            declared[name] = False
    return declared


def compare_source_state():
    actual = source_layers()
    declared = declared_layers()
    mismatches = []
    for name in sorted(set(actual) | set(declared)):
        has_code = actual.get(name)
        is_declared = declared.get(name)
        if has_code is None or is_declared is None:
            mismatches.append('%s: actual=%s declared=%s' % (name, has_code, is_declared))
        elif has_code != is_declared:
            mismatches.append('%s: actual=%s declared=%s' % (
                name, 'nonempty' if has_code else 'empty',
                'nonempty' if is_declared else 'empty'))

    check('source.state_matches_docs', not mismatches,
          'layers=%d mismatches=%s' % (len(actual), mismatches))


def write_report():
    os.makedirs(REPORT_DIR, exist_ok=True)
    with open(REPORT_JSON, 'w', encoding='utf-8') as handle:
        json.dump(checks, handle, ensure_ascii=False, indent=2)
    passed = sum(1 for entry in checks if entry['ok'])
    lines = ['# 文档索引校验报告', '',
             '共 %d 项，通过 %d 项，失败 %d 项。' % (len(checks), passed, len(checks) - passed), '']
    for entry in checks:
        suffix = (' — ' + entry['detail']) if entry['detail'] else ''
        lines.append('- %s **%s**%s' % ('PASS' if entry['ok'] else 'FAIL', entry['label'], suffix))
    with open(REPORT_MD, 'w', encoding='utf-8') as handle:
        handle.write('\n'.join(lines) + '\n')


def main():
    scan_triad()
    scan_md_refs()
    compare_source_state()
    write_report()
    failed = [entry for entry in checks if not entry['ok']]
    log('report: %s' % rel(REPORT_MD))
    if failed:
        log('%d check(s) failed' % len(failed))
        sys.exit(1)
    log('all %d checks passed' % len(checks))


if __name__ == '__main__':
    main()
