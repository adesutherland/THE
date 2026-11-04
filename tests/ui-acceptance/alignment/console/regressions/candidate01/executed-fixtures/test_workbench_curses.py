#!/usr/bin/env python3
"""Native Menu, adjacent command/response rows and compact layout on a PTY."""
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
import signal
import struct
import subprocess
import tempfile
import termios
import time

binary = Path(sys.argv[1]).resolve()
repo = Path(__file__).resolve().parent.parent

for engine in ['CREXX', 'REGINA']:
    with tempfile.TemporaryDirectory(prefix='THE terminal workbench ') as temporary:
        root = Path(temporary)
        source = root / 'sample.txt'
        source.write_text('alpha\nbeta\ngamma\ndelta\n')
        chosen = root / 'other path Ω.txt'
        chosen.write_text('chooser file bytes\n')
        profile = root / 'keys.the'
        stock = repo / ('profile.the' if engine == 'CREXX' else 'profile_regina.the')
        profile.write_text(stock.read_text() +
                           "\n'define f9 qquit';\n'define c-u cmsg';\n"
                           "'workbench begin owner native.probe';\n"
                           "'workbench menu put native.tools label \"Tools / \"\"quoted\"\" Ω\" order 15';\n"
                           "'workbench action put native.check label \"Check / \"\"quoted\"\" Ω\" command \"msg NATIVE_MENU_INVOKED\"';\n"
                           "'workbench item put native.tools native.item label \"Check / \"\"quoted\"\" Ω\" action native.check';\n"
                           "'workbench commit';\n"
                           "'msg PROFILE_READY';\n")
        env = dict(os.environ, THE_REXX_INTERPRETER=engine, TERM='xterm-256color',
                   THE_CREXX_CACHE_DIR=str(root / 'cache'),
                   THE_TOOL_CONFIG_DIR=str(root / 'tools'))
        master, slave = pty.openpty()
        rows, cols = 24, 80
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))
        screen = pyte.Screen(cols, rows)
        stream = pyte.ByteStream(screen)
        process = subprocess.Popen([str(binary), '-p', str(profile), str(source)],
                                   stdin=slave, stdout=slave, stderr=slave, env=env)
        os.close(slave)

        def wait(predicate, label, timeout=40):
            deadline = time.monotonic() + timeout
            matched = None
            while time.monotonic() < deadline:
                if select.select([master], [], [], .04)[0]:
                    try:
                        stream.feed(os.read(master, 65536))
                    except OSError:
                        if predicate():
                            return
                        break
                if predicate():
                    if matched is None:
                        matched = time.monotonic()
                    if process.poll() is not None or time.monotonic() - matched >= .15:
                        return
                else:
                    matched = None
            raise AssertionError(engine + ': ' + label + ' ' + str(screen.cursor) + '\n' + '\n'.join(screen.display))

        def send(keys):
            os.write(master, keys)

        def click_at(row, col):
            # Xterm button-1 press/release, decoded by the native mouse reader.
            position = bytes((col + 33, row + 33))
            send(b'\x1b[M ' + position + b'\x1b[M#' + position)

        def command(text):
            send(b'\x1b')
            wait(lambda: screen.cursor.y == rows - 3, 'command focus')
            send(b'\x15')
            wait(lambda: not screen.display[rows - 3][8:].strip(), 'clear command')
            send(text.encode() + b'\r')
            wait(lambda: not screen.display[rows - 3][8:].strip(), 'submitted command: ' + text)

        def response(token):
            wait(lambda: token in screen.display[rows - 4], 'adjacent response: ' + token)

        def saved_bytes_match(expected):
            try:
                return source.read_bytes() == expected
            except FileNotFoundError:
                # BACKUP renames the old file before writing its replacement.
                return False

        try:
            wait(lambda: 'F10 Menu' in screen.display[rows - 2]
                 and 'PROFILE_READY' in screen.display[rows - 4], 'guided footer')
            assert 'MENU (F10)' in screen.display[0][:14], screen.display
            assert 'Console' in screen.display[0][14:25], screen.display
            assert 'Tools & Profiles' in screen.display[0][25:45], screen.display
            click_at(0, 2)
            wait(lambda: any('File: Open' in row for row in screen.display),
                 'top-left mouse menu opens below its button')
            send(b'\x1b')
            wait(lambda: 'File: Open' not in '\n'.join(screen.display),
                 'mouse menu cancels')
            # Definitions do not execute; poisoned labels remain visible data.
            assert not any('NATIVE_MENU_INVOKED' in row for row in screen.display)
            send(b'\x1b[21~')
            wait(lambda: any('Check / "quoted" Ω' in row for row in screen.display),
                 'shared UTF-8 slash/quote label')
            custom = next(y for y, line in enumerate(screen.display) if 'Check / "quoted" Ω' in line)
            click_at(custom, 4)
            response('NATIVE_MENU_INVOKED')
            command('define f10 msg CUSTOM_F10_BINDING')
            send(b'\x1b[21~')
            response('CUSTOM_F10_BINDING')
            assert not any('File: Open' in row for row in screen.display)
            command('define f10 macro workbenchmenu' + ('_regina' if engine == 'REGINA' else ''))
            command('workbench begin owner native.live')
            command('workbench menu put native.live label "Live / label" order 15')
            command('workbench action put native.liveaction label "Live" command "msg NATIVE_LIVE_INVOKED"')
            command('workbench item put native.live native.liveitem label "Replaced / label" action native.liveaction')
            command('workbench commit')
            send(b'\x1b[21~')
            wait(lambda: any('Replaced / label' in row for row in screen.display), 'published replacement')
            send(b'\x1b')
            wait(lambda: not any('Replaced / label' in row for row in screen.display), 'cancel updated menu')
            command('workbench begin owner native.live')
            command('workbench menu remove native.live')
            command('workbench action remove native.liveaction')
            command('workbench commit')
            # Keyboard-only path filtering and cancellation leave the caller intact.
            send(b'CHOOSER_PENDING')
            wait(lambda: 'CHOOSER_PENDING' in screen.display[rows - 3], 'chooser command draft')
            chooser_draft = screen.display[rows - 3]
            chooser_caret = (screen.cursor.x, screen.cursor.y)
            chooser_tabs = screen.display[0]
            send(b'\x1b[21~\x1b[H\x1b[B\r')
            wait(lambda: any('Choose path' in row for row in screen.display), 'keyboard path chooser')
            send('other'.encode())
            wait(lambda: any('other path Ω.txt' in row for row in screen.display), 'filtered Unicode filename')
            send(b'\t\x1b')
            wait(lambda: not any('Choose path' in row for row in screen.display), 'cancel path chooser')
            assert screen.display[rows - 3] == chooser_draft, screen.display
            assert (screen.cursor.x, screen.cursor.y) == chooser_caret, screen.cursor
            assert screen.display[0] == chooser_tabs, screen.display
            assert source.read_bytes() == b'alpha\nbeta\ngamma\ndelta\n'
            send(b'\x1b[21~\x1b[H\x1b[B\r')
            wait(lambda: any('Choose path' in row for row in screen.display), 'retry path chooser')
            send('other'.encode())
            wait(lambda: any('other path Ω.txt' in row for row in screen.display), 'retry filtered filename')
            send(b'\x1b[B\r')
            wait(lambda: any('chooser file bytes' in row for row in screen.display), 'keyboard opens chosen file')
            assert chosen.read_bytes() == b'chooser file bytes\n'
            command('qquit')
            wait(lambda: any('alpha' in row for row in screen.display), 'return to original buffer')
            send(b'\x15')
            command('showkey')
            wait(lambda: any('Press the key' in row for row in screen.display), 'key guide')
            send(b'\x1b[Z')
            wait(lambda: any('rx controls' in row.lower() for row in screen.display),
                 'key guide reports the actual terminal Shift-Tab binding')
            send(b' ')
            command('msg FIRST_RESPONSE')
            response('FIRST_RESPONSE')
            assert screen.cursor.y == rows - 3, screen.cursor
            send(b'UNSUBMITTED')
            wait(lambda: 'UNSUBMITTED' in screen.display[rows - 3], 'editor command draft')
            draft = screen.display[rows - 3]
            send(b'\x1b[Z')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == (15, 0), 'Shift-Tab focuses Console')
            assert screen.buffer[0][15].bg != screen.buffer[0][1].bg, screen.display
            send(b'\x1b[Z')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == (1, 0), 'Shift-Tab then focuses Menu')
            for keys, x in [(b'\t', 15), (b'\x1b[D', 1), (b'\x1b[C', 15)]:
                send(keys)
                wait(lambda: (screen.cursor.x, screen.cursor.y) == (x, 0), 'chrome traversal')
            send(b'\x1b')
            wait(lambda: screen.cursor.y == rows - 3,
                 'chrome Escape follows profile command shortcut')
            assert screen.display[rows - 3] == draft, screen.display
            send(b'\x15')
            send(b'\x1b[17~')  # F6: return to file
            wait(lambda: screen.cursor.y < rows - 4, 'file focus')
            send(b'Z')  # Leave a pending working record before opening Menu.
            wait(lambda: any('Zalpha' in row for row in screen.display), 'pending edit')
            caret = (screen.cursor.x, screen.cursor.y)
            send(b'\x1b[Z\r')
            wait(lambda: 'Program console |' in screen.display[rows - 8],
                 'Console launcher opens the dock')
            assert any('Zalpha' in row for row in screen.display), screen.display
            send(b'\x1b')
            wait(lambda: screen.display[screen.cursor.y].startswith('======>'),
                 'console Escape follows profile command shortcut')
            send(b'\x1b[17~')  # F6: return to file for the pending edit.
            wait(lambda: (screen.cursor.x, screen.cursor.y) == caret,
                 'F6 restores pending editor caret')
            click_at(rows - 7, screen.display[rows - 7].index('Hide'))
            wait(lambda: not any('Program console |' in row for row in screen.display)
                 and (screen.cursor.x, screen.cursor.y) == caret, 'Hide preserves pending editor record')
            assert source.read_text() == 'alpha\nbeta\ngamma\ndelta\n', source.read_text()
            send(b'\x1b[Z\x1b[Z\r')
            wait(lambda: any('File: Open' in row for row in screen.display),
                 'Shift-Tab reaches and activates Menu')
            send(b'\x1b')
            wait(lambda: not any('File: Open' in row for row in screen.display)
                 and (screen.cursor.x, screen.cursor.y) == (1, 0),
                 'chrome Menu cancel restores the invoking control')
            send(b'\x1b')
            wait(lambda: screen.display[screen.cursor.y].startswith('======>'),
                 'leave top controls for THE command')
            send(b'\x1b[17~')
            wait(lambda: (screen.cursor.x, screen.cursor.y) == caret,
                 'F6 returns to pending editor caret after top Menu')
            send(b'\x1b[21~')  # F10
            wait(lambda: any('File: Open' in row for row in screen.display), 'Menu')
            send(b'\x1b')
            wait(lambda: not any('File: Open' in row for row in screen.display)
                 and (screen.cursor.x, screen.cursor.y) == caret, 'Menu cancel restores caret')
            send(b'Q\x13')
            expected_saved = b'ZQalpha\nbeta\ngamma\ndelta\n'
            wait(lambda: saved_bytes_match(expected_saved), 'save after Menu')
            assert source.read_bytes() == expected_saved
            send(b'\x1bOS')  # F4: the profile's Prefix command.
            wait(lambda: screen.cursor.x < 8 and screen.cursor.y < rows - 4, 'prefix focus')
            send(b'\x1b[C')
            wait(lambda: 0 < screen.cursor.x < 8, 'prefix caret')
            prefix_caret = (screen.cursor.x, screen.cursor.y)
            send(b'\x1b[21~')
            wait(lambda: any('File: Open' in row for row in screen.display), 'prefix Menu')
            send(b'\x1b')
            wait(lambda: not any('File: Open' in row for row in screen.display)
                 and (screen.cursor.x, screen.cursor.y) == prefix_caret, 'Menu cancel restores exact prefix caret')
            send(b'\x1b[21~')
            wait(lambda: any('File: Open' in row for row in screen.display), 'Menu reopened')
            find_row = next(y for y, line in enumerate(screen.display) if 'Find...' in line)
            click_at(find_row, screen.display[find_row].index('Find...') + 1)
            wait(lambda: 'locate /' in screen.display[rows - 3], 'Menu Find opens command')
            send(b'\x15')  # Clear the prepared command before a new command.
            command('set msgline on 3 1')
            command('msg CUSTOM_RESPONSE')
            wait(lambda: any('CUSTOM_RESPONSE' in row for row in screen.display[:rows - 4]),
                 'custom message position')
            command('set msgline on -3 1')
            for index, (height, width) in enumerate([(16, 60), (16, 32), (15, 40), (24, 80)]):
                if index == 0:
                    send(b'\x1b[21~')
                    wait(lambda: 'File: Open' in '\n'.join(screen.display),
                         'menu open before resize')
                rows, cols = height, width
                fcntl.ioctl(master, termios.TIOCSWINSZ,
                            struct.pack('HHHH', rows, cols, 0, 0))
                screen.resize(rows, cols)
                process.send_signal(signal.SIGWINCH)
                wait(lambda: screen.display[rows - 3].startswith('======>')
                     and 'F10 Menu' in screen.display[rows - 2], 'resize footer')
                assert 'File: Open' not in '\n'.join(screen.display), screen.display
                command('msg COMPACT_RESPONSE')
                response('COMPACT_RESPONSE')
                assert 'F10 Menu' in screen.display[rows - 2], screen.display
                assert any('ZQalpha' in row for row in screen.display[:rows - 4]), screen.display
            send(b'\x1b[20~')  # F9: bounded test quit
            wait(lambda: process.poll() is not None, 'quit after resize', timeout=10)
            assert process.returncode == 0, process.returncode
            assert source.read_bytes() == expected_saved
        finally:
            if process.poll() is None:
                process.kill()
            process.wait()
            os.close(master)

print('Both engines: Shift-Tab launchers, truthful key guide, draft/caret preservation, Menu, response rows and compact resize passed')
