#!/usr/bin/env python3
"""Check real curses input mode after OSQ in an owned hidden Windows console."""
import ctypes
from ctypes import wintypes
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

if os.name != 'nt':
    sys.exit(77)

binary = Path(sys.argv[1]).resolve()
engine = sys.argv[2]
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.FreeConsole.argtypes = []
kernel.FreeConsole.restype = wintypes.BOOL
kernel.AttachConsole.argtypes = [wintypes.DWORD]
kernel.AttachConsole.restype = wintypes.BOOL
kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                              wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD,
                              wintypes.HANDLE]
kernel.CreateFileW.restype = wintypes.HANDLE
kernel.GetConsoleMode.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
kernel.GetConsoleMode.restype = wintypes.BOOL
kernel.CloseHandle.argtypes = [wintypes.HANDLE]
kernel.CloseHandle.restype = wintypes.BOOL
kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel.OpenProcess.restype = wintypes.HANDLE
kernel.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD,
                                             wintypes.LPWSTR,
                                             ctypes.POINTER(wintypes.DWORD)]
kernel.QueryFullProcessImageNameW.restype = wintypes.BOOL


def input_mode(process):
    """Attach only to this test's live, path-verified child; never change its mode."""
    assert process.poll() is None, 'Editor exited before console inspection'
    handle = kernel.OpenProcess(0x1000, False, process.pid)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        image = ctypes.create_unicode_buffer(32768)
        length = wintypes.DWORD(len(image))
        if not kernel.QueryFullProcessImageNameW(handle, 0, image, ctypes.byref(length)):
            raise ctypes.WinError(ctypes.get_last_error())
        assert Path(image.value).resolve() == binary, image.value
    finally:
        kernel.CloseHandle(handle)

    # Attachment changes only this helper process. The CTest parent and user's
    # consoles are untouched; restore the parent attachment where available.
    kernel.FreeConsole()
    if not kernel.AttachConsole(process.pid):
        error = ctypes.get_last_error()
        kernel.AttachConsole(0xffffffff)
        raise ctypes.WinError(error)
    try:
        console = kernel.CreateFileW('CONIN$', 0xc0000000, 3, None, 3, 0, None)
        if console in (None, wintypes.HANDLE(-1).value):
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            mode = wintypes.DWORD()
            if not kernel.GetConsoleMode(console, ctypes.byref(mode)):
                raise ctypes.WinError(ctypes.get_last_error())
            return mode.value
        finally:
            kernel.CloseHandle(console)
    finally:
        kernel.FreeConsole()
        kernel.AttachConsole(0xffffffff)


with tempfile.TemporaryDirectory(prefix='CRXTHE console mode ') as temporary:
    root = Path(temporary)
    sample = root / 'sample.txt'
    sample.write_bytes(b'console mode probe\r\n')
    for shell_escape in (False, True):
        ready = root / ('osq-ready.txt' if shell_escape else 'startup-ready.txt')
        profile = root / 'probe.the'
        save = ('save "' + ready.as_posix() + '"').replace("'", "''")
        profile.write_text(('options levelb\n' if engine == 'CREXX' else '') +
                           "address THE\n" + ("'osq ver';\n" if shell_escape else '') +
                           "'" + save + "';\n", encoding='utf-8')
        startup = subprocess.STARTUPINFO()
        startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE
        env = dict(os.environ, THE_REXX_INTERPRETER=engine)
        log_path = root / ('osq-startup.log' if shell_escape else 'startup.log')
        startup_log = log_path.open('w', encoding='utf-8')
        # Curses opens its owned CONIN$/CONOUT$ directly; capture ordinary
        # startup diagnostics so a loader failure is visible on headless CI.
        try:
            process = subprocess.Popen([str(binary), '--driver', 'curses', '-p',
                                        str(profile), str(sample)], env=env,
                                       stdout=startup_log, stderr=subprocess.STDOUT,
                                       creationflags=subprocess.CREATE_NEW_CONSOLE,
                                       startupinfo=startup)
        except BaseException:
            startup_log.close()
            raise
        try:
            deadline = time.monotonic() + 40
            while not ready.exists() or ready.read_bytes() != sample.read_bytes():
                assert process.poll() is None, ('Editor exited before ready marker', process.returncode,
                    log_path.read_text(encoding='utf-8', errors='replace'))
                if time.monotonic() >= deadline:
                    raise AssertionError(('Editor did not complete the console probe profile',
                        log_path.read_text(encoding='utf-8', errors='replace')))
                time.sleep(0.05)
            assert ready.read_bytes() == sample.read_bytes(), ready.read_bytes()
            mode = input_mode(process)
            phase = 'after OSQ' if shell_escape else 'at startup'
            assert mode & 7 == 0, (engine, phase, hex(mode),
                                   'Processed, line and echo input must be disabled')
            print(f'{engine}: real console input mode {phase}: {mode:#x}')
        finally:
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=10)
            startup_log.close()
