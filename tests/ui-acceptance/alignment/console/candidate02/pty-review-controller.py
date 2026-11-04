import os,sys,json,pty,fcntl,termios,struct,subprocess,select,time,re,signal,hashlib
from pathlib import Path
import pyte
from pyte import graphics
base=Path('/tmp/the-alignment-console-review02');base.mkdir(exist_ok=True)
(base/'workspace').mkdir(exist_ok=True)
fixtures={'Welcome.txt':'C02 console review. Plain Text remains usable.\n', 'Reader.rexx':"do i=1 to 65\n say 'Retained row' i 'stable passage'\nend\npull first\nsay 'ARRIVED WHILE READING' first\npull second\nsay 'DONE' second\nreturn 0\n",'Colours.profile.the':"options levelb\naddress THE\n'set reserved -2 off';\n"+"\n".join("'set reserved -%d %s on %s %s λ界';\n'set footer COLOUR%d -%d AFTERCMDLINE';"%(10-i,c,'white' if c=='black' else 'black',c.upper(),i,10-i) for i,c in enumerate(['black','blue','green','cyan','red','magenta','yellow','white']))+"\n'set statusline top';\n'set cmdline top';\n'set msgline on -9 1';\nreturn 0\n"}

for name,text in fixtures.items():(base/'workspace'/name).write_text(text)
initial={n:hashlib.sha256((base/'workspace'/n).read_bytes()).hexdigest() for n in fixtures}
rows,cols=40,140
master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',rows,cols,0,0))
screen=pyte.Screen(cols,rows);stream=pyte.ByteStream(screen);raw=bytearray();palette=list(graphics.FG_BG_256)
repo=Path('/Users/adrian/CLionProjects/THE');build=repo/'build/gtk-focus-review';release=build/'release'
env=dict(os.environ,THE_CREXX_RXC='/Users/adrian/CLionProjects/CREXX/cmake-build-debug/bin/rxc',THE_CREXX_RXAS='/Users/adrian/CLionProjects/CREXX/cmake-build-debug/bin/rxas',THE_CREXX_IMPORT_DIR='/Users/adrian/CLionProjects/CREXX/cmake-build-debug/bin',THE_CREXX_LIBRARY_RXBIN='/Users/adrian/CLionProjects/CREXX/cmake-build-debug/bin/library.rxbin',TERM='xterm-256color',THE_REXX_INTERPRETER='CREXX',THE_HOME_DIR=str(release),THE_DRIVER_PATH=str(build),THE_TOOL_ROOT=str(repo/'build/native-tools-gtk-aligned/tools'),THE_TOOL_CONFIG_DIR=str(base/'config'),XDG_CONFIG_HOME=str(base/'xdg'),THE_CREXX_CACHE_DIR=str(base/'cache'),THE_MACRO_PATH=str(repo),PATH='/Users/adrian/CLionProjects/CREXX/cmake-build-debug/bin:/opt/homebrew/bin:/usr/bin:/bin',DYLD_LIBRARY_PATH='/Users/adrian/CLionProjects/CREXX/cmake-build-debug/bin')
(base/'environment.json').write_text(json.dumps({k:v for k,v in env.items() if k.startswith(('THE_','XDG_')) or k in ['TERM','PATH','DYLD_LIBRARY_PATH']},indent=2))
process=subprocess.Popen([str(build/'the'),'--driver','curses','-p',str(repo/'profile.the'),str(base/'workspace/Welcome.txt')],stdin=slave,stdout=slave,stderr=slave,cwd=base/'workspace',env=env);os.close(slave)
def pump(timeout=1.0):
 deadline=time.monotonic()+timeout;last=time.monotonic();got=False
 while time.monotonic()<deadline:
  if select.select([master],[],[],.03)[0]:
   try:data=os.read(master,65536)
   except OSError:break
   if not data:break
   raw.extend(data);got=True;last=time.monotonic();graphics.FG_BG_256[:]=palette
   for m in re.finditer(rb'\x1b\]4;(\d+);rgb:([0-9a-fA-F]+)/([0-9a-fA-F]+)/([0-9a-fA-F]+)',raw):
    index=int(m[1])
    if index<256:graphics.FG_BG_256[index]=''.join(f'{round(int(x,16)*255/(16**len(x)-1)):02x}' for x in m.groups()[1:])
   stream.feed(data)
  # Keep pumping through ncurses ESC delay; no idle shortcut.
 return got

