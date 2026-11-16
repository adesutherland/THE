#!/usr/bin/env python3
"""Exercise real mouse records in path-verified, exclusively owned consoles."""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

if os.name != 'nt':
    sys.exit(77)
if not __debug__:
    raise RuntimeError('Assertions required')

parser = argparse.ArgumentParser()
parser.add_argument('binary', type=Path)
parser.add_argument('engine', choices=['CREXX', 'REGINA', 'OOREXX'])
parser.add_argument('--profile', type=Path)
parser.add_argument('--record-json', type=Path)
args = parser.parse_args()
binary = args.binary.resolve()
repo = Path(__file__).resolve().parent.parent
stock = args.profile or repo / ('profile.the' if args.engine == 'CREXX' else 'profile_regina.the')

# Reuse the existing owned-console support, without executing its repaint
# journey. Fail closed if its explicit helper/journey boundary changes.
support_path = Path(__file__).with_name('test_windows_console_repaint.py')
helpers, boundary, _ = support_path.read_text(encoding='utf-8').partition('\nfailures = []')
assert boundary, 'Console helper/journey boundary missing'
saved_arguments = sys.argv
sys.argv = [str(support_path), str(binary),
            'REGINA' if args.engine == 'OOREXX' else args.engine, '--profile', str(stock)]
support = {'__file__': str(support_path), '__name__': 'mouse_regression_support'}
try:
    exec(compile(helpers, str(support_path), 'exec'), support)
finally:
    sys.argv = saved_arguments
kernel, Coord, Info, checked = (support[name] for name in ('kernel', 'Coord', 'Info', 'checked'))


class Mouse(C.Structure):
    _fields_ = [('position', Coord), ('buttons', W.DWORD),
                ('control', W.DWORD), ('flags', W.DWORD)]


class Event(C.Union):
    _fields_ = [('mouse', Mouse), ('padding', C.c_byte * 16)]


class Input(C.Structure):
    _fields_ = [('type', W.WORD), ('event', Event)]


assert C.sizeof(Input) == 20
kernel.WriteConsoleInputW.argtypes = [W.HANDLE, W.LPVOID, W.DWORD, C.POINTER(W.DWORD)]
kernel.GetConsoleMode.argtypes = [W.HANDLE, C.POINTER(W.DWORD)]
record = dict(binary=str(binary), engine=args.engine,
              binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(), phases=[], failures=[])


def expect(ok, label, actual):
    if not ok:
        record['failures'].append(dict(check=label, actual=actual))


def input_mode(console):
    mode = W.DWORD()
    checked(kernel.GetConsoleMode(console.input, C.byref(mode)))
    return mode.value


def wait_mouse_mode(console, enabled):
    deadline = time.monotonic() + 5
    while True:
        mode = input_mode(console)
        if bool(mode & 0x10) == enabled:
            return mode
        assert console.process.poll() is None, 'Editor exited during mouse mode change'
        if time.monotonic() >= deadline:
            raise AssertionError(('mouse mode change', enabled, hex(mode)))
        time.sleep(.04)


