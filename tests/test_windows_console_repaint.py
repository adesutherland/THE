#!/usr/bin/env python3
"""Read actual native console cells during tab/Unicode cursor repaint and READ."""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

if os.name != 'nt':
    sys.exit(77)

parser = argparse.ArgumentParser()
parser.add_argument('binary', type=Path)
parser.add_argument('engine', choices=['CREXX', 'REGINA'])
parser.add_argument('--profile', type=Path)
parser.add_argument('--record-json', type=Path)
parser.add_argument('--visible', action='store_true')
args = parser.parse_args()
binary = args.binary.resolve()
repo = Path(__file__).resolve().parent.parent
stock = args.profile or repo/('profile.the' if args.engine == 'CREXX' else 'profile_regina.the')


class Coord(C.Structure):
    _fields_ = [('X', W.SHORT), ('Y', W.SHORT)]


class Rect(C.Structure):
    _fields_ = [('Left', W.SHORT), ('Top', W.SHORT), ('Right', W.SHORT), ('Bottom', W.SHORT)]


class Info(C.Structure):
    _fields_ = [('size', Coord), ('cursor', Coord), ('attributes', W.WORD),
                ('window', Rect), ('maximum', Coord)]


class Char(C.Union):
    _fields_ = [('UnicodeChar', W.WCHAR), ('AsciiChar', C.c_char)]


class Cell(C.Structure):
    _fields_ = [('char', Char), ('attributes', W.WORD)]


class Key(C.Structure):
    _fields_ = [('down', W.BOOL), ('repeat', W.WORD), ('vk', W.WORD),
                ('scan', W.WORD), ('char', Char), ('control', W.DWORD)]


class Event(C.Union):
    _fields_ = [('key', Key), ('padding', C.c_byte*16)]


class Input(C.Structure):
    _fields_ = [('type', W.WORD), ('event', Event)]


assert C.sizeof(Cell) == 4 and C.sizeof(Input) == 20
kernel = C.WinDLL('kernel32', use_last_error=True)
kernel.CreateFileW.argtypes = [W.LPCWSTR, W.DWORD, W.DWORD, W.LPVOID, W.DWORD, W.DWORD, W.HANDLE]
kernel.CreateFileW.restype = W.HANDLE
kernel.CloseHandle.argtypes = [W.HANDLE]
kernel.AttachConsole.argtypes = [W.DWORD]
kernel.FreeConsole.argtypes = []
kernel.GetConsoleScreenBufferInfo.argtypes = [W.HANDLE, C.POINTER(Info)]
kernel.ReadConsoleOutputW.argtypes = [W.HANDLE, C.POINTER(Cell), Coord, Coord, C.POINTER(Rect)]
kernel.WriteConsoleInputW.argtypes = [W.HANDLE, C.POINTER(Input), W.DWORD, C.POINTER(W.DWORD)]
kernel.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
kernel.OpenProcess.restype = W.HANDLE
kernel.QueryFullProcessImageNameW.argtypes = [W.HANDLE, W.DWORD, W.LPWSTR, C.POINTER(W.DWORD)]


def checked(ok):
    if not ok:
        raise C.WinError(C.get_last_error())


