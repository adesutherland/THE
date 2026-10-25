#!/usr/bin/env python3
"""Exercise the patched, hash-identified original macro packages."""
import json
from pathlib import Path
import sys
from macro_harness import MacroHarness, interpreter_errors

engine = sys.argv[3] if len(sys.argv) > 3 else 'REGINA'
harness = MacroHarness(sys.argv[1], sys.argv[2], engine, sys.argv[4] if len(sys.argv) > 4 else None)
results = []
counts = {
    'pfxrexx': {'DO': 5, 'IF': 8, 'OTHE': 5, 'OTHER': 5, 'RI': 3, 'RIC': 3,
                'SEL': 10, 'SELE': 10, 'SELEC': 10, 'WHE': 5, 'WHEN': 5},
    'pfxhlasm': {'ASML': 4, 'ASMLE': 4, 'CASE': 4, 'CASEE': 6, 'CASEN': 6,
                 'DO': 5, 'DOEX': 4, 'DOEXI': 4, 'ELSE': 4, 'ELSEI': 4,
                 'ENDIF': 3, 'EXITI': 6, 'IF': 9, 'ITERA': 3, 'ORELS': 4,
                 'OTHER': 4, 'OTHRW': 4, 'RI': 3, 'RIC': 3, 'SEA': 11,
                 'SEAR': 11, 'SEARC': 11, 'SEL': 8, 'SELE': 8, 'SELEC': 8,
                 'STRTS': 11, 'WHEN': 4},
}


def check(package, prefix, **kwargs):
    result = harness.run(package, prefix, **kwargs)
    assert not interpreter_errors(result), (package, prefix, result['errors'])
    assert not result['errors'], (package, prefix, result['errors'])
    assert result['text'][0] == kwargs.get('seed', 'seed') or prefix.startswith(('CU', 'CL', 'UC', 'LC', 'STA', 'UNST')), (package, prefix, result['text'])
    assert result['text'][-1] == 'tail' or prefix.startswith(('CU', 'CL', 'UC', 'LC', 'STA', 'UNST')), (package, prefix, result['text'])
    snap = result['snapshots'][-1]
    assert 0 <= snap['focus']['line'] <= snap['buffer']['lines'] + 1, (package, prefix, snap)
    results.append({'package': package, 'prefix': prefix, 'text': result['text'],
                    'focus': snap['focus'],
                    'pending': next(r['pending_prefix'] for r in reversed(result['records']) if 'pending_prefix' in r)})
    return result


for package in ('pfxrexx', 'pfxhlasm'):
    for alias in harness.aliases(package):
        result = check(package, alias)
        assert len(result['text']) == counts[package].get(alias, 2), (package, alias, result['text'])

    # No untouched continuation grows indefinitely. Repeated batches clean up
    # every placeholder and leave the original records intact.
    result = check(package, 'RI3', passes=4)
    assert result['text'] == ['seed', 'tail'], (package, result)
    result = check(package, 'DO', passes=4)
    assert len(result['text']) <= 5 and not any(row.strip().startswith('.') for row in result['text']), (package, result)

    for count in (2, 99):
        result = check(package, 'DO' + str(count))
        assert len(result['text']) == 2 + 3 * count, (package, count, result['text'])

    # Invalid input must display a pending error, retain the user's text and
    # avoid executing uninitialized cursor targets or OS commands.
    for count in (0, 100):
        result = harness.run(package, 'DO' + str(count))
        assert not interpreter_errors(result), (package, count, result['errors'])
        assert 'Expected count from 1 to 99' in result['errors'], result
        assert result['text'] == ['seed', 'tail'], result
        assert any(row.get('p', '').startswith('?') for snap in result['snapshots']
                   for row in snap['screen_rows']), result
        results.append({'package': package, 'prefix': 'DO' + str(count),
                        'text': result['text'], 'focus': result['snapshots'][-1]['focus'],
                        'pending': next(r['pending_prefix'] for r in reversed(result['records']) if 'pending_prefix' in r),
                        'error_prefix': next(row['p'] for snap in result['snapshots']
                                             for row in snap['screen_rows'] if row.get('p', '').startswith('?'))})

    upper, lower = ('CU', 'CL') if package == 'pfxrexx' else ('UC', 'LC')
    for alias, seed, expected in ((upper, 'aBcD', 'ABCD'), (lower, 'aBcD', 'abcd'),
                                  (upper + ' C', 'aBcD', 'ABcD'),
                                  (upper + '2 3', 'aBcD', 'ABCD')):
        result = check(package, alias, seed=seed)
        assert result['text'][0] == expected, (package, alias, result['text'])

