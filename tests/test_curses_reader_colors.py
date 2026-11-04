#!/usr/bin/env python3
"""Real PTY: named colour translation and retained Program/Build reader cues."""
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
from pathlib import Path
import pty
import re
import select
import struct
import subprocess
import tempfile
import termios
import time

binary = Path(sys.argv[1]).resolve()
repo = Path(__file__).resolve().parent.parent
palette = list(graphics.FG_BG_256)


class Terminal:
    def __init__(self, root, rows=40, cols=140, engine='CREXX', extra=''):
        self.rows, self.cols = rows, cols
        self.screen = pyte.Screen(cols, rows)
        self.stream = pyte.ByteStream(self.screen)
        self.output = bytearray()
        profile = root/'startup.the'
        stock = repo/('profile.the' if engine == 'CREXX' else 'profile_regina.the')
        profile.write_text(stock.read_text()+"\n'define c-u cmsg';\n"+extra+"\n'msg C02_READY';\n")
        env = dict(os.environ, TERM='xterm-256color', THE_REXX_INTERPRETER=engine,
                   THE_MACRO_PATH=str(root)+os.pathsep+str(repo),
                   THE_TOOL_CONFIG_DIR=str(root/'config'), THE_CREXX_CACHE_DIR=str(root/'cache'))
        cache = binary.parent/'CMakeCache.txt'
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
            env['PATH'] = sdk+os.pathsep+env.get('PATH', '')
        self.master, slave = pty.openpty()
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))
        self.process = subprocess.Popen([str(binary), '--driver', 'curses', '-p', str(profile), str(root/'notes.txt')],
                                        cwd=root, env=env, stdin=slave, stdout=slave, stderr=slave)
        os.close(slave)

    def text(self):
        return '\n'.join(self.screen.display)

    def has(self, text):
        return text in self.text()

    def send(self, keys):
        os.write(self.master, keys)

    def wait(self, predicate, label, timeout=30):
        end = time.monotonic()+timeout
        matched = None
        while time.monotonic() < end:
            if select.select([self.master], [], [], .025)[0]:
                try:
                    data = os.read(self.master, 65536)
                except OSError:
                    break
                self.output.extend(data)
                graphics.FG_BG_256[:] = palette
                for match in re.finditer(rb'\x1b\]4;(\d+);rgb:([0-9a-fA-F]+)/([0-9a-fA-F]+)/([0-9a-fA-F]+)', self.output):
                    index = int(match[1])
                    if index < 256:
                        parts = [round(int(part, 16)*255/(16**len(part)-1)) for part in match.groups()[1:]]
                        graphics.FG_BG_256[index] = ''.join(f'{part:02x}' for part in parts)
                self.stream.feed(data)
            if predicate():
                if matched is None:
                    matched = time.monotonic()
                if time.monotonic()-matched >= .075:
                    return
            else:
                matched = None
            assert self.process.poll() is None, label+'\n'+self.text()
        raise AssertionError(label+'\n'+self.text())

    def command(self, command):
        self.send(b'\x1b')
        self.wait(lambda: self.screen.display[self.screen.cursor.y].startswith('======>'), 'editor command focus')
        self.send(b'\x15'+command.encode()+b'\r')
        self.wait(lambda: command not in self.screen.display[self.screen.cursor.y], 'command: '+command)

    def click(self, row, col):
        position = bytes((col+33, row+33))
        self.send(b'\x1b[M '+position+b'\x1b[M#'+position)

    def origin(self, title='Program console |'):
        row = next(i for i, line in enumerate(self.screen.display) if title in line)
        return row, self.screen.display[row].index(title)-1

    def body(self, build=False):
        row, left = self.origin('Tools & Profiles | Build results' if build else 'Program console |')
        return [line[left:].rstrip() for line in self.screen.display[row+(3 if build else 4):self.rows-1]]

    def colour(self, token, foreground, background):
        row = next(i for i, line in enumerate(self.screen.display) if token in line)
        col = self.screen.display[row].index(token)
        cell = self.screen.buffer[row][col]
        # pyte names ANSI colour 3 "brown"; THE's yellow foreground adds
        # bold, while its background uses the same standard palette slot.
        named = lambda colour: 'yellow' if colour == 'brown' else colour
        assert (named(cell.fg), named(cell.bg)) == (foreground, background), (token, cell, foreground, background)
        if foreground in ('white', 'yellow'):
            assert cell.bold, (token, cell)

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
        self.process.wait(timeout=10)
        os.close(self.master)


