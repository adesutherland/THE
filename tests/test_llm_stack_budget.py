#!/usr/bin/env python3
"""Exercise real snapshot/editing paths with a bounded native process stack."""
import json
import os
from pathlib import Path
import resource
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()


def limit_stack():
    _, hard = resource.getrlimit(resource.RLIMIT_STACK)
    limit = 4 * 1024 * 1024
    if hard != resource.RLIM_INFINITY:
        limit = min(limit, hard)
    resource.setrlimit(resource.RLIMIT_STACK, (limit, hard))


with tempfile.TemporaryDirectory(prefix='CRXTHE stack budget ') as temporary:
    root = Path(temporary)
    sample = root/'sample.txt'
    sample.write_text('seed\ntail\n', encoding='utf-8')
    env = dict(os.environ, THE_REXX_INTERPRETER='NONE', HOME=str(root))
    commands = ('look full compact\ncommand set insertmode on\n'
                'command cursor goto 2 3\nlook filearea compact\n'
                'text Z\ndelta filearea compact\ncommand save\n'
                'look full compact\nquit\n')
    result = subprocess.run([str(binary), '--driver', 'llm', '-n', str(sample)],
                            input=commands, text=True, encoding='utf-8',
                            capture_output=True, env=env, preexec_fn=limit_stack,
                            timeout=30)
    assert result.returncode == 0, (result.returncode, result.stderr, result.stdout[-1000:])
    records = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
    assert any('screen_rows' in record for record in records), records
    assert any(record.get('focus', {}).get('line') == 2 for record in records), records
    assert sample.read_bytes() == b'seed\ntaZil\n', sample.read_bytes()
    assert not any(word in result.stderr for word in ('Error ', 'Unable to')), result.stderr
print('Snapshots, delta, cursor, editing and save pass with a 4 MiB native stack')
