#!/usr/bin/env python3
"""Run existing web smoke contracts with native paths and explicit engine."""
import os
from pathlib import Path
import queue
import shutil
import subprocess
import sys
import tempfile
import threading

binary, assets = [str(Path(p).resolve()) for p in sys.argv[1:3]]
engine = sys.argv[3]
chromium = sys.argv[4] if len(sys.argv) > 4 else None
with tempfile.TemporaryDirectory(prefix='THE web smoke ') as tmp:
    root = Path(tmp)
    sample = root / 'sample.txt'
    fixtures = {'sample.txt': 'alpha beta gamma\ndelete me\ngamma\n' if chromium else 'alpha beta gamma\n',
                'console.rexx': "say 'native graphical proof'\npull who\nsay 'Hello' who\nexit 7\n", 'second.txt': 'second buffer\n', 'tabs.txt': 'a\tb\n', 'tab-entry.txt': 'alpha\n', 'sample.crexx': '/* crexx comment */\nsay "hello"\n',
                'sample.md': '# Heading\n\nA **bold** [link](target).\n',
                'sample.py': 'def greet(name):\n    return "hello " + name\n'}
    for name, content in fixtures.items():
        (root / name).write_bytes(content.encode())
    env = dict(os.environ, THE_REXX_INTERPRETER=engine, THE_WEB_ROOT=assets, THE_WEB_WORKSPACE=tmp)
    server = subprocess.Popen([binary, '--driver', 'web', str(sample)],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                              encoding='utf-8', env=env)
    lines = queue.Queue()

    def collect():
        for line in server.stdout:
            lines.put(line)

    threading.Thread(target=collect, daemon=True).start()
    succeeded = False
    try:
        while True:
            line = lines.get(timeout=30)
            if line.startswith('THE web UI: '):
                env['THE_WEB_URL'] = line.split('THE web UI: ', 1)[1].strip()
                break
        name = 'browser-smoke.mjs' if chromium else 'runtime-smoke.mjs'
        script = Path(__file__).resolve().parent.parent / 'web/test' / name
        if chromium:
            env['CHROMIUM_BIN'] = chromium
            env['THE_WEB_TEST_MDP'] = '1' if shutil.which('mdp') else '0'
            env['THE_WEB_TEST_PYP'] = '1' if shutil.which('pyp') else '0'
        subprocess.run([shutil.which('node'), str(script)], env=env, check=True, timeout=90)
        server.wait(timeout=10)
        assert server.returncode == 0, server.stderr.read()
        expected = 'Zalpha beta gamma\ngamma\n' if chromium else 'alphaX beta gamma\n'
        # Logical contents are independent of the platform's native EOL.
        assert sample.read_text(encoding='utf-8') == expected, sample.read_bytes()
        if chromium:
            native_eol = b'\r\n' if os.name == 'nt' else b'\n'
            tabs = (root/'tabs.txt').read_bytes()
            tab_entry = (root/'tab-entry.txt').read_bytes()
            assert tabs == b'a\tb' + native_eol, tabs
            assert tab_entry == b'        alpha' + native_eol, tab_entry
        succeeded = True
    finally:
        if server.poll() is None:
            server.terminate()
            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait(timeout=5)

        if not succeeded:
            print(server.stderr.read(), file=sys.stderr)
