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
    assert sample.read_text(encoding='utf-8')=='alpha\nfindme\n'
    assert any(row['role']=='reserved' and 'F6 Cmd' in row['t'] and 'F10 Keys' in row['t']
               for row in snap['screen_rows']),snap
    for key in ['f6','esc','ctrl-l']:
        records,_=run('key '+key+'\nlook full compact')
        assert any(record.get('focus',{}).get('zone')=='command' for record in records),(key,records)
    records,_=run('key f10\nlook full compact')
    assert any('define ' in row['t'].lower() for record in records
               for row in record.get('screen_rows',[])),records
    records,_=run('command query colour filetabs\nlook full compact')
    assert any('#1E1E1E' in (record.get('last_message','')+record.get('status','')).upper() for record in records),records


    records,_=run('command cursor goto 1 1\ntext X\nkey ctrl-s\nlook filearea compact')
    assert sample.read_text(encoding='utf-8')=='Xalpha\nfindme\n',sample.read_text(encoding='utf-8')
    records,_=run('key ctrl-f\nlook full compact')
    assert any(r.get('command')=='locate /' for r in records),records
    records,_=run('key ctrl-o\nlook full compact')
    assert any(r.get('command')=='edit ' for r in records),records
    records,_=run('key ctrl-g\nlook full compact')
    assert any(r.get('command')==':' for r in records),records
    records,_=run('command cursor goto 1 1\nkey f4\nlook full compact')
    assert any(r.get('focus',{}).get('zone')=='prefix' for r in records),records
    records,_=run('command cursor goto 1 1\nkey end\nkey enter\ntext beta\nkey ctrl-s')
    assert sample.read_text(encoding='utf-8')=='Xalpha\nbeta\nfindme\n',sample.read_text(encoding='utf-8')
    empty=root/'new-file.txt'
    records,_=run('text A\nkey enter\ntext B\nkey ctrl-s',empty)
    assert empty.read_text(encoding='utf-8')=='A\nB\n',empty.read_text(encoding='utf-8')

    navigation=root/'navigation.txt'
    original='α界e\u0301\n\nsecond\n'
    navigation.write_text(original,encoding='utf-8')
    original_bytes=navigation.read_bytes()
    def last_focus(commands):
        records,_=run(commands+'\nlook full compact',navigation)
        return next(record['focus'] for record in reversed(records) if 'focus' in record)
    def expect_focus(commands, zone, line, cell):
        focus=last_focus(commands)
        assert (focus['zone'],focus['line'],focus['cell'])==(zone,line,cell),(commands,focus)
    expect_focus('command cursor goto 3 1\nkey left','prefix',3,6)
    traverse='command cursor goto 3 1\n'+('key left\n'*8)
    expect_focus(traverse,'filearea',2,0)
    expect_focus(traverse+('key left\n'*8),'filearea',1,4)
    expect_focus(traverse+('key left\n'*9),'filearea',1,3)
    expect_focus('command cursor goto 1 1\n'+('key left\n'*9),'prefix',1,0)
    expect_focus('command set prefix off\ncommand cursor goto 3 1\nkey left','filearea',2,0)
    expect_focus('command set prefix on right 8 1\ncommand cursor goto 3 1\n'+('key left\n'*8),'filearea',2,0)
    expect_focus('command cursor goto 3 1\nkey shift-left','filearea',2,0)
    expect_focus('command cursor goto 3 3\nkey f6\nkey f6','filearea',3,2)
    expect_focus('command cursor goto 3 1\nkey f4\nkey f6\nkey f6','filearea',3,0)
    assert navigation.read_bytes()==original_bytes,navigation.read_bytes()

    # A panel is temporary UI: preserve the caller's footer, custom rows and
    # exact caret, then type/save directly without a corrective CURSOR command.
    demo = 'paneldemo_regina' if engine == 'REGINA' else 'paneldemo'
    setup = 'command set reserved 4 yellow on blue Caller row\ncommand cursor goto 3 3\n'
    baseline,_ = run(setup+'look full compact',navigation)
    baseline = next(r for r in reversed(baseline) if 'screen_rows' in r)
    reserved = lambda s: [(r['r'],r['t']) for r in s['screen_rows'] if r['role']=='reserved']
    for action in ['enter','esc']:
        navigation.write_bytes(original_bytes)
        commands = setup+'command macro '+demo+'\nkey '+action
        records,_ = run(commands+'\nlook full compact',navigation)
        restored = next(r for r in reversed(records) if 'screen_rows' in r)
        assert restored['focus']==baseline['focus'],(engine,action,restored['focus'],baseline['focus'])
        assert reserved(restored)==reserved(baseline),(engine,action,reserved(restored))
        assert navigation.read_bytes()==original_bytes
        run(commands+'\ntext Z\nkey ctrl-s',navigation)
        assert navigation.read_text(encoding='utf-8')=='α界e\u0301\n\nseZcond\n'
    navigation.write_bytes(original_bytes)
    for position in ['command cursor goto 1 4',
                     'command cursor goto 3 3\ncommand cursor prefix',
                     'command cursor goto 3 3\ncommand cursor cmdline']:
        before,_ = run(position+'\nlook full compact',navigation)
        after,_ = run(position+'\ncommand macro '+demo+'\nkey esc\nlook full compact',navigation)
        before = next(r for r in reversed(before) if 'screen_rows' in r)
        after = next(r for r in reversed(after) if 'screen_rows' in r)
        assert after['focus']==before['focus'],(engine,position,before['focus'],after['focus'])

    # Tab inserts ordinary spaces and uses logical cells rather than UTF-8 bytes.
    for name, original, column, expected in [
        ('ascii', 'alpha', 1, '        alpha'),
        ('greek', 'αZ', 2, 'α       Z'),
        ('wide', '界Z', 3, '界      Z'),
    ]:
        target=root/(name+'-tab.txt')
        target.write_text(original+'\n',encoding='utf-8')
        run('command cursor goto 1 '+str(column)+'\nkey tab\nkey ctrl-s',target)
        assert target.read_text(encoding='utf-8')==expected+'\n',(name,target.read_text(encoding='utf-8'))
    new_tab=root/'new-tab.txt'
    run('key tab\ntext beta\nkey ctrl-s',new_tab)
    assert new_tab.read_text(encoding='utf-8')=='        beta\n',new_tab.read_text(encoding='utf-8')
    raw=root/'retained-tab.txt'
    raw.write_bytes(b'a\tb\n')
    records,_=run('look full compact\nkey ctrl-s',raw)
    native_eol=b'\r\n' if os.name=='nt' else b'\n'
    assert raw.read_bytes()==b'a\tb'+native_eol,raw.read_bytes()
    assert any(row.get('t')=='a\tb' for record in records for row in record.get('screen_rows',[])),records

print(engine+': first-launch ruler, colors/profile, quiet optional parsers, insert/Enter, save, find/open/line prompts, prefix and empty-file editing passed')
