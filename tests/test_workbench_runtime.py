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
