#!/usr/bin/env python3
"""Physical QUERY WRITE -> typed APPLY, then a direct command-window SAVE key."""
import os
import sys
if os.name == 'nt':
    raise SystemExit(77)
try:
    import pyte
except ImportError:
    raise SystemExit(77)
import fcntl
import json
from pathlib import Path
import pty
import select
import struct
import subprocess
import tempfile
import termios
import time

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='THE group PTY ') as temporary:
    root = Path(temporary)
    sample = root / 'sample.txt'
    sample.write_bytes(b'alpha\nbeta\n')
    profile = root / 'profile.the'
    profile.write_text("options levelb\naddress the\n'define enter enter cua';\n'define f2 save';\n'define f6 cursor home';\n"
                       "'set prefix off';\n'set case mixed respect';\n'cursor goto 1 1';\n'msg GROUP_PTY_READY';\n")
    env = dict(os.environ, TERM='xterm-256color', THE_REXX_INTERPRETER='CREXX',
               THE_CREXX_CACHE_DIR=str(root / 'cache'), THE_TOOL_CONFIG_DIR=str(root / 'tools'))
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 30, 500, 0, 0))
    screen = pyte.Screen(500, 30)
    stream = pyte.ByteStream(screen)
    process = subprocess.Popen([str(binary), '--driver', 'curses', '-p', str(profile), str(sample)],
                               stdin=slave, stdout=slave, stderr=slave, env=env)
    os.close(slave)

    def wait(predicate, description):
        deadline = time.monotonic() + 25
        quiet = time.monotonic()
        while time.monotonic() < deadline:
            if select.select([master], [], [], .03)[0]:
                data = os.read(master, 65536)
                if not data:
                    break
                stream.feed(data)
                quiet = time.monotonic()
            if predicate() and time.monotonic() - quiet > .1:
                return
        raise AssertionError(description + '\n' + '\n'.join(f'{i}: {row.rstrip()}' for i, row in enumerate(screen.display) if row.strip()))

    def has(value):
        return any(value in row for row in screen.display)

    def send(value):
        os.write(master, value)

    def metadata():
        send(b'query workbench write\r')
        wait(lambda: has('"section":"write"'), 'physical QUERY WRITE response')
        row = next(row for row in screen.display if '"section":"write"' in row)
        return json.loads(row[row.index('{'):row.rindex('}') + 1])

    try:
        wait(lambda: has('GROUP_PTY_READY'), 'profile reached command window')
        send(b'\x1b[17~')
        wait(lambda: screen.cursor.y == 28, 'F6 enters command window before token')
        t = metadata()
        command = (f'workbench edit apply document {t["documentId"]} expect {t["writeRevision"]}'
                   ' line 1 before "alpha" after "typed" line 2 before "beta" after "tail"')
        # Backspace is command-record editing too. It must not stale EXPECT.
        send(command.encode() + b'X\x7f\r')
        wait(lambda: has('Alt=1,1') and screen.cursor.y == 28,
             'typed APPLY completed and returned to command origin')
        applied = metadata()
        assert applied['undoAvailable'] and sample.read_bytes() == b'alpha\nbeta\n', applied
        send(b'\x1bOQ')  # F2 is a direct SAVE binding, still in command origin.
        wait(lambda: sample.read_bytes() == b'typed\ntail\n', 'bound SAVE persisted complete group')
        saved = metadata()
        assert not saved['undoAvailable'] and saved['writeRevision'] != applied['writeRevision'], saved
        send((f'workbench edit undo {applied["group"]} document {applied["documentId"]}'
              f' expect {applied["writeRevision"]}\r').encode())
        wait(lambda: has('stale-revision'), 'physical UNDO refused after bound SAVE')
        assert sample.read_bytes() == b'typed\ntail\n'
        send(b'quit\r')
        deadline = time.monotonic() + 10
        while process.poll() is None and time.monotonic() < deadline:
            if select.select([master], [], [], .03)[0]:
                try:
                    data = os.read(master, 65536)
                except OSError:
                    break
                stream.feed(data)
        process.wait(timeout=10)
        assert process.returncode == 0
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        os.close(master)
print('physical edit group token/typing/Enter CUA/Backspace/bound SAVE passed')
