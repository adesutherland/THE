#!/usr/bin/env python3
"""Check native form isolation/colours and default-profile syntax colours.

Read real PTY output, including ncurses OSC palette definitions. This checks
terminal cells, not a particular terminal application's final font pixels.
"""
import os
import sys
if os.name == 'nt':
    raise SystemExit(77)
try:
    import pyte
    from pyte import graphics
except ImportError:
    raise SystemExit(77)

import errno
import fcntl
from pathlib import Path
import pty
import re
import select
import shutil
import struct
import subprocess
import tempfile
import termios
import time

binary = Path(sys.argv[1]).resolve()
engine = sys.argv[2]
repo = Path(__file__).resolve().parent.parent
stock = repo / ('profile.the' if engine == 'CREXX' else 'profile_regina.the')
palette = list(graphics.FG_BG_256)


class Terminal:
    def __init__(self, profile, sample, env):
        self.output = bytearray()
        self.master, slave = pty.openpty()
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 27, 100, 0, 0))
        self.process = subprocess.Popen([str(binary), '-p', str(profile), str(sample)],
            stdin=slave, stdout=slave, stderr=slave, env=env)
        os.close(slave)

    def screen(self):
        # pyte ignores OSC 4. Apply the emitted palette before replaying SGR
        # indexes, so a custom terminal colour is not mistaken for xterm grey.
        graphics.FG_BG_256[:] = palette
        for match in re.finditer(rb'\x1b\]4;(\d+);rgb:([0-9a-fA-F]+)/([0-9a-fA-F]+)/([0-9a-fA-F]+)', self.output):
            index = int(match[1])
            if index < 256:
                parts = [round(int(part, 16) * 255 / (16 ** len(part) - 1)) for part in match.groups()[1:]]
                graphics.FG_BG_256[index] = ''.join(f'{part:02x}' for part in parts)
        screen = pyte.Screen(100, 27)
        pyte.ByteStream(screen).feed(bytes(self.output))
        return screen

    def wait(self, predicate, timeout=15):
        deadline = time.monotonic() + timeout
        quiet = time.monotonic()
        while time.monotonic() < deadline:
            if select.select([self.master], [], [], 0.04)[0]:
                try:
                    self.output.extend(os.read(self.master, 65536))
                except OSError as error:
                    if error.errno == errno.EIO:
                        break
                    raise
                quiet = time.monotonic()
            elif time.monotonic() - quiet > .15:
                screen = self.screen()
                if predicate(screen):
                    return screen
            assert self.process.poll() is None, self.output.decode(errors='replace')[-1000:]
        raise AssertionError('\n'.join(self.screen().display))

    def send(self, keys):
        os.write(self.master, keys)

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
        self.process.wait()
        os.close(self.master)


def cell_at(screen, token):
    row = next(i for i, text in enumerate(screen.display) if token in text)
    col = screen.display[row].index(token)
    return screen.buffer[row][col]


def colour(actual, expected):
    assert len(actual) == 6, (actual, expected)
    # ncurses scales RGB through the terminal's 0..1000 palette API.
    assert all(abs(int(actual[i:i+2], 16)-int(expected[i:i+2], 16)) <= 2
               for i in (0, 2, 4)), (actual, expected)


with tempfile.TemporaryDirectory(prefix='CRXTHE presentation ') as temporary:
    root = Path(temporary)
    env = dict(os.environ, THE_REXX_INTERPRETER=engine, TERM='xterm-256color',
        THE_CREXX_CACHE_DIR=str(root/'cache'), CREXXSAA_CACHE_DIR=str(root/'cache'))
    sdk = env.get('CREXX_BIN_DIR')
    if sdk:
        env['PATH'] = sdk + os.pathsep + env.get('PATH', '')
    sample = root/'sample.txt'
    original = b'seed\ntail\n'
    sample.write_bytes(original)
    profile = root/'profile.the'
    demo = 'paneldemo' if engine == 'CREXX' else 'paneldemo_regina'
    profile.write_text(stock.read_text() + "\n'cursor goto 2 3';\n'define f7 macro " + demo + "';\n")
    terminal = Terminal(profile, sample, env)
    try:
        terminal.wait(lambda s: any('tail' in row for row in s.display))
        terminal.send(b'\x1b[18~')  # F7
        form = terminal.wait(lambda s: any('Guest' in row for row in s.display))
        assert not any('seed' in row or 'tail' in row or re.match(r'\d{7} ', row) for row in form.display)
        name_row = next(row for row in form.display if 'Name:' in row)
        assert name_row.startswith('Name:'), name_row
        colour(cell_at(form, 'Name:').fg, 'DCE5F2')
        colour(cell_at(form, 'Name:').bg, '1E1E1E')
        colour(cell_at(form, 'Guest').fg, 'BDE9D9')
        colour(cell_at(form, 'Guest').bg, '243B36')
        terminal.send(b'Alice\tRexx\tB')
        terminal.wait(lambda s: any('Blice' in row for row in s.display) and any('Rexx' in row for row in s.display))
        terminal.send(b'\x1b[ZQ')
        terminal.wait(lambda s: any('Blice' in row for row in s.display) and any('Qexx' in row for row in s.display))
        terminal.send(b'\r')
        terminal.wait(lambda s: any('tail' in row for row in s.display) and any('F10 Keys' in row for row in s.display))
        terminal.send(b'\x1b[18~')
        terminal.wait(lambda s: any('Guest' in row for row in s.display))
        terminal.send(b'\x1b')
        terminal.wait(lambda s: any('tail' in row for row in s.display) and any('F10 Keys' in row for row in s.display))
        assert sample.read_bytes() == original
    finally:
        terminal.close()

    if not shutil.which('rxc', path=env.get('PATH')):
        print('SKIP: syntax presentation needs rxc on PATH')
        raise SystemExit(77)
    source = root/'sample.crexx'
    source.write_text('/* comment */\nsay "hello"\nanswer = 42\n')
    profile.write_text(stock.read_text() + "\n'set curline off';\n'sdslhwait 5000';\n"
                       "'msg SYNTAX_READY_' || rc;\n")
    terminal = Terminal(profile, source, env)
    try:
        # The file can be painted before its profile/parser wait finishes.
        # Use the profile's completion marker, independent of the expected
        # colours, then retain every cell's exact foreground assertion.
        screen = terminal.wait(lambda s: any('SYNTAX_READY_' in row for row in s.display))
        assert any('SYNTAX_READY_0' in row for row in screen.display), '\n'.join(screen.display)
        expected = {'/* comment */': '6A9955', 'say': '569CD6', '"hello"': 'CE9178',
                    'answer': '9CDCFE', '42': 'B5CEA8'}
        for token, foreground in expected.items():
            row = next(i for i, text in enumerate(screen.display) if token in text)
            start = screen.display[row].index(token)
            for col in range(start, start + len(token)):
                colour(screen.buffer[row][col].fg, foreground)
    finally:
        terminal.close()

print(engine + ': native form isolation, declared colours, Tab/Shift-Tab, restoration and five syntax colours passed')
