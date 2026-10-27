#!/usr/bin/env python3
"""First-launch editing and save keys through a real terminal, both engines."""
import os
if os.name == 'nt':
    raise SystemExit(77)
import errno
import fcntl
from pathlib import Path
import pty
import select
import struct
import subprocess
import sys
import tempfile
import termios
import time

binary=Path(sys.argv[1]).resolve()
repo=Path(__file__).resolve().parent.parent
for engine in ['CREXX','REGINA']:
    profile=repo/('profile.the' if engine=='CREXX' else 'profile_regina.the')
    with tempfile.TemporaryDirectory(prefix='CRXTHE first terminal ') as temporary:
        sample=Path(temporary)/'sample.txt'
        sample.write_text('alpha\n',encoding='utf-8')
        env=dict(os.environ,THE_REXX_INTERPRETER=engine,TERM='xterm-256color')
        # Warm source profiles before timing native key input.
        subprocess.run([str(binary),'--driver','llm','-p',str(profile),str(sample)],input='quit\n',
                       text=True,encoding='utf-8',capture_output=True,env=env,timeout=45,check=True)
        master,slave=pty.openpty()
        fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',24,80,0,0))
        process=subprocess.Popen([str(binary),'-p',str(profile),str(sample)],stdin=slave,stdout=slave,stderr=slave,env=env)
        os.close(slave)
        output=bytearray(); stage=0; start=time.monotonic(); last_output=start
        try:
            while process.poll() is None and time.monotonic()-start<15:
                elapsed=time.monotonic()-start
                if stage==0 and elapsed>1 and output and time.monotonic()-last_output>0.35:
                    # Delete X under the caret, then backspace over Y. The
                    # saved control below proves neither deleted byte returns.
                    os.write(master,b'XY\x1b[H\x1b[3~\x1b[C\x08Z\t\x13');stage=1
                if stage==1 and sample.read_text()=='Z       alpha\n':
                    os.write(master,b'\x1b[H'+b'\x1b[D'*8+b'\x1b[17~\x1b[17~\x1b[F\rbeta\x13');stage=2
                if stage==2 and sample.read_text()=='Z       alpha\nbeta\n':
                    os.write(master,b'\x1b[H'+b'\x1b[D'*8+b'Q\x13');stage=3
                if stage==3 and sample.read_text()=='Z       alphaQ\nbeta\n':
                    os.write(master,b'\x1b[17~file\r');stage=4
                if select.select([master],[],[],0.05)[0]:
                    try:
                        output.extend(os.read(master,65536))
                        last_output=time.monotonic()
                    except OSError as error:
                        if error.errno!=errno.EIO:raise
            assert stage==4,(engine,'native save/Enter workflow stalled',sample.read_text(),output.decode(errors='replace')[-2500:])
            process.wait(timeout=10)
            assert process.returncode==0,(engine,output.decode(errors='replace')[-1200:])
            assert sample.read_text()=='Z       alphaQ\nbeta\n'
            assert 'Error 00' not in output.decode(errors='replace'),output.decode(errors='replace')[-1500:]
        finally:
            if process.poll() is None:process.kill()
            process.wait()
            os.close(master)
print('CREXX and Regina default profiles: real terminal Delete/Backspace, insert, Ctrl-S save, End/Enter, command focus and save/exit passed')