def capture(name):
 pump(.5)
 cells=[]
 for y in range(screen.lines):
  row=[]
  for x in range(screen.columns):
   c=screen.buffer[y][x]
   row.append(dict(text=c.data,fg=c.fg,bg=c.bg,bold=c.bold,italics=c.italics,underscore=c.underscore,reverse=c.reverse,strikethrough=c.strikethrough,blink=c.blink))
  cells.append(row)
 result={'kind':'real-PTY-projection-not-native-screenshot','rows':screen.lines,'columns':screen.columns,'cursor':{'x':screen.cursor.x,'y':screen.cursor.y,'hidden':screen.cursor.hidden},'display':screen.display,'cells':cells,'pid':process.pid,'returncode':process.poll()}
 dest=Path('/Users/adrian/CLionProjects/THE/tests/ui-acceptance/alignment/console/candidate02/evidence')
 (dest/(name+'.json')).write_text(json.dumps(result,indent=2)+'\n');(dest/(name+'.txt')).write_text('\n'.join(screen.display)+'\n');(base/'raw.ansi').write_bytes(raw)
 render(result,dest/(name+'.png'))
 print(json.dumps({'capture':str(dest/(name+'.json')),'cursor':result['cursor'],'pid':process.pid,'returncode':process.poll()}),flush=True)

def render(result,path):
 import html
 def colour(c,default):
  names={'default':default,'black':'000000','red':'cd0000','green':'00cd00','brown':'cdcd00','blue':'0000ee','magenta':'cd00cd','cyan':'00cdcd','white':'e5e5e5','brightblack':'7f7f7f','brightwhite':'ffffff'}
  v=names.get(c,c)
  return '#'+v if len(v)==6 else '#'+default
 rows_html=[]
 for row in result['cells']:
  cells=[]
  for c in row:
   fg=colour(c['fg'],'dddddd');bg=colour(c['bg'],'000000')
   if c['reverse']:fg,bg=bg,fg
   cells.append(f'<span style="color:{fg};background:{bg}">{html.escape(c['text'] or ' ')}</span>')
  rows_html.append(''.join(cells))
 path.with_suffix('.html').write_text('<!doctype html><meta charset="utf-8"><title>PTY cell projection</title><style>body{background:#222;color:white;font:13px monospace}pre{font:14px/1.25 Menlo,monospace;white-space:pre}</style><p>REAL PTY CELL PROJECTION — '+str(cols)+'×'+str(rows)+'; not a native Terminal screenshot. Browser font metrics are illustrative. DEC ACS line-drawing may be projected as letters by pyte.</p><pre>'+"\n".join(rows_html)+'</pre>')

pump(3);print(json.dumps({'ready':True,'pid':process.pid,'grid':[cols,rows]}),flush=True)
try:
 for line in sys.stdin:
  try:
   cmd=json.loads(line)
   if 'send' in cmd:os.write(master,bytes.fromhex(cmd['send']));pump(cmd.get('wait',1.0))
   if 'text' in cmd:os.write(master,cmd['text'].encode());pump(cmd.get('wait',1.0))
   if 'menu' in cmd:
    os.write(master,b'\x1b[21~');pump(.5);os.write(master,b'\x1b[H');pump(.3)
    for _ in range(120):
     if cmd['menu'] in screen.display[screen.cursor.y]:break
     os.write(master,b'\x1b[B');pump(.2)
    if cmd.get('invoke',True):os.write(master,b'\r');pump(cmd.get('wait',1))
   if 'resize' in cmd:
    cols,rows=cmd['resize'];fcntl.ioctl(master,termios.TIOCSWINSZ,struct.pack('HHHH',rows,cols,0,0));screen.resize(rows,cols);process.send_signal(signal.SIGWINCH);pump(1)
   if 'capture' in cmd:capture(cmd['capture'])
   else:print(json.dumps({'ok':True,'returncode':process.poll()}),flush=True)
   if cmd.get('finish'):break
  except Exception as ex:print(json.dumps({'error':str(ex)}),flush=True)
finally:
 if process.poll() is None:
  process.terminate()
  try:process.wait(timeout=3)
  except subprocess.TimeoutExpired:process.kill();process.wait(timeout=3)
 os.close(master);(base/'raw.ansi').write_bytes(raw)
 current={n:hashlib.sha256((base/'workspace'/n).read_bytes()).hexdigest() for n in fixtures}
 (base/'fixture-bytes.json').write_text(json.dumps({'initial':initial,'final':current,'unchanged':initial==current,'process_returncode':process.returncode},indent=2)+'\n')
 print(json.dumps({'closed':True,'fixture_bytes_unchanged':initial==current,'returncode':process.returncode}),flush=True)
