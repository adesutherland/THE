#!/usr/bin/env python3
"""Native tool policy through the real restricted browser protocol."""
import os
from pathlib import Path
import queue
import shutil
import subprocess
import sys
import tempfile
import threading

binary, assets = (str(Path(p).resolve()) for p in sys.argv[1:3])
with tempfile.TemporaryDirectory(prefix='THE native tool browser ') as temporary:
    root = Path(temporary)
    source = root / 'source.rexx'
    original = b"say 'ordinary document'\n"
    source.write_bytes(original)
    env = dict(os.environ, THE_REXX_INTERPRETER='NONE', THE_WEB_ROOT=assets,
               THE_WEB_WORKSPACE=str(root), THE_TOOL_CONFIG_DIR=str(root / 'config'))
    server = subprocess.Popen([binary, '--driver', 'web', '-n', str(source)],
                              env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    lines, errors = queue.Queue(), []
    threading.Thread(target=lambda: [lines.put(line) for line in server.stdout], daemon=True).start()
    threading.Thread(target=lambda: errors.extend(server.stderr), daemon=True).start()
    try:
        while True:
            line = lines.get(timeout=30)
            if line.startswith('THE web UI: '):
                env['THE_WEB_URL'] = line.split('THE web UI: ', 1)[1].strip()
                break
        script = Path(__file__).resolve().parent.parent / 'web/test/native-tools-policy.mjs'
        subprocess.run([shutil.which('node'), str(script)], env=env, check=True, timeout=60)
        assert source.read_bytes() == original
    except Exception:
        print(''.join(errors), file=sys.stderr)
        raise
    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait(timeout=5)
