#!/usr/bin/env python3
"""Drive synchronous macro READ through real shared editor fields, both engines."""
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading

binary = str(Path(sys.argv[1]).resolve())
engines = sys.argv[2:] or ['REGINA', 'CREXX']


def run(engine, options, actions, seed='seed\ntail\n', before='', occupied=False):
    with tempfile.TemporaryDirectory(prefix='THE panel workflow ') as tmp:
        root = Path(tmp)
        sample = root / 'sample.txt'
        sample.write_text(seed, encoding='utf-8')
        macro = root / 'read-panel.the'
        if engine == 'REGINA':
            macro.write_text("address THE\n" + ("queue 'existing'\n" if occupied else '') +
                             "'read " + options + "'\nresult_rc=rc\n'set linend off'\n" +
                             "if result_rc=0 then do\n do while queued()>0\n parse pull value\n 'emsg PANEL_RESULT' value\n end\nend\n" +
                             "'emsg PANEL_RC' result_rc 'QUEUED' queued()\nexit 0\n")
        else:
            macro.write_text("options levelg\nnamespace panel_test\nimport rxfnsb\nmain: procedure\n" +
                             "  pool = .standardaddresssandbox()\n  address the 'read " + options + "' sandbox pool;\n  result_rc=rc\n  address the 'set linend off';\n" +
                             "  if result_rc=0 then do\n    count=pool.get('read.0') as .int\n    do i=count to 1 by -1\n      value=pool.get('read.' || i)\n      push value\n    end\n    do while queued()>0\n      parse pull value\n      address the 'emsg PANEL_RESULT ' || value;\n    end\n  end\n" +
                             "  address the 'emsg PANEL_RC ' || result_rc || ' QUEUED ' || queued();\n  exit 0\n")
        env = dict(os.environ, THE_REXX_INTERPRETER=engine, TERM='')
        proc = subprocess.Popen([binary, '--driver', 'llm', '-n', str(sample)],
                                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                text=True, encoding='utf-8', env=env)
        output = []
        incoming = queue.Queue()

        def collect():
            for line in proc.stdout:
                output.append(line)
                if line.startswith('{'):
                    incoming.put(json.loads(line))
            incoming.put(None)

        reader = threading.Thread(target=collect, daemon=True)
        reader.start()
        proc.stdin.write('command set eolout lf\ncommand set case mixed respect\ncommand set prefix off\n'
                         'command set curline off\ncommand set ctlchar ! escape\n'
                         'command set ctlchar p protect white\ncommand set ctlchar u noprotect green\n'
                         'command set reserved 2 !pName: !uold     !p END\n'
                         'command set reserved 4 !pTopic: !uTHE     !p END\n' + before +
                         '\ncommand macro "' + str(macro).replace('\\', '/') + '"\n')
        proc.stdin.flush()
        panel = None
        try:
            while True:
                record = incoming.get(timeout=30)
                assert record is not None, ''.join(output)
                if 'panel' in record:
                    panel = record['panel']
                    break
                if occupied and record.get('last_message', '').startswith('PANEL_RC'):
                    break
            if panel:
                proc.stdin.write(actions(panel) + '\n')
            proc.stdin.write('command set reserved * off\ncommand set curline on M\ncommand set readonly off file\ncommand save\nlook filearea compact\nquit\n')
            proc.stdin.close()
            proc.wait(timeout=30)
            reader.join(timeout=5)
            errors = proc.stderr.read()
            assert proc.returncode == 0, errors
            assert 'Error ' not in errors and 'Unable to' not in errors, errors
            results = [line.split('PANEL_RESULT ', 1)[1].rstrip() for line in errors.splitlines() if 'PANEL_RESULT ' in line]
            records = [json.loads(line) for line in output if line.startswith('{')]
            return sample.read_text(encoding='utf-8'), results, errors, records
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait()


def field(panel, kind, line=0):
    return next(f for f in panel['fields'] if f['kind'] == kind and f['editable'] and (not line or f['line'] == line))


def hit(f):
    return f"hit {f['row']} {f['cell']}\nkey home\n"


