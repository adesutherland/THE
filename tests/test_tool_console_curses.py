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
 root=Path(d);source=root/'program.rexx';source.write_text("say 'Console ready'\npull name\nsay 'Hello' name\npull name\nsay 'Again' name\npull name\nsay 'Third' name\nexit 7\n")
 profile=root/'keys.the';profile.write_text((repo/'profile.the').read_text()+"\n'define C-R RX RUN';\n'define C-T RX CONSOLE';\n'define f9 qquit';\n")
 master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',30,100,0,0))
 screen=pyte.Screen(100,30);stream=pyte.ByteStream(screen)
 p=subprocess.Popen([str(binary),'-p',str(profile),str(source)],stdin=slave,stdout=slave,stderr=slave,env=dict(os.environ,TERM='xterm-256color',THE_TOOL_CONFIG_DIR=str(root/'config')));os.close(slave)
 def wait(check,label,timeout=20):
  end=time.monotonic()+timeout
  quiet=time.monotonic()
  while time.monotonic()<end:
   if select.select([master],[],[],.05)[0]:
    try:stream.feed(os.read(master,65536))
    except OSError:
     if check():return
     break
    quiet=time.monotonic()
   if check() and (p.poll() is not None or time.monotonic()-quiet>=.15):return
  raise AssertionError(label+'\n'+'\n'.join(screen.display))
 try:
  wait(lambda:any('Console ready' in s for s in screen.display),'editor source')
  # Use the actual native menu for launch, focus, hide, and re-open.
  def menu(index):
   os.write(master,b'\x1b[21~');wait(lambda:any('Open or create file' in s for s in screen.display),'menu opens')
   os.write(master,b'\x1b[B'*(index-1)+b'\r')
  menu(8);wait(lambda:any('Program console |' in s for s in screen.display),'menu run opens split console')
  assert any('say' in s for s in screen.display[:20]),screen.display
  menu(10);wait(lambda:not any('Program console |' in s for s in screen.display),'menu hides running console')
  menu(9);wait(lambda:any('Esc: editor' in s for s in screen.display),'menu reopens and focuses console')
  menu(10);wait(lambda:not any('Program console |' in s for s in screen.display),'F10 menu hides focused console')
  menu(9);wait(lambda:any('Esc: editor' in s for s in screen.display),'refocus after hide')
  assert any('Program input>' in s for s in screen.display),screen.display
  assert any('Ctrl-D: EOF' in s for s in screen.display),screen.display
  fcntl.ioctl(master,termios.TIOCSWINSZ,struct.pack('HHHH',16,32,0,0));screen.resize(16,32);p.send_signal(signal.SIGWINCH)
  wait(lambda:any('Esc: editor' in s for s in screen.display[-5:]),'compact program focus hint')
  assert any('^D EOF' in s and '^C stop' in s for s in screen.display[-5:]),screen.display
  assert any('F10 Menu' in s for s in screen.display[:-5]),screen.display
  os.write(master,'Zoë😀'.encode());wait(lambda:'Zoë😀' in screen.display[-1],'Unicode input field')
  assert screen.cursor.x==len('Program input> ')+5,screen.cursor
  os.write(master,b'\r');wait(lambda:any('Hello ZOË😀' in s for s in screen.display),'Unicode stdin and output')
  os.write(master,b'second\r');wait(lambda:any('Again SECOND' in s for s in screen.display),'second input')
  os.write(master,b'third\r');wait(lambda:any('Third THIRD' in s for s in screen.display),'third input')
  fcntl.ioctl(master,termios.TIOCSWINSZ,struct.pack('HHHH',24,80,0,0));screen.resize(24,80);p.send_signal(signal.SIGWINCH)
  wait(lambda:any('Program console |' in s for s in screen.display[-5:]),'resized split')
  wait(lambda:any('status 7' in s for s in screen.display),'completion')
  os.write(master,b'late\r')
  wait(lambda:any('Input closed>' in s for s in screen.display[-5:]),'completed input is visibly closed')
  assert not any('late' in s.lower() for s in screen.display[-5:]),screen.display
  os.write(master,b'\x1b');wait(lambda:any('RX CONSOLE: input' in s for s in screen.display),'editor focus before rerun')
  menu(10);wait(lambda:not any('Program console |' in s for s in screen.display),'hide completed console')
  menu(8);wait(lambda:any('Program console |' in s for s in screen.display),'rerun after hide')
  menu(9);wait(lambda:any('Esc: editor' in s for s in screen.display),'focus rerun console')
  os.write(master,b'one\rtwo\rthree\r');wait(lambda:any('Third THREE' in s for s in screen.display),'rerun repeated input')
  wait(lambda:any('status 7' in s for s in screen.display),'rerun completion')
  os.write(master,b'\x1b');wait(lambda:any('RX CONSOLE: input' in s for s in screen.display),'return to editor')
  os.write(master,b'\x1b');wait(lambda:screen.cursor.y==17,'editor command focus')
  os.write(master,b'msg EDITOR_RESPONSE\r')
  wait(lambda:any('EDITOR_RESPONSE' in s for s in screen.display[:-5]),'separate editor response')
  assert not any('EDITOR_RESPONSE' in s for s in screen.display[-5:]),screen.display
  assert any('Third THREE' in s for s in screen.display[-5:]),screen.display
  os.write(master,b'\x14');wait(lambda:any('Esc: editor' in s for s in screen.display),'focus before compact resize')
  fcntl.ioctl(master,termios.TIOCSWINSZ,struct.pack('HHHH',15,60,0,0));screen.resize(15,60);p.send_signal(signal.SIGWINCH)
  wait(lambda:not any('Program console |' in s for s in screen.display),'hide split below minimum height')
  os.write(master,b'\x1b');wait(lambda:screen.cursor.y==13,'compact command focus')
  os.write(master,b'msg COMPACT_EDITOR\r');wait(lambda:any('COMPACT_EDITOR' in s for s in screen.display),'editing with hidden split')
  os.write(master,b'\x1b[20~');wait(lambda:p.poll() is not None,'quit after restoring editor focus',8)
  assert source.read_text()=="say 'Console ready'\npull name\nsay 'Hello' name\npull name\nsay 'Again' name\npull name\nsay 'Third' name\nexit 7\n"
 finally:
  if p.poll() is None:p.kill();p.wait()
  os.close(master)
print('Terminal split console, Unicode stdin/output, focus and resize passed')