colours = ['black', 'blue', 'green', 'cyan', 'red', 'magenta', 'yellow', 'white']
for engine in ('CREXX', 'REGINA'):
    with tempfile.TemporaryDirectory(prefix='THE C02 colours ') as temporary:
        root = Path(temporary)
        original = b'colour fixture\n'
        (root/'notes.txt').write_bytes(original)
        commands = ['set curline off', 'set reserved -2 off',
                    'set reserved -4 white on blue NATIVEONE', 'set footer keys -4 aftercmdline',
                    'set reserved -3 yellow on black NATIVETWO λ界', 'set footer second -3 aftercmdline']
        commands += [f'set reserved +{4+i} {name} on {colours[(i+1)%8]} BASIC_{name.upper()}' for i, name in enumerate(colours)]
        commands += ['set color consolebody blue on black', 'set color consoleheader white on blue',
                     'set color consoleinput yellow on black', 'set color consolecontrol green on black',
                     'set color consoleselected cyan on red', 'set color consoledivider magenta on black',
                     'set color consolestatus red on black', 'set color consoleerror white on red']
        terminal = Terminal(root, engine=engine, extra='\n'.join("'"+command+"';" for command in commands))
        try:
            terminal.wait(lambda: terminal.has('C02_READY') and terminal.has('NATIVETWO λ界'), engine+' named profile')
            for i, name in enumerate(colours):
                terminal.colour('BASIC_'+name.upper(), name, colours[(i+1)%8])
            terminal.colour('NATIVEONE', 'white', 'blue')
            terminal.colour('NATIVETWO λ界', 'yellow', 'black')
            # Verify the emitted standard SGR palette as well as interpreted cells.
            sgr = {int(value) for match in re.finditer(rb'\x1b\[([0-9;]+)m', terminal.output)
                   for value in match[1].split(b';') if value}
            assert set(range(30, 38)) <= sgr and set(range(40, 48)) <= sgr, sgr
            terminal.command('rx console on')
            terminal.wait(lambda: terminal.has('Program input>') or terminal.has('Input closed>'), 'named console roles')
            terminal.colour('Program console |', 'white', 'blue')
            terminal.colour('Tools Stop Clear Hide Editor', 'green', 'black')
            terminal.colour('Input closed>', 'yellow', 'black')
            terminal.colour('Ready', 'red', 'black')
            row, left = terminal.origin()
            assert (terminal.screen.buffer[row+4][left].fg, terminal.screen.buffer[row+4][left].bg) == ('blue', 'black')
            assert (terminal.screen.buffer[row+4][left-1].fg, terminal.screen.buffer[row+4][left-1].bg) == ('magenta', 'black')
            terminal.send(b'\t')
            terminal.wait(lambda: terminal.screen.cursor.y == row+1, 'named selected control')
            cell = terminal.screen.buffer[terminal.screen.cursor.y][terminal.screen.cursor.x]
            assert (cell.fg, cell.bg) == ('cyan', 'red'), cell
            terminal.command('set color consolebody #123456 on #234567')
            terminal.wait(lambda: len(terminal.screen.buffer[row+4][left].fg) == 6, 'RGB body still allocated')
            rgb = (terminal.screen.buffer[row+4][left].fg, terminal.screen.buffer[row+4][left].bg)
            # ncurses quantizes RGB through its 0..1000 palette API.
            for actual, expected in zip(rgb, ('123456', '234567')):
                assert len(actual) == 6 and all(abs(int(actual[i:i+2], 16)-int(expected[i:i+2], 16)) <= 2 for i in (0, 2, 4)), (actual, expected)
            before = len(terminal.output)
            terminal.command('set color blue 100 200 300')
            terminal.wait(lambda: re.search(rb'\x1b\]4;4;rgb:', terminal.output[before:]) is not None, 'SET COLOR blue targets native blue slot')
            assert re.search(rb'\x1b\]4;1;rgb:', terminal.output[before:]) is None
            assert (terminal.screen.buffer[row+4][left].fg, terminal.screen.buffer[row+4][left].bg) == rgb
            assert (root/'notes.txt').read_bytes() == original
            print(engine+': eight named fg/bg colours, two Unicode bands, Console roles and basic palette write/RGB isolation passed', flush=True)
        finally:
            terminal.close()

