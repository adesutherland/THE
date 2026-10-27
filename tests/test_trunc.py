#!/usr/bin/env python3
"""Check actual inserted/replaced/edited text and macro TRUNC state."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix='THE trunc ') as tmp:
    work = Path(tmp)
    sample = work / 'sample.txt'
    sample.write_text('original-long-line\ntail\n', encoding='utf-8')
    (work / 'inspect.the').write_text(
        "address THE\n'extract /trunc/curline/'\n"
        "'emsg TRUNC='trunc.1 'TEXT=['curline.3']'\n", encoding='utf-8')

    def run(commands):
        env = dict(os.environ, THE_REXX_INTERPRETER='REGINA',
                   THE_MACRO_PATH=str(work), TERM='')
        p = subprocess.run([binary, '--driver', 'llm', '-n', str(sample)],
            input='command set compat xedit xedit xedit\n' + commands + '\nquit\n',
            text=True, encoding='utf-8', capture_output=True, check=True, env=env, timeout=15)
        return [json.loads(s) for s in p.stdout.splitlines() if s.startswith('{')], p.stderr

    def texts(records):
        return [r['t'] for s in records if 'screen_rows' in s
                for r in s['screen_rows'] if r.get('kind') == 'file' or r.get('line', 0) > 0]

    # Changing a limit does not itself destroy existing records. CURLINE is
    # limited, while the on-screen file content remains intact until edited.
    records, errors = run('command :1\ncommand set trunc 5\ncommand macro inspect\n'
                          'look filearea compact\ncommand preserve\ncommand set trunc 3\n'
                          'command restore\ncommand macro inspect')
    assert errors.count('TRUNC=5 TEXT=[origi]') == 2, (records, errors)
    assert any(r.get('t') == 'original-long-line' for s in records
               for r in s.get('screen_rows', [])), records

    for command in ('input abcdefgh', 'replace abcdefgh'):
        records, errors = run('command :1\ncommand set trunc 5\ncommand ' + command +
                              '\nlook filearea compact')
        assert any(r.get('t') == 'abcde' for s in records for r in s.get('screen_rows', [])), (records, errors)
        assert 'not yet implemented' not in errors and 'Error ' not in errors, errors
        assert 'Truncated at SET TRUNC column' in errors, errors
        assert any(r.get('rc') == 3 for r in records), records

    records, errors = run('command :1\ncommand replace abc\ncommand set trunc 5\n'
                          'command clocate :2\ncommand cinsert XYZ123\nlook filearea compact')
    assert any(r.get('t') == 'aXYZ1' for s in records for r in s.get('screen_rows', [])), (records, errors)

    # The limit is measured in display cells and must never split UTF-8 text.
    for limit, expected in ((1, ''), (2, '界'), (3, '界a')):
        records, errors = run(f'command :1\ncommand set trunc {limit}\ncommand input 界abc\nlook filearea compact')
        assert any(r.get('t') == expected for s in records for r in s.get('screen_rows', [])), (limit, records, errors)

    records, errors = run('command set trunc 3\ncommand set trunc *\ncommand :1\n'
                          'command input abcdefgh\nlook filearea compact')
    assert any(r.get('t') == 'abcdefgh' for s in records for r in s.get('screen_rows', [])), (records, errors)

    for invalid in ('0', '-1', 'abc', '5 6', '99999999'):
        records, errors = run('command set trunc 7\ncommand set trunc ' + invalid + '\ncommand :1\ncommand macro inspect')
        assert 'Invalid operand' in errors and 'TRUNC=7' in errors, (invalid, records, errors)
print('TRUNC query, preserve/restore, editing and UTF-8 limits passed')
