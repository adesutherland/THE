#!/usr/bin/env python3
"""Check physical tab/status repainting through a real POSIX terminal.

Requires pyte (the Mac candidate workflow supplies it). Native Windows has a
separate console-buffer test; semantic LLM snapshots cannot prove these paints.
"""
import os
import sys

if os.name == 'nt':
    raise SystemExit(77)
try:
    import pyte
except ImportError:
    print('SKIP: physical terminal repaint test requires pyte')
    raise SystemExit(77)

import errno
import fcntl
from pathlib import Path
import pty
import select
import signal
import struct
import subprocess
import tempfile
import termios
import time


binary = Path(sys.argv[1]).resolve()
repo = Path(__file__).resolve().parent.parent
failures = []


with tempfile.TemporaryDirectory(prefix='CRXTHE physical repaint ') as tmp:
    sample = Path(tmp) / 'sample.txt'
    original = 'aa\tbb\ne\u0301\n\u6f22\n\u2764\ufe0f\n\U0001f600\nplain\n'.encode('utf-8')
    sample.write_bytes(original)
    profile = Path(tmp) / 'profile.the'
    profile.write_text(
        "address THE\n'macro " + str(repo / 'profile_regina.the') + "';\n"
        "'set tabsin off';\n'set hexdisplay both';\n'set clock off';\n"
        "'cursor goto 1 1';\n", encoding='utf-8')
    env = dict(os.environ, THE_REXX_INTERPRETER='REGINA',
               THE_CREXX_CACHE_DIR=str(Path(tmp) / 'cache'), TERM='xterm-256color')
    master, slave = pty.openpty()
    rows, cols = 24, 80
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))
    screen = pyte.Screen(cols, rows)
    stream = pyte.ByteStream(screen)
    process = subprocess.Popen([str(binary), '-p', str(profile), str(sample)],
                               stdin=slave, stdout=slave, stderr=slave, env=env)
    os.close(slave)

    def settle(startup=False):
        deadline = time.monotonic() + (15 if startup else 3)
        quiet = None
        while time.monotonic() < deadline:
            if select.select([master], [], [], 0.05)[0]:
                try:
                    data = os.read(master, 65536)
                except OSError as error:
                    if error.errno == errno.EIO:
                        break
                    raise
                if not data:
                    break
                stream.feed(data)
                quiet = time.monotonic()
            elif quiet is not None and time.monotonic() - quiet > 0.15:
                # A quiet PTY can be between profile drawing and the first
                # file paint on a loaded runner. Wait for the initial fixture,
                # retaining the same tab/status assertions and hard deadline.
                if not startup or (any('aa\u2192bb' in row for row in screen.display)
                                   and 'U+61' in screen.display[-1]):
                    break
        assert process.poll() is None, '\n'.join(screen.display)

    def send(keys):
        os.write(master, keys)
        settle()

    def check(condition, description):
        if not condition:
            failures.append(description + '\n' + '\n'.join(screen.display))

    def status(code, label):
        row = screen.display[-1]
        check(code in row, label + ': wrong character code: ' + repr(row))
        if '[' in row:
            check(row.count(']') == 1, label + ': stale preview delimiter: ' + repr(row))
        background = screen.buffer[rows - 1][0].bg
        wrong = [(col, screen.buffer[rows - 1][col].bg)
                 for col in range(cols)
                 if screen.buffer[rows - 1][col].bg != background]
        check(not wrong, label + ': incomplete status background: ' + repr(wrong))
        check(row.rstrip().endswith('INS'), label + ': missing status mode flags')

    def tab_row(label):
        matches = [row for row in screen.display if 'aa' in row and 'bb' in row]
        check(len(matches) == 1 and 'aa\u2192bb' in matches[0],
              label + ': tab marker/text changed: ' + repr(matches))

    def command(text):
        send(b'\x1b[17~' + text.encode('ascii') + b'\r')  # F6 -> command
        send(b'\x1b[17~')  # F6 -> saved file position

    try:
        settle(startup=True)
        tab_row('initial')
        status('U+61', 'initial')
        for cycle in range(2):
            for col, code in [(2, 'U+61'), (3, 'U+9'), (4, 'U+62'), (5, 'U+62')]:
                send(b'\x1b[C')
                tab_row(f'right {cycle}:{col}')
                status(code, f'right {cycle}:{col}')
                if col == 3:
                    check('[\u2192]' in screen.display[-1], 'tab status marker missing')
            for col in range(4, 0, -1):
                send(b'\x1b[D')
                tab_row(f'left {cycle}:{col}')
        send(b'\x1b[B')
        status('U+65+301', 'combining cluster')
        send(b'\x1b[B')
        status('U+6F22', 'wide character')
        send(b'\x1b[B')
        status('U+2764+FE0F', 'variation selector')
        send(b'\x1b[B')
        status('U+1F600', 'supplementary character')
        send(b'\x1b[C')
        status('U+0', 'past supplementary character')
        check(screen.display[-1][-27:-7] == '[ ] U+0'.ljust(20),
              'stale status tail after supplementary character')
        send(b'\x1b[H\x1b[A\x1b[A\x1b[A\x1b[A')
        tab_row('returned to tab line')
        for mode in ['codes', 'chars', 'off', 'both']:
            command('set hexdisplay ' + mode)
            status('U+61' if mode in ['codes', 'both'] else '', 'mode ' + mode)
        for clock in ['on', 'off']:
            command('set clock ' + clock)
            status('U+61', 'clock ' + clock)
        for cols in [40, 120, 80]:
            screen.resize(lines=rows, columns=cols)
            fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))
            os.kill(process.pid, signal.SIGWINCH)
            settle()
            status('U+61', f'resize {cols}')
            tab_row(f'resize {cols}')
        assert not failures, f'{len(failures)} paint failures:\n' + '\n\n'.join(failures[:4])
        send(b'\x1b[17~')
        os.write(master, b'qquit\r')
        deadline = time.monotonic() + 5
        while process.poll() is None and time.monotonic() < deadline:
            if select.select([master], [], [], 0.05)[0]:
                try:
                    stream.feed(os.read(master, 65536))
                except OSError as error:
                    if error.errno != errno.EIO:
                        raise
        assert process.poll() is not None, '\n'.join(screen.display)
        assert process.returncode == 0, process.returncode
        assert sample.read_bytes() == original, 'cursor repaint changed the file bytes'
    finally:
        if process.poll() is None:
            process.kill()
        process.wait()
        os.close(master)

assert not failures, '\n\n'.join(failures)
print('Native tab markers, slow cursor repaint, UTF status codes and full status backgrounds passed')
