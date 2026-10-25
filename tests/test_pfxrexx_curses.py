#!/usr/bin/env python3
"""Type a template and its body through curses, then assert the saved bytes."""
import os
from pathlib import Path
import sys
if os.name == 'nt':
    sys.exit(77)
import errno
import fcntl
import pty
import select
import struct
import subprocess
import tempfile
import termios
import time

binary = Path(sys.argv[1]).resolve()
originals = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None
with tempfile.TemporaryDirectory() as tmp:
    sample = Path(tmp) / 'sample.rexx'
    sample.write_text('seed\ntail\n')
    profile = Path(tmp) / 'profile.the'
    setup = 'pfxsetup-original REXX' if originals else 'pfxsetup'
    profile.write_text("address THE\n'macro " + setup + "'\n'define c-g file'\n"
                       "'cursor goto 1 1'\n'cursor prefix'\n")
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 27, 80, 0, 0))
    env = dict(os.environ, THE_REXX_INTERPRETER='REGINA', TERM='xterm-256color',
               THE_HOME_DIR=str(binary.parent / 'release'),
               THE_MACRO_PATH=str(originals or binary.parent / 'release'))
    process = subprocess.Popen([str(binary), '-p', str(profile), str(sample)],
                               stdin=slave, stdout=slave, stderr=slave, env=env)
    os.close(slave)
    start = time.monotonic()
    sent = False
    output = bytearray()
    try:
        while process.poll() is None and time.monotonic() - start < 10:
            if not sent and time.monotonic() - start > 0.5:
                keys = b"DO\r" + (b"\x1b[C" * 3 if originals else b'') + b"say 'hello'\r\r\x07"
                os.write(master, keys)
                sent = True
            if select.select([master], [], [], 0.05)[0]:
                try:
                    output.extend(os.read(master, 65536))
                except OSError as error:
                    if error.errno != errno.EIO:
                        raise
        if process.poll() is None:
            raise AssertionError('Curses template workflow did not exit within 10 seconds')
        assert process.returncode == 0, output.decode(errors='replace')
        if originals:
            body = "   say 'hello'".ljust(39) + '/*' + ' ' * 28 + '*/'
            assert sample.read_text() == 'seed\ndo\n' + body + '\n   end\ntail\n', sample.read_text()
        else:
            assert sample.read_text() == "seed\ndo\n   say 'hello'\nend\ntail\n", sample.read_text()
    finally:
        if process.poll() is None:
            process.kill()
        process.wait()
        os.close(master)
print('Curses PFXREXX text-entry workflow passed')
