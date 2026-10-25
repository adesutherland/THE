#!/usr/bin/env python3
"""Verify Shmuel's originals and apply the separately distributed THE patches.

Original source is downloaded only on explicit --download, and is never
overwritten or redistributed by the THE package. Python 3 standard library.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import urllib.request

SOURCES = {
    'pfxrexx.the': 'd6915e9f470cda917ac29004a402b5faab787fb73f513f1b1adc763f1b0dc654',
    'pfxhlasm.the': '95abb6790df0b5ccfc302d1792f9f282f3406e5cfc6302e9461c81df493ac4b0',
}


def apply_patch(original, patch):
    source = original.splitlines(keepends=True)
    result, position = [], 0
    lines = patch.splitlines(keepends=True)
    index = 0
    while index < len(lines):
        line = lines[index]
        index += 1
        if not line.startswith('@@ '):
            continue
        match = re.match(r'@@ -(\d+)(?:,(\d+))? \+\d+(?:,\d+)? @@', line)
        if not match:
            raise ValueError('Invalid patch hunk')
        start = int(match[1]) - 1
        if start < position:
            raise ValueError('Overlapping patch hunks')
        result.extend(source[position:start])
        position = start
        expected = int(match[2] or 1)
        consumed = 0
        while index < len(lines) and not lines[index].startswith('@@ '):
            edit = lines[index]
            index += 1
            if edit.startswith('\\ No newline'):
                continue
            if not edit or edit[0] not in ' +-':
                raise ValueError('Invalid patch edit')
            if edit[0] in ' -':
                if position >= len(source) or source[position] != edit[1:]:
                    raise ValueError(f'Patch context mismatch at line {position + 1}')
                position += 1
                consumed += 1
            if edit[0] in ' +':
                result.append(edit[1:])
        if consumed != expected:
            raise ValueError('Patch hunk line count mismatch')
    result.extend(source[position:])
    return ''.join(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-dir', type=Path)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--patch-dir', type=Path,
                        default=Path(__file__).resolve().parent.parent / 'compat/shmuel')
    parser.add_argument('--download', action='store_true')
    args = parser.parse_args()
    if not args.source_dir and not args.download:
        parser.error('Supply --source-dir or explicitly request --download')
    args.output_dir.mkdir(parents=True, exist_ok=True)
    manifest = {}
    for name, digest in SOURCES.items():
        if args.source_dir:
            data = (args.source_dir / name).read_bytes()
        else:
            with urllib.request.urlopen('https://mason.gmu.edu/~smetz3/source/' + name, timeout=30) as response:
                data = response.read()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(name + ': original source SHA-256 differs; review before applying patches')
        original = data.decode('utf-8').replace('\r\n', '\n')
        target = args.output_dir / name
        if args.source_dir and target.resolve() == (args.source_dir / name).resolve():
            raise ValueError('Output must not overwrite originals')
        patched = apply_patch(original, (args.patch_dir / (name + '.patch')).read_text())
        if target.exists() and target.read_text(encoding='utf-8') != patched:
            raise ValueError(str(target) + ': existing output differs; refusing to overwrite local edits')
        target.write_bytes(patched.encode('utf-8'))
        manifest[name] = {'original_sha256': digest,
                          'patched_sha256': hashlib.sha256(patched.encode()).hexdigest()}
    (args.output_dir / 'provenance.json').write_text(json.dumps(manifest, indent=2) + '\n')
    setup = ["address THE", "parse upper arg package", "if package = '' then package = 'REXX'",
             "'set compat xedit xedit xedit'", "'set case mixed respect'", "'set prefix on left 12 0'", 'select']
    for package in ('rexx', 'hlasm'):
        setup.append("when package = '" + package.upper() + "' then do")
        text = (args.output_dir / ('pfx' + package + '.the')).read_text(encoding='utf-8')
        for alias in dict.fromkeys(re.findall(r'SET PREFIX SYNONYM\s+(\S+)\s+PFX' + package, text, re.I)):
            for name in dict.fromkeys((alias.upper(), alias.lower())):
                setup.append("'set prefix synonym " + name + ' pfx' + package + "'")
        setup.append('end')
    setup += ["otherwise 'emsg Expected REXX or HLASM'", 'end', 'exit 0']
    (args.output_dir / 'pfxsetup-original.the').write_text('\n'.join(setup) + '\n', encoding='utf-8')
    print('Prepared verified Regina compatibility macros in', args.output_dir)


if __name__ == '__main__':
    main()
