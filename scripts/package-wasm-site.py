#!/usr/bin/env python3
"""Add licenses and dependency/source metadata to a static preview."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('site', type=Path)
parser.add_argument('version')
parser.add_argument('sdk_version')
parser.add_argument('--emscripten-license', type=Path, required=True)
parser.add_argument('--utf8-license', type=Path)
parser.add_argument('--crexx-source', type=Path)
parser.add_argument('--crexx-revision')
parser.add_argument('--vm-only', action='store_true')
args = parser.parse_args()
site = args.site
licenses = site / 'licenses'
licenses.mkdir(exist_ok=True)
license_files = [(root / 'LICENSE', 'CRXTHE-GPL-2.0.txt'),
                 (args.emscripten_license, 'Emscripten.txt')]
dependencies = {'emscripten': args.sdk_version}
if not args.vm_only:
    license_files += [(args.utf8_license, 'utf8proc.txt'),
                      (root / 'web/node_modules/preact/LICENSE', 'Preact.txt'),
                      (root / 'web/node_modules/lucide-preact/LICENSE', 'Lucide.txt')]
    lock = json.loads((root / 'web/package-lock.json').read_text())['packages']
    dependencies.update(utf8proc='e5e799221b45bbb90f5fdc5c69b6b8dfbf017e78',
                        **{name: lock[f'node_modules/{name}']['version'] for name in ['preact', 'lucide-preact']})
if args.crexx_source:
    source = args.crexx_source
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=source, text=True).strip()
    if revision != args.crexx_revision:
        raise RuntimeError('cREXX revision changed after configuration; configure the pinned checkout again')
    dependencies['crexx'] = {'revision': revision,
                             'source_url': f'https://github.com/adesutherland/CREXX/tree/{revision}'}
    license_files += [(source / 'LICENSE', 'cREXX-MIT.txt'),
                      (source / 'interpreter/rxvmplugin/rxvmplugins/mc_decimal/decnumber/ICU-license.html', 'decNumber-ICU.html')]
    # These dependencies keep their complete license notices in source headers.
    (licenses / 'AVL-CC0.txt').write_text((source / 'third_party/avl_tree/avl_tree.c').read_text().split('*/', 1)[0] + '*/\n')
    (licenses / 'utf8-Unlicense.txt').write_text((source / 'third_party/utf8/utf.h').read_text().split('#ifndef', 1)[0])
for source, name in license_files:
    shutil.copy2(source, licenses / name)
revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
# Personal untracked scratch files are excluded from source snapshot metadata.
dirty = bool(subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=root, text=True))
capabilities = {'crexx_vm': bool(args.crexx_source), 'source_compiler': False, 'external_parsers': False}
if args.vm_only:
    capabilities.update(stdin='binary/explicit EOF', stdout='binary', stderr='binary', editor_address=False)
else:
    capabilities.update(browser_storage='IDBFS/IndexedDB', workspace_lock='Web Locks',
                        editor_address=bool(args.crexx_source))
manifest = {
    'product': 'CRXTHE', 'version': args.version,
    'variant': 'wasm-vm-preview' if args.vm_only else 'wasm-editor-preview',
    'source_revision': revision, 'source_dirty': dirty,
    'source_url': f'https://github.com/adesutherland/THE/tree/{revision}',
    'dependencies': dependencies, 'capabilities': capabilities, 'files': {},
}
for file in sorted(site.rglob('*')):
    if file.is_file() and file.name not in ['manifest.json', '.build-stamp']:
        data = file.read_bytes()
        manifest['files'][str(file.relative_to(site))] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
(site / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