class Console:
    def __init__(self, process):
        self.process = process
        self.input = None
        self.frames = []
        handle = kernel.OpenProcess(0x1000, False, process.pid)
        checked(handle)
        try:
            image = C.create_unicode_buffer(32768)
            length = W.DWORD(len(image))
            checked(kernel.QueryFullProcessImageNameW(handle, 0, image, C.byref(length)))
            assert Path(image.value).resolve() == binary, image.value
        finally:
            kernel.CloseHandle(handle)
        kernel.FreeConsole()
        for _ in range(100):
            if kernel.AttachConsole(process.pid):
                break
            assert process.poll() is None, ('startup exit', process.returncode)
            time.sleep(.05)
        else:
            raise RuntimeError('Cannot attach the owned editor console')
        self.input = kernel.CreateFileW('CONIN$', 0xc0000000, 3, None, 3, 0, None)
        assert self.input not in (None, W.HANDLE(-1).value)

    def read(self, label, record=True):
        # PDCurses switches active buffers at startup. Reopen CONOUT$ each time.
        output = kernel.CreateFileW('CONOUT$', 0xc0000000, 3, None, 3, 0, None)
        assert output not in (None, W.HANDLE(-1).value)
        try:
            info = Info()
            checked(kernel.GetConsoleScreenBufferInfo(output, C.byref(info)))
            r = info.window
            width, height = r.Right-r.Left+1, r.Bottom-r.Top+1
            cells = (Cell*(width*height))()
            region = Rect(r.Left, r.Top, r.Right, r.Bottom)
            checked(kernel.ReadConsoleOutputW(output, cells, Coord(width, height), Coord(0, 0), C.byref(region)))
            rows = []
            for y in range(height):
                units = [ord(cells[y*width+x].char.UnicodeChar) for x in range(width)]
                attrs = [cells[y*width+x].attributes for x in range(width)]
                # Wide BMP glyphs occupy LVB leading/trailing cells; omit only
                # the duplicate trailing cell when comparing human-readable text.
                text = ''.join(chr(u) for u, a in zip(units, attrs) if not a & 0x200)
                rows.append(dict(text=text, units=units, attrs=attrs))
            frame = dict(label=label, width=width, height=height, rows=rows)
            if record:
                self.frames.append(frame)
            return frame
        finally:
            kernel.CloseHandle(output)

    def wait(self, label, predicate, timeout=12):
        deadline = time.monotonic()+timeout
        previous = None
        stable_since = None
        while True:
            frame = self.read(label, False)
            if predicate(frame):
                signature = [(row['units'], row['attrs']) for row in frame['rows']]
                if signature != previous:
                    previous, stable_since = signature, time.monotonic()
                elif time.monotonic()-stable_since >= .08:
                    self.frames.append(frame)
                    return frame
            else:
                previous = stable_since = None
            assert self.process.poll() is None, ('Editor exited', self.process.returncode)
            if time.monotonic() >= deadline:
                self.frames.append(frame)
                raise AssertionError(json.dumps(dict(label=label, rows=[row['text'] for row in frame['rows']]), ensure_ascii=True))
            time.sleep(.04)

    def key(self, vk, char='\0', control=0):
        assert self.process.poll() is None
        if 0x25 <= vk <= 0x28:
            control |= 0x100  # ENHANCED_KEY: dedicated arrows, not keypad keys.
        events = (Input*2)()
        for i, down in enumerate((True, False)):
            events[i].type = 1
            events[i].event.key = Key(down, 1, vk, 0, Char(UnicodeChar=char), control)
        written = W.DWORD()
        checked(kernel.WriteConsoleInputW(self.input, events, 2, C.byref(written)))
        assert written.value == 2

    def command(self, text):
        self.key(0x4c, '\x0c', 8)
        for char in text:
            self.key(ord(char.upper()) if char.isalnum() or char == ' ' else 0, char)
        self.key(0x0d, '\r')

    def close(self):
        if self.input:
            kernel.CloseHandle(self.input)
        kernel.FreeConsole()
        kernel.AttachConsole(0xffffffff)


def position(frame):
    for row in frame['rows']:
        match = re.search(r'Line=(\d+)\s+Col=(\d+)', row['text'])
        if match:
            return tuple(map(int, match.groups()))
    return None


def contains(frame, value):
    return any(value in row['text'] for row in frame['rows'])


failures = []


def expect(ok, label, detail):
    if not ok:
        failures.append(dict(check=label, actual=detail))


def check_status(frame, code):
    row = frame['rows'][-1]
    start, end = frame['width']-27, frame['width']-7
    field = ''.join(chr(u) for u, a in zip(row['units'][start:end], row['attrs'][start:end]) if not a & 0x200)
    match = re.search(r'U\+[0-9A-F]+(?:\+[0-9A-F]+)*', field)
    expect(match is not None and match.group() == code, frame['label']+' exact code', field)
    expect(field.count('[') == 1 and field.count(']') == 1, frame['label']+' bracket bounds', field)
    expect(match is not None and not field[match.end():].strip(), frame['label']+' clean field tail', field)
    backgrounds = {a & 0xf0 for a in row['attrs']}
    expect(backgrounds == {row['attrs'][0] & 0xf0}, frame['label']+' whole-row background', sorted(backgrounds))
    expect('INS' in row['text'][-7:], frame['label']+' mode flags', row['text'][-7:])


temporary = tempfile.TemporaryDirectory(prefix='CRXTHE native repaint ')
root = Path(temporary.name).resolve()
assert root.parent == Path(tempfile.gettempdir()).resolve() and root.name.startswith('CRXTHE native repaint ')
sample = root/'sample.txt'
original = 'seed\ntail\nUnicode: café 日本語 e\u0301 😀\nTabs:\ta\tb\n'.encode('utf-8')
sample.write_bytes(original)
profile = root/'profile.the'
demo = 'paneldemo' if args.engine == 'CREXX' else 'paneldemo_regina'
profile.write_text(stock.read_text(encoding='utf-8')+
                   "\n'cursor goto 4 1';\n'define f7 macro "+demo+"';\n", encoding='utf-8')
env = dict(os.environ, THE_REXX_INTERPRETER=args.engine,
           THE_CREXX_CACHE_DIR=str(root/'cache'), CREXXSAA_CACHE_DIR=str(root/'cache'))
