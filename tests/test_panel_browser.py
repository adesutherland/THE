#!/usr/bin/env python3
"""Launch the actual web driver and run a rendered Chromium panel smoke."""
import os
from pathlib import Path
import queue
import shutil
import subprocess
import sys
import tempfile
import threading

binary, assets, chromium = map(str, map(Path, sys.argv[1:4]))
engine = sys.argv[4] if len(sys.argv) > 4 else 'CREXX'
with tempfile.TemporaryDirectory(prefix='THE browser panel ') as tmp:
    root = Path(tmp)
    sample = root / 'sample.txt'
    sample.write_text('seed\ntail\n')
    profile = root / 'profile.the'
    demo = 'paneldemo' if engine == 'CREXX' else 'paneldemo_regina'
    repo = Path(__file__).resolve().parent.parent
    stock = repo / ('web-profile.the' if engine == 'CREXX' else 'web-profile-regina.the')
    profile.write_text(stock.read_text(encoding='utf-8') +
                       "'cursor goto 2 3';\n'define f7 macro " + demo + "';\n", encoding='utf-8')
    env = dict(os.environ, THE_REXX_INTERPRETER=engine,
               THE_WEB_ROOT=str(Path(assets).resolve()), THE_WEB_WORKSPACE=tmp, THE_WEB_PROFILE=str(profile))
    # A stock profile and repeated macros may emit more than a pipe buffer of
    # compiler diagnostics. Keep stderr drained while browser input is active.
    errors_log = (root / 'server.stderr').open('w+', encoding='utf-8')
    server = subprocess.Popen([str(Path(binary).resolve()), '--driver', 'web', '-p', str(profile), str(sample)],
                              stdout=subprocess.PIPE, stderr=errors_log, text=True, encoding='utf-8', env=env)
    output = queue.Queue()

    def read_stdout():
        for line in server.stdout:
            output.put(line)

    thread = threading.Thread(target=read_stdout, daemon=True)
    thread.start()
    try:
        while True:
            line = output.get(timeout=30)
            if line.startswith('THE web UI: '):
                env.update(THE_WEB_URL=line.split('THE web UI: ', 1)[1].strip(), CHROMIUM_BIN=chromium)
                break
        script = Path(__file__).resolve().parent.parent / 'web/test/panel-smoke.mjs'
        subprocess.run([shutil.which('node'), str(script)], env=env, check=True, timeout=90)
        eol = b'\r\n' if os.name == 'nt' else b'\n'
        assert sample.read_bytes() == b'seed'+eol+b'taZil'+eol,sample.read_bytes()
    except subprocess.CalledProcessError as error:
        if server.poll() is not None:
            errors_log.seek(0)
            raise AssertionError(('Web driver exited during panel test',server.returncode,
                                  errors_log.read())) from error
        raise
    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait(timeout=5)
        errors_log.seek(0)
        errors = errors_log.read()
        errors_log.close()
        assert 'Error 00' not in errors and 'Unable to' not in errors, errors