# A six-row dock must keep completion and stdin visible beside the cue.
with tempfile.TemporaryDirectory(prefix='THE C02 compact reader ') as temporary:
    root = Path(temporary)
    (root/'notes.txt').write_text('compact reader\n')
    (root/'prompt.rexx').write_text("do i = 1 to 30\n say 'SMALL_ROW' i\nend\npull name\nsay 'SMALL_TAIL' name\nexit 0\n")
    terminal = Terminal(root, 20, 32)
    try:
        terminal.wait(lambda: terminal.has('C02_READY'), 'compact reader startup')
        terminal.command('edit '+str(root/'prompt.rexx'))
        terminal.command('rx run')
        terminal.wait(lambda: terminal.has('Program input>') and terminal.has('SMALL_ROW 30'), 'compact PULL')
        terminal.send(b'\x1b[5~')
        terminal.wait(lambda: terminal.has('Earlier output | End: Latest'), 'compact retained header hint')
        top = terminal.screen.display[17]
        terminal.send(b'Ada\r')
        terminal.wait(lambda: terminal.has('Completed') and terminal.has('status 0') and terminal.has('New output | End: Latest'), 'compact completion and unread remain separate')
        assert terminal.screen.display[17] == top and terminal.has('Input closed>'), terminal.text()
        terminal.send(b'\x1b[F')
        terminal.wait(lambda: terminal.has('SMALL_TAIL ADA') and not terminal.has('End: Latest'), 'compact End recovery')
        print('32x20: six-row reader preserves completion/status0 and closed input beside unread/End cue', flush=True)
    finally:
        terminal.close()


