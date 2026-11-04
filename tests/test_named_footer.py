#!/usr/bin/env python3
"""Reserved rows acquire footer semantics only through explicit profile names."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()

with tempfile.TemporaryDirectory(prefix='CRXTHE named footer ') as temporary:
    root = Path(temporary)
    sample, other = root/'sample.txt', root/'other.txt'
    sample.write_bytes(b'alpha\nsecond\n')
    other.write_bytes(b'other\n')
    env = dict(os.environ, THE_REXX_INTERPRETER='NONE')

    def run(commands, target=sample):
        result = subprocess.run(
            [str(binary), '--driver', 'llm', '-n', str(target)],
            input='\n'.join(commands + ['quit'])+'\n', cwd=root, env=env,
            text=True, encoding='utf-8', capture_output=True, timeout=30,
            check=True)
        records, remaining = [], result.stdout.strip()
        decoder = json.JSONDecoder()
        while remaining:
            record, consumed = decoder.raw_decode(remaining)
            records.append(record)
            remaining = remaining[consumed:].lstrip()
        return records

    setup = [
        'command set prefix on left 8 1',
        'command set reserved 4 normal F10 Menu is an ordinary note',
        'command set reserved -3 normal My custom guide without key labels',
        'command set footer KeYs -3',
        'command set reserved -2 normal Second custom footer',
        'command set footer build-hints_2 -2',
        'command cursor goto 1 3',
    ]
    records = run(setup + [
        'look full compact',
        'command set footer keys -1',  # Missing row must leave old name intact.
        'command set footer 1bad -2',
        'command set footer bad.name -2',
        'command set footer '+'a'*32+' -2',
        'look full compact',
        'command set footer keys -2',
        'command set footer build-hints_2 -3',
        'delta full compact',
        'look full compact',
        'command set footer keys off',
        'delta full compact',
        'look full compact',
        'command set footer keys -2',
        'command set reserved -2 normal Replaced guide is ordinary',
        'look full compact',
        'command set footer keys -2',
        'command set reserved -2 off',
        'command set reserved -2 normal New F10 Menu row is ordinary',
        'look full compact',
        'command set footer keys -2',
        'command edit "'+str(other)+'"',
        'look full compact',
        'command set reserved * off',
        'look full compact',
        'command edit "'+str(sample)+'"',
        'look full',
        'command set eolout lf',
        'command save',
        'look full compact',
    ])
    snapshots = [r for r in records if 'screen_rows' in r or 'rows' in r]
    full = [r for r in snapshots if r.get('mode') != 'delta']
    assert len(full) == 10, records
    initial, invalid, reordered, off, replaced, recreated, copied, cleared, original, saved = full

    def rows(snapshot):
        return snapshot.get('screen_rows', snapshot.get('rows', []))

    def names(snapshot):
        return [(r.get('r', r.get('screen_row')), r['footer'])
                for r in rows(snapshot) if 'footer' in r]

    assert [name for _, name in names(initial)] == ['keys', 'build-hints_2'], initial
    assert any(r['t'].endswith('is an ordinary note') and 'footer' not in r
               for r in rows(initial)), initial
    assert next(r['footer_text'] for r in rows(initial) if r.get('footer')=='keys') == 'My custom guide without key labels', initial
    assert names(invalid) == names(initial), invalid
    assert names(reordered) == [(names(initial)[0][0], 'build-hints_2'),
                                (names(initial)[1][0], 'keys')], reordered
    assert [name for _, name in names(off)] == ['build-hints_2'], off
    assert any(r['t'].endswith('footer') and 'footer' not in r
               for r in rows(off)), off
    assert [name for _, name in names(replaced)] == ['build-hints_2'], replaced
    assert [name for _, name in names(recreated)] == ['build-hints_2'], recreated
    assert any(r['t'].endswith('row is ordinary') and 'footer' not in r
               for r in rows(recreated)), recreated
    assert [name for _, name in names(copied)] == ['build-hints_2', 'keys'], copied
    assert copied['buffer']['path'].endswith('other.txt'), copied
    assert not names(cleared) and all(r['role']!='reserved' for r in rows(cleared)), cleared
    assert [name for _, name in names(original)] == ['build-hints_2', 'keys'], original
    assert original['buffer']['path'].endswith('sample.txt'), original
    for snapshot in [initial, invalid, reordered, off]:
        assert snapshot['focus']==initial['focus'], snapshot
    for snapshot in [initial, invalid, reordered, off, replaced, recreated, saved]:
        assert snapshot['buffer']['dirty']==0, snapshot
    deltas = [r for r in snapshots if r.get('mode')=='delta']
    assert len(deltas)==2 and any(r.get('footer')=='keys' for r in rows(deltas[0])), deltas
    assert any(r['t'].endswith('footer') and 'footer' not in r
               for r in rows(deltas[1])), deltas
    assert sample.read_bytes()==b'alpha\nsecond\n', sample.read_bytes()
    assert other.read_bytes()==b'other\n', other.read_bytes()

    coloured = run([
        'command set reserved -3 #AA2200 on #FFF4CC Build on Friday',
        'command set footer build -3',
        'look full compact',
    ])
    coloured_snapshot = next(r for r in coloured if 'screen_rows' in r)
    custom_row = next(r for r in rows(coloured_snapshot) if r.get('footer') == 'build')
    assert custom_row['footer_text'] == 'Build on Friday', custom_row
    assert custom_row['footer_colour']['foreground'] == '#AA2200', custom_row
    assert custom_row['footer_colour']['background'] == '#FFF4CC', custom_row

    # Existing XEDIT compatibility still leaves reserved rows out of new files.
    records = run(['command set compat xedit xedit xedit'] + setup[:-1] + [
        'command edit "'+str(other)+'"', 'look full compact'])
    snapshot = next(r for r in reversed(records) if 'screen_rows' in r)
    assert not names(snapshot) and all(r['role']!='reserved' for r in rows(snapshot)), snapshot
    assert sample.read_bytes()==b'alpha\nsecond\n' and other.read_bytes()==b'other\n'

    # Explicit band placement changes geometry for any name, never by text.
    records = run([
        'command set prefix on left 8 1',
        'command set reserved -3 #AA2200 on #FFF4CC Build on Friday',
        'command set footer build -3 aftercmdline',
        'command set reserved -2 normal Second custom guide',
        'command set footer arbitrary-name -2 aftercmdline',
        'command set reserved -1 normal Response area',
        'command set footer answer -1',
        'command cursor goto 1 3', 'look full compact',
        'command set cmdline top', 'look full compact',
        'command set cmdline off', 'command set statusline off', 'look full compact',
        'command set cmdline bottom', 'command set statusline bottom',
        'command set footer build -3', 'look full compact',
        'command set footer arbitrary-name off', 'look full compact',
        'command set footer build -3 aftercmdline',
        'command edit "'+str(other)+'"', 'look full compact',
        'command set reserved * off', 'look full compact',
        'command edit "'+str(sample)+'"', 'look full compact',
        'command set eolout lf', 'command save',
    ])
    band, top, hidden, legacy, detached, inherited, removed, original = [
        r for r in records if 'screen_rows' in r]
    band_rows = lambda s: [r for r in rows(s) if r.get('footer_placement') == 'aftercmdline']
    assert [r['footer'] for r in band_rows(band)] == ['build', 'arbitrary-name'], band
    answer = next(r for r in rows(band) if r.get('footer') == 'answer')
    assert answer['physical_row'] + 1 == band['regions']['command']['row'], band
    assert [r['physical_row'] for r in band_rows(band)] == [
        band['regions']['command']['row']+1, band['regions']['command']['row']+2], band
    assert band_rows(band)[-1]['physical_row']+1 == band['regions']['status']['row'], band
    assert band_rows(band)[0]['footer_colour']['foreground'] == '#AA2200', band
    assert top['regions']['command']['row'] < top['regions']['filearea']['row'], top
    assert top['regions']['filearea']['row']+top['regions']['filearea']['rows'] == top['regions']['footer']['row'], top
    assert hidden['regions']['command']['rows'] == hidden['regions']['status']['rows'] == 0, hidden
    assert hidden['regions']['footer']['rows'] == 2, hidden
    assert [r['footer'] for r in band_rows(legacy)] == ['arbitrary-name'], legacy
    assert next(r for r in rows(legacy) if r.get('footer') == 'build')['physical_row'] < legacy['regions']['command']['row'], legacy
    assert not band_rows(detached) and detached['regions']['footer']['rows'] == 0, detached
    assert any(r['t'].endswith('guide') and 'footer' not in r for r in rows(detached)), detached
    assert [r['footer'] for r in band_rows(inherited)] == ['build'], inherited
    assert not band_rows(removed) and removed['regions']['footer']['rows'] == 0, removed
    assert [r['footer'] for r in band_rows(original)] == ['build'], original
    assert all(s['buffer']['dirty'] == 0 for s in [band,top,hidden,legacy,detached,inherited,removed,original])
    assert sample.read_bytes()==b'alpha\nsecond\n' and other.read_bytes()==b'other\n'

    split = run([
        'command set reserved -2 normal Any custom band',
        'command set footer arbitrary -2 aftercmdline',
        'command set screen 2 horizontal', 'look full compact',
        'command nextwindow', 'look full compact',
        'command set footer arbitrary off', 'look full compact',
        'command nextwindow', 'look full compact',
        'command set footer arbitrary -2 aftercmdline',
        'command set screen 2 vertical', 'look full compact',
        'command nextwindow', 'look full compact',
    ])
    first, second, off_second, off_first, left, right = [r for r in split if 'screen_rows' in r]
    assert first['regions']['footer']['rows'] == second['regions']['footer']['rows'] == 1, split
    assert first['regions']['footer']['row'] < second['regions']['filearea']['row'], split
    assert off_second['regions']['footer']['rows'] == off_first['regions']['footer']['rows'] == 0, split
    assert left['regions']['footer']['rows'] == right['regions']['footer']['rows'] == 1, split
    assert left['regions']['footer']['col'] != right['regions']['footer']['col'], split
    assert sample.read_bytes()==b'alpha\nsecond\n' and other.read_bytes()==b'other\n'

    # Docked rows are already outside FILEAREA; page stepping counts only
    # inline reserved rows, and both directions retain the expected overlap.
    paging = root/'paging.txt'
    paging_bytes = b''.join(f'line {i:03d}\n'.encode() for i in range(1,301))
    paging.write_bytes(paging_bytes)
    page_records = run([
        'command set scale off', 'command set tabline off', 'command set hexshow off',
        'command set reserved -4 normal Band one',
        'command set footer docked-one -4 aftercmdline',
        'command set reserved -3 normal Band two',
        'command set footer docked-two -3 aftercmdline',
        'command set reserved -2 normal Inline one',
        'command set reserved -1 normal Inline two',
        'command cursor goto 100 1', 'look full compact',
        'command forward', 'look full compact',
        'command backward', 'look full compact',
        'command backward', 'look full compact',
        'command forward', 'look full compact',
        'command set eolout lf', 'command save',
    ], paging)
    page_views = [r for r in page_records if 'screen_rows' in r]
    first_lines = [min(row['line'] for row in rows(s) if row['role']=='file') for s in page_views]
    page_step = page_views[0]['regions']['filearea']['rows'] - 1 - 2
    assert first_lines == [first_lines[0], first_lines[0]+page_step,
                           first_lines[0], first_lines[0]-page_step, first_lines[0]], first_lines
    assert all(len(band_rows(s)) == 2 and s['buffer']['dirty'] == 0 for s in page_views), page_views
    assert paging.read_bytes() == paging_bytes

print('Named reserved footers: legacy and explicit band geometry, colour, OFF, replacement, inheritance, panel round trips and saved bytes passed')
