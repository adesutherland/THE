#!/usr/bin/env python3
"""Adaptive native console, real mouse/keyboard controls and nonblocking stdin."""
import os
import sys

if os.name == 'nt':
    raise SystemExit(77)
try:
    import pyte
    from pyte import graphics
except ImportError:
    raise SystemExit(77)

import fcntl
import json
import pty
import re
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
menu_labels = [label for label, _ in json.loads(
    (repo / 'profiles/default-profile.json').read_text())['native_menu']]
program = ("say 'Console ready'\n"
           "say 'ASCII:' || copies('A', 41) || 'TAIL'\n"
           "say 'WIDE:' || copies('B', 41) || '界e\u0301TAIL'\n"
           "say 'TAB:' || '09'x || 'TAIL'\n"
           "say 'CRLF' || '0d0a'x || 'NEXT'\n"
           "say 'BLANK-BEFORE'\nsay ''\nsay 'BLANK-AFTER'\n"
           "call charout , 'NO-NL' || copies('Z', 198) || 'END'\n"
           "pull name\nsay 'Hello' name\npull name\n"
           "say 'Again' name\npull name\nsay 'Third' name\nexit 7\n")
palette = list(graphics.FG_BG_256)


def colour(actual, expected):
    assert len(actual) == 6, (actual, expected)
    assert all(abs(int(actual[i:i+2], 16) - int(expected[i:i+2], 16)) <= 2
               for i in (0, 2, 4)), (actual, expected)