for phase in ('startup', 'after-osq'):
    process = console = None
    entry = dict(phase=phase)
    with tempfile.TemporaryDirectory(prefix='CRXTHE mouse regression ') as temporary:
        root = Path(temporary)
        sample = root / 'mouse.rexx'
        original = ''.join('mouse row %03d text for cursor placement\n' % n
                           for n in range(1, 101)).encode()
        sample.write_bytes(original)
        profile = root / 'profile.the'
        profile.write_text(stock.read_text(encoding='utf-8')
                           + ("\n'osq ver';\n" if phase == 'after-osq' else '\n')
                           + "'cursor goto 30 3';\n", encoding='utf-8')
        env = dict(os.environ, THE_REXX_INTERPRETER=args.engine,
                   THE_CREXX_CACHE_DIR=str(root / 'cache'), CREXXSAA_CACHE_DIR=str(root / 'cache'),
                   THE_TOOL_CONFIG_DIR=str(root / 'tools'), LOCALAPPDATA=str(root / 'localappdata'))
        startup = subprocess.STARTUPINFO()
        startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE
        try:
            with (root / 'startup.log').open('w', encoding='utf-8') as log:
                process = subprocess.Popen([str(binary), '--driver', 'curses', '-p',
                                            str(profile), str(sample)], env=env, cwd=root,
                                           stdout=log, stderr=subprocess.STDOUT,
                                           creationflags=subprocess.CREATE_NEW_CONSOLE, startupinfo=startup)
                console = support['Console'](process)
                frame = console.wait(phase + '-ready', lambda f: support['position'](f) == (30, 3))
                mode = input_mode(console)
                entry['input_mode'] = hex(mode)
                expect(bool(mode & 0x10), phase + ' mouse input enabled', hex(mode))
                expect(not mode & 0x40, phase + ' QuickEdit disabled', hex(mode))
                expect(mode & 7 == 0, phase + ' raw input retained', hex(mode))
                row_number, row = next((i, row) for i, row in enumerate(frame['rows'])
                                       if 'mouse row 031 text' in row['text'])
                column = row['text'].index('mouse row 031 text') + 5
                output = kernel.CreateFileW('CONOUT$', 0xc0000000, 3, None, 3, 0, None)
                try:
                    info = Info()
                    checked(kernel.GetConsoleScreenBufferInfo(output, C.byref(info)))
                    point = Coord(info.window.Left + column, info.window.Top + row_number)
                finally:
                    kernel.CloseHandle(output)

                def send_mouse(buttons, flags=0):
                    event = Input()
                    event.type = 2
                    event.event.mouse = Mouse(point, buttons, 0, flags)
                    written = W.DWORD()
                    checked(kernel.WriteConsoleInputW(console.input, C.byref(event), 1, C.byref(written)))
                    assert written.value == 1

                send_mouse(1)
                time.sleep(.025)
                send_mouse(0)
                try:
                    clicked = console.wait(phase + '-clicked', lambda f: support['position'](f) == (31, 6), 2)
                except AssertionError:
                    clicked = console.read(phase + '-click-not-routed')
                entry['click_position'] = support['position'](clicked)
                expect(entry['click_position'] == (31, 6), phase + ' left click logical line/column',
                       entry['click_position'])
                console.command('cursor goto 30 3')
                console.wait(phase + '-wheel-ready', lambda f: support['position'](f) == (30, 3))
                send_mouse(0xff880000, 4)  # high word signed -120, MOUSE_WHEELED
                try:
                    scrolled = console.wait(phase + '-wheel-down', lambda f: support['position'](f) == (35, 3), 2)
                except AssertionError:
                    scrolled = console.read(phase + '-wheel-not-routed')
                entry['wheel_position'] = support['position'](scrolled)
                expect(entry['wheel_position'] == (35, 3), phase + ' wheel scroll five lines',
                       entry['wheel_position'])
                console.command('set mouse off')
                mouse_off = wait_mouse_mode(console, False)
                entry['mouse_off_mode'] = hex(mouse_off)
                console.command('set mouse on')
                mouse_on = wait_mouse_mode(console, True)
                expect(not mouse_on & 0x40 and mouse_on & 7 == 0,
                       phase + ' reenabled mouse preserves raw mode and disables QuickEdit', hex(mouse_on))
                expect(sample.read_bytes() == original, phase + ' unchanged saved bytes',
                       hashlib.sha256(sample.read_bytes()).hexdigest())
                console.command('qquit')
                process.wait(timeout=8)
                expect(process.returncode == 0, phase + ' clean exit', process.returncode)
                restored = input_mode(console)
                entry['exit_input_mode'] = hex(restored)
                expect(restored & 0x40 == mouse_off & 0x40,
                       phase + ' exit restores original QuickEdit', hex(restored))
        except Exception as error:
            record['failures'].append(dict(check=phase + ' harness', actual=repr(error)))
        finally:
            if console:
                entry['frames'] = console.frames
                console.close()
            if process and process.poll() is None:
                process.terminate()
                process.wait(timeout=8)
            entry['editor_exit'] = process.returncode if process else None
            entry['sample_unchanged'] = sample.read_bytes() == original
            expect(entry['sample_unchanged'], phase + ' final saved bytes unchanged', entry['sample_unchanged'])
            entry['startup_log'] = ((root / 'startup.log').read_text(encoding='utf-8', errors='replace')
                                    if (root / 'startup.log').exists() else '')
    record['phases'].append(entry)

record['passed'] = not record['failures']
if args.record_json:
    args.record_json.parent.mkdir(parents=True, exist_ok=True)
    args.record_json.write_text(json.dumps(record, ensure_ascii=True, indent=2), encoding='utf-8')
print(json.dumps({**record, 'phases': [{k: v for k, v in row.items() if k not in ('frames', 'startup_log')}
                                     for row in record['phases']]}, indent=2), flush=True)
sys.exit(0 if record['passed'] else 1)
