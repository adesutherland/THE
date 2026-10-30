#!/usr/bin/env python3
"""Exercise native workbench controls and console scrollback at two PTY sizes."""
import os
import sys

if os.name == 'nt':
    raise SystemExit(77)
try:
    import pyte
except ImportError:
    raise SystemExit(77)

import fcntl
from pathlib import Path
import pty
import select
import struct
import subprocess
import tempfile
import termios
import time

binary = Path(sys.argv[1]).resolve()
repo = Path(__file__).resolve().parent.parent
program = "do i = 1 to 40\n  say 'LINE' i\nend\npull answer\nsay 'ECHO' answer\n"

with tempfile.TemporaryDirectory(prefix='THE workbench sizes ') as temporary:
    root = Path(temporary)
    source = root / 'program.rexx'
    source.write_text(program)
    profile = root / 'profile.the'
    profile.write_text((repo / 'profile.the').read_text() +
                       "\n'define tab msg EDITOR_TAB';\n'msg PROFILE_READY';\n")
    env = dict(os.environ, TERM='xterm-256color', THE_REXX_INTERPRETER='CREXX',
               THE_HOME_DIR=str(repo), THE_MACRO_PATH=str(repo),
               THE_CREXX_CACHE_DIR=str(root / 'cache'),
               THE_TOOL_CONFIG_DIR=str(root / 'tools'))

    for rows, cols in ((30, 117), (24, 80)):
        master, slave = pty.openpty()
        fcntl.ioctl(slave, termios.TIOCSWINSZ,
                    struct.pack('HHHH', rows, cols, 0, 0))
        screen = pyte.Screen(cols, rows)
        stream = pyte.ByteStream(screen)
        process = subprocess.Popen([str(binary), '--driver', 'curses',
                                    '-p', str(profile), str(source)],
                                   stdin=slave, stdout=slave, stderr=slave, env=env)
        os.close(slave)

        def wait(predicate, label, timeout=30):
            deadline = time.monotonic() + timeout
            quiet = time.monotonic()
            while time.monotonic() < deadline:
                if select.select([master], [], [], .05)[0]:
                    try:
                        stream.feed(os.read(master, 65536))
                    except OSError:
                        break
                    quiet = time.monotonic()
                if predicate() and time.monotonic() - quiet >= .15:
                    return
            raise AssertionError(f'{cols}x{rows}: {label}; cursor '
                                 f'{screen.cursor.x},{screen.cursor.y}\n' +
                                 '\n'.join(f'{n:02d} {line}' for n, line
                                           in enumerate(screen.display)))

        def send(keys):
            os.write(master, keys)

        def click_at(row, col):
            position = bytes((col + 33, row + 33))
            send(b'\x1b[M ' + position + b'\x1b[M#' + position)

        def has(text):
            return any(text in line for line in screen.display)

        def body():
            if cols == 117:
                return '\n'.join(screen.display[row][73:] for row in range(4, rows - 1))
            return '\n'.join(screen.display[row] for row in range(rows - 4, rows - 1))

        def output_line(number):
            return any(line.strip() == f'LINE {number}' for line in body().splitlines())

        def console_focused():
            heading = screen.display[0][73:] if cols == 117 else screen.display[rows - 8]
            return 'Esc Cmd' in heading

        try:
            wait(lambda: 'F10 Menu' in screen.display[rows - 4] and
                 'PROFILE_READY' in screen.display[rows - 3] and
                 'Run Rexx' in screen.display[0][:37], 'profile and chrome')
            assert 'MENU (F10)' in screen.display[0][:14], screen.display
            assert 'Console' in screen.display[0][14:25], screen.display
            send(b'\x1b[21~')
            wait(lambda: has('Run current Rexx'), 'F10 menu exposes Run Rexx')
            send(b'\x1b')
            wait(lambda: not has('Run current Rexx'), 'menu closes')
            send(b'\x1b[Z')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == (15, 0),
                 'Shift-Tab focuses Console')
            send(b'\x1b[21~')
            wait(lambda: has('Run current Rexx'), 'top chrome F10 opens menu')
            send(b'\x1b')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == (15, 0) and
                 not has('Run current Rexx'), 'menu cancel restores top chrome focus')
            send(b'\x1b[Z')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == (1, 0),
                 'Shift-Tab focuses Menu')
            send(b'\r' if cols == 117 else b' ')
            wait(lambda: has('Run current Rexx'), 'Menu control opens popup')
            send(b'\x1b')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == (1, 0) and
                 not has('Run current Rexx'),
                 'menu cancel restores keyboard selected Menu')
            send(b'\t')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == (15, 0),
                 'Tab returns to Console control')
            send(b'\t')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == (26, 0),
                 'Tab focuses direct Run Rexx')
            if cols == 117:
                click_at(0, 29)
            else:
                send(b'\r')
            wait(lambda: 'Program input>' in screen.display[-1] and output_line(40),
                 'Run Rexx starts program and opens console', timeout=40)
            if cols == 117:
                assert 'Program console |' in screen.display[0][73:], screen.display
            else:
                assert 'Program console |' in screen.display[rows - 8], screen.display
            click_at(0, 15)
            wait(lambda: console_focused() and 'Program input>' in screen.display[-1],
                 'Console launcher focuses program input')
            send(b'DRAFT')
            wait(lambda: 'DRAFT' in screen.display[-1], 'unsent program input')
            input_caret = (screen.cursor.x, screen.cursor.y)
            send(b'\x1b[21~')
            wait(lambda: has('Run current Rexx'), 'Program input F10 opens menu')
            send(b'\x1b')
            wait(lambda: console_focused() and
                 (screen.cursor.x, screen.cursor.y) == input_caret and
                 not has('Run current Rexx'),
                 'menu cancel restores Program input caret and draft')
            assert 'DRAFT' in screen.display[-1], screen.display
            send(b'\t\x1b[C\x1b[C')
            wait(lambda: screen.cursor.y == (1 if cols == 117 else rows - 7),
                 'Program toolbar control selected')
            toolbar_caret = (screen.cursor.x, screen.cursor.y)
            send(b'\x1b[21~')
            wait(lambda: has('Run current Rexx'), 'Program toolbar F10 opens menu')
            send(b'\x1b')
            wait(lambda: console_focused() and
                 (screen.cursor.x, screen.cursor.y) == toolbar_caret and
                 not has('Run current Rexx'),
                 'menu cancel restores selected Program toolbar control')
            assert 'DRAFT' in screen.display[-1], screen.display
            send(b'\t')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == input_caret,
                 'Tab returns to Program input')
            send(b'\x1b[H')
            wait(lambda: output_line(1), 'Home reaches first output line')
            assert 'DRAFT' in screen.display[-1], screen.display
            send(b'\x1b[6~')
            wait(lambda: not output_line(1), 'PageDown scrolls output')
            send(b'\x1b[5~')
            wait(lambda: output_line(1), 'PageUp scrolls output')
            send(b'\x1b[F')
            wait(lambda: output_line(40), 'End returns to output tail')
            assert 'DRAFT' in screen.display[-1], screen.display
            send(b'\x1b[17~')  # F6
            wait(lambda: screen.display[screen.cursor.y].startswith('======>')
                 and not console_focused(), 'F6 follows editor Cmd/File toggle')
            send(b'\x1b[17~')
            wait(lambda: screen.cursor.y < (rows - 8 if cols == 80 else rows - 4),
                 'second F6 reaches editor file')
            click_at(0, 15)
            wait(lambda: console_focused() and 'DRAFT' in screen.display[-1],
                 'F6 leaves unsent input available')
            send(b'\t\x1b')
            wait(lambda: screen.display[screen.cursor.y].startswith('======>'),
                 'toolbar Escape follows command shortcut')
            click_at(0, 15)
            wait(lambda: console_focused() and 'DRAFT' in screen.display[-1],
                 'Escape leaves unsent input available')
            send(b'\t')
            wait(lambda: screen.cursor.y == (1 if cols == 117 else rows - 7),
                 'Tab focuses Program toolbar')
            send(b'\x1b[17~')
            wait(lambda: not console_focused() and
                 screen.cursor.y < (rows - 8 if cols == 80 else rows - 4),
                 'toolbar F6 reaches editor file')
            assert 'DRAFT' in screen.display[-1], screen.display
            send(b'\t')
            wait(lambda: has('EDITOR_TAB'), 'editor Tab retains profile binding')
        finally:
            if process.poll() is None:
                process.kill()
            process.wait()
            os.close(master)

print('117x30 and 80x24: menu, direct Run, keyboard focus, scrollback, F6/Esc and input draft passed')