for rows, cols in ((40, 140), (24, 80)):
    with tempfile.TemporaryDirectory(prefix='THE C02 readers ') as temporary:
        root = Path(temporary)
        files = {'notes.txt': 'reader fixture\n',
                 'prompt.rexx': "do i = 1 to 50\n say 'READER_ROW' i\nend\npull name\nsay 'APPEND_TAIL' name\nexit 0\n",
                 'source.probe': 'saved build input\n',
                 'build-log.the': "options levelb\ndo i = 1 to 60\n say 'BUILD_ROW' i\nend\nreturn 0\n"}
        for name, content in files.items():
            (root/name).write_text(content)
        extra = ("'workbench begin owner public.reader-test';\n"
                 f"'workbench action put public.reader-build label \"Reader build\" macro \"{root/'build-log.the'}\" FILEEXT \".probe\" PURPOSE BUILD';\n"
                 "'workbench commit';\n")
        terminal = Terminal(root, rows, cols, extra=extra)
        try:
            terminal.wait(lambda: terminal.has('C02_READY'), 'reader profile')
            terminal.command('edit '+str(root/'prompt.rexx'))
            terminal.command('rx run')
            terminal.wait(lambda: terminal.has('Running') and any('READER_ROW 50' in line for line in terminal.body()), 'live PULL output')
            terminal.send(b'\x1b[5~')
            terminal.wait(lambda: terminal.has('Earlier output | End: Latest'), 'backscroll recovery hint')
            top = terminal.body()[0]
            assert not terminal.has('New output |'), terminal.text()
            terminal.send('Ada😀'.encode())
            terminal.wait(lambda: terminal.has('Program input> Ada😀'), 'unsent Unicode draft')
            caret = (terminal.screen.cursor.x, terminal.screen.cursor.y)
            terminal.click(0, 17)
            terminal.wait(lambda: not terminal.has('Program console |'), 'hide reader')
            terminal.click(0, 17)
            terminal.wait(lambda: terminal.has('Program input> Ada😀') and terminal.has('Earlier output | End: Latest'), 'restore retained reader/draft')
            assert terminal.body()[0] == top and (terminal.screen.cursor.x, terminal.screen.cursor.y) == caret
            terminal.send(b'\r')
            terminal.wait(lambda: terminal.has('Completed') and terminal.has('status 0') and terminal.has('New output | End: Latest'), 'completion announces unseen output')
            assert terminal.body()[0] == top, terminal.text()
            assert not any('APPEND_TAIL ADA😀' in line for line in terminal.body()), terminal.text()

            terminal.command('edit '+str(root/'source.probe'))
            terminal.command('workbench invoke public.reader-build')
            terminal.wait(lambda: terminal.has('Tools & Profiles | Build results') and any('BUILD_ROW 60' in line for line in terminal.body(True)), 'separate Build output')
            assert not terminal.has('New output |'), terminal.text()
            terminal.send(b'\x1b[H')
            terminal.wait(lambda: terminal.has('Earlier output | End: Latest') and any('BUILD_ROW 1' == line.strip() for line in terminal.body(True)), 'Build reader recovery hint')
            build_top = terminal.body(True)[0]
            terminal.click(0, 17)
            terminal.wait(lambda: terminal.has('New output | End: Latest') and terminal.has('Program console |'), 'Program unread survives Build selection')
            assert terminal.body()[0] == top
            terminal.send(b'\x1b[F')
            terminal.wait(lambda: any('APPEND_TAIL ADA😀' in line for line in terminal.body()) and not terminal.has('End: Latest'), 'End reveals appended tail and clears cue')
            terminal.click(0, 30)
            terminal.wait(lambda: terminal.has('Tools & Profiles | Build results') and terminal.has('Earlier output | End: Latest'), 'Build reader independently retained')
            assert terminal.body(True)[0] == build_top and not terminal.has('New output |')
            terminal.command('workbench invoke public.reader-build')
            terminal.wait(lambda: any('BUILD_ROW 60' in line for line in terminal.body(True)) and not terminal.has('End: Latest'), 'new Build job resets reader cue')
            terminal.send(b'\x1b[H')
            terminal.wait(lambda: terminal.has('End: Latest'), 'Build backscroll before Clear')
            terminal.command('rx clear')
            terminal.wait(lambda: not terminal.has('End: Latest') and not any('BUILD_ROW' in line for line in terminal.body(True)), 'Build Clear resets reader cue')

            terminal.command('edit '+str(root/'prompt.rexx'))
            terminal.click(0, 17)
            terminal.wait(lambda: terminal.has('Program console |'), 'retained Program after Build Clear')
            terminal.send(b'\x1b[H')
            terminal.wait(lambda: terminal.has('End: Latest'), 'Program backscroll before new job')
            terminal.command('rx run')
            terminal.wait(lambda: terminal.has('Running') and any('READER_ROW 50' in line for line in terminal.body()) and not terminal.has('End: Latest'), 'new Program job resets unread and viewport')
            terminal.send(b'\x1b[5~')
            terminal.wait(lambda: terminal.has('End: Latest'), 'Program backscroll before Clear')
            terminal.command('rx clear')
            terminal.wait(lambda: not terminal.has('End: Latest') and not any('READER_ROW' in line for line in terminal.body()), 'Program Clear resets cue')
            terminal.command('rx stop')
            for name, content in files.items():
                assert (root/name).read_bytes() == content.encode(), name
            print(f'{cols}x{rows}: live PULL completion unread cue/retained viewport, End recovery, independent Build reader, new-job/Clear resets and fixture bytes passed', flush=True)
        finally:
            terminal.close()
