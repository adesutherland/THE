#!/usr/bin/env python3
"""Add licenses and reproducible dependency/source metadata to a static preview."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
site, utf8_license, emscripten_license = map(Path, sys.argv[1:4])
version, sdk_version = sys.argv[4:6]
licenses = site / 'licenses'
licenses.mkdir(exist_ok=True)
for source, name in [(root / 'LICENSE', 'CRXTHE-GPL-2.0.txt'),
                     (utf8_license, 'utf8proc.txt'), (emscripten_license, 'Emscripten.txt'),
                     (root / 'web/node_modules/preact/LICENSE', 'Preact.txt'),
                     (root / 'web/node_modules/lucide-preact/LICENSE', 'Lucide.txt')]:
    shutil.copy2(source, licenses / name)
lock = json.loads((root / 'web/package-lock.json').read_text())['packages']
revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
# Only tracked source changes qualify a source snapshot as dirty; personal
# untracked scratch files are deliberately excluded from this artifact.
dirty = bool(subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=root, text=True))
manifest = {
    'product': 'CRXTHE', 'version': version, 'variant': 'wasm-editor-preview',
    'source_revision': revision, 'source_dirty': dirty,
    'source_url': f'https://github.com/adesutherland/THE/tree/{revision}',
    'dependencies': {'emscripten': sdk_version, 'utf8proc': 'e5e799221b45bbb90f5fdc5c69b6b8dfbf017e78',
                     **{name: lock[f'node_modules/{name}']['version'] for name in ['preact', 'lucide-preact']}},
    'capabilities': {'browser_storage': 'IDBFS/IndexedDB', 'workspace_lock': 'Web Locks',
                     'crexx_vm': False, 'external_parsers': False},
    'files': {},
}
for file in sorted(site.rglob('*')):
    if file.is_file() and file.name not in ['manifest.json', '.build-stamp']:
        data = file.read_bytes()
        manifest['files'][str(file.relative_to(site))] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
(site / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
