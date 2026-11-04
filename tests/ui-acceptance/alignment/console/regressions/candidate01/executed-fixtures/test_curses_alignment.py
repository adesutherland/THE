#!/usr/bin/env python3
"""Role actions, independent result lanes and target toggles through a real PTY."""
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


class Terminal:
    def __init__(self, root, rows, cols):
        self.rows, self.cols = rows, cols
        self.screen = pyte.Screen(cols, rows)
        self.stream = pyte.ByteStream(self.screen)
        self.menu_active = False
        profile = root / 'startup.the'
        profile.write_text((repo / 'profile.the').read_text() +
                           "\n'define f9 qquit';\n'define c-u cmsg';\n'msg ALIGNMENT_READY';\n")
        self.master, slave = pty.openpty()
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))
        env = dict(os.environ, TERM='xterm-256color', THE_REXX_INTERPRETER='CREXX',
                   THE_MACRO_PATH=str(repo), THE_TOOL_CONFIG_DIR=str(root/'config'),
                   THE_CREXX_CACHE_DIR=str(root/'cache'))
        # CTest supplies the configured matching SDK. A direct invocation may
        # recover those same paths from this binary's build cache.
        cache = binary.parent / 'CMakeCache.txt'
        configured = {}
        if cache.exists():
            for line in cache.read_text().splitlines():
                if '=' in line and ':' in line.split('=', 1)[0]:
                    key, value = line.split('=', 1)
                    configured[key.split(':', 1)[0]] = value
        for variable, key in [('THE_CREXX_RXC', 'CREXX_RXC_EXECUTABLE'),
                              ('THE_CREXX_RXAS', 'CREXX_RXAS_EXECUTABLE'),
                              ('THE_CREXX_LIBRARY_RXBIN', 'CREXX_LIBRARY_RXBIN')]:
            if variable not in env and configured.get(key):
                env[variable] = configured[key]
        sdk = env.get('THE_CREXX_IMPORT_DIR') or env.get('CREXX_BIN_DIR')
        if not sdk and env.get('THE_CREXX_LIBRARY_RXBIN'):
            sdk = str(Path(env['THE_CREXX_LIBRARY_RXBIN']).parent)
        if sdk:
            env.setdefault('THE_CREXX_IMPORT_DIR', sdk)
            env['PATH'] = sdk + os.pathsep + env.get('PATH', '')
        self.process = subprocess.Popen([str(binary), '--driver', 'curses', '-p', str(profile), str(root/'notes.txt')],
                                        cwd=root, env=env, stdin=slave, stdout=slave, stderr=slave)
        os.close(slave)

    def text(self):
        return '\n'.join(self.screen.display)

    def send(self, keys):
        os.write(self.master, keys)

    def wait(self, predicate, label, timeout=35):
        end = time.monotonic() + timeout
        matched = None
        while time.monotonic() < end:
            if select.select([self.master], [], [], .025)[0]:
                try:
                    self.stream.feed(os.read(self.master, 65536))
                except OSError:
                    break
            if predicate():
                if matched is None:
                    matched = time.monotonic()
                if time.monotonic() - matched > .075:
                    return
            else:
                matched = None
            assert self.process.poll() is None, label + '\n' + self.text()
        raise AssertionError(f'{self.cols}x{self.rows}: {label}; cursor={self.screen.cursor.x},{self.screen.cursor.y}\n{self.text()}')

    def has(self, text):
        return text in self.text()

    def tools_pane(self):
        return self.text().count('Tools & Profiles |') > 1

    def profiles(self):
        row = next(i for i, line in enumerate(self.screen.display) if 'Profiles Tools Build Edit Hide Editor' in line)
        self.click(row, self.screen.display[row].index('Profiles Tools'))
        self.wait(lambda: self.has('Active profile:'), 'Profiles section')

    def click(self, row, col):
        position = bytes((col+33, row+33))
        self.send(b'\x1b[M ' + position + b'\x1b[M#' + position)

    def resize(self, rows, cols):
        fcntl.ioctl(self.master, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))
        self.screen.resize(rows, cols)
        self.process.send_signal(signal.SIGWINCH)

    def command(self, text):
        self.send(b'\x1b')
        self.wait(lambda: '======>' in self.screen.display[self.screen.cursor.y], 'THE command focus')
        self.send(b'\x15' + text.encode() + b'\r')
        self.wait(lambda: text not in self.screen.display[self.screen.cursor.y], 'command submitted: '+text)

    def menu(self):
        if not self.menu_active:
            self.send(b'\x1b[21~')
            self.wait(lambda: self.has('File: Open'), 'F10 menu')
            self.menu_active = True
        self.send(b'\x1b[H')
        self.wait(lambda: 'File:' in self.screen.display[self.screen.cursor.y], 'first menu entry')

    def choose(self, label, invoke=True):
        self.menu()
        for _ in range(160):
            row = self.screen.display[self.screen.cursor.y]
            if label in row:
                if invoke:
                    self.send(b'\r')
                    self.menu_active = False
                return
            before = (self.screen.cursor.y, row)
            self.send(b'\x1b[B')
            self.wait(lambda: (self.screen.cursor.y, self.screen.display[self.screen.cursor.y]) != before,
                      'menu navigation', 3)
        raise AssertionError('menu action missing: '+label+'\n'+self.text())

    def cancel_menu(self):
        self.send(b'\x1b')
        self.menu_active = False
        self.wait(lambda: not self.has('File: Open'), 'menu cancelled')

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
        self.process.wait(timeout=10)
        os.close(self.master)


