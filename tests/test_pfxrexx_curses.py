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
with tempfile.TemporaryDirectory() as tmp:
    sample = Path(tmp) / 'sample.rexx'
    sample.write_text('seed\ntail\n')
    profile = Path(tmp) / 'profile.the'
    profile.write_text("address THE\n'macro pfxsetup'\n'define c-g file'\n"
                       "'cursor goto 1 1'\n'cursor prefix'\n")
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 27, 80, 0, 0))
    env = dict(os.environ, THE_REXX_INTERPRETER='REGINA', TERM='xterm-256color',
               THE_HOME_DIR=str(binary.parent / 'release'),
               THE_MACRO_PATH=str(binary.parent / 'release'))
    process = subprocess.Popen([str(binary), '-p', str(profile), str(sample)],
                               stdin=slave, stdout=slave, stderr=slave, env=env)
    os.close(slave)
    start = time.monotonic()
    sent = False
    output = bytearray()
    try:
        while process.poll() is None and time.monotonic() - start < 10:
            if not sent and time.monotonic() - start > 0.5:
                os.write(master, b"DO\rsay 'hello'\r\r\x07")
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
        assert sample.read_text() == "seed\ndo\n   say 'hello'\nend\ntail\n", sample.read_text()
    finally:
        if process.poll() is None:
            process.kill()
        process.wait()
        os.close(master)
print('Curses PFXREXX text-entry workflow passed')
