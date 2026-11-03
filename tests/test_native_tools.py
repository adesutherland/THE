#!/usr/bin/env python3
"""Real editor contracts: snapshot, pipes, status, cancellation and profiles."""
import json, os, subprocess, sys, tempfile
from pathlib import Path
binary=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='THE tools ') as d:
    root=Path(d); source=root/'program.rexx'; config=root/'config';config.mkdir()
    env=dict(os.environ,THE_TOOL_CONFIG_DIR=str(config))
    def run(text,commands,path=None):
        target=path or source
        target.write_text(text)
        p=subprocess.run([str(binary),'--driver','llm','-n',str(target)],input=commands+'\nquit\n',text=True,capture_output=True,env=env,timeout=50)
        assert p.returncode==0,p.stderr
        return [json.loads(s) for s in p.stdout.splitlines() if s.startswith('{')]
    def tools(records): return [r for r in records if r.get('type')=='tools']
    source.write_text("say 'optional tools'\n")
    disabled=subprocess.run([str(binary),'--driver','llm','-n',str(source)],input='tools\nquit\n',text=True,capture_output=True,env=dict(env,THE_TOOL_ROOT=str(root/'unavailable')),timeout=20)
    assert disabled.returncode==0,disabled.stderr
    disabled_state=next(json.loads(line) for line in disabled.stdout.splitlines() if line.startswith('{') and json.loads(line).get('type')=='tools')
    assert not any(t['available'] for t in disabled_state['catalog']),disabled_state
    records=run("say 'before'\npull name\nsay 'hello' name\nexit 7\n",'command RX RUN\ntoolwait 300\ncommand RX INPUT Zoë😀\ntoolwait 10000\ntools\nlook filearea compact')
    session=tools(records)[-1];assert session['job']['state']=='Completed · status 7',session
    output=''.join(c['text'] for c in session['chunks']);assert 'hello ZOË😀' in output,output
    snap=next(r for r in records if 'screen_rows' in r);assert snap['buffer']['dirty']==0 and source.read_text().startswith("say 'before'"),snap
    records=run("parse arg text\nsay text\nexit 0\n",'command rX RuN classic-rexx "Mixed Case"\ntoolwait 10000\ntools')
    assert tools(records)[-1]['job']['state']=='Completed · status 0',records
    assert 'Mixed Case\n' in ''.join(c['text'] for c in tools(records)[-1]['chunks']),records
    # Direct RXBIN names (including quoted spaces and case) select the program adapter.
    compiler=Path(env['THE_CREXX_RXC']);assembler=Path(env['THE_CREXX_RXAS'])
    native_source=root/'Compiled Program.crexx';stem=root/'Compiled Program'
    native_source.write_text("options levelb\nsay 'RX RUN bytecode proof'\nreturn 9\n")
    subprocess.run([str(compiler),'-i',env['THE_CREXX_IMPORT_DIR'],'-o',str(stem),str(native_source)],capture_output=True,text=True,check=True,timeout=30)
    subprocess.run([str(assembler),'-o',str(stem),str(stem)],capture_output=True,text=True,check=True,timeout=30)
    image=root/'Compiled Program.RXBIN';stem.with_suffix('.rxbin').rename(image)
    records=run("say 'buffer is not executed'\n",f'command RX RUN "{image}" "Preserved Argument"\ntoolwait 10000\ntools\nlook filearea compact')
    session=tools(records)[-1]
    assert session['job']['state']=='Completed · status 9' and session['job']['tool']=='rxbin',session
    assert 'RX RUN bytecode proof\n' in ''.join(c['text'] for c in session['chunks']),session
    assert 'buffer is not executed\n' not in ''.join(c['text'] for c in session['chunks']),session
    snap=next(r for r in records if 'screen_rows' in r);assert snap['buffer']['dirty']==0 and snap['focus']['cell']==0,snap
    assert source.read_text()=="say 'buffer is not executed'\n"
    # A profile selects a source-file RXBIN runner by extension. It receives an
    # immutable unsaved snapshot as argv[1], with Program stdin kept separate.
    runner_stem=root/'Source File Runner'
    fixture=Path(__file__).with_name('source-file-runner.crexx')
    subprocess.run([str(compiler),'-i',env['THE_CREXX_IMPORT_DIR'],'-o',str(runner_stem),str(fixture)],capture_output=True,text=True,check=True,timeout=30)
    subprocess.run([str(assembler),'-o',str(runner_stem),str(runner_stem)],capture_output=True,text=True,check=True,timeout=30)
    runner_image=runner_stem.with_suffix('.rxbin');dsl_source=root/'Lesson Zoë.BAS'
    disk_text='DISK FIRST\nSECOND\n'
    records=run(disk_text,
        f'command RX REGISTER source-probe runner "{runner_image}" .bas "Source probe"\n'
        'command set insertmode on\ncommand cursor goto 1 1\ntext PENDING \nlook full compact\n'
        'command RX RUN\ntoolwait 300\ntools\nlook full compact\n'
        'text LATER \nlook full compact\ncommand RX INPUT Zoë😀\ncommand RX EOF\n'
        'toolwait 10000\ntools\nlook full compact\n'
        'command RX RUN source-probe "Mixed Case" "" "Zoë😀"\n'
        'command RX EOF\ntoolwait 10000\ntools',dsl_source)
    assert all(r.get('rc',0)==0 for r in records),records
    before,launched,edited,completed=[r for r in records if 'screen_rows' in r]
    assert before['buffer']['dirty'] and before['buffer']==launched['buffer'],records
    assert before['focus']==launched['focus'] and edited['buffer']==completed['buffer'],records
    assert edited['focus']==completed['focus'] and dsl_source.read_text()==disk_text,records
    active,finished,explicit=tools(records)
    assert active['job']['running'] and active['job']['tool']=='source-probe',active
    assert Path(active['job']['filename']).resolve()==dsl_source.resolve() and active['job']['sourceId'],active
    assert finished['job']['state']=='Completed · status 0',finished
    assert finished['job']['sourceId']==active['job']['sourceId'] and finished['job']['revision']==active['job']['revision'],finished
    captured='PENDINGDISK FIRST\nSECOND\n'
    output=''.join(c['text'] for c in finished['chunks'])
    assert output.count('SOURCE-BEGIN\n'+captured+'SOURCE-END\n')==2,output
    assert 'SOURCE-NAME:Lesson Zoë.BAS\n' in output and 'STDIN-BEGIN\n' in output and '\nZoë😀\nSTDIN-END' in output,output
    assert 'LATER' not in output and 'crxthe-job-' not in output,output
    assert explicit['job']['state']=='Completed · status 0' and explicit['job']['revision']>finished['job']['revision'],explicit
    output=''.join(c['text'] for c in explicit['chunks'])
    assert 'ARGC:3\nARG:Mixed Case\nARG:\nARG:Zoë😀\n' in output,output
    # Registry restore retains the runner entry and extension dispatch.
    restored=run(disk_text,'command RX RUN\ncommand RX EOF\ntoolwait 10000\ntools',dsl_source)
    assert tools(restored)[-1]['job']['tool']=='source-probe' and tools(restored)[-1]['job']['state']=='Completed · status 0',restored
    records=run("say 'console controls'\n",'command RX CONSOLE ON\ntools\ncommand RX CONSOLE ON\ntools\ncommand RX CONSOLE OFF\ntools')
    opened,still_open,closed=tools(records)
    assert opened['visible'] and still_open['visible'] and not closed['visible'],records
    records=run("do forever\nnop\nend\n",'command RX RUN\ntoolwait 200\ncommand RX STOP\ntools\ncommand RX RUN\ncommand RX STOP\ntools')
    first,last=tools(records);assert first['job']['state']==last['job']['state']=='Stopped';assert first['job']['id']!=last['job']['id']
    records=run("signal missing\n",'command RX RUN\ntoolwait 10000\ntools');assert tools(records)[-1]['job']['state']=='Runtime condition',records
    fresh_profile=tools(run("say 'untouched'\n",'command RX PROFILE\ntools'))[-1]
    assert Path(fresh_profile['currentFile']['path']).resolve()==(config/'user.profile.the').resolve(),fresh_profile
    assert (config/'user.profile.the').is_file(),fresh_profile
    legacy_source=config/'profile.the';legacy_source.write_text('options levelb\nreturn 0\n')
    legacy_profile=tools(run("say 'untouched'\n",'command RX PROFILE\ntools'))[-1]
    assert Path(legacy_profile['currentFile']['path']).resolve()==legacy_source.resolve(),legacy_profile
    assert legacy_profile['currentFile']['role']=='profile-source',legacy_profile
    # Primary file purpose is explicit. Generic THE macros do not expose Apply;
    # an intentional legacy RX ACTIVATE can still apply their current build.
    for name,role in [('Probe.the','macro-source'),('User.profile.the','profile-source'),
                      ('browser.the','profile-source'),('MiniBASIC-profile.the','profile-source'),
                      ('program.crexx','crexx-source')]:
        state=tools(run('options levelb\nreturn 0\n','tools',root/name))[-1]
        assert state['currentFile']['role']==role,state
        actions={a['id']:a for a in state['currentFile']['actions']}
        assert actions['build']['enabled'],state
        assert bool(actions['activate']['tool'])==(role=='profile-source'),state
        assert bool(actions['run']['tool'])==(role in ('macro-source','crexx-source')),state
        if role=='crexx-source':
            assert actions['run']['tool']=='crexx' and not actions['run']['enabled'],state
    # Compiled source programs use the isolated VM, with their source revision
    # kept distinct from ADDRESS THE macros and profile application.
    levelg_source=root/'LevelGAccumulator.crexx'
    levelg_text=(Path(__file__).parent.parent/'examples/LevelGAccumulator.crexx').read_text()
    records=run(levelg_text,
                'command RX BUILD\ntoolwait 30000\ntools\n'
                'command RX RUN\ntoolwait 10000\ntools\n'
                'command cursor goto 5 1\ncommand replace do n = 1 to 6\n'
                'command RX RUN\ntools\nlook full compact',levelg_source)
    built,completed,refused=tools(records)
    # Compiling a program does not select it as the editable/active profile.
    assert Path(built['profile']['sourcePath']).resolve()==legacy_source.resolve(),built
    assert not built['profile']['active']['image'],built
    assert built['profile']['build']['role']=='crexx-source',built
    assert built['profile']['build']['image'].endswith('.rxbin'),built
    assert completed['job']['tool']=='crexx' and completed['job']['state']=='Completed · status 0',completed
    assert 'sum of squares: 55\n' in ''.join(c['text'] for c in completed['chunks']),completed
    assert completed['job']['sourceId']==built['profile']['build']['sourceId'],records
    assert completed['job']['revision']==built['profile']['build']['revision'],records
    assert refused['job']['id']==completed['job']['id'],records
    assert not next(a for a in refused['currentFile']['actions'] if a['id']=='run')['enabled'],refused
    changed=next(r for r in records if 'screen_rows' in r)
    assert changed['buffer']['dirty'] and 'changed; build' in changed['last_message'],changed
    assert levelg_source.read_text()==levelg_text
    macro_source=root/'Probe.the'
    records=run('options levelb\nsay \'macro proof\'\nreturn 0\n',
                'command RX BUILD\ntoolwait 30000\ncommand RX ACTIVATE\ntools',macro_source)
    macro_state=tools(records)[-1]
    assert macro_state['profile']['build']['role']=='macro-source',macro_state
    assert macro_state['profile']['build']['name']=='Probe.rxbin',macro_state
    assert macro_state['profile']['build']['image'].endswith('.rxbin') and not macro_state['profile']['build']['image'].endswith('.profile.rxbin'),macro_state
    assert (config/'active').is_file(),macro_state
    assert any('macro proof' in c['text'] for c in macro_state['build']['chunks']),macro_state
    macro_actions={a['id']:a for a in macro_state['currentFile']['actions']}
    assert not macro_actions['activate']['enabled'] and not macro_actions['activate']['tool'],macro_state
    profile_source=root/'User.profile.the'
    records=run("options levelb\naddress THE\n'set number on';\nsay 'profile console proof'\nreturn 0\n",'command RX BUILD\ntoolwait 30000\ncommand RX ACTIVATE\ntools',profile_source)
    assert 'profile console proof' in ''.join(c['text'] for c in tools(records)[-1]['build']['chunks']),records
    selected=(config/'active').read_text();assert Path(selected.splitlines()[0]).is_file()
    profile_state=tools(records)[-1]
    assert Path(profile_state['profile']['sourcePath']).resolve()==profile_source.resolve(),profile_state
    assert profile_state['profile']['build']['role']=='profile-source',profile_state
    assert profile_state['profile']['build']['name']=='User.profile.rxbin',profile_state
    assert selected.splitlines()[0].endswith('.profile.rxbin'),selected
    # A new editor restores the immutable artifact and its addressed settings.
    records=run("say 'untouched'\n",'tools');assert 'profile console proof' in ''.join(c['text'] for c in tools(records)[-1]['build']['chunks']),records
    active_edit=tools(run("say 'untouched'\n",'command RX PROFILE\ntools'))[-1]
    assert Path(active_edit['currentFile']['path']).resolve()==profile_source.resolve(),active_edit
    records=run("options levelb\nthis is invalid !!\n",'command RX BUILD\ntoolwait 30000\ntools',profile_source);assert tools(records)[-1]['build']['job']['state']!='Completed · status 0';assert (config/'active').read_text()==selected
    # Profile registrations replace the prior profile's own associations.
    # Failure restores the old catalog, and reload replays profile-only tools
    # rather than persisting them as independent registry definitions.
    # Existing immutable build-*.rxbin selections remain restorable after the
    # compound-extension introduction. Their source identity remains stable.
    old_image=config/'build-legacy.rxbin'
    old_image.write_bytes(Path(selected.splitlines()[0]).read_bytes())
    legacy_selection='\n'.join([str(old_image),*selected.splitlines()[1:3]])+'\n'
    (config/'active').write_text(legacy_selection)
    restored_legacy=tools(run("say 'untouched'\n",'tools'))[-1]
    assert restored_legacy['profile']['active']['image']==str(old_image),restored_legacy
    assert 'profile console proof' in ''.join(c['text'] for c in restored_legacy['build']['chunks']),restored_legacy
    (config/'active').write_text(selected)
    # Imported profiles retain an immutable copy and never send binary bytes to
    # Edit profile. Startup replays the copy even after the import is changed.
    imported=root/'Imported.profile.rxbin';imported.write_bytes(Path(selected.splitlines()[0]).read_bytes())
    records=run("say 'untouched'\n",f'look filearea compact\ncommand RX APPLY-PROFILE "{imported}"\ntools\nlook filearea compact')
    imported_state=tools(records)[-1]
    assert imported_state['fileResult']['action']=='apply-profile' and imported_state['fileResult']['code']=='applied',imported_state
    imported_active=imported_state['profile']['active']
    assert imported_active['image']!=str(imported) and imported_active['image'].endswith('.profile.rxbin'),imported_active
    assert imported_active['kind']=='compiled-image' and not imported_active['sourcePath'],imported_active
    assert Path(imported_active['image']).read_bytes()==imported.read_bytes(),imported_active
    snapshots=[r for r in records if 'screen_rows' in r]
    assert snapshots[0]['buffer']==snapshots[1]['buffer'] and snapshots[0]['focus']==snapshots[1]['focus'],records
    imported_selected=(config/'active').read_text()
    imported.write_bytes(b'import changed after Apply')
    imported_restored=tools(run("say 'untouched'\n",'tools'))[-1]
    assert imported_restored['profile']['active']['image']==imported_active['image'],imported_restored
    assert not imported_restored['profile']['active']['sourcePath'],imported_restored
    assert 'profile console proof' in ''.join(c['text'] for c in imported_restored['build']['chunks']),imported_restored
    fallback_edit=tools(run("say 'untouched'\n",'command RX PROFILE\ntools'))[-1]
    assert Path(fallback_edit['currentFile']['path']).resolve()==legacy_source.resolve(),fallback_edit
    invalid_profile=root/'Invalid.profile.rxbin';invalid_profile.write_bytes(b'not RXBIN')
    oversized=root/'Oversized.profile.rxbin'
    with oversized.open('wb') as image_file:
        image_file.write(b'cReXx007');image_file.truncate(64*1024*1024+1)
    for refused in [invalid_profile,oversized,root/'missing.profile.rxbin',runner_image]:
        refused_state=tools(run("say 'untouched'\n",f'command RX APPLY-PROFILE "{refused}"\ntools'))[-1]
        assert refused_state['fileResult']['code']=='invalid-image',refused_state
        assert refused_state['profile']['active']['image']==imported_active['image'],refused_state
        assert (config/'active').read_text()==imported_selected,refused_state
    # A persistence failure after successful execution must report partial
    # success honestly and retain the applied image for recovery/retry.
    (config/'active').unlink();(config/'active').mkdir()
    partial=tools(run("say 'untouched'\n",f'command RX APPLY-PROFILE "{selected.splitlines()[0]}"\ntools'))[-1]
    assert partial['fileResult']['code']=='applied-unsaved' and partial['fileResult']['rc'],partial
    assert Path(partial['profile']['active']['image']).is_file(),partial
    assert partial['profile']['active']['kind']=='compiled-image',partial
    assert 'profile console proof' in ''.join(c['text'] for c in partial['build']['chunks']),partial
    (config/'active').rmdir();(config/'active').write_text(imported_selected)
    failing_source=root/'Failure.crexx';failing_stem=root/'Failure'
    failing_source.write_text('options levelb\nreturn 7\n')
    subprocess.run([str(compiler),'-i',env['THE_CREXX_IMPORT_DIR'],'-o',str(failing_stem),str(failing_source)],capture_output=True,text=True,check=True,timeout=30)
    subprocess.run([str(assembler),'-o',str(failing_stem),str(failing_stem)],capture_output=True,text=True,check=True,timeout=30)
    failing_image=root/'Failure.profile.rxbin';failing_stem.with_suffix('.rxbin').rename(failing_image)
    retained=set(config.glob('build-import-*.profile.rxbin'))
    failure_state=tools(run("say 'untouched'\n",f'command RX APPLY-PROFILE "{failing_image}"\ntools'))[-1]
    assert failure_state['fileResult']['code']=='failed' and failure_state['fileResult']['rc'],failure_state
    assert failure_state['profile']['active']['image']==imported_active['image'],failure_state
    assert (config/'active').read_text()==imported_selected and set(config.glob('build-import-*.profile.rxbin'))==retained,failure_state
    owned=root/'Owned.profile.the';omitted=root/'Omitted.profile.the';failed=root/'Failed.profile.the'
    owned.write_text(f'options levelb\naddress THE\n\'rx register owned-probe runner "{runner_image}" .own "Owned runner"\';\nreturn 0\n')
    omitted.write_text('options levelb\nreturn 0\n')
    failed.write_text(f'options levelb\naddress THE\n\'rx register owned-probe runner "{runner_image}" .bad "Bad replacement"\';\nreturn 7\n')
    def activate_path(path):
        return f'command edit "{path}"\ncommand RX BUILD\ntoolwait 30000\ncommand RX ACTIVATE\ntools\n'
    commands=activate_path(owned)+activate_path(omitted)+activate_path(owned)+activate_path(failed)
    p=subprocess.run([str(binary),'--driver','llm','-n',str(source)],input=commands+'quit\n',text=True,capture_output=True,env=env,timeout=150)
    assert p.returncode==0,p.stderr
    states=tools([json.loads(s) for s in p.stdout.splitlines() if s.startswith('{')])
    assert len(states)==4,states
    assert any(t['id']=='owned-probe' and t['extensions']=='.own' for t in states[0]['catalog']),states[0]
    assert not any(t['id']=='owned-probe' for t in states[1]['catalog']),states[1]
    assert any(t['id']=='owned-probe' and t['extensions']=='.own' for t in states[3]['catalog']),states[3]
    assert states[3]['profile']['active']['image']==states[2]['profile']['active']['image'],states[3]
    assert 'owned-probe' not in (config/'tools.registry').read_text()
    restored=tools(run("say 'untouched'\n",'tools'))[-1]
    assert any(t['id']=='owned-probe' and t['extensions']=='.own' for t in restored['catalog']),restored
    records=run("/* Classic parser */\nsay 'hello'\n",'command RX LANGUAGE classic-highlight\ncommand sdslhwait 10000\nlook filearea compact')
    assert all(r.get('rc',0)==0 for r in records),records
    snap=next(r for r in records if 'screen_rows' in r)
    styles=[style[2] for row in snap['screen_rows'] for style in row.get('s',[])]
    assert 'comment' in styles and 'string' in styles,records
print('Native tool jobs, source-file snapshots/argv/stdin, input, status, Stop, conditions and profile activation/restore passed')
