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
        profile = root / 'keys.the'
        stock = repo / ('profile.the' if engine == 'CREXX' else 'profile_regina.the')
        profile.write_text(stock.read_text() + "\n'define f9 qquit';\n'define c-u cmsg';\n")
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

        def wait(predicate, label, timeout=25):
            deadline = time.monotonic() + timeout
            quiet = time.monotonic()
            while time.monotonic() < deadline:
                if select.select([master], [], [], .04)[0]:
                    try:
                        stream.feed(os.read(master, 65536))
                    except OSError:
                        if predicate():
                            return
                        break
                    quiet = time.monotonic()
                if predicate() and (process.poll() is not None or time.monotonic() - quiet >= .15):
                    return
            raise AssertionError(engine + ': ' + label + '\n' + '\n'.join(screen.display))

        def send(keys):
            os.write(master, keys)

        def click_at(row, col):
            # Xterm button-1 press/release, decoded by the native mouse reader.
            position = bytes((col + 33, row + 33))
            send(b'\x1b[M ' + position + b'\x1b[M#' + position)

        def command(text):
            send(b'\x1b')
            wait(lambda: screen.cursor.y == rows - 2, 'command focus')
            send(b'\x15')
            wait(lambda: not screen.display[rows - 2][8:].strip(), 'clear command')
            send(text.encode() + b'\r')

        def response(token):
            wait(lambda: token in screen.display[rows - 3], 'adjacent response: ' + token)

        try:
            wait(lambda: 'F10 Menu' in screen.display[rows - 4]
                 and 'CRXTHE 0.10.dev' in screen.display[rows - 3], 'guided footer')
            assert 'MENU (F10)' in screen.display[0][:14], screen.display
            click_at(0, 2)
            wait(lambda: 'Open or create file' in screen.display[2][:40],
                 'top-left mouse menu opens below its button')
            send(b'\x1b')
            wait(lambda: 'Open or create file' not in '\n'.join(screen.display),
                 'mouse menu cancels')
            command('msg FIRST_RESPONSE')
            response('FIRST_RESPONSE')
            assert screen.cursor.y == rows - 2, screen.cursor
            send(b'\x1b[17~')  # F6: return to file
            wait(lambda: screen.cursor.y < rows - 4, 'file focus')
            send(b'Z')  # Leave a pending working record before opening Menu.
            wait(lambda: any('Zalpha' in row for row in screen.display), 'pending edit')
            caret = (screen.cursor.x, screen.cursor.y)
            send(b'\x1b[21~')  # F10
            wait(lambda: any('Open or create file' in row for row in screen.display), 'Menu')
            send(b'\x1b')
            wait(lambda: not any('Open or create file' in row for row in screen.display)
                 and (screen.cursor.x, screen.cursor.y) == caret, 'Menu cancel restores caret')
            send(b'Q\x13')
            wait(lambda: source.read_text() == 'ZQalpha\nbeta\ngamma\ndelta\n', 'save after Menu')
            send(b'\x1b[21~')
            wait(lambda: any('Open or create file' in row for row in screen.display), 'Menu reopened')
            find_row = next(y for y, line in enumerate(screen.display) if 'Find...' in line)
            click_at(find_row, screen.display[find_row].index('Find...') + 1)
            wait(lambda: 'locate /' in screen.display[rows - 2], 'Menu Find opens command')
            send(b'\x15')  # Clear the prepared command before a new command.
            command('set msgline on 3 1')
            command('msg CUSTOM_RESPONSE')
            wait(lambda: any('CUSTOM_RESPONSE' in row for row in screen.display[:rows - 4]),
                 'custom message position')
            command('set msgline on -2 1')
            for index, (height, width) in enumerate([(16, 60), (16, 32), (15, 40), (24, 80)]):
                if index == 0:
                    send(b'\x1b[21~')
                    wait(lambda: 'Open or create file' in '\n'.join(screen.display),
                         'menu open before resize')
                rows, cols = height, width
                fcntl.ioctl(master, termios.TIOCSWINSZ,
                            struct.pack('HHHH', rows, cols, 0, 0))
                screen.resize(rows, cols)
                process.send_signal(signal.SIGWINCH)
                wait(lambda: screen.display[rows - 2].startswith('======>')
                     and 'F10 Menu' in screen.display[rows - 4], 'resize footer')
                assert 'Open or create file' not in '\n'.join(screen.display), screen.display
                command('msg COMPACT_RESPONSE')
                response('COMPACT_RESPONSE')
                assert 'F10 Menu' in screen.display[rows - 4], screen.display
                assert any('ZQalpha' in row for row in screen.display[:rows - 4]), screen.display
            send(b'\x1b[20~')  # F9: bounded test quit
            wait(lambda: process.poll() is not None, 'quit after resize', timeout=10)
            assert process.returncode == 0, process.returncode
            assert source.read_text() == 'ZQalpha\nbeta\ngamma\ndelta\n'
        finally:
            if process.poll() is None:
                process.kill()
            process.wait()
            os.close(master)

print('Both engines: terminal Menu, response/command adjacency, pending edits, custom MSGLINE and compact resize passed')