with tempfile.TemporaryDirectory(prefix='THE console PTY ') as directory:
    root = Path(directory)
    source = root / 'program.rexx'
    source.write_text(program)
    profile = root / 'keys.the'
    profile.write_text((repo / 'profile.the').read_text() + "\n"
                       "'define C-T RX CONSOLE';\n'define f9 qquit';\n"
                       "'define tab msg EDITOR_TAB';\n"
                       "'set color consolebody #D4D4D4 on #1E1E1E';\n"
                       "'set color consoleheader #F1F5FA on #25415F';\n"
                       "'set color consoleinput #D4D4D4 on #2D2D30';\n"
                       "'set color consolecontrol #D4D4D4 on #1E1E1E';\n"
                       "'set color consoleselected #E6F7FF on #264F78';\n"
                       "'set color consoledivider #6C7B90 on #1E1E1E';\n"
                       "'set color consoleerror tomato on #1E1E1E';\n"
                       "'set color consolestatus #858585 on #1E1E1E';\n"
                       "'define c-y set color consolebody #A1B2C3 on #243648';\n"
                       "'define c-u set color consolebody #D4D4D4 on #1E1E1E';\n"
                       "'define c-b msg RESPONSE_PROBE';\n")
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 30, 144, 0, 0))
    screen = pyte.Screen(144, 30)
    stream = pyte.ByteStream(screen)
    output = bytearray()
    process = subprocess.Popen(
        [str(binary), '-p', str(profile), str(source)], stdin=slave, stdout=slave,
        stderr=slave, env=dict(os.environ, TERM='xterm-256color',
                              THE_MACRO_PATH=str(repo),
                              THE_TOOL_CONFIG_DIR=str(root / 'config')))
    os.close(slave)

    def wait(check, label, timeout=15):
        end = time.monotonic() + timeout
        quiet = time.monotonic()
        while time.monotonic() < end:
            if select.select([master], [], [], .05)[0]:
                try:
                    data = os.read(master, 65536)
                    output.extend(data)
                    # ncurses defines RGB colours with OSC 4 before emitting
                    # indexed SGR. pyte needs that palette applied explicitly.
                    graphics.FG_BG_256[:] = palette
                    for match in re.finditer(rb'\x1b\]4;(\d+);rgb:([0-9a-fA-F]+)/([0-9a-fA-F]+)/([0-9a-fA-F]+)', output):
                        index = int(match[1])
                        if index < 256:
                            parts = [round(int(part, 16) * 255 / (16 ** len(part) - 1))
                                     for part in match.groups()[1:]]
                            graphics.FG_BG_256[index] = ''.join(f'{part:02x}' for part in parts)
                    stream.feed(data)
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

    def pane(row, left=96):
        # Display strings omit a wide glyph's continuation cell. Slice actual
        # terminal columns so Unicode in the editor cannot shift pane offsets.
        return ''.join(screen.buffer[row][col].data for col in range(left, screen.columns))

    def resize(rows, cols):
        fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))
        screen.resize(rows, cols)
        process.send_signal(signal.SIGWINCH)

    def menu(label):
        os.write(master, b'\x1b[21~')
        wait(lambda: contains('Open or create file'), 'F10 opens native menu')
        # Read the labels actually displayed, so delimiter mistakes cannot
        # silently shift a visible action onto a different numeric dispatch.
        positions = []
        for expected in menu_labels:
            matches = [row for row, line in enumerate(screen.display)
                       if line.startswith('  ' + expected)]
            assert len(matches) == 1, (expected, screen.display)
            positions.append(matches[0])
        assert positions == list(range(positions[0], positions[0] + len(menu_labels))), screen.display
        selected = positions[menu_labels.index(label)] - positions[0]
        os.write(master, b'\x1b[B' * selected + b'\r')

    def click_at(row, col):
        # Real xterm mouse press/release, decoded by curses.
        position = bytes((col + 33, row + 33))
        os.write(master, b'\x1b[M ' + position + b'\x1b[M#' + position)

    def click(text):
        row, col = next((y, line.index(text)) for y, line in enumerate(screen.display)
                        if 'Run Stop Clear Hide Editor' in line and text in line)
        click_at(row, col)

    try:
        wait(lambda: contains('Console ready'), 'editor source')
        menu('Run current Rexx')
        wait(lambda: 'Program console |' in screen.display[0][96:], 'wide right dock')
        assert any('say' in row[:95] for row in screen.display), screen.display
        assert 'Run Stop Clear Hide Editor' in screen.display[1][96:], screen.display
        assert 'F10 Menu' in screen.display[-4][:95], screen.display
        assert not screen.display[-3][:95].strip(), screen.display
        assert '======>' in screen.display[-2][:95], screen.display
        assert 'CRXTHE' in screen.display[-1][:95], screen.display
        assert 'Program input>' in screen.display[-1][96:], screen.display
        os.write(master, b'\x02')
        wait(lambda: 'RESPONSE_PROBE' in screen.display[-3][:95],
             'editor response appears above command line')
        wait(lambda: any(pane(row).startswith('NO-NL') for row in range(4, screen.lines - 1)),
             'output without a final newline')
        console = [pane(row) for row in range(4, screen.lines - 1)]
        assert console[1] == 'ASCII:' + 'A' * 41 + ' ', console
        assert console[2].startswith('TAIL'), console
        assert console[3] == 'WIDE:' + 'B' * 41 + '  ', console
        assert console[4].startswith('界éTAIL'), console
        assert console[5].startswith('TAB:  TAIL'), console
        assert console[6].startswith('CRLF') and console[7].startswith('NEXT'), console
        assert console[8].startswith('BLANK-BEFORE') and not console[9].strip(), console
        assert console[10].startswith('BLANK-AFTER') and console[11].startswith('NO-NL'), console
        assert console[15].startswith('Z' * 15 + 'END'), console
        assert all(screen.buffer[row][95].data == '|' for row in range(30)), screen.display
        colour(screen.buffer[0][97].fg, 'F1F5FA')
        colour(screen.buffer[0][143].bg, '25415F')
        colour(screen.buffer[4][96].fg, 'D4D4D4')
        colour(screen.buffer[4][143].bg, '1E1E1E')
        colour(screen.buffer[29][96].fg, 'D4D4D4')
        colour(screen.buffer[29][143].bg, '2D2D30')
        colour(screen.buffer[10][95].fg, '6C7B90')
        os.write(master, b'\x19')
        wait(lambda: screen.buffer[4][96].fg != 'D4D4D4',
             'SET COLOR CONSOLEBODY repaints open pane')
        colour(screen.buffer[4][96].fg, 'A1B2C3')
        colour(screen.buffer[4][143].bg, '243648')
        os.write(master, b'\x15')
        wait(lambda: screen.buffer[4][96].fg != 'A1B2C3',
             'restore console body colour')
        colour(screen.buffer[4][96].fg, 'D4D4D4')
        # The divider owns its column without moving editor/input focus.
        caret = (screen.cursor.x, screen.cursor.y)
        click_at(4, 95)
        wait(lambda: (screen.cursor.x, screen.cursor.y) == caret, 'divider click keeps focus')
        resize(30, 117)
        wait(lambda: 'Program console |' in screen.display[0][73:]
             and any(pane(row, 73).startswith('AAAATAIL') for row in range(4, screen.lines - 1)),
             'output reflows at the minimum right dock width')
        assert all(screen.buffer[row][72].data == '|' for row in range(30)), screen.display
        assert any(pane(row, 73).startswith('BBB界éTAIL') for row in range(4, screen.lines - 1)), screen.display
        resize(30, 144)
        wait(lambda: pane(5).startswith('ASCII:' + 'A' * 41)
             and pane(6).startswith('TAIL'), 'output reflows back to wide dock')
        menu('Hide program console')
        wait(lambda: not contains('Program console |'), 'menu hides running console')
        menu('Show or focus Program console')
        wait(lambda: contains('Esc: editor'), 'menu reopens running console')
        assert contains('Running'), screen.display
        menu('Stop program')
        wait(lambda: contains('Stopped'), 'visible Stop menu action stops the job')
        menu('Run current Rexx')
        wait(lambda: contains('Running') and contains('Console ready'),
             'visible Run menu action restarts the job')
        menu('Show or focus Program console')
        wait(lambda: contains('Esc: editor'), 'focus restarted program input')
        os.write(master, b'\x1b')
        wait(lambda: contains('F10: Menu'), 'return editor before mouse focus')
        click_at(0, 15)
        wait(lambda: contains('Esc: editor'), 'persistent launcher focuses input')
        os.write(master, 'Zoë😀x'.encode() + b'\x7f')
        wait(lambda: 'Zoë😀' in screen.display[-1] and 'Zoë😀x' not in screen.display[-1],
             'Mac Backspace edits Unicode input')
        assert screen.cursor.x == 96 + len('Program input> ') + 5, screen.cursor
        os.write(master, b'\r')
        wait(lambda: contains('Hello ZOË😀'), 'Unicode stdin/output')
        os.write(master, b'sec')
        wait(lambda: 'sec' in screen.display[-1], 'draft before resize')
        os.write(master, b'\x1b[Z\x1b[Z')
        wait(lambda: (screen.cursor.x, screen.cursor.y) == (15, 0),
             'console Shift-Tab returns through toolbar to Console launcher')
        colour(screen.buffer[0][15].fg, 'E6F7FF')
        colour(screen.buffer[0][15].bg, '264F78')
        os.write(master, b'\x1b')
        wait(lambda: contains('F10: Menu') and 0 < screen.cursor.y < 26,
             'launcher Escape restores editor')
        click_at(0, 15)
        wait(lambda: contains('Esc: editor') and 'sec' in screen.display[-1],
             'Console launcher retains pending Program input')
        click('Editor')
        wait(lambda: contains('F10: Menu') and 0 < screen.cursor.y < 26,
             'Editor control returns keyboard focus')
        click('Hide')
        wait(lambda: not contains('Program console |'), 'Hide with running program draft')
        click_at(0, 15)
        wait(lambda: contains('Esc: editor') and 'sec' in screen.display[-1],
             'persistent Console launcher restores hidden program draft')
        assert source.read_text() == program
        resize(30, 100)
        wait(lambda: 'Program console |' in screen.display[-8], 'narrow bottom dock')
        assert 'sec' in screen.display[-1], screen.display
        # Tail visual rows, including continuation rows from one long record.
        assert screen.display[-4] == 'Z' * 99 + ' ', screen.display
        assert screen.display[-3].startswith('Z' * 5 + 'END› Zoë😀'), screen.display
        assert screen.display[-2].startswith('Hello ZOË😀'), screen.display
        assert all(screen.buffer[21][col].data == '-' for col in range(100)), screen.display
        caret = (screen.cursor.x, screen.cursor.y)
        click_at(21, 50)
        wait(lambda: (screen.cursor.x, screen.cursor.y) == caret,
             'horizontal divider click keeps input focus')
        os.write(master, b'ond\rthird\r')
        wait(lambda: contains('Third THIRD') and contains('status 7'), 'queued repeated input and completion')
        os.write(master, b'late\r')
        wait(lambda: 'Input closed>' in screen.display[-1], 'completed input closes')
        assert 'late' not in screen.display[-1].lower(), screen.display

        # Tab belongs to controls only while the console owns focus.
        os.write(master, b'\x1b[Z\x1b[H\x1b[C\x1b[C')
        wait(lambda: screen.cursor.y == screen.lines - 7 and screen.cursor.x == 10,
             'toolbar Clear control selected')
        colour(screen.buffer[screen.lines - 7][10].fg, 'E6F7FF')
        colour(screen.buffer[screen.lines - 7][10].bg, '264F78')
        os.write(master, b'\r')
        wait(lambda: not contains('Third THIRD'), 'keyboard Clear control')
        os.write(master, b'\x1b[H\r')
        wait(lambda: contains('Console ready') and contains('Running'), 'keyboard Run control')
        os.write(master, b'\t\x1b[C\r')
        wait(lambda: contains('Stopped'), 'keyboard Stop control')
        os.write(master, b'\x1b[F\r')
        wait(lambda: contains('RX CONSOLE: input') and screen.cursor.y < 21,
             'keyboard Editor control returns to editor')
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
        click('Hide')
        wait(lambda: not contains('Program console |'), 'mouse Hide control')
        menu('Show or focus Program console')
        wait(lambda: contains('Esc: editor'), 'menu reopens and focuses console')
        resize(16, 32)
        wait(lambda: contains('Esc: editor') and contains('Run Stop Clear Hide Editor'),
             'compact console keeps controls and editor return visible')
        assert contains('F10 Menu'), screen.display
        resize(24, 117)
        wait(lambda: 'Program console |' in screen.display[0][73:], 'minimum useful right dock')
        assert all(screen.buffer[row][72].data == '|' for row in range(24)), screen.display
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

print('Themed terminal console, cell wrapping/tailing, divider, controls, Unicode stdin and resize passed')
