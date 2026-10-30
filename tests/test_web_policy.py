#!/usr/bin/env python3
"""Exercise real browser workspace containment, including Windows junctions."""
import os
from pathlib import Path
import queue
import shutil
import subprocess
import sys
import tempfile
import threading

binary, assets = [str(Path(p).resolve()) for p in sys.argv[1:3]]
with tempfile.TemporaryDirectory(prefix='THE workspace policy ') as tmp:
    root = Path(tmp) / 'workspace'
    outside = Path(tmp) / 'outside'
    root.mkdir()
    outside.mkdir()
    sample = root / 'sample.txt'
    sample_text = ''.join(f'safe {line}\n' for line in range(1, 81))
    sample.write_text(sample_text)
    (outside / 'secret.txt').write_text('outside\n')
    link = root / 'escape'
    if os.name == 'nt':
        subprocess.run([os.environ['COMSPEC'], '/d', '/c', 'mklink', '/J', str(link), str(outside)],
                       check=True, capture_output=True)
    else:
        link.symlink_to(outside, target_is_directory=True)
    engine = os.environ.get('THE_REXX_INTERPRETER', 'NONE')
    macro_header = 'options levelb\n' if engine == 'CREXX' else '/* prefix probe */\n'
    (root / 'pfx.the').write_text(macro_header + "address the\n'input POLICY_ESCAPE';\nreturn 0\n")
    env = dict(os.environ, THE_WEB_ROOT=assets, THE_WEB_WORKSPACE=str(root),
               THE_WEB_MACRO_PATH=str(root))
    server = subprocess.Popen([binary, '--driver', 'web', '-n', str(sample)],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)
    lines = queue.Queue()
    errors = []

    def collect():
        for line in server.stdout:
            lines.put(line)

    threading.Thread(target=collect, daemon=True).start()
    error_reader = threading.Thread(target=lambda: errors.extend(server.stderr), daemon=True)
    error_reader.start()
    succeeded = False
    try:
        while True:
            line = lines.get(timeout=30)
            if line.startswith('THE web UI: '):
                env['THE_WEB_URL'] = line.split('THE web UI: ', 1)[1].strip()
                break
        script = Path(__file__).resolve().parent.parent / 'web/test/policy-smoke.mjs'
        subprocess.run([shutil.which('node'), str(script)], env=env, check=True, timeout=75)
        assert sample.read_text() == sample_text
        assert not (root / 'policy-escape.txt').exists()
        assert list(outside.iterdir()) == [outside / 'secret.txt']
        assert (outside / 'secret.txt').read_text() == 'outside\n'
        succeeded = True
    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait(timeout=5)
        if not succeeded:
            error_reader.join(timeout=2)
            print(''.join(errors), file=sys.stderr)