for rows, cols in ((40, 140), (24, 80)):
    with tempfile.TemporaryDirectory(prefix='THE alignment PTY ') as temporary:
        root = Path(temporary)
        fixtures = {
            'notes.txt': 'Text remains usable. Zoë😀\n',
            'compile.crexx': "options levelb\nsay 'COMPILED_PROGRAM_OK'\nexit 0\n",
            'prompt.rexx': "say 'NAME_REQUIRED'\npull name\nsay 'HELLO' name\nexit 0\n",
            'custom.profile.the': "options levelb\naddress THE\n'set number on';\nreturn 0\n",
        }
        for name, text in fixtures.items():
            (root/name).write_text(text)
        terminal = Terminal(root, rows, cols)
        try:
            terminal.wait(lambda: terminal.has('ALIGNMENT_READY'), 'Text startup')
            assert 'Tools & Profiles' in terminal.screen.display[0]
            assert 'Run' not in terminal.screen.display[0]
            terminal.choose('View: Tools & Profiles')
            terminal.wait(lambda: terminal.has('Active profile:'), 'Tools / Profiles destination')
            terminal.click(0, 30)
            terminal.wait(lambda: not terminal.has('Active profile:') and not terminal.has('Program console |'), 'repeated Tools hides to editor')

            terminal.command('edit '+str(root/'compile.crexx'))
            terminal.wait(lambda: terminal.has('COMPILED_PROGRAM_OK'), 'compiled source')
            terminal.choose('Tools: Run', invoke=False)
            terminal.wait(lambda: terminal.has('Build this cREXX source before running'), 'Run prerequisite gives Build recovery')
            terminal.cancel_menu()
            terminal.choose('Tools: Build source')
            terminal.wait(lambda: terminal.has('Tools & Profiles | Build results'), 'Build results destination')
            terminal.wait(lambda: terminal.has('Completed') and terminal.has('status 0'), 'compiled source build')
            assert not terminal.has('Program input>'), terminal.text()
            terminal.choose('Tools: Run')
            terminal.wait(lambda: terminal.has('Program console |') and terminal.has('COMPILED_PROGRAM_OK') and terminal.has('Completed'), 'compiled program output')
            terminal.choose('Tools: Build results')
            terminal.wait(lambda: terminal.has('Tools & Profiles | Build results'), 'retained Build lane')
            assert not terminal.has('Program input>'), terminal.text()
            terminal.click(0, 17)
            terminal.wait(lambda: terminal.has('Program console |') and terminal.has('COMPILED_PROGRAM_OK'), 'retained Program lane')

            terminal.command('edit '+str(root/'prompt.rexx'))
            terminal.wait(lambda: terminal.has('NAME_REQUIRED'), 'Classic source')
            terminal.choose('Tools: Run')
            terminal.wait(lambda: terminal.has('Program input>') and terminal.has('Running') and terminal.has('NAME_REQUIRED'), 'Classic PULL')
            terminal.send('Ada😀'.encode())
            terminal.wait(lambda: terminal.has('Program input> Ada😀'), 'Unicode stdin draft')
            caret = (terminal.screen.cursor.x, terminal.screen.cursor.y)
            terminal.send(b'\x1b[Z\x1b[Z\r')
            terminal.wait(lambda: not terminal.has('Program console |') and not terminal.tools_pane(), 'keyboard Console toggle hides without sibling')
            terminal.send(b'\x1b[Z\r')
            terminal.wait(lambda: terminal.has('Program input> Ada😀') and terminal.has('Running'), 'keyboard Console restores live draft')
            assert (terminal.screen.cursor.x, terminal.screen.cursor.y) == caret, (caret, terminal.text())
            terminal.choose('View: Tools & Profiles')
            terminal.wait(terminal.tools_pane, 'Tools restores its retained section')
            terminal.profiles()
            terminal.wait(lambda: terminal.has('Active profile:'), 'Tools while Program waits')
            terminal.click(0, 30)
            terminal.wait(lambda: not terminal.has('Active profile:') and not terminal.has('Program console |'), 'Tools hide does not reveal retained Program')
            terminal.click(0, 17)
            terminal.wait(lambda: terminal.has('Program input> Ada😀') and terminal.has('Running'), 'mouse Console restores draft')
            terminal.send(b'\r')
            terminal.wait(lambda: terminal.has('HELLO ADA😀') and terminal.has('Completed'), 'stdin submission and completion')

            terminal.command('edit '+str(root/'custom.profile.the'))
            terminal.wait(lambda: terminal.has('address THE') and terminal.has("'set number on'"), 'profile source')
            terminal.choose('Tools: Build profile')
            terminal.wait(lambda: terminal.has('Completed') and terminal.has('status 0'), 'profile built')
            terminal.choose('Tools: Apply profile')
            terminal.wait(lambda: terminal.has('Profile activated'), 'profile applied')
            terminal.click(0, 30)
            terminal.wait(lambda: not terminal.tools_pane(), 'hide results')
            terminal.click(0, 30)
            terminal.wait(terminal.tools_pane, 'restore results')
            terminal.profiles()
            terminal.wait(lambda: terminal.has('Active profile: custom.profile.the') and terminal.has('startup selection saved'), 'applied profile identity')
            terminal.choose('Tools: Edit profile')
            terminal.wait(lambda: terminal.has('address THE') and terminal.has("'set number on'"), 'Edit profile returns to source')
            terminal.profiles()
            terminal.resize(15, 60)
            terminal.wait(lambda: not terminal.tools_pane(), 'insufficient-space Tools collapse')
            terminal.command('msg SMALL_TOOLS_EDITOR_RECOVERY')
            terminal.wait(lambda: terminal.has('SMALL_TOOLS_EDITOR_RECOVERY'), 'collapsed Tools returns physical input to editor')
            terminal.resize(rows, cols)
            terminal.wait(terminal.tools_pane, 'Tools restores after resize')
            assert not terminal.has('WORKBENCH command-failed'), terminal.text()
            for name, text in fixtures.items():
                assert (root/name).read_bytes() == text.encode(), name
            print(f'{cols}x{rows}: Text, compiled Build/Run, profile Build/Apply/Edit, separate lanes, keyboard/mouse target toggles, Unicode PULL draft/caret and unchanged fixture bytes passed')
        finally:
            terminal.close()