# Real normalized text entry followed by Enter must retain user text while
# removing only untouched continuation lines.
result = check('pfxrexx', 'DO', commands="command cursor goto 3 4\ntext say 'hello'\nkey enter\nkey enter\nkey enter")
assert any("say 'hello'" in row for row in result['text']), result
assert len(result['text']) == 5, result

for operand, expected in (('WHILE', 'do while .'), ('UNTIL', 'do until .'),
                          ('FOREVER', 'do while 1')):
    result = check('pfxrexx', 'DO ' + operand)
    assert result['text'][1] == expected, (operand, result)

result = check('pfxrexx', 'OTHE')
assert result['text'][1] == 'otherwise do', result

# HLASM siblings must produce assembler templates, not copied Rexx syntax.
for prefix, keyword in (('CASEE', 'CASENTRY'), ('ENDIF', 'ENDIF'), ('ITERA', 'ITERATE'),
                         ('ORELS', 'ORELSE'), ('OTHER', 'OTHRWISE'), ('WHEN', 'WHEN')):
    result = check('pfxhlasm', prefix)
    assert result['text'][1].lstrip().startswith(keyword), (prefix, result)
    assert not any('then do' in row.lower() for row in result['text']), (prefix, result)

result = check('pfxhlasm', 'STAR')
assert result['text'][0][71:72] == '*', result
result = check('pfxhlasm', 'UNSTA', seed='seed' + ' ' * 67 + '*')
assert result['text'][0].rstrip() == 'seed', result

# Indentation and context are part of the contract, not just template text
# starting from a plain record. These cases run under both engines.
contexts = [
    ('pfxrexx', 'DO', 'do'),
    ('pfxrexx', 'DO', 'label:'),
    ('pfxrexx', 'IF', ' label:say 1'),
    ('pfxrexx', 'IF', ' label: say 1'),
    ('pfxrexx', 'WHEN', 'select'),
    ('pfxrexx', 'OTHE', ' when . then do'),
    ('pfxrexx', 'RI', '  if . then do'),
    ('pfxrexx', 'RIC', '  else'),
    ('pfxhlasm', 'CASE', '         CASENTRY R1'),
    ('pfxhlasm', 'WHEN', '         SELECT'),
    ('pfxhlasm', 'ELSEI', '         IF EQ'),
    ('pfxhlasm', 'DO', 'label    MVC'),
    ('pfxhlasm', 'SEA', '         DO INF'),
]
for package, prefix, seed in contexts:
    check(package, prefix, seed=seed)

for operand in ('ONCE', 'INF', 'FOREVER', 'FROM', 'BY 2', 'UNTIL', 'WHILE'):
    check('pfxhlasm', 'DO ' + operand)
for operand in ('CC', 'EQ'):
    check('pfxhlasm', 'IF ' + operand)
for package, prefix in (('pfxrexx', 'CU C'), ('pfxrexx', 'CL C'),
                         ('pfxhlasm', 'UC C'), ('pfxhlasm', 'LC C')):
    check(package, prefix, seed='aBcDeF', column=3)
check('pfxhlasm', 'STAR2')

for package, prefix, seed, message in (
        ('pfxrexx', 'CU BAD', 'seed', 'Invalid column designator'),
        ('pfxhlasm', 'UC BAD', 'seed', 'Invalid column designator'),
        ('pfxhlasm', 'IF BAD', 'seed', 'invalid'),
        ('pfxhlasm', 'DO', '         CASENTRY R1', 'not allowed')):
    result = harness.run(package, prefix, seed=seed)
    assert not interpreter_errors(result), (package, prefix, result['errors'])
    assert message.lower() in result['errors'].lower(), (package, prefix, result['errors'])
    assert result['text'] == [seed, 'tail'], (package, prefix, result['text'])
    results.append({'package': package, 'prefix': prefix, 'text': result['text'],
                    'focus': result['snapshots'][-1]['focus'],
                    'pending': next(r['pending_prefix'] for r in reversed(result['records']) if 'pending_prefix' in r),
                    'error_prefix': next(row['p'] for snap in result['snapshots']
                                         for row in snap['screen_rows'] if row.get('p', '').startswith('?'))})

out = Path(sys.argv[2]) / ('levelg-qualification.json' if engine == 'CREXX' else 'qualification.json')
out.write_text(json.dumps(results, indent=2), encoding='utf-8')
print(('Level G port:' if engine == 'CREXX' else 'Patched originals:'), len(results),
      'behavioral workflows passed, including all 57 documented aliases')
