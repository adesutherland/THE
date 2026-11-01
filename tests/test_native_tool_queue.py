#!/usr/bin/env python3
"""Queued source switches/close reach the real editor's bound invocation."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary, module = (str(Path(value).resolve()) for value in sys.argv[1:3])
with tempfile.TemporaryDirectory(prefix='THE native queue ') as temporary:
    root = Path(temporary)
    source, other = root / 'source.txt', root / 'other.txt'
    source.write_bytes(b'original source\n')
    other.write_bytes(b'different current source\n')
    env = dict(os.environ, THE_REXX_INTERPRETER='NONE', THE_DRIVER_PATH=module,
               THE_NATIVE_QUEUE_OTHER=str(other), THE_TOOL_ROOT=str(root / 'missing-helper'),
               THE_TOOL_CONFIG_DIR=str(root / 'config'))
    result = subprocess.run([binary, '--driver', 'curses', '-n', str(source)],
                            cwd=root, env=env, stdin=subprocess.DEVNULL,
                            capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, (result.stdout, result.stderr)
    if 'NATIVE_QUEUE_UNQUALIFIED_HOST' in result.stdout:
        raise SystemExit(77)
    assert 'NATIVE_TOOL_QUEUED_SWITCH_AND_CLOSED_SOURCE_REFUSED_BEFORE_CAPTURE_PASS' in result.stdout, (result.stdout, result.stderr)
    assert source.read_bytes() == b'original source\n'
    assert other.read_bytes() == b'different current source\n'
    print(result.stdout.strip())
