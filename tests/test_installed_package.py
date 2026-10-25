#!/usr/bin/env python3
"""Run an installed/relocated payload with build and user tool paths removed."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import queue
import threading
import urllib.parse
import urllib.request
import uuid

parser = argparse.ArgumentParser()
parser.add_argument('prefix', type=Path)
parser.add_argument('--regina', type=Path)
parser.add_argument('--macros', type=Path)
parser.add_argument('--read-only-prefix', action='store_true',
                    help='Read installed browser assets without writing to a system-owned installation')
args = parser.parse_args()
prefix = args.prefix.resolve()
manifest = json.loads((prefix / 'manifest.json').read_text())
for relative, expected in manifest['files_sha256'].items():
    assert hashlib.sha256((prefix / relative).read_bytes()).hexdigest() == expected, relative
assert manifest['regina_bundled'] is False
assert not any('regina.dll' in name.lower() for name in manifest['files_sha256'])


def check_installed_web(command, env):
    if not any('the_driver_web.' in name for name in manifest['files_sha256']):
        return
    env = dict(env, THE_WEB_WORKSPACE=env['HOME'])
    # A unique file existing only in the installed assets proves the server
    # did not silently fall back to the build tree on the test machine.
    name = 'index.html' if args.read_only_prefix else 'installed-probe-' + uuid.uuid4().hex + '.txt'
    probe = prefix / 'share/the/web' / name
    if args.read_only_prefix:
        expected = probe.read_bytes()
    else:
        probe.write_text('INSTALLED_WEB_OK')
        expected = probe.read_bytes()
    web_command = command.replace('--driver llm', '--driver web') if isinstance(command, str) else [
        'web' if word == 'llm' else word for word in command]
    server = subprocess.Popen(web_command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              text=True, encoding='utf-8', env=env)
    lines = queue.Queue()

    def collect():
        for line in server.stdout:
            lines.put(line)
        lines.put(None)

    threading.Thread(target=collect, daemon=True).start()
    try:
        while True:
            line = lines.get(timeout=30)
            assert line is not None, 'Installed web launcher exited: ' + server.stderr.read()
            if line.startswith('THE web UI: '):
                parsed = urllib.parse.urlsplit(line.split('THE web UI: ', 1)[1].strip())
                url = urllib.parse.urlunsplit((parsed.scheme, parsed.netloc, '/' + name, parsed.query, ''))
                break
        with urllib.request.urlopen(url, timeout=10) as response:
            assert response.read() == expected
        print('WEB: installed launcher served ' + ('read-only installed' if args.read_only_prefix else 'its own relocated') +
              ' assets with an isolated PATH')
    finally:
        if server.poll() is None:
            if os.name == 'nt':
                subprocess.run(['taskkill', '/T', '/F', '/PID', str(server.pid)],
                               check=True, capture_output=True, env=env)
            else:
                server.terminate()
            server.wait(timeout=10)
        if not args.read_only_prefix:
            probe.unlink()
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
            commands += 'command macro pfxsetup\ncommand :1\ncommand cursor goto 1 1\ncommand set pending on DO\ncommand sos doprefix\n'
        if engine == 'CREXX':
            commands += 'command macro pfxsetupg REXX\ncommand :1\ncommand cursor goto 1 1\ncommand set pending on DO\ncommand sos doprefix\n'
            commands += 'look filearea compact\n'
            commands += 'command macro pfxsetupg HLASM\ncommand :1\ncommand cursor goto 1 1\ncommand set pending on ENDIF\ncommand sos doprefix\n'
        commands += 'look filearea compact\nquit\n'
        if os.name == 'nt':
            command = f'"{env["COMSPEC"]}" /d /s /c ""{prefix / "bin/the.cmd"}" --driver llm -n "{sample}""'
        else:
            command = [str(prefix / 'bin/the'), '--driver', 'llm', '-n', str(sample)]
        result = subprocess.run(command, input=commands, text=True, encoding="utf-8", capture_output=True,
                                env=env, timeout=30, check=True)
        records = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
        caps = next(r for r in records if 'macro_available' in r)
        assert caps['macro_interpreter'] == engine and caps['macro_available'] == (engine != 'NONE'), (caps, result.stderr)
        assert not any(word in result.stderr for word in ('Error ', 'Unable to', 'not found')), result.stderr
        if engine != 'NONE':
            assert any(r.get('last_message') == 'PACKAGE_MACRO_OK' for r in records), result.stdout
        if engine in ('REGINA', 'CREXX'):
            views = [r for r in records if 'screen_rows' in r]
            assert any(line.get('t') == 'do' for view in views for line in view['screen_rows']), views
            if engine == 'CREXX':
                assert any(line.get('t', '').strip().startswith('ENDIF') for line in views[-1]['screen_rows']), views[-1]
        print(f'{engine}: installed payload passed with an isolated PATH')
        if engine == 'NONE':
            check_installed_web(command, env)
        if engine == 'REGINA' and args.macros:
            env['THE_HOME_DIR'] = str(prefix / 'share/the')
            env['THE_DRIVER_PATH'] = str(prefix / 'lib/the/drivers')
            executable = prefix / 'libexec/the' / ('the.exe' if os.name == 'nt' else 'the')
            prepared = Path(tmp) / 'installed patched macros'
            tool = prefix / 'share/the/macro-patches/scripts/prepare-shmuel-macros.py'
            subprocess.run([sys.executable, str(tool), '--download', '--output-dir', str(prepared)],
                           env=env, check=True, timeout=90)
            assert json.loads((prepared / 'provenance.json').read_text()) == json.loads(
                (args.macros / 'provenance.json').read_text())
            subprocess.run([sys.executable, str(Path(__file__).with_name('test_shmuel_macros.py')),
                            str(executable), str(prepared)], env=env, check=True, timeout=120)
            # Exercise the compiled ports with Regina unavailable, using only
            # the tools and shared library actually installed in the payload.
            g_env = dict(env, THE_REXX_INTERPRETER='CREXX', THE_REGINA_LIBRARY=str(Path(tmp) / 'no-regina'))
            runtime = prefix / 'libexec/the/crexx'
            libraries = [p for p in runtime.iterdir() if 'crexxsaa' in p.name and p.suffix in ('.dll', '.so', '.dylib')]
            assert len(libraries) == 1, libraries
            suffix = '.exe' if os.name == 'nt' else ''
            g_env.update(THE_CREXX_LIBRARY=str(libraries[0]), THE_CREXX_LOCATION=str(runtime),
                         THE_CREXX_IMPORT_DIR=str(runtime), THE_CREXX_LIBRARY_RXBIN=str(runtime / 'library.rxbin'),
                         THE_CREXX_RXC=str(runtime / ('rxc' + suffix)), THE_CREXX_RXAS=str(runtime / ('rxas' + suffix)))
            g_env['PATH'] = str(runtime) + os.pathsep + env['PATH']
            subprocess.run([sys.executable, str(Path(__file__).with_name('test_shmuel_macros.py')),
                            str(executable), str(prepared), 'CREXX', str(prefix / 'share/the')],
                           env=g_env, check=True, timeout=180)
            subprocess.run([sys.executable, str(Path(__file__).with_name('test_prefix_port_parity.py')),
                            str(prepared)], env=g_env, check=True, timeout=15)
            for panel_engine, panel_env in [('REGINA', env), ('CREXX', g_env)]:
                subprocess.run([sys.executable, str(Path(__file__).with_name('test_panels.py')),
                                str(executable), panel_engine], env=panel_env, check=True, timeout=180)
