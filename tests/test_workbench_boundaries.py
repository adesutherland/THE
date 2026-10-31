#!/usr/bin/env python3
"""Real-editor regressions for typed operands, macro bounds and invocation depth."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
engine = sys.argv[2] if len(sys.argv) > 2 else 'CREXX'
with tempfile.TemporaryDirectory(prefix='THE workbench boundaries ') as temporary:
    root = Path(temporary)
    sample = root / 'sample.txt'
    original = b'alpha\nbeta\n'
    env = dict(os.environ, THE_REXX_INTERPRETER=engine,
               THE_CREXX_CACHE_DIR=str(root / 'cache'), THE_TOOL_CONFIG_DIR=str(root / 'tools'))

    def run(commands):
        sample.write_bytes(original)
        result = subprocess.run([str(binary), '--driver', 'llm', '-n', str(sample)],
                                input='command cursor goto 2 3\nlook full compact\n' + commands +
                                      '\nlook full compact\nquit\n',
                                cwd=root, text=True, encoding='utf-8', capture_output=True,
                                env=env, timeout=30)
        assert result.returncode == 0, (result.returncode, result.stderr)
        records = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
        assert records, result.stdout
        return records

    def snapshots(records):
        return [record for record in records if 'screen_rows' in record]

    def state(records):
        return json.loads(next(record['last_message'] for record in reversed(records)
                               if record.get('last_message', '').startswith('{"schemaVersion":1')))

    def unchanged(records):
        before, after = snapshots(records)[0], snapshots(records)[-1]
        assert before['buffer'] == after['buffer'], (before['buffer'], after['buffer'])
        for key in ['zone', 'line', 'cell']:
            assert before['focus'][key] == after['focus'][key], (before['focus'], after['focus'])
        assert sample.read_bytes() == original and not after['buffer']['dirty'], after

    # The typed file path contains two active LINEND bytes around a real
    # editor command. It must open that exact file and never split the path.
    target = root / 'chosen#input SENTINEL#trailer.txt'
    target.write_bytes(b'target bytes\n')
    records = run('command set linend on #\ncommand workbench begin owner public.boundary\n'
                  f'command workbench action put public.open label "Open" builtin file.open argument "{target}"\n'
                  f'command workbench action put public.back label "Return" builtin buffer.switch argument "{sample}"\n'
                  'command workbench commit\ncommand workbench invoke public.open\nlook full compact\n'
                  'command workbench invoke public.back')
    opened = snapshots(records)[1]
    assert Path(opened['buffer']['path']).resolve() == target.resolve(), opened
    assert not opened['buffer']['dirty'] and opened['focus']['zone'] == 'command' and opened['focus']['cell'] == 0, opened
    assert [row['t'] for row in opened['screen_rows'] if row['role'] == 'file'] == ['target bytes'], opened
    assert all(record.get('rc', 0) == 0 for record in records), records
    assert target.read_bytes() == b'target bytes\n'
    unchanged(records)

    # The scoped single-command dispatch must also restore ordinary LINEND
    # parsing after a typed action returns.
    records = run('command set linend on #\ncommand workbench invoke the.file.save\n'
                  'command input FIRST#input SECOND\ncommand save')
    assert sample.read_bytes() == b'alpha\nbeta\nFIRST\nSECOND\n', sample.read_bytes()
    assert not snapshots(records)[-1]['buffer']['dirty']

    # The same shared macro resolver admits neither oversized input nor
    # directory/suffix/resolved-path growth beyond its 1000-byte buffers.
    macro_cases = [
        ('operand', '', 'x' * 1001),
        ('search-path', 'command set macropath /' + 'd' * 998 + '\n', 'probe'),
        ('resolved-path', '', './' + 'x' * (1000 - len(str(root)) + 12)),
        ('equals-expansion', '', './=.' + 'x' * 996),
    ]
    if os.name != 'nt':
        import pwd
        account = pwd.getpwuid(os.getuid())
        operand = '~' + account.pw_name + '/' + 'x' * (1000 - len(account.pw_dir))
        if len(operand) <= 1000:
            macro_cases.append(('user-home-expansion', '', operand))
    for label, prefix, operand in macro_cases:
        records = run(prefix + 'command workbench begin owner public.boundary\n'
                      f'command workbench action put public.macro label "Macro" macro "{operand}"\n'
                      'command workbench commit\ncommand workbench invoke public.macro\ncommand query workbench')
        result = state(records)['lastResult']
        assert result['code'] == 'command-failed' and result['rc'] == 5, (label, result)
        unchanged(records)
    # A valid equivalence expansion keeps its existing macro-path meaning.
    equivalent = root / 'sample.the'
    equivalent.write_text(('options levelb\n' if engine == 'CREXX' else '/* Rexx */\n') + 'return 7\n')
    records = run('command workbench begin owner public.boundary\n'
                  'command workbench action put public.macro label "Equivalent" macro "./=.the"\n'
                  'command workbench commit\ncommand workbench invoke public.macro\ncommand query workbench')
    assert state(records)['lastResult']['rc'] == 7, state(records)
    unchanged(records)

    records = run('command macro "' + 'x' * 1001 + '"')
    assert any(record.get('rc') == 5 for record in records), records
    unchanged(records)

    # Reject self-reference and a multi-action cycle before recursive dispatch
    # can exhaust the editor stack. A subsequent ordinary invoke still works.
    cycles = [
        [('public.a', 'workbench invoke public.a')],
        [('public.a', 'workbench invoke public.b'), ('public.b', 'workbench invoke public.a')],
    ]
    for actions in cycles:
        definitions = '\n'.join(f'command workbench action put {identifier} label "Cycle" command "{command}"'
                                for identifier, command in actions)
        records = run('command workbench begin owner public.boundary\n' + definitions +
                      '\ncommand workbench commit\ncommand workbench invoke public.a\ncommand query workbench\n'
                      'look full compact\ncommand workbench invoke the.file.save')
        result = state(records)['lastResult']
        assert result['code'] == 'object-in-use' and result['rc'] == -3, result
        assert snapshots(records)[-1]['workbench']['lastResult']['code'] == 'ok', records
        unchanged(records)

    for count in [8, 9]:
        definitions = []
        for index in range(count):
            command = (f'workbench invoke public.depth{index + 1}' if index + 1 < count
                       else 'query cursor' if count == 8 else 'input DEPTH_SENTINEL')
            definitions.append(f'command workbench action put public.depth{index} label "Depth" command "{command}"')
        records = run('command workbench begin owner public.boundary\n' + '\n'.join(definitions) +
                      '\ncommand workbench commit\ncommand workbench invoke public.depth0\ncommand query workbench')
        result = state(records)['lastResult']
        expected = ('ok', 0) if count == 8 else ('limit-exceeded', 94)
        assert (result['code'], result['rc']) == expected, result
        unchanged(records)

print(f'Workbench shared typed-action LINEND, macro bounds and invocation admission passed with {engine}')
