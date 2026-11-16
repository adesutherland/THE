#!/usr/bin/env python3
"""Exercise the optional ooRexx macro bridge through the real editor."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()
library = os.environ.get('THE_OOREXX_LIBRARY')
if not library or not Path(library).is_file():
    print('ooRexx library not configured for real macro-engine checks')
    sys.exit(77)


with tempfile.TemporaryDirectory(prefix='THE ooRexx macro ') as temporary:
    root = Path(temporary)
    home = root / 'home'
    home.mkdir()
    sample = root / 'sample.txt'
    sample.write_bytes(b'alpha\nbeta\n')
    (root / 'child.the').write_text("address THE\n'emsg OOREXX_CHILD_OK';\nreturn 0\n",
                                    encoding='utf-8')
    (root / 'parent.the').write_text(
        "address THE\n"
        "'extract /curline/';\n"
        "if curline.3 <> 'alpha' then return 81\n"
        "if batch() <> 0 then return 82\n"
        "object_value = 'alpha'~upper\n"
        "if object_value <> 'ALPHA' then return 87\n"
        "probevar = 'VALUE'\n"
        "'editv put probevar';\n"
        "probevar = ''\n"
        "'editv get probevar';\n"
        "if probevar <> 'VALUE' then return 83\n"
        "queue 'QUEUED_VALUE'\n"
        "if queued() <> 1 then return 84\n"
        "parse pull queued_value\n"
        "if queued_value <> 'QUEUED_VALUE' then return 85\n"
        "'macro child';\n"
        "if rc <> 0 then return 86\n"
        "':1';\n"
        "'replace changed';\n"
        "'emsg OOREXX_PARENT_OK';\n"
        "return 0\n", encoding='utf-8')
    (root / 'panel.the').write_text(
        "address THE\n"
        "'read NOCHANGE TAG';\n"
        "if rc <> 0 then return 91\n"
        "if queued() <> 2 then return 92\n"
        "parse pull key_result\n"
        "if left(key_result, 3) <> 'ETK' then return 94\n"
        "parse pull result\n"
        "if result <> 'RES 2 7 Alice' then return 93\n"
        "'emsg OOREXX_PANEL_OK';\n"
        "return 0\n", encoding='utf-8')
    (root / 'halt.the').write_text(
        "address THE\n"
        "'emsg OOREXX_HALT_FIRST';\n"
        "'emsg OOREXX_HALT_SHOULD_NOT_RUN';\n"
        "return 0\n", encoding='utf-8')
    startup_profile = root / 'startup-read.the'
    startup_profile.write_text(
        "address THE\n"
        "'set prefix off';\n"
        "'set ctlchar ! escape';\n"
        "'set ctlchar p protect white';\n"
        "'set ctlchar u noprotect green';\n"
        "'set reserved 2 !pName: !uold     !p END';\n"
        "'read NOCHANGE TAG';\n"
        "if rc <> 0 then return 95\n"
        "if queued() <> 2 then return 96\n"
        "parse pull key_result\n"
        "if left(key_result, 3) <> 'ETK' then return 97\n"
        "parse pull field_result\n"
        "if field_result <> 'RES 2 7 Alice' then return 98\n"
        "'emsg STARTUP_READ_OK';\n"
        "return 0\n", encoding='utf-8')

    env = dict(os.environ, THE_REXX_INTERPRETER='OOREXX',
               THE_MACRO_PATH=str(root), HOME=str(home), USERPROFILE=str(home),
               TERM='')

    def run(commands, *, no_profile=True, profile=None, **overrides):
        args = [str(binary), '--driver', 'llm']
        if profile is not None:
            args.extend(['-p', str(profile)])
        elif no_profile:
            args.append('-n')
        args.append(str(sample))
        result = subprocess.run(
            args,
            input='\n'.join([*commands, 'quit', '']), text=True,
            encoding='utf-8', capture_output=True, env=dict(env, **overrides),
            timeout=45, check=True)
        records = [json.loads(line) for line in result.stdout.splitlines()
                   if line.startswith('{')]
        return records, result.stderr

    # No -p or -n: ooRexx must select the shared Classic default profile.
    records, errors = run(['capabilities', 'look full compact'], no_profile=False)
    capabilities = next(record for record in records if 'macro_available' in record)
    assert capabilities['macro_interpreter'] == 'OOREXX' and capabilities['macro_available'], (capabilities, errors)
    first_launch = next(record for record in records if 'screen_rows' in record)
    assert any(row.get('footer') == 'keys' and 'F10 Menu' in row.get('t', '')
               for row in first_launch['screen_rows']), (first_launch, errors)

    records, errors = run([
        'capabilities', 'command set eolout lf', 'command :1', 'command macro parent',
        'look filearea compact', 'command save'])
    capabilities = next(record for record in records if 'macro_available' in record)
    assert capabilities['macro_interpreter'] == 'OOREXX' and capabilities['macro_available'], (capabilities, errors)
    assert 'OOREXX_CHILD_OK' in errors and 'OOREXX_PARENT_OK' in errors, (records, errors)
    assert not any(word in errors for word in ('Unable to', 'REXX interpreter returned an error')), errors
    edited = next(record for record in records if 'screen_rows' in record)
    assert edited['buffer']['dirty'] == 1, edited
    assert any(row.get('t') == 'changed' for row in edited['screen_rows']), edited
    assert sample.read_bytes() == b'changed\nbeta\n', sample.read_bytes()

    records, errors = run(['look filearea compact'])
    reopened = next(record for record in records if 'screen_rows' in record)
    assert reopened['buffer']['dirty'] == 0 and sample.read_bytes() == b'changed\nbeta\n', reopened

    # A synonym keeps its compiled in-store image between invocations. The
    # padded numeric return forces ooRexx to allocate a result over 260 bytes.
    instore = "address THE 'input OOREXX_INSTORE'; return '0' || copies(' ', 300)"
    records, errors = run([
        'command set eolout lf', 'command :1',
        'command set synonym instore rexx ' + instore,
        'command instore', 'command instore',
        'look filearea compact', 'command save'])
    acks = [record for record in records if 'rc' in record and record.get('status') != 'bye']
    assert len(acks) == 6 and all(record.get('rc') == 0 for record in acks[3:5]), (records, errors)
    instore_view = next(record for record in records if 'screen_rows' in record)
    assert instore_view['buffer']['dirty'] == 1, instore_view
    assert sample.read_bytes().splitlines().count(b'OOREXX_INSTORE') == 2, (records, errors)
    expected_bytes = sample.read_bytes()
    records, errors = run(['look filearea compact'])
    instore_reopened = next(record for record in records if 'screen_rows' in record)
    assert instore_reopened['buffer']['dirty'] == 0 and sample.read_bytes() == expected_bytes, (records, errors)

    # One addressed command reaches the configured limit. The following
    # addressed command must be halted, and editor input must still resume.
    records, errors = run([
        'command set rexxhalt command 1', 'command macro halt',
        'command set rexxhalt command off', 'command emsg OOREXX_HALT_RECOVERED'])
    assert 'OOREXX_HALT_FIRST' in errors, (records, errors)
    assert 'OOREXX_HALT_SHOULD_NOT_RUN' not in errors, (records, errors)
    assert 'OOREXX_HALT_RECOVERED' in errors, (records, errors)
    recovered = [record for record in records if record.get('last_message') == 'OOREXX_HALT_RECOVERED']
    assert recovered and recovered[-1].get('rc') == 0, (records, errors)
    assert sample.read_bytes() == expected_bytes

    sample.write_bytes(b'changed\nbeta\n')
    expected_bytes = sample.read_bytes()
    records, errors = run(['look filearea compact'])
    panel_baseline = next(record for record in records if 'screen_rows' in record)
    assert panel_baseline['buffer']['dirty'] == 0 and panel_baseline['buffer']['lines'] == 2, (records, errors)

    # READ consumes the next LLM input as a panel action before the macro returns.
    records, errors = run([
        'command set prefix off', 'command set ctlchar ! escape',
        'command set ctlchar p protect white',
        'command set ctlchar u noprotect green',
        'command set reserved 2 !pName: !uold     !p END',
        'command macro panel', 'text Alice', 'key enter',
        'look filearea compact'])
    assert any('panel' in record for record in records), records
    assert 'OOREXX_PANEL_OK' in errors, (records, errors)
    assert sample.read_bytes() == expected_bytes

    # Profile READ begins before the ordinary LLM command loop. Its buffered
    # panel input must not consume the following protocol commands.
    for engine in ('OOREXX', 'REGINA'):
        if engine == 'REGINA':
            regina = os.environ.get('THE_REGINA_LIBRARY')
            if not regina or not Path(regina).is_file():
                continue
        records, errors = run(
            ['text Alice', 'key enter', 'capabilities', 'look filearea compact'],
            profile=startup_profile, THE_REXX_INTERPRETER=engine)
        assert 'STARTUP_READ_OK' in errors, (engine, records, errors)
        caps = next(record for record in records if 'macro_available' in record)
        assert caps['macro_interpreter'] == engine and caps['macro_available'], (engine, records, errors)
        assert any('screen_rows' in record for record in records), (engine, records, errors)
        assert sample.read_bytes() == expected_bytes, (engine, sample.read_bytes())

    missing = root / 'missing-ooRexx-library'
    records, errors = run(['capabilities', 'command macro parent'],
                          THE_OOREXX_LIBRARY=str(missing))
    unavailable = next(record for record in records if 'macro_available' in record)
    assert unavailable['macro_interpreter'] == 'OOREXX' and not unavailable['macro_available'], (unavailable, errors)
    assert 'THE_OOREXX_LIBRARY' in errors and 'ooRexx' in errors, errors
    assert 'OOREXX_PARENT_OK' not in errors and sample.read_bytes() == expected_bytes

    # A cREXX request must never fall back to the available ooRexx library.
    records, errors = run(['capabilities'], THE_REXX_INTERPRETER='CREXX',
                          THE_CREXX_LIBRARY=str(root / 'missing-crexx-library'))
    crexx = next(record for record in records if 'macro_available' in record)
    assert crexx['macro_interpreter'] == 'CREXX' and not crexx['macro_available'], (crexx, errors)
    assert 'OOREXX_PARENT_OK' not in errors

    if os.environ.get('THE_REXX_INTERPRETER', 'CREXX').upper() == 'CREXX':
        records, errors = run(['capabilities'], THE_REXX_INTERPRETER='')
        default = next(record for record in records if 'macro_available' in record)
        assert default['macro_interpreter'] == 'CREXX', (default, errors)

print('Real ooRexx macro bridge: commands, function, variables, queue, panel, nested macro and loader isolation passed')
