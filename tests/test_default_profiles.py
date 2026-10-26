#!/usr/bin/env python3
"""Exercise the first-launch layout and editing keys with either macro engine."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
engine = sys.argv[2] if len(sys.argv) > 2 else 'CREXX'
repo = Path(__file__).resolve().parent.parent
subprocess.run([sys.executable,str(repo/'scripts/generate-default-profiles.py'),'--check'],check=True)
profile = repo / ('profile_regina.the' if engine == 'REGINA' else 'profile.the')
with tempfile.TemporaryDirectory(prefix='CRXTHE first launch ') as temporary:
    root = Path(temporary)
    sample = root/'sample.txt'
    sample.write_text('alpha\nfindme\n',encoding='utf-8')
    env = dict(os.environ,THE_REXX_INTERPRETER=engine)
    if os.name == 'nt':
        system=Path(env.get('SystemRoot') or env['SYSTEMROOT'])
        env['PATH']=str(system/'System32')+os.pathsep+str(system)
    else:
        env['PATH']='/usr/bin:/bin'

    def run(commands, target=sample):
        result=subprocess.run([str(binary),'--driver','llm','-p',str(profile),str(target)],
                              input=commands+'\nquit\n',env=env,text=True,encoding='utf-8',capture_output=True,timeout=45,check=True)
        assert not any(word in result.stderr for word in ['Error ','Unable to','not found','not recognized']),result.stderr
        records=[json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
        assert all(not line.strip() or line.startswith('{') for line in result.stdout.splitlines()),result.stdout
        return records,result.stderr

    records,errors=run('capabilities\nlook full compact')
    caps=next(r for r in records if 'macro_available' in r)
    assert caps['macro_available'] and caps['macro_interpreter']==engine,caps
    snap=next(r for r in records if 'screen_rows' in r)
    assert any(r['role']=='scale' and r['r']==0 for r in snap['screen_rows']),snap
    ruler=next(r['t'] for r in snap['screen_rows'] if r['role']=='scale')
    assert '+' in ruler and '1' in ruler,ruler
    assert 'CRXTHE 0.9.0' in snap['status'],snap['status']
    assert sample.read_text()=='alpha\nfindme\n'

    records,_=run('command cursor goto 1 1\ntext X\nkey ctrl-s\nlook filearea compact')
    assert sample.read_text()=='Xalpha\nfindme\n',sample.read_text()
    records,_=run('key ctrl-f\nlook full compact')
    assert any(r.get('command')=='locate /' for r in records),records
    records,_=run('key ctrl-o\nlook full compact')
    assert any(r.get('command')=='edit ' for r in records),records
    records,_=run('key ctrl-g\nlook full compact')
    assert any(r.get('command')==':' for r in records),records
    records,_=run('command cursor goto 1 1\nkey f4\nlook full compact')
    assert any(r.get('focus',{}).get('zone')=='prefix' for r in records),records
    records,_=run('command cursor goto 1 1\nkey end\nkey enter\ntext beta\nkey ctrl-s')
    assert sample.read_text()=='Xalpha\nbeta\nfindme\n',sample.read_text()
    empty=root/'new-file.txt'
    records,_=run('text A\nkey enter\ntext B\nkey ctrl-s',empty)
    assert empty.read_text()=='A\nB\n',empty.read_text()

print(engine+': first-launch ruler, colors/profile, quiet optional parsers, insert/Enter, save, find/open/line prompts, prefix and empty-file editing passed')
