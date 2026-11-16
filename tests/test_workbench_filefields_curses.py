#!/usr/bin/env python3
"""Exercise the native CTLCHAR field pane through a real fixed-size PTY."""
import os
import sys

if os.name == "nt":
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

for engine in ("CREXX", "REGINA"):
    with tempfile.TemporaryDirectory(prefix="THE filefields PTY ") as temporary:
        root = Path(temporary)
        source = root / "fields.txt"
        original = "!pName: !uGuest     !p LOCK Ω\n" + "".join(
            f"!pRow {row:02} !uvalue{row:02}   !p locked\n" for row in range(2, 16))
        source.write_text(original)
        profile = root / "keys.the"
        stock = repo / ("profile.the" if engine == "CREXX" else "profile_regina.the")
        profile.write_text(stock.read_text() + "\n'define f9 qquit';\n'define c-u cmsg';\n'msg FILEFIELDS_READY';\n")
        environment = dict(os.environ, TERM="xterm-256color", THE_REXX_INTERPRETER=engine,
                           THE_CREXX_CACHE_DIR=str(root / "cache"), THE_TOOL_CONFIG_DIR=str(root / "tools"))
        master, slave = pty.openpty()
        rows, columns = 30, 140
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", rows, columns, 0, 0))
        screen = pyte.Screen(columns, rows)
        stream = pyte.ByteStream(screen)
        process = subprocess.Popen([str(binary), "-p", str(profile), str(source)],
                                   stdin=slave, stdout=slave, stderr=slave, env=environment, cwd=root)
        os.close(slave)

        def wait(predicate, label, timeout=25):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                if select.select([master], [], [], .03)[0]:
                    try:
                        data = os.read(master, 65536)
                    except OSError:
                        data = b""
                    if data:
                        stream.feed(data)
                if predicate():
                    if os.environ.get("THE_FILEFIELDS_TRACE"):
                        print(engine, label, flush=True)
                    return
                if process.poll() is not None:
                    break
            if predicate():
                return
            raise AssertionError((engine, label, screen.display, screen.cursor, process.poll()))

        def send(data):
            os.write(master, data)

        def text():
            return "\n".join(screen.display)

        def saved_text_is(expected):
            # SAVE can briefly move the old file aside before writing its
            # replacement. Keep polling until the exact saved text is ready.
            try:
                return source.read_text() == expected
            except FileNotFoundError:
                return False

        def command(value):
            send(b"\x15" + value.encode() + b"\r")
            # Wait for a separate response command, so a fast command cannot
            # accidentally satisfy its own wait from the typed command line.
            marker = "ACK_" + str(command.sequence)
            command.sequence += 1
            send(("msg " + marker).encode() + b"\r")
            wait(lambda: marker in screen.display[rows - 3], value)

        command.sequence = 0
        def click_at(row, column):
            send(b"\x1b[M" + bytes([32, 33 + column, 33 + row]))
            send(b"\x1b[M" + bytes([35, 33 + column, 33 + row]))
        try:
            wait(lambda: "FILEFIELDS_READY" in text(), "profile")
            send(b"\x1b")
            wait(lambda: screen.display[screen.cursor.y].startswith("======>"), "command focus")
            for value in ("set ctlchar ! escape", "set ctlchar p protect white", "set ctlchar u noprotect green",
                          "set filectlchar on", "workbench begin owner p.owner",
                          'workbench panel put p.fields kind fields buffer active markup ctlchar access staged',
                          "workbench commit", "workbench panel show p.fields"):
                command(value)
            send(b"workbench panel focus p.fields\r")
            wait(lambda: "File fields: p.fields" in text(), "field pane")
            wait(lambda: "Guest" in screen.display[screen.cursor.y] and screen.cursor.y != rows - 2, "editable field focus")
            # The response probes after capture are ordinary editor commands;
            # explicitly recapture before admitting a staged edit.
            send(b"\t" * 17 + b"\r")
            wait(lambda: "Ready" in text(), "initial Reload")
            first_caret = (screen.cursor.x, screen.cursor.y)
            send("界é".encode())
            wait(lambda: "界ést" in text(), "fixed-cell Unicode overwrite")
            assert source.read_text() == original
            caret = (screen.cursor.x, screen.cursor.y)
            send(b"\x1b")
            wait(lambda: screen.display[screen.cursor.y].startswith("======>"), "Return to editor")
            click_at(caret[1], caret[0])
            wait(lambda: (screen.cursor.x, screen.cursor.y) == caret, "retained field focus and caret")
            send(b"\t" * 15 + b"\r")  # fifteen fields, then Apply.
            wait(lambda: "Ready" in text() or "stale-revision" in text(), "Apply receipt")
            assert "stale-revision" not in text(), text()
            assert source.read_text() == original  # Apply is not Save.
            # All controls remain keyboard reachable at a 32-column viewport.
            rows, columns = 24, 32
            fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack("HHHH", rows, columns, 0, 0))
            screen.resize(rows, columns)
            process.send_signal(signal.SIGWINCH)
            wait(lambda: "Apply Cancel Reload Hide Return" in text(), "narrow controls")
            send(b"\t\r")  # Cancel releases the projection while retaining applied bytes.
            wait(lambda: "Cancelled" in text(), "Cancel receipt")
            send(b"\t\r")
            wait(lambda: "Ready" in text(), "Reload receipt")
            send(b"\x1b")
            wait(lambda: screen.display[screen.cursor.y].startswith("======>"), "Return before Save")
            send(b"save\r")
            expected = original.replace("Guest     ", "界ést     ", 1)
            wait(lambda: saved_text_is(expected), "saved applied bytes")
            send(b"workbench panel focus p.fields\r")
            wait(lambda: screen.cursor.y < rows - 2 and "File fields:" in text(), "refocus after save")
            # Reload is explicit after an editor command invalidates a capture.
            send(b"\t" * 17 + b"\r")
            wait(lambda: "Ready" in text(), "explicit reload after save")
            send(b"\t" * 16 + b"\r")
            wait(lambda: "Cancelled" in text(), "release before quit")
            send(b"\x1b")
            wait(lambda: screen.display[screen.cursor.y].startswith("======>"), "Return before quit")
            send(b"\x1b[20~")
            wait(lambda: process.poll() is not None, "quit")
            assert process.returncode == 0
            assert source.read_text() == expected
            assert first_caret[0] < caret[0]
        finally:
            if process.poll() is None:
                process.kill()
            process.wait()
            os.close(master)

print("CREXX and Regina: fixed-cell Unicode, retained focus/caret, Apply/Cancel/Reload, narrow controls and saved bytes passed")
