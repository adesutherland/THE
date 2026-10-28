#!/usr/bin/env python3
"""Physical split, focus, Unicode stdin, resizing and return to editor on a PTY."""
import os,sys
if os.name=='nt':raise SystemExit(77)
try:import pyte
except ImportError:raise SystemExit(77)
import fcntl,pty,select,signal,struct,subprocess,tempfile,termios,time
from pathlib import Path
binary=Path(sys.argv[1]).resolve();repo=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='THE console PTY ') as d:
 root=Path(d);source=root/'program.rexx';source.write_text("say 'Console ready'\npull name\nsay 'Hello' name\nexit 7\n")
 profile=root/'keys.the';profile.write_text((repo/'profile.the').read_text()+"\n'define C-R tool run';\n'define C-T tool console';\n'define f9 qquit';\n")
 master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',30,100,0,0))
 screen=pyte.Screen(100,30);stream=pyte.ByteStream(screen)
 p=subprocess.Popen([str(binary),'-p',str(profile),str(source)],stdin=slave,stdout=slave,stderr=slave,env=dict(os.environ,TERM='xterm-256color',THE_TOOL_CONFIG_DIR=str(root/'config')));os.close(slave)
 def wait(check,label,timeout=20):
  end=time.monotonic()+timeout
  while time.monotonic()<end:
   if select.select([master],[],[],.05)[0]:
    try:stream.feed(os.read(master,65536))
    except OSError:
     if check():return
     break
   if check():return
  raise AssertionError(label+'\n'+'\n'.join(screen.display))
 try:
  wait(lambda:any('Console ready' in s for s in screen.display),'editor source')
  os.write(master,b'\x12');wait(lambda:any('Console |' in s for s in screen.display),'split console')
  assert any('say' in s for s in screen.display[:20]),screen.display
  os.write(master,b'\x14');wait(lambda:any('Esc: editor' in s for s in screen.display),'console focus')
  os.write(master,'Zoë😀\r'.encode());wait(lambda:any('Hello ZOË😀' in s for s in screen.display),'Unicode stdin and output')
  wait(lambda:any('status 7' in s for s in screen.display),'completion')
  fcntl.ioctl(master,termios.TIOCSWINSZ,struct.pack('HHHH',24,80,0,0));screen.resize(24,80);p.send_signal(signal.SIGWINCH)
  wait(lambda:any('Console |' in s for s in screen.display[-5:]),'resized split')
  os.write(master,b'\x1b');wait(lambda:any('TOOL CONSOLE: focus' in s for s in screen.display),'return to editor');os.write(master,b'\x1b[20~');wait(lambda:p.poll() is not None,'quit after restoring editor focus',8)
  assert source.read_text()=="say 'Console ready'\npull name\nsay 'Hello' name\nexit 7\n"
 finally:
  if p.poll() is None:p.kill();p.wait()
  os.close(master)
print('Terminal split console, Unicode stdin/output, focus and resize passed')
