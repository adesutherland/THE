#!/usr/bin/env python3
"""Exercise supported command aliases through the real native editor."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
engine = sys.argv[2] if len(sys.argv) > 2 else os.environ.get('THE_REXX_INTERPRETER', 'NONE')

with tempfile.TemporaryDirectory(prefix='CRXTHE synonym ') as temporary:
    root = Path(temporary)
    target = root / 'sample.txt'
    original = b'alpha\nbeta\ngamma\n'
    target.write_bytes(original)
    env = dict(os.environ, THE_REXX_INTERPRETER=engine,
               THE_HOME_DIR=str(binary.parent / 'release'),
               THE_CREXX_CACHE_DIR=str(root / 'cache'))

    def run(commands):
        result = subprocess.run(
            [str(binary), '--driver', 'llm', '-n', str(target)],
            input='\n'.join([*commands, 'quit', '']), cwd=root, env=env,
            text=True, encoding='utf-8', capture_output=True, timeout=45, check=True)
        records = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
        acks = [record for record in records if 'rc' in record and record.get('status') != 'bye']
        assert len(acks) == sum(command.startswith('command ') for command in commands), result.stdout
        return acks, records

    commands = [
        'query synonym', 'set synonym ender 1 bottom', 'set synonym energy 1 top',
        'e', 'query synonym e', 'query synonym end', 'enderx',
        'set synonym end 1 bottom', 'end',
        'set synonym ender', 'query synonym ender', 'set synonym absent',
        'set synonym broken 0 bottom', 'set synonym broken 99 bottom',
        'set synonym broken 999999999999999999999999999999999999 bottom',
        'set synonym badname1 bottom', 'set synonym command bottom',
        'set synonym end notacommand', 'query synonym end',
        'set synonym bottom set number off', 'bottom', 'query number',
        'command bottom', 'set synonym bottom',
        'set synonym off', 'end', 'set synonym on', 'end',
    ]
    acks, _ = run(['command ' + command for command in commands])
    assert acks[0]['last_message'].strip().lower() == 'synonym on'
    assert acks[3]['rc'] != 0 and 'Ambiguous' in acks[3]['last_message'], acks[3]
    assert 'Ambiguous' in acks[4]['last_message'], acks[4]
    assert acks[5]['last_message'] == 'synonym ender 1 bottom', acks[5]
    assert acks[6]['rc'] == -1, acks[6]
    assert acks[8]['rc'] == 0, acks[8]  # exact END wins over ENDER and ENERGY
    assert acks[10]['last_message'] == 'synonym ender 5 ender', acks[10]
    for index in range(12, 18):
        assert acks[index]['rc'] != 0, (commands[index], acks[index])
    assert acks[18]['last_message'] == 'synonym end 1 bottom', acks[18]
    assert acks[21]['last_message'].strip().lower() == 'number off', acks[21]
    assert acks[22]['rc'] == 0, acks[22]  # COMMAND bypasses a builtin shadow
    assert acks[25]['rc'] == -1 and acks[27]['rc'] == 0, acks[25:]
    assert target.read_bytes() == original

    # Chained aliases stop on failure, publish the real last RC, and survive
    # removal/redefinition during their own execution.
    commands = [
        'set synonym linend # fail set number invalid#input UNEXPECTED',
        'fail', 'set linend on #', 'fail#input ALSO_UNEXPECTED', 'set linend off',
        'set synonym linend # vanish set synonym vanish#input survived',
        'vanish', 'query synonym vanish', 'set synonym gone set synonym gone bottom',
        'gone', 'query synonym gone',
    ]
    acks, records = run(['command ' + command for command in commands] + ['look filearea compact'])
    assert acks[1]['rc'] != 0 and acks[1]['lastrc'] == acks[1]['rc'], acks[1]
    assert acks[3]['rc'] == acks[1]['rc'] and acks[3]['buffer']['lines'] == 3, acks[3]
    assert acks[6]['rc'] == 0 and acks[6]['buffer']['lines'] == 4, acks[6]
    assert acks[7]['last_message'] == 'synonym vanish 6 vanish', acks[7]
    assert acks[10]['last_message'] == 'synonym gone 4 bottom', acks[10]
    assert target.read_bytes() == original

    # A view's LINEND setting is retained in query/replay; chains appear once
    # and the public definition can be fed straight back to SET SYNONYM.
    commands = [
        'set linend on #', 'set synonym chain 2 set number off#set prefix off',
        'set linend off', 'query synonym chain', 'chain', 'query number', 'query prefix',
    ]
    acks, _ = run(['command ' + command for command in commands])
    definition = acks[3]['last_message'].removeprefix('synonym ')
    assert definition == 'LINEND # chain 2 set number off#set prefix off', definition
    assert acks[5]['last_message'].strip().lower() == 'number off', acks[5]
    assert acks[6]['last_message'].strip().lower() == 'prefix off', acks[6]
    acks, _ = run(['command set synonym ' + definition, 'command chain', 'command query prefix'])
    assert all(ack['rc'] == 0 for ack in acks), acks
    assert acks[-1]['last_message'].strip().lower() == 'prefix off'

    # Fixed definition operands precede invocation operands. Verify the edit,
    # logical cursor, dirty tracking, saved bytes and a clean reopen.
    acks, records = run(['command set eolout lf', 'command cursor goto 1 1',
                         'command set synonym put 2 input fixed', 'command pu tail',
                         'look filearea compact', 'command save'])
    edited = next(record for record in records if 'screen_rows' in record)
    assert [row['t'] for row in edited['screen_rows'] if row['role'] == 'file'] == [
        'alpha', 'fixed tail', 'beta', 'gamma'], edited
    assert edited['buffer']['dirty'] == 1 and edited['focus']['line'] == 2, edited
    assert target.read_bytes() == b'alpha\nfixed tail\nbeta\ngamma\n'
    _, records = run(['look filearea compact'])
    reopened = next(record for record in records if 'screen_rows' in record)
    assert reopened['buffer']['dirty'] == 0 and reopened['buffer']['lines'] == 4, reopened

    if engine != 'NONE':
        # Test EXTRACT's grouping, complete definition and unknown-name identity.
        if engine == 'CREXX':
            source = '''options levelb
import rxfnsb
synonym = .string[]
address the 'extract /synonym */' expose synonym[]
if synonym[0] <> '1' then return 81
if synonym[1] <> 'LINEND # chain 2 set number off#set prefix off' then return 82
address the 'extract /synonym chain/' expose synonym[]
if synonym[0] <> '4' then return 83
if synonym[3] <> 'set number off#set prefix off' then return 84
if synonym[4] <> '#' then return 85
address the 'extract /synonym missing/' expose synonym[]
if synonym[1] <> 'missing' then return 86
if synonym[3] <> 'missing' then return 87
if synonym[4] <> '' then return 88
address the 'msg SYNONYM_EXTRACT_OK'
return 0
'''
        else:
            source = '''/* alias extraction probe */
address the 'extract /synonym */'
if synonym.0 <> 1 then return 81
if synonym.1 <> 'LINEND # chain 2 set number off#set prefix off' then return 82
address the 'extract /synonym chain/'
if synonym.0 <> 4 then return 83
if synonym.3 <> 'set number off#set prefix off' then return 84
if synonym.4 <> '#' then return 85
address the 'extract /synonym missing/'
if synonym.1 <> 'missing' then return 86
if synonym.3 <> 'missing' then return 87
if synonym.4 <> '' then return 88
address the 'msg SYNONYM_EXTRACT_OK'
return 0
'''
        probe = root / 'probe.the'
        probe.write_text(source)
        wrapper = root / 'wrapper.the'
        wrapper.write_text(('options levelb\n' if engine == 'CREXX' else '/* wrapper */\n') + 'return 11\n')
        inline = 'options levelb; return 7' if engine == 'CREXX' else 'return 7'
        acks, _ = run(['command set synonym ' + definition, 'command macro "' + str(probe) + '"',
                        'command set synonym runmacro macro "' + str(wrapper) + '"', 'command runmacro',
                        'command set synonym inline rexx ' + inline, 'command inline',
                        'command set synonym missingmacro macro absent-macro-file.the',
                        'command missingmacro'])
        assert acks[1]['rc'] == 0 and acks[1]['last_message'] == 'SYNONYM_EXTRACT_OK', acks[1]
        assert acks[3]['rc'] == 11 and acks[3]['lastrc'] == 11, acks[3]
        assert acks[5]['rc'] == 7 and acks[5]['lastrc'] == 7, acks[5]
        assert acks[7]['rc'] != 0 and acks[7]['lastrc'] == acks[7]['rc'], acks[7]

print(engine + ': synonym resolution, removal, chains, query, RC and saved/reopened edits passed')
