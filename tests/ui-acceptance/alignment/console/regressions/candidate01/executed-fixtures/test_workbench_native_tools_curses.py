#!/usr/bin/env python3
"""A native menu TOOL action launches captured source at a safe PTY boundary."""
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
import platform
import pty
import select
import struct
import subprocess
import tempfile
import termios
import time

if sys.platform != 'darwin' or platform.machine() != 'arm64':
    raise SystemExit(77)
binary = Path(sys.argv[1]).resolve()
repo = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='THE native menu ') as temporary:
    root = Path(temporary)
    source = root / 'menu.py'
    source.write_text("import json,sys\nfrom pathlib import Path\n"
                      "Path('menu-launched.json').write_text(json.dumps(sys.argv[1:]))\n"
                      "print('NATIVE_TOOL_MENU_FINISHED')\n")
    original = source.read_bytes()
    marker = root / 'menu-launched.json'
    profile = root / 'menu.the'
    profile.write_text((repo / 'profile.the').read_text() +
                       "\n'define f9 qquit';\n"
                       "'workbench begin owner public.nativemenu';\n"
                       f"'workbench tool put public.python role runner adapter native-process executable \"{sys.executable}\" source snapshot';\n"
                       "'workbench action put public.run label \"Native adapter run\" tool public.python source active ARG \"menu argument\"';\n"
                       "'workbench menu put public.tools label \"Native tools\" order 15';\n"
                       "'workbench item put public.tools public.item label \"Native adapter run\" action public.run';\n"
                       "'workbench commit';\n'msg NATIVE_PROFILE_READY';\n")
    env = dict(os.environ, THE_REXX_INTERPRETER='CREXX', TERM='xterm-256color',
               THE_TOOL_ROOT=str(root / 'no-rxbin-helper'), THE_TOOL_CONFIG_DIR=str(root / 'config'),
               THE_CREXX_CACHE_DIR=str(root / 'cache'))
    master, slave = pty.openpty()
    rows, cols = 28, 100
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))
    screen = pyte.Screen(cols, rows)
    stream = pyte.ByteStream(screen)
    process = subprocess.Popen([str(binary), '-p', str(profile), str(source)],
                               cwd=root, env=env, stdin=slave, stdout=slave, stderr=slave)
    os.close(slave)

    def wait(predicate, label):
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            if select.select([master], [], [], .03)[0]:
                try:
                    stream.feed(os.read(master, 65536))
                except OSError:
                    break
            if predicate():
                return
        raise AssertionError(label + '\n' + '\n'.join(screen.display))

    try:
        wait(lambda: 'NATIVE_PROFILE_READY' in '\n'.join(screen.display), 'profile ready')
        assert not marker.exists(), 'definition/profile import launched a tool'
        os.write(master, b'\x1b[21~')
        wait(lambda: any('Native adapter run' in line for line in screen.display), 'native F10 menu')
        row = next(y for y, line in enumerate(screen.display) if 'Native adapter run' in line)
        # The menu lowers the action to the editor's ordinary normalized input
        # boundary, after the popup has returned; no macro owns source capture.
        position = bytes((4 + 33, row + 33))
        os.write(master, b'\x1b[M ' + position + b'\x1b[M#' + position)
        wait(marker.exists, 'TOOL action ran from native menu')
        wait(lambda: 'NATIVE_TOOL_MENU_FINISHED' in '\n'.join(screen.display), 'program completion')
        assert json.loads(marker.read_text()) == ['menu argument']
        assert source.read_bytes() == original
        os.write(master, b'\x1b')
        wait(lambda: screen.display[screen.cursor.y].startswith('======>'), 'return from Program input before profile quit binding')
        os.write(master, b'\x1b[20~')
        wait(lambda: process.poll() is not None, 'quit after native menu')
        assert process.returncode == 0
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=10)
        os.close(master)
print('Native PTY menu invoked TOOL action through safe source capture without saving')