for engine in engines:
    _, _, _, colour_records = run(engine, 'NOCHANGE TAG', lambda p: 'key esc',
        before='command set ctlchar u noprotect #BDE9D9 on #243B36')
    colour_panel = next(r['panel'] for r in colour_records if r.get('panel'))
    assert field(colour_panel, 'R')['colour'] == {
        'foreground': '#BDE9D9', 'background': '#243B36', 'style': 0}, colour_panel
    _, _, _, no_command_records = run(engine, 'NOCHANGE TAG', lambda p: 'key esc',
        before='command set cmdline off')
    no_command_panel = next(r['panel'] for r in no_command_records if r.get('panel'))
    assert not any(f['kind'] == 'C' for f in no_command_panel['fields']), no_command_panel
    text, values, errors, records = run(engine, 'NOCHANGE TAG', lambda p: 'text Alice\nkey tab\ntext Rexx\nkey f3')
    assert text == 'seed\ntail\n', text
    assert values[0].startswith('PFK 3'), values
    assert values[1:] == ['RES 2 7 Alice', 'RES 4 8 Rexx'], values
    assert 'PANEL_RC 0 QUEUED 0' in errors, errors
    assert not any(r.get('buffer', {}).get('dirty') for r in records if 'panel' in r), records[-1]

    for mode in ('ALL', 'NOCHANGE'):
        text, values, errors, _ = run(engine, mode + ' NUMBER TAG', lambda p: hit(field(p, 'F', 1)) + 'text NEW\nkey enter')
        assert text == ('NEWd\ntail\n' if mode == 'ALL' else 'seed\ntail\n'), (mode, text, values)
        assert values[-1].endswith('1 NEWd') and values[-1].startswith('FIL '), values

    text, values, errors, _ = run(engine, 'ALL TAG', lambda p: hit(field(p, 'F', 1)) + 'text NEW\nkey esc')
    assert text == 'seed\ntail\n' and not values and 'PANEL_RC 0 ' not in errors, (text, values, errors)

    text, values, _, _ = run(engine, 'NOCHANGE TAG', lambda p: 'text changed\nkey clear\nkey pa2')
    assert len(values) == 1 and values[0].startswith('PAK 2'), values
    assert text == 'seed\ntail\n'

    # Edits cannot cross the eight-cell boundary into a protected label.
    _, values, _, records = run(engine, 'NOCHANGE TAG', lambda p: 'text 123456789012\nkey left\nkey delete\nkey enter')
    assert values[-1] == 'RES 2 7 1234567', values
    panels = [r['panel'] for r in records if 'panel' in r]
    assert all(any(f['text'] == ' END' and not f['editable'] for f in p['fields']) for p in panels)

    _, values, _, _ = run(engine, 'NOCHANGE TAG', lambda p: 'text 界é\nkey backspace\ntext A\nkey enter')
    assert values[-1] == 'RES 2 7 界A', values

    # A READ command does not dispatch arbitrary editor or OS actions midway.
    text, values, _, _ = run(engine, 'NOCHANGE TAG', lambda p: 'command delete all\ntext safe\nkey enter')
    assert text == 'seed\ntail\n' and values[-1] == 'RES 2 7 safe', (text, values)

    text, values, _, _ = run(engine, 'CMDLINE TAG', lambda p: 'text hello\nkey enter')
    assert text == 'seed\ntail\n' and values[-1] == 'CMD hello', (text, values)
    _, values, _, _ = run(engine, 'NOCHANGE NOTAG', lambda p: 'text Alice\nkey enter')
    assert values[-1] == 'Alice', values

    text, _, errors, _ = run(engine, 'ALL TAG', lambda p: hit(field(p, 'F', 1)) + 'text NEW\n' +
                            hit(field(p, 'F', 2)) + 'text too-long\nkey enter', before='command set trunc 6\n')
    assert text == 'seed\ntail\n' and 'PANEL_RC 5 QUEUED 0' in errors, (text, errors)

    text, values, _, records = run(engine, 'ALL NUMBER TAG', lambda p: hit(field(p, 'P', 1)) + 'text D\nkey enter',
                                  before='command set prefix on left 6 0\n')
    assert text == 'seed\ntail\n' and values[-1].endswith('1 D') and values[-1].startswith('PRF '), (text, values)
    assert any(r.get('pending_prefix', {}).get('count', 0) == 1 for r in records), records[-1]

    text, values, _, records = run(engine, 'ALL NUMBER TAG', lambda p: hit(field(p, 'P', 1)) + 'text X#TOP\nkey enter',
                                  before='command set prefix on left 6 0\ncommand set linend on #\n')
    assert text == 'seed\ntail\n' and values[-1].endswith('X#TOP'), (text, values)
    assert any(c['original'] == 'X#TOP' for r in records for c in r.get('pending_prefix', {}).get('commands', []))

    text, values, _, records = run(engine, 'ALL NUMBER TAG', lambda p: hit(field(p, 'P', 1)) + 'key eraseeof\nkey enter',
                                  before='command set prefix on left 6 0\ncommand :1\ncommand set pending on D\n')
    assert text == 'seed\ntail\n' and values[-1].startswith('PRF ') and values[-1].endswith('1'), (text, values)
    assert [r['pending_prefix']['count'] for r in records if 'pending_prefix' in r][-1] == 0

    text, _, _, records = run(engine, 'ALL TAG', lambda p: 'text Alice\nkey enter', before='command set readonly on file\n')
    assert text == 'seed\ntail\n'
    assert not any(f['editable'] for r in records if 'panel' in r for f in r['panel']['fields'] if f['kind'] == 'F')

    # File CTLCHAR markup retains protected text and the raw attribute bytes.
    text, values, _, _ = run(engine, 'ALL TAG', lambda p: hit(field(p, 'F', 1)) + 'text NEW\nkey enter',
                            seed='!pLabel: !uold     !p END\ntail\n', before='command set filectlchar on\n')
    assert text == '!pLabel: !uNEW     !p END\ntail\n', text

    # A full form can reserve every row, including the usual current-line row.
    full = '\n'.join('command set reserved ' + str(n) + ' !pForm row' for n in range(1, 22))
    _, values, _, records = run(engine, 'NOCHANGE TAG', lambda p: 'key enter', before=full)
    assert not any(f['kind'] == 'F' for f in next(r['panel'] for r in records if 'panel' in r)['fields'])
    assert len(values) == 1, values

    if engine == 'REGINA':
        _, values, errors, _ = run(engine, 'ALL TAG', lambda p: 'key enter', occupied=True)
        assert not values and 'PANEL_RC 8 QUEUED 1' in errors, errors
    print(engine + ': panel fields, key stack, ALL/NOCHANGE, cancel, Unicode, protection and full forms passed')
