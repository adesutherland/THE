#!/usr/bin/env python3
"""Real editor contracts: snapshot, pipes, status, cancellation and profiles."""
import json, os, subprocess, sys, tempfile
from pathlib import Path
binary=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='THE tools ') as d:
    root=Path(d); source=root/'program.rexx'; config=root/'config';config.mkdir()
    env=dict(os.environ,THE_TOOL_CONFIG_DIR=str(config))
    def run(text,commands):
        source.write_text(text)
        p=subprocess.run([str(binary),'--driver','llm','-n',str(source)],input=commands+'\nquit\n',text=True,capture_output=True,env=env,timeout=50)
        assert p.returncode==0,p.stderr
        return [json.loads(s) for s in p.stdout.splitlines() if s.startswith('{')]
    def tools(records): return [r for r in records if r.get('type')=='tools']
    source.write_text("say 'optional tools'\n")
    disabled=subprocess.run([str(binary),'--driver','llm','-n',str(source)],input='tools\nquit\n',text=True,capture_output=True,env=dict(env,THE_TOOL_ROOT=str(root/'unavailable')),timeout=20)
    assert disabled.returncode==0,disabled.stderr
    disabled_state=next(json.loads(line) for line in disabled.stdout.splitlines() if line.startswith('{') and json.loads(line).get('type')=='tools')
    assert not any(t['available'] for t in disabled_state['catalog']),disabled_state
    records=run("say 'before'\npull name\nsay 'hello' name\nexit 7\n",'command tool run\ntoolwait 300\ncommand tool input Zoë😀\ntoolwait 10000\ntools\nlook filearea compact')
    session=tools(records)[-1];assert session['job']['state']=='Completed · status 7',session
    output=''.join(c['text'] for c in session['chunks']);assert 'hello ZOË😀' in output,output
    snap=next(r for r in records if 'screen_rows' in r);assert snap['buffer']['dirty']==0 and source.read_text().startswith("say 'before'"),snap
    records=run("do forever\nnop\nend\n",'command tool run\ntoolwait 200\ncommand tool stop\ntools\ncommand tool run\ncommand tool stop\ntools')
    first,last=tools(records);assert first['job']['state']==last['job']['state']=='Stopped';assert first['job']['id']!=last['job']['id']
    records=run("signal missing\n",'command tool run\ntoolwait 10000\ntools');assert tools(records)[-1]['job']['state']=='Runtime condition',records
    records=run("options levelb\naddress THE\n'set number on';\nsay 'profile console proof'\nreturn 0\n",'command tool build\ntoolwait 30000\ncommand tool activate\ntools')
    assert 'profile console proof' in ''.join(c['text'] for c in tools(records)[-1]['chunks']),records
    selected=(config/'active').read_text();assert Path(selected.splitlines()[0]).is_file()
    # A new editor restores the immutable artifact and its addressed settings.
    records=run("say 'untouched'\n",'tools');assert 'profile console proof' in ''.join(c['text'] for c in tools(records)[-1]['chunks']),records
    records=run("options levelb\nthis is invalid !!\n",'command tool build\ntoolwait 30000\ntools');assert tools(records)[-1]['job']['state']!='Completed · status 0';assert (config/'active').read_text()==selected
    records=run("/* Classic parser */\nsay 'hello'\n",'command tool language classic-highlight\ncommand sdslhwait 10000\nlook filearea compact')
    assert all(r.get('rc',0)==0 for r in records),records
    snap=next(r for r in records if 'screen_rows' in r)
    styles=[style[2] for row in snap['screen_rows'] for style in row.get('s',[])]
    assert 'comment' in styles and 'string' in styles,records
print('Native tool jobs, input, status, Stop, conditions and profile activation/restore passed')
