#!/usr/bin/env python3
"""Run an installed/relocated payload with build and user tool paths removed."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('prefix', type=Path)
parser.add_argument('--regina', type=Path)
args = parser.parse_args()
prefix = args.prefix.resolve()
manifest = json.loads((prefix / 'manifest.json').read_text())
assert manifest['regina_bundled'] is False
assert not any('regina.dll' in name.lower() for name in manifest['files_sha256'])
with tempfile.TemporaryDirectory(prefix='THE package smoke ') as tmp:
    sample = Path(tmp) / 'sample.txt'
    sample.write_text('seed\ntail\n')
    for engine in ['CREXX', 'NONE'] + (['REGINA'] if args.regina else []):
        env = {k: v for k, v in os.environ.items() if not k.startswith(('THE_', 'CREXX'))}
        env['THE_REXX_INTERPRETER'] = engine
        env['HOME'] = tmp
        env['TERM'] = ''
        if os.name == 'nt':
            system = Path(env.get('SystemRoot') or env['SYSTEMROOT'])
            env['PATH'] = str(system / 'System32') + os.pathsep + str(system)
        else:
            env['PATH'] = '/usr/bin:/bin'
        if engine == 'REGINA':
            env['THE_REGINA_LIBRARY'] = str(args.regina.resolve())
        else:
            env['THE_REGINA_LIBRARY'] = str(Path(tmp) / 'absent-regina')
        macro = Path(tmp) / 'package-macro.the'
        macro.write_text(('options levelb\n' if engine == 'CREXX' else '') +
                         "address THE\n'emsg PACKAGE_MACRO_OK';\n")
        commands = 'capabilities\n'
        if engine != 'NONE':
            commands += 'command macro "' + str(macro).replace('\\', '/') + '"\n'
        if engine == 'REGINA':
            commands += 'command macro pfxsetup\ncommand :1\ncommand set pending on DO\ncommand sos doprefix\n'
        commands += 'look filearea compact\nquit\n'
        if os.name == 'nt':
            command = f'"{env["COMSPEC"]}" /d /s /c ""{prefix / "bin/the.cmd"}" --driver llm -n "{sample}""'
        else:
            command = [str(prefix / 'bin/the'), '--driver', 'llm', '-n', str(sample)]
        result = subprocess.run(command, input=commands, text=True, capture_output=True,
                                env=env, timeout=30, check=True)
        records = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
        caps = next(r for r in records if 'macro_available' in r)
        assert caps['macro_interpreter'] == engine and caps['macro_available'] == (engine != 'NONE'), (caps, result.stderr)
        assert not any(word in result.stderr for word in ('Error ', 'Unable to', 'not found')), result.stderr
        if engine != 'NONE':
            assert any(r.get('last_message') == 'PACKAGE_MACRO_OK' for r in records), result.stdout
        if engine == 'REGINA':
            view = next(r for r in records if 'screen_rows' in r)
            assert any(line.get('t') == 'do' for line in view['screen_rows']), view
        print(f'{engine}: installed payload passed with an isolated PATH')
