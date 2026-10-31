#!/usr/bin/env python3
"""Workbench admission and publication through the real editor and Rexx bridge."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
engine = sys.argv[2] if len(sys.argv) > 2 else 'CREXX'
with tempfile.TemporaryDirectory(prefix='THE workbench ') as temporary:
    root = Path(temporary)
    sample = root / 'sample.txt'
    original = b'alpha\nbeta\n'
    env = dict(os.environ, THE_REXX_INTERPRETER=engine,
               THE_CREXX_CACHE_DIR=str(root / 'cache'), THE_TOOL_CONFIG_DIR=str(root / 'tools'))

    def run(commands, profile=None, reset=True):
        if reset:
            sample.write_bytes(original)
        argv = [str(binary), '--driver', 'llm']
        argv += ['-p', str(profile)] if profile else ['-n']
        result = subprocess.run(argv + [str(sample)], input=commands + '\nquit\n',
                                text=True, encoding='utf-8', capture_output=True,
                                env=env, timeout=45)
        assert result.returncode == 0, (result.returncode, result.stderr)
        records = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
        assert records, result.stdout
        return records

    def snapshot(records):
        return next(record for record in reversed(records) if 'screen_rows' in record)

    def query(records):
        return json.loads(next(record['last_message'] for record in reversed(records)
                               if record.get('last_message', '').startswith('{"schemaVersion":1')))

    definition = '\n'.join([
        'command workbench begin owner public.probe',
        'command workbench action put public.add label "Declared action" command "input EXECUTED"',
        'command workbench menu put public.tools label "Tools / < & >" order 50',
        'command workbench item put public.tools public.item label "Say ""hello""" action public.add',
        'command workbench commit',
    ])
    records = run('command cursor goto 2 3\nlook full compact\n' + definition + '\nlook full compact')
    snaps = [record for record in records if 'screen_rows' in record]
    before, after = snaps[0], snaps[-1]
    assert before['buffer'] == after['buffer'] and before['focus'] == after['focus'], (before, after)
    assert sample.read_bytes() == original and not after['buffer']['dirty']
    assert all(record.get('rc', 0) == 0 for record in records), records
    records = run(definition + '\ncommand workbench invoke public.add\ncommand save\nlook full compact')
    assert b'EXECUTED\n' in sample.read_bytes() and not snapshot(records)['buffer']['dirty']
    records = run('look full compact', reset=False)
    assert snapshot(records)['buffer']['lines'] == 3 and sample.read_bytes().count(b'EXECUTED') == 1

    # Invalid complete references and referenced removals publish nothing.
    records = run(definition + '\ncommand workbench begin owner public.probe\n'
                  'command workbench action remove public.add\ncommand workbench commit\n'
                  'command query workbench\nlook full compact')
    state = query(records)
    assert state['lastResult']['code'] == 'object-in-use' and state['lastResult']['rc'] == -3, state
    assert state['actions'] == 7 and state['menus'] == 3, state
    assert not snapshot(records)['buffer']['dirty'] and sample.read_bytes() == original
    records = run('command workbench begin owner public.probe\n'
                  'command workbench menu put public.tools label "Tools"\n'
                  'command workbench item put public.tools public.item label "Missing" action public.absent\n'
                  'command workbench commit\ncommand query workbench\nlook full compact')
    state = query(records)
    assert state['lastResult']['code'] == 'missing-action' and state['lastResult']['rc'] == 5, state
    assert state['actions'] == 6 and state['menus'] == 2, state

    records = run(definition + '\ncommand workbench begin owner public.probe\n'
                  'command workbench action put public.add label "Replacement" command "input REPLACED"\n'
                  'command workbench menu put public.tools label "Replacement menu"\ncommand workbench commit\n'
                  'command query workbench actions\nlook full compact')
    actions = query(records)
    assert actions['total'] == 7 and actions['records'][-1]['label'] == 'Replacement', actions
    records = run(definition + '\ncommand workbench begin owner public.probe\n'
                  'command workbench menu remove public.tools\ncommand workbench action remove public.add\n'
                  'command workbench commit\ncommand query workbench\nlook full compact')
    state = query(records)
    assert state['actions'] == 6 and state['menus'] == 2 and state['items'] == 5, state
    assert not snapshot(records)['buffer']['dirty'] and sample.read_bytes() == original

    # LINEND and a synonym cannot reinterpret quoted declaration data.
    records = run('command set linend on #\ncommand set synonym wb workbench\n' +
                  definition + '\ncommand wb begin owner public.probe\n'
                  'command wb menu put public.tools label "Tools#input POISON"\ncommand wb commit\n'
                  'command query workbench menus\nlook full compact')
    assert all(record.get('rc', 0) == 0 for record in records), records
    assert query(records)['records'][-1]['label'] == 'Tools#input POISON'
    assert not snapshot(records)['buffer']['dirty'] and sample.read_bytes() == original
    records = run('command set linend on #\ncommand workbench begin owner public.probe\n'
                  'command workbench menu put public.bad label "unterminated#input POISON\n'
                  'command workbench commit\ncommand query workbench\nlook full compact')
    state = query(records)
    assert state['lastResult']['code'] == 'transaction-failed', state
    assert state['menus'] == 2 and not snapshot(records)['buffer']['dirty'], records

    # Generation admission refuses a queued stale action without running it.
    records = run(definition + '\ncommand workbench invoke public.add expect 1\n'
                  'command query workbench\nlook full compact')
    assert query(records)['lastResult']['code'] == 'stale-generation', records
    assert not snapshot(records)['buffer']['dirty'] and sample.read_bytes() == original

    # Read-only failures do not poison an otherwise valid declaration draft.
    records = run('command workbench begin owner public.probe\n'
                  'command workbench menu put public.tools label "Survives inspection refusal"\n'
                  'command workbench invoke absent.action\ncommand query workbench nonsense\n'
                  'command workbench commit\ncommand query workbench\nlook full compact')
    state = query(records)
    assert state['menus'] == 3 and state['lastResult']['code'] == 'ok', state
    assert any(record.get('rc') == 5 for record in records), records

    # Typed builtins retain the intentional built-in synonym bypass.
    records = run('command workbench begin owner public.probe\n'
                  'command workbench action put public.save label "Save" builtin file.save\n'
                  'command workbench commit\ncommand set synonym save input ALIAS\n'
                  'command set insertmode on\ncommand cursor goto 1 1\ntext X\ncommand workbench invoke public.save\nlook full compact')
    assert sample.read_bytes() == b'Xalpha\nbeta\n' and not snapshot(records)['buffer']['dirty'], records

    # Explicit profile COMMIT is held until the complete profile returns.
    def profile(name, commands, result):
        path = root / name
        source = ('options levelb\nimport rxfnsb\n' if engine == 'CREXX' else '/* Rexx */\n') + 'address THE\n'
        source += ''.join("'" + command.replace("'", "''") + "';\n" for command in commands)
        source += f'return {result}\n'
        path.write_text(source, encoding='utf-8')
        return path

    declarations = [line.removeprefix('command ') for line in definition.splitlines()]
    failed = profile('failed.the', declarations, 7)
    records = run('command query workbench\nlook full compact', failed)
    state = query(records)
    assert state['actions'] == 6 and state['menus'] == 2 and state['lastResult']['code'] == 'activation-failed', state
    assert not snapshot(records)['buffer']['dirty'] and sample.read_bytes() == original
    succeeded = profile('success.the', declarations, 0)
    records = run('command query workbench\nlook full compact', succeeded)
    state = query(records)
    assert state['actions'] == 7 and state['menus'] == 3 and state['revision'] == 2, state
    assert not snapshot(records)['buffer']['dirty'] and sample.read_bytes() == original

    explicit_macro = profile('explicit macro.the', [], 7)
    records = run('command workbench begin owner public.probe\n'
                  f'command workbench action put public.macro label "Explicit macro" macro "{explicit_macro}"\n'
                  'command workbench commit\ncommand workbench invoke public.macro\n'
                  'command query workbench\nlook full compact')
    state = query(records)
    assert state['lastResult']['code'] == 'command-failed' and state['lastResult']['rc'] == 7, state
    assert not snapshot(records)['buffer']['dirty'] and sample.read_bytes() == original

    # Phase 2A declarations publish LOG models only at final activation.
    log_definitions = [
        'workbench begin owner public.logs',
        'workbench action put public.show label "Show output" panel public.output op show',
        'workbench panel put public.output kind log model public.programlog',
        'workbench panel place public.output right fallback below,tab mincols 32',
        'workbench route put public.stdout from tool:classic-rexx/stdout to log:public.programlog',
        'workbench route put public.stderr from tool:classic-rexx/stderr to log:public.programlog',
        'workbench commit',
    ]
    log_commands = '\n'.join('command ' + line for line in log_definitions)
    records = run('command cursor goto 2 3\nlook full compact\n' + log_commands +
                  '\ncommand query workbench panels\nlook full compact')
    panels = query(records)
    assert panels['total'] == 1 and panels['records'][0]['kind'] == 'log', panels
    panel = panels['records'][0]
    assert isinstance(panel['generation'], str) and isinstance(panel['modelGeneration'], str), panel
    assert not panel['visible'] and not panel['focused'] and panel['mincols'] == 32, panel
    snaps = [record for record in records if 'screen_rows' in record]
    assert snaps[0]['focus'] == snaps[-1]['focus'] and snaps[0]['buffer'] == snaps[-1]['buffer'], snaps
    assert sample.read_bytes() == original
    records = run(log_commands + '\ncommand workbench invoke public.show\n'
                  'command workbench panel focus public.output\ncommand query workbench panels\nlook full compact')
    panel = query(records)['records'][0]
    assert panel['visible'] and panel['focused'] and not snapshot(records)['buffer']['dirty'], panel
    records = run(log_commands + '\ncommand workbench panel show public.output\n'
                  'command workbench panel hide public.output\ncommand query workbench panels\nlook full compact')
    assert not query(records)['records'][0]['visible'] and not snapshot(records)['buffer']['dirty'], records
    records = run('command workbench begin owner public.logs\n'
                  'command workbench panel put public.doc kind document file "unsupported.txt" access readonly\n'
                  'command query workbench\nlook full compact')
    assert query(records)['lastResult']['code'] == 'unsupported-feature' and sample.read_bytes() == original
    failed_log = profile('failed-log.the', log_definitions, 7)
    records = run('command query workbench\nlook full compact', failed_log)
    state = query(records)
    assert state['panels'] == 0 and state['routes'] == 0 and state['logs'] == 0, state
    startup_log = profile('startup-log.the', log_definitions, 0)
    records = run('command query workbench routes\nlook full compact', startup_log)
    state = snapshot(records)['workbench']
    assert state['panels'] == 1 and state['routes'] == 2 and state['logs'] == 1, state
    assert query(records)['total'] == 2 and not snapshot(records)['buffer']['dirty'], records
    records = run('command query workbench logs\nlook full compact', startup_log)
    assert query(records)['records'][0]['bytes'] == 0 and sample.read_bytes() == original
    records = run(log_commands + '\ncommand query workbench log public.programlog expect 18446744073709551615\n'
                  'command query workbench\nlook full compact')
    assert any(record.get('rc') == -3 for record in records), records
    assert query(records)['lastResult']['code'] == 'stale-generation', records
    # Preserve optional backend configurations; declarations still validate an
    # unavailable catalog entry. Exercise actual routing when the host exists.
    records = run('tools')
    tools = next(record for record in records if record.get('type') == 'tools')
    if any(tool['id'] == 'classic-rexx' and tool['available'] for tool in tools['catalog']):
        program = "say '€ routed stdout'\nexit 0\n".encode('utf-8')
        rc_probe = profile('log-inspection-rc.the', [], 0)
        if engine == 'CREXX':
            probe = "options levelb\nworkbench = .string[]\n"
            expose = ' expose workbench[]'
            result_value = 'workbench[1]'
        else:
            probe = '/* Rexx */\nsignal on error name unexpected_error\n'
            expose = ''
            result_value = 'workbench.1'
        probe += "address the 'extract /workbench logs/'" + expose + ';\n'
        probe += 'saved = ' + result_value + '\n'
        probe += "address the '  qUeRy WoRkBeNcH log public.programlog expect 18446744073709551615';\n"
        probe += 'if rc <> -3 then return 80\n'
        probe += "address the '  eXtRaCt /WoRkBeNcH log public.programlog expect 18446744073709551615/'" + expose + ';\n'
        probe += 'if rc <> -3 then return 81\nif ' + result_value + ' <> saved then return 82\n'
        probe += "address the 'extract /workbench log public.programlog bytes 1/'" + expose + ';\n'
        probe += 'if rc <> 94 then return 83\nif ' + result_value + ' <> saved then return 84\n'
        probe += "address the 'extract /messages nonsense/';\nif rc <> 5 then return 85\n"
        probe += "address the 'msg LOG_INSPECTION_RC_OK';\nreturn 0\n"
        if engine == 'REGINA':
            probe += 'unexpected_error:\nreturn 87\n'
        rc_probe.write_text(probe, encoding='utf-8')
        negative_probe = root / 'unrelated-negative.the'
        if engine == 'REGINA':
            control = "/* Rexx */\nsignal on error name expected_error\naddress the 'fillbox X';\nreturn 86\nexpected_error:\n"
        else:
            control = "options levelb\naddress the 'fillbox X';\nif rc <> -3 then return 86\n"
        control += "address the 'msg UNRELATED_NEGATIVE_OK';\nreturn 0\n"
        negative_probe.write_text(control, encoding='utf-8')
        sample.write_bytes(program)
        records = run('command cursor goto 2 3\nlook full compact\n' + log_commands +
                      '\ncommand workbench panel show public.output\ncommand RX RUN classic-rexx\n'
                      'toolwait 10000\ncommand query workbench log public.programlog limit 8 bytes 4096\nlook full compact\n'
                      'command query workbench log public.programlog bytes 1\ncommand query workbench\nlook full compact\n'
                      f'command macro "{rc_probe}"\nlook full compact\n'
                      f'command macro "{negative_probe}"\nlook full compact',
                      reset=False)
        pages = [json.loads(record['last_message']) for record in records
                 if record.get('last_message', '').startswith('{"schemaVersion":1')]
        page = next(page for page in pages if page['section'] == 'log')
        assert any(record.get('rc') == 94 for record in records), records
        assert any('LOG_INSPECTION_RC_OK' in record.get('last_message', '') for record in records), records
        assert any('UNRELATED_NEGATIVE_OK' in record.get('last_message', '') for record in records), records
        assert page['section'] == 'log' and 'routed stdout' in ''.join(record['text'] for record in page['records']), page
        assert all(record['stream'] == 1 and int(record['job']) > 0 and int(record['sequence']) > 0 for record in page['records']), page
        assert sample.read_bytes() == program and not snapshot(records)['buffer']['dirty'], records
        snaps = [record for record in records if 'screen_rows' in record]
        assert snaps[0]['focus'] == snaps[-1]['focus'], snaps
        assert not snapshot(records)['workbench']['focusRequest']['panel'], snapshot(records)['workbench']

        # A persisted custom tool is metadata before -p route validation. Its
        # language service remains deferred until normal startup has finished.
        def compile_restart_probe(name, marker):
            source = root / (name + '.crexx')
            stem = root / name
            source.write_text("options levelb\nsay '" + marker + "'\nreturn 0\n", encoding='utf-8')
            subprocess.run([env['THE_CREXX_RXC'], '-i', env['THE_CREXX_IMPORT_DIR'],
                            '-o', str(stem), str(source)], check=True, capture_output=True,
                           text=True, timeout=30)
            subprocess.run([env['THE_CREXX_RXAS'], '-o', str(stem), str(stem)],
                           check=True, capture_output=True, text=True, timeout=30)
            return stem.with_suffix('.rxbin')

        saved_image = compile_restart_probe('Saved program', 'SAVED_TOOL_RESTART_OUTPUT')
        replacement_image = compile_restart_probe('Replacement program', 'PROFILE_TOOL_REPLACEMENT_OUTPUT')
        records = run(f'command RX REGISTER restartprogram program "{saved_image}" .restart "Saved tool"\n'
                      f'command RX REGISTER restartlanguage language "{saved_image}" .unused_restart_language "Deferred language"\n'
                      'tools\nlook full compact')
        seed_tools = next(record for record in records if record.get('type') == 'tools')
        assert seed_tools['job']['id'] == 0 and not seed_tools['job']['running'], seed_tools
        registry = root / 'tools' / 'tools.registry'
        assert 'restartprogram\t' in registry.read_text(), registry.read_text()
        language_saved = 'restartlanguage\t' in registry.read_text()

        def parser_checked_profile(name, commands, present, marker):
            path = profile(name, commands, 0)
            if engine == 'CREXX':
                check = "parser = .string[]\naddress THE 'extract /parser */' expose parser[];\n"
                count, value = 'parser[0]', 'parser[i]'
            else:
                check = "address THE 'extract /parser */';\n"
                count, value = 'parser.0', 'parser.i'
            check += f'found = 0\ndo i = 1 to {count}\n'
            check += f"if word({value},1) = 'restartlanguage' then found = 1\nend\n"
            check += f'if found <> {int(present)} then return 89\n'
            check += "address THE 'msg " + marker + "';\n"
            path.write_text(path.read_text().replace('address THE\n', 'address THE\n' + check, 1),
                            encoding='utf-8')
            return path

        saved_routes = [
            'workbench begin owner public.saved',
            'workbench panel put public.savedoutput kind log model public.savedlog',
            'workbench route put public.savedstdout from tool:restartprogram/stdout to log:public.savedlog',
            'workbench commit',
        ]
        saved_profile = parser_checked_profile('saved-tool-startup.the', saved_routes, False,
                                               'REGISTRY_METADATA_WITHOUT_EARLY_LANGUAGE')
        restart_commands = ('look full compact\ntools\ncommand query workbench log public.savedlog\n'
                            'look full compact\ncommand RX RUN restartprogram\ntoolwait 10000\ntools\n'
                            'command query workbench log public.savedlog\nlook full compact')
        records = run(restart_commands, saved_profile)
        assert any('REGISTRY_METADATA_WITHOUT_EARLY_LANGUAGE' in record.get('last_message', '')
                   for record in records), records
        restarted_tools = [record for record in records if record.get('type') == 'tools']
        assert restarted_tools[0]['job']['id'] == 0 and not restarted_tools[0]['job']['running'], restarted_tools
        assert restarted_tools[-1]['job']['state'] == 'Completed · status 0', restarted_tools
        pages = [json.loads(record['last_message']) for record in records
                 if record.get('last_message', '').startswith('{"schemaVersion":1')]
        pages = [page for page in pages if page['section'] == 'log']
        assert not pages[0]['records'] and int(pages[0]['omittedBytes']) == 0, pages
        assert 'SAVED_TOOL_RESTART_OUTPUT' in ''.join(item['text'] for item in pages[-1]['records']), pages
        assert sample.read_bytes() == original and not snapshot(records)['buffer']['dirty'], records
        if language_saved:
            late_language = parser_checked_profile('late-language-check.the', [], True, 'REGISTRY_LANGUAGE_ACTIVATED_LATE')
            records = run(f'command macro "{late_language}"\nlook full compact', saved_profile)
            assert any('REGISTRY_LANGUAGE_ACTIVATED_LATE' in record.get('last_message', '')
                       for record in records), records

        # Later restoration must neither overwrite a startup replacement nor
        # activate a saved language that the profile explicitly changed to data.
        replacements = [
            f'RX REGISTER restartprogram program "{replacement_image}" .restart "Profile replacement"',
            f'RX REGISTER restartlanguage program "{replacement_image}" .unused_restart_language "No language service"',
        ]
        replacement_profile = parser_checked_profile('saved-tool-replacement.the', replacements + saved_routes,
                                                     False, 'REGISTRY_PROFILE_REPLACEMENT')
        records = run(restart_commands, replacement_profile)
        restarted_tools = [record for record in records if record.get('type') == 'tools']
        restored = next(tool for tool in restarted_tools[0]['catalog'] if tool['id'] == 'restartprogram')
        assert restored['image'] == str(replacement_image) and restored['label'] == 'Profile replacement', restored
        assert next(tool for tool in restarted_tools[0]['catalog'] if tool['id'] == 'restartlanguage')['role'] == 'program'
        page = query(records)
        output = ''.join(item['text'] for item in page['records'])
        assert 'PROFILE_TOOL_REPLACEMENT_OUTPUT' in output and 'SAVED_TOOL_RESTART_OUTPUT' not in output, page
        absent_language = parser_checked_profile('replaced-language-check.the', [], False,
                                                 'REGISTRY_REPLACED_LANGUAGE_STAYS_INACTIVE')
        records = run(f'command macro "{absent_language}"\nlook full compact', replacement_profile)
        assert any('REGISTRY_REPLACED_LANGUAGE_STAYS_INACTIVE' in record.get('last_message', '')
                   for record in records), records

    # Round-trip a near-worst-case escaped page through the actual engine bridge.
    large = ['workbench begin owner public.large']
    for index in range(8):
        large.append('workbench action put public.large%d label "%s" command "%s"' %
                     (index, '""' * 255, '\\' * 2048))
    large += ['workbench commit']
    bridge = profile('bridge.the', large, 0)
    if engine == 'CREXX':
        extraction = ("workbench = .string[]\n"
                      "address the 'extract /workbench actions offset 6 limit 8/' expose workbench[];\n"
                      "if rc <> 0 then return 81\n"
                      "if workbench[0] <> '1' then return 82\n"
                      "if length(workbench[1]) < 30000 then return 83\n"
                      "'input 'workbench[0]' 'length(workbench[1]);\nreturn 0\n")
    else:
        extraction = ("'extract /workbench actions offset 6 limit 8/';\n"
                      "if rc <> 0 then return 81\n"
                      "if workbench.0 <> 1 then return 82\n"
                      "if length(workbench.1) < 30000 then return 83\n"
                      "'input 'workbench.0' 'length(workbench.1);\nreturn 0\n")
    bridge.write_text(bridge.read_text().replace('return 0\n', extraction))
    records = run('command query workbench\ncommand save\nlook full compact', bridge)
    state = query(records)
    assert state['actions'] == 14 and state['lastResult']['code'] == 'ok', state
    assert b'1 ' in sample.read_bytes() and snapshot(records)['buffer']['lines'] == 3, sample.read_bytes()

print(f'Workbench real-editor declarations, transactions, invocation, rollback and {engine} EXTRACT passed')
