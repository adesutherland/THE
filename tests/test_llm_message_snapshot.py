#!/usr/bin/env python3
"""Check that semantic message events come from THE's editor message owner."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()

with tempfile.TemporaryDirectory(prefix='CRXTHE message snapshot ') as temporary:
    root = Path(temporary)
    sample = root / 'sample.txt'
    sample.write_text('alpha\n', encoding='utf-8')
    commands = [
        'look full compact',
        'command query number',
        'look filearea compact status=0',
        'focus command',
        'delta filearea compact status=0',
        'command query prefix',
        'look full compact',
        'quit',
    ]
    env = dict(os.environ, THE_HOME_DIR=str(binary.parent),
               THE_REXX_INTERPRETER='NONE')
    result = subprocess.run(
        [str(binary), '--driver', 'llm', '-n', str(sample)],
        input='\n'.join(commands) + '\n', cwd=root, env=env,
        text=True, encoding='utf-8', capture_output=True, timeout=30,
        check=True,
    )
    records = [json.loads(line) for line in result.stdout.splitlines()
               if line.startswith('{')]
    snapshots = [record for record in records if 'mode' in record]
    acks = [record for record in records if 'message_changed' in record
            and record.get('status') != 'bye']
    assert len(snapshots) == 4 and len(acks) == 3, records
    before, after_number, after_focus, after_prefix = snapshots
    number_ack, focus_ack, prefix_ack = acks

    assert number_ack['message_changed'] == 1, number_ack
    assert after_number['last_message_serial'] > before['last_message_serial'], snapshots
    assert after_number['last_message'] == number_ack['last_message'], snapshots
    assert after_number['last_message'].lower().startswith('number '), snapshots
    assert 'status' not in after_number, after_number

    assert focus_ack['message_changed'] == 0, focus_ack
    assert after_focus['mode'] == 'delta', after_focus
    assert after_focus['last_message_serial'] == after_number['last_message_serial'], snapshots
    assert after_focus['last_message'] == after_number['last_message'], snapshots
    assert 'status' not in after_focus, after_focus

    assert prefix_ack['message_changed'] == 1, prefix_ack
    assert after_prefix['last_message_serial'] > after_focus['last_message_serial'], snapshots
    assert after_prefix['last_message'] == prefix_ack['last_message'], snapshots
    assert after_prefix['last_message'].lower().startswith('prefix '), snapshots
    assert after_prefix['status'].startswith('focus=command '), after_prefix
    assert 'message=' + after_prefix['last_message'] in after_prefix['status'], after_prefix
    assert sample.read_bytes() == b'alpha\n', sample.read_bytes()

print('LLM snapshot message serial and text follow editor messages')