startup = subprocess.STARTUPINFO()
startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 1 if args.visible else subprocess.SW_HIDE
process = console = None
record = dict(engine=args.engine, binary=str(binary), stock_profile=str(stock), failures=failures)
try:
    with (root/'startup.log').open('w', encoding='utf-8') as log:
        process = subprocess.Popen([str(binary), '--driver', 'curses', '-p', str(profile), str(sample)],
                                   cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT,
                                   creationflags=subprocess.CREATE_NEW_CONSOLE, startupinfo=startup)
        console = Console(process)
        frame = console.wait('initial', lambda f: contains(f, 'Tabs:') and contains(f, 'Unicode:') and position(f) == (4, 1), 35)
        def check_tab(f):
            row = next((r['text'].rstrip() for r in f['rows'] if 'Tabs:' in r['text']), '')
            expect(row.endswith('Tabs:→a→b'), f['label']+' stable tab arrows', row)
        check_tab(frame)
        check_status(frame, 'U+54')
        for step in range(1, 9):
            before = position(frame)
            console.key(0x27)
            frame = console.wait('tab-right-'+str(step), lambda f: position(f) != before)
            check_tab(frame)
            if step == 5:
                expect(position(frame) == (4, 6), 'tab has its own logical cell', position(frame))
                check_status(frame, 'U+9')
            if step == 6:
                expect(position(frame) == (4, 7), 'letter after tab logical cell', position(frame))
                check_status(frame, 'U+61')
        console.key(0x52, '\x12', 8)
        check_tab(console.wait('tab-direct-redraw', lambda f: position(f) == position(frame)))
        # Continuous movement matters: explicit commands visit the command
        # window and can clear ghosts before the next Unicode field is painted.
        console.command('cursor goto 3 1')
        frame = console.wait('unicode-walk-start', lambda f: position(f) == (3, 1))
        codes = {13: 'U+E9', 15: 'U+65E5', 17: 'U+672C', 19: 'U+8A9E',
                 22: 'U+65+301', 24: 'U+1F600', 26: 'U+0'}
        for step in range(1, 22):
            before = position(frame)
            console.key(0x27)
            frame = console.wait('unicode-walk-'+str(step), lambda f: position(f) != before)
            current = position(frame)
            if current and current[0] == 3 and current[1] in codes:
                check_status(frame, codes[current[1]])
        for column, code in [(13, 'U+E9'), (15, 'U+65E5'), (17, 'U+672C'), (19, 'U+8A9E'),
                             (22, 'U+65+301'), (24, 'U+1F600')]:
            console.command('cursor goto 3 '+str(column))
            frame = console.wait('unicode-'+str(column), lambda f: position(f) == (3, column) and code in f['rows'][-1]['text'])
            check_status(frame, code)
        console.key(0x27)
        frame = console.wait('unicode-after-emoji', lambda f: position(f) == (3, 26))
        check_status(frame, 'U+0')
        console.command('cursor goto 3 1')
        frame = console.wait('unicode-back-to-ascii', lambda f: position(f) == (3, 1) and 'U+55' in f['rows'][-1]['text'])
        check_status(frame, 'U+55')
        console.command('cursor goto 2 3')
        caller = console.wait('panel-caller', lambda f: position(f) == (2, 3))
        expect(contains(caller, 'F10 Keys'), 'caller footer', [r['text'] for r in caller['rows'][-3:]])
        for action, rc in [(0x0d, 0), (0x1b, 4)]:
            console.key(0x76)
            console.wait('panel-open-'+str(rc), lambda f: contains(f, 'Name:') and contains(f, 'Guest'))
            console.key(action, '\r' if rc == 0 else '\x1b')
            returned = console.wait('panel-return-'+str(rc), lambda f: contains(f, 'PANEL_RC '+str(rc)) and not contains(f, 'Guest'))
            expect(position(returned) == (2, 3), 'panel restored caret '+str(rc), position(returned))
            expect(contains(returned, 'F10 Keys'), 'panel restored footer '+str(rc), [r['text'] for r in returned['rows'][-3:]])
        expect(sample.read_bytes() == original, 'rendering preserves original bytes', hashlib.sha256(sample.read_bytes()).hexdigest())
        console.command('qquit')
        process.wait(timeout=8)
        expect(process.returncode == 0, 'editor exit', process.returncode)
finally:
    if console:
        record['frames'] = console.frames
        console.close()
    if process is not None and process.poll() is None:
        process.terminate()
        process.wait(timeout=8)
    record['exit_code'] = process.returncode if process else None
    record['fixture_unchanged'] = sample.read_bytes() == original
    record['startup_log'] = (root/'startup.log').read_text(encoding='utf-8', errors='replace')
    if args.record_json:
        args.record_json.parent.mkdir(parents=True, exist_ok=True)
        args.record_json.write_text(json.dumps(record, ensure_ascii=True, indent=2), encoding='utf-8')
    temporary.cleanup()  # Exclusively created root checked against the resolved temp directory above.
print(json.dumps({k:v for k,v in record.items() if k != 'frames'}, ensure_ascii=True, indent=2))
assert not failures, json.dumps(failures, ensure_ascii=True)
print(args.engine+': native tab arrows, status codes/background, Unicode field bounds and panel return passed')
