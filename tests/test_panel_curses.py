#!/usr/bin/env python3
"""Real terminal input through a blocking macro panel, then ordinary editing."""
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
for engine, demo in [('REGINA', 'paneldemo_regina'), ('CREXX', 'paneldemo')]:
    with tempfile.TemporaryDirectory(prefix='THE panel terminal ') as tmp:
        sample = Path(tmp) / 'sample.txt'
        sample.write_text('seed\ntail\n')
        profile = Path(tmp) / 'profile.the'
        profile.write_text(('options levelg\n' if engine == 'CREXX' else '') +
                           "address THE\n'macro " + demo + "';\n'cursor goto 1 1';\n'set insertmode on';\n'define c-g file';\n")
        env = dict(os.environ, THE_REXX_INTERPRETER=engine, TERM='xterm-256color')
        if engine == 'CREXX':
            subprocess.run([str(binary), '--driver', 'llm', '-p', str(profile), str(sample)],
                           input='key esc\nquit\n', text=True, capture_output=True,
                           env=env, timeout=30, check=True)
        master, slave = pty.openpty()
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 27, 80, 0, 0))
        proc = subprocess.Popen([str(binary), '-p', str(profile), str(sample)], stdin=slave, stdout=slave, stderr=slave, env=env)
        os.close(slave)
        output = bytearray()
        start = time.monotonic()
        sent = False
        edited = False
        try:
            while proc.poll() is None and time.monotonic()-start < 12:
                if not sent and time.monotonic()-start > 0.7:
                    os.write(master, b'Alice\tRexx\r')
                    sent = True
                if sent and not edited and time.monotonic()-start > 1.5:
                    os.write(master, b'Z\x07')
                    edited = True
                if select.select([master], [], [], 0.05)[0]:
                    try:
                        output.extend(os.read(master, 65536))
                    except OSError as error:
                        if error.errno != errno.EIO:
                            raise
            proc.wait(timeout=10)
            text = output.decode(errors='replace')
            assert proc.returncode == 0, text[-1500:]
            assert sample.read_text() == 'Zseed\ntail\n', (engine, sample.read_text(), text[-1500:])
            assert 'Error 00' not in text, text[-1500:]
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait()
            os.close(master)
print('Regina and Level G curses panels: field typing, Tab, submit and restored file editing passed')
