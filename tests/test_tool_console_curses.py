#!/usr/bin/env python3
"""Adaptive native console, real mouse/keyboard controls and nonblocking stdin."""
import os
import sys

if os.name == 'nt':
    raise SystemExit(77)
try:
    import pyte
except ImportError:
    raise SystemExit(77)

import fcntl
import pty
import select
import signal
import struct
import subprocess
import tempfile
import termios
import time
from pathlib import Path

binary = Path(sys.argv[1]).resolve()
repo = Path(__file__).resolve().parent.parent
program = ("say 'Console ready'\npull name\nsay 'Hello' name\npull name\n"
           "say 'Again' name\npull name\nsay 'Third' name\nexit 7\n")

with tempfile.TemporaryDirectory(prefix='THE console PTY ') as directory:
    root = Path(directory)
    source = root / 'program.rexx'
    source.write_text(program)
    profile = root / 'keys.the'
    profile.write_text((repo / 'profile.the').read_text() + "\n"
                       "'define C-T RX CONSOLE';\n'define f9 qquit';\n"
                       "'define tab msg EDITOR_TAB';\n")
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 30, 144, 0, 0))
    screen = pyte.Screen(144, 30)
    stream = pyte.ByteStream(screen)
    process = subprocess.Popen(
        [str(binary), '-p', str(profile), str(source)], stdin=slave, stdout=slave,
        stderr=slave, env=dict(os.environ, TERM='xterm-256color',
                              THE_TOOL_CONFIG_DIR=str(root / 'config')))
    os.close(slave)

    def wait(check, label, timeout=15):
        end = time.monotonic() + timeout
        quiet = time.monotonic()
        while time.monotonic() < end:
            if select.select([master], [], [], .05)[0]:
                try:
                    stream.feed(os.read(master, 65536))
                except OSError:
                    if check():
                        return
                    break
                quiet = time.monotonic()
            if check() and (process.poll() is not None or time.monotonic() - quiet >= .15):
                return
        raise AssertionError(label + '\n' + '\n'.join(screen.display))

    def contains(text):
        return any(text in row for row in screen.display)

    def resize(rows, cols):
        fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))
        screen.resize(rows, cols)
        process.send_signal(signal.SIGWINCH)

    def menu(index):
        os.write(master, b'\x1b[21~')
        wait(lambda: contains('Open or create file'), 'F10 opens native menu')
        os.write(master, b'\x1b[B' * (index - 1) + b'\r')

    def click_at(row, col):
        # Real xterm mouse press/release, decoded by curses.
        position = bytes((col + 33, row + 33))
        os.write(master, b'\x1b[M ' + position + b'\x1b[M#' + position)

    def click(text):
        row, col = next((y, line.index(text)) for y, line in enumerate(screen.display)
                        if 'Run Stop Clear Close Focus' in line and text in line)
        click_at(row, col)

    try:
        wait(lambda: contains('Console ready'), 'editor source')
        menu(8)
        wait(lambda: 'Program console |' in screen.display[0][96:], 'wide right dock')
        assert any('say' in row[:95] for row in screen.display), screen.display
        assert 'Run Stop Clear Close Focus' in screen.display[1][96:], screen.display
        assert 'F10 Menu' in screen.display[-4][:95], screen.display
        assert 'Response: Ready' in screen.display[-3][:95], screen.display
        assert '======>' in screen.display[-2][:95], screen.display
        assert 'CRXTHE' in screen.display[-1][:95], screen.display
        assert 'Program input>' in screen.display[-1][96:], screen.display
        menu(10)
        wait(lambda: not contains('Program console |'), 'menu hides running console')
        menu(9)
        wait(lambda: contains('Esc: editor'), 'menu reopens running console')
        os.write(master, b'\x1b')
        wait(lambda: contains('F10: Menu'), 'return editor before mouse focus')
        click('Focus')
        wait(lambda: contains('Esc: editor'), 'mouse focuses input')
        os.write(master, 'Zoë😀x'.encode() + b'\x7f')
        wait(lambda: 'Zoë😀' in screen.display[-1] and 'Zoë😀x' not in screen.display[-1],
             'Mac Backspace edits Unicode input')
        assert screen.cursor.x == 96 + len('Program input> ') + 5, screen.cursor
        os.write(master, b'\r')
        wait(lambda: contains('Hello ZOË😀'), 'Unicode stdin/output')
        os.write(master, b'sec')
        wait(lambda: 'sec' in screen.display[-1], 'draft before resize')
        resize(30, 100)
        wait(lambda: 'Program console |' in screen.display[-8], 'narrow bottom dock')
        assert 'sec' in screen.display[-1], screen.display
        os.write(master, b'ond\rthird\r')
        wait(lambda: contains('Third THIRD') and contains('status 7'), 'queued repeated input and completion')
        os.write(master, b'late\r')
        wait(lambda: 'Input closed>' in screen.display[-1], 'completed input closes')
        assert 'late' not in screen.display[-1].lower(), screen.display

        # Tab belongs to controls only while the console owns focus.
        os.write(master, b'\x1b[Z\x1b[H\x1b[C\x1b[C\r')
        wait(lambda: not contains('Third THIRD'), 'keyboard Clear control')
        os.write(master, b'\x1b[H\r')
        wait(lambda: contains('Console ready') and contains('Running'), 'keyboard Run control')
        os.write(master, b'\t\x1b[C\r')
        wait(lambda: contains('Stopped'), 'keyboard Stop control')
        os.write(master, b'\x1b[F\r')
        wait(lambda: screen.cursor.y == 29, 'keyboard Focus control returns to input')
        click_at(4, 10)
        wait(lambda: contains('RX CONSOLE: input') and screen.cursor.y == 4,
             'editor mouse route restores focus and places cursor')
        os.write(master, b'\t')
        wait(lambda: contains('EDITOR_TAB'), 'editor retains its profile Tab binding')

        click('Run')
        wait(lambda: contains('Running'), 'mouse Run control')
        click('Clear')
        wait(lambda: not any('Console ready' in row for row in screen.display[-4:]),
             'mouse Clear control')
        click('Stop')
        wait(lambda: contains('Stopped'), 'mouse Stop control')
        click('Close')
        wait(lambda: not contains('Program console |'), 'mouse Close control')
        menu(9)
        wait(lambda: contains('Esc: editor'), 'menu reopens and focuses console')
        resize(16, 32)
        wait(lambda: contains('Esc: editor') and contains('Run Stop Clear Close Focus'),
             'compact console keeps controls and editor return visible')
        assert contains('F10 Menu'), screen.display
        resize(24, 117)
        wait(lambda: 'Program console |' in screen.display[0][73:], 'minimum useful right dock')
        resize(24, 116)
        wait(lambda: 'Program console |' in screen.display[-8], 'below threshold uses bottom dock')
        resize(15, 60)
        wait(lambda: not contains('Program console |'), 'small terminal hides console')
        os.write(master, b'\x1b')
        wait(lambda: screen.cursor.y == 13, 'hidden console returns input to editor')
        os.write(master, b'msg COMPACT_EDITOR\r')
        wait(lambda: contains('COMPACT_EDITOR'), 'editor responds after compact resize')
        os.write(master, b'\x1b[20~')
        wait(lambda: process.poll() is not None, 'quit', 8)
        assert source.read_text() == program
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        os.close(master)

print('Adaptive terminal console, mouse/keyboard controls, Unicode stdin and resize passed')
