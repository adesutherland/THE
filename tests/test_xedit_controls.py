#!/usr/bin/env python3
"""Exercise the shared controls through real macros and normalized UI events."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as tmp:
    work = Path(tmp)
    sample = work / 'sample.txt'
    sample.write_text('alpha\nbeta\ngamma\n')
    (work / 'inspect.the').write_text("address THE\n'extract /mask/flscreen/cursor/'\n"
        "'emsg MASK=['mask.1'] RANGE='flscreen.0 flscreen.1 flscreen.2 'CURSOR='cursor.0 cursor.3 cursor.4 cursor.9\n")
    (work / 'priority.the').write_text("address THE\n"
        "'cursor goto 2 3 priority 70'\n'cursor goto 1 1 priority 30'\n"
        "'cursor goto 3 2 priority 70'\n'macro inspect'\n")
    (work / 'first.the').write_text("address THE\n'cursor file 2 3 priority 70'\n")
    (work / 'second.the').write_text("address THE\n'cursor file 3 1 priority 20'\n")

    def run(commands, engine='REGINA'):
        env = dict(os.environ, THE_REXX_INTERPRETER=engine, THE_MACRO_PATH=str(work), TERM='')
        result = subprocess.run([str(binary), '--driver', 'llm', '-n', str(sample)],
                                input='command set compat xedit xedit xedit\n' + commands + '\nquit\n',
                                text=True, capture_output=True, env=env, timeout=15, check=True)
        records = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
        assert not any(word in result.stderr for word in ('Error ', 'Invalid ', 'Unable to')), result.stderr
        return records, result.stderr

    rows, messages = run('command macro priority\nlook filearea compact')
    assert 'CURSOR=9 2 3 70' in messages, (rows, messages)
    assert rows[-2]['focus']['line'] == 2 and rows[-2]['focus']['cell'] == 2, rows[-2]

    rows, messages = run('command set prefix synonym AA first\ncommand set prefix synonym BB second\n'
                         'command :1\ncommand set pending on AA\ncommand :3\ncommand set pending on BB\n'
                         'command sos doprefix\nlook filearea compact')
    assert rows[-2]['focus']['line'] == 2 and rows[-2]['focus']['cell'] == 2, rows[-2]

    # Unicode mask text is kept whole when insertion replaces half of a wide cell.
    for engine in ('REGINA', 'CREXX'):
        rows, messages = run('command set mask immed 界XYZ\ncommand :1\ncommand input a\n'
                             'look filearea compact\ncommand preserve\ncommand set mask immed other\n'
                             'command restore\ncommand :1\ncommand input\nlook filearea compact', engine)
        snapshots = [r for r in rows if 'screen_rows' in r]
        assert any(line.get('t') == 'a XYZ' for line in snapshots[0]['screen_rows']), snapshots[0]
        # INPUT without operands is input mode; ADD uses the preserved mask.
        rows, messages = run('command set mask immed   . /* */\ncommand preserve\n'
                             'command set mask immed BAD\ncommand restore\ncommand :1\ncommand add 1\n'
                             'look filearea compact', engine)
        assert any(line.get('t') == '  . /* */' for line in rows[-2]['screen_rows']), rows[-2]

    rows, messages = run('command macro inspect\ncommand :2\ncommand set mask immed abc\n'
                         'command replace X\ncommand macro inspect\nlook filearea compact')
    assert 'RANGE=2 1 3' in messages, messages
    assert any(line.get('t') == 'Xbc' for line in rows[-2]['screen_rows']), rows[-2]

    rows, messages = run('command set mask immed abc\ncommand set mask modify\nlook full compact')
    assert rows[-2]['command'] == 'set mask immed abc', rows[-2]
print('Shared MASK/FLSCREEN/cursor controls passed')
