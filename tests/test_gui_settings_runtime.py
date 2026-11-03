#!/usr/bin/env python3
"""Authoritative GUI settings/shortcut projection without file mutation."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='THE GUI settings ') as temporary:
    root = Path(temporary).resolve()
    source = root / 'notes.txt'
    original = 'alpha\nZoë界\n'.encode()
    source.write_bytes(original)
    commands = '\n'.join([
        'command cursor goto 1 2', 'look full compact', 'tools',
        'command set number on', 'command set prefix off',
        'command set wordwrap on', 'command set tabs incr 4',
        'command define c-c input OVERRIDE', 'command define c-s locate /missing/',
        'tools', 'look full compact',
        'command define c-c clipboard copy', 'tools', 'quit', ''])
    result = subprocess.run([str(binary), '--driver', 'llm', '-n', str(source)],
                            input=commands, text=True, capture_output=True, cwd=root,
                            env=dict(os.environ, THE_REXX_INTERPRETER='NONE',
                                     THE_TOOL_CONFIG_DIR=str(root / 'config')), timeout=20)
    assert result.returncode == 0, (result.stdout, result.stderr)
    records = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
    tools = [record for record in records if record.get('type') == 'tools']
    assert len(tools) == 3, records
    initial, changed, restored = tools
    assert initial['currentFile']['role'] == 'text', initial
    assert initial['editorSettings']['scope'] == 'current-file', initial
    assert initial['editorSettings']['provenance'] == 'profile-defined-or-unknown', initial
    assert 'C-C' in initial['editorSettings']['frontendKeys']
    assert 'C-S' in initial['editorSettings']['frontendKeys']
    assert not initial['profile']['active']['startupSaved']
    settings = changed['editorSettings']
    assert settings['lineNumbers'] and not settings['prefix'] and settings['wordwrap'], settings
    assert settings['tabIncrement'] == 4, settings
    assert 'C-C' not in settings['frontendKeys'] and 'C-S' not in settings['frontendKeys'], settings
    assert 'C-C' in restored['editorSettings']['frontendKeys'], restored
    assert 'C-S' not in restored['editorSettings']['frontendKeys'], restored
    frames = [record for record in records if 'screen_rows' in record]
    assert len(frames) == 2, frames
    for frame in frames:
        assert not frame['buffer']['dirty'], frame
        assert frame['focus']['zone'] == 'filearea', frame
        # CURSOR operands are one based; the semantic cell index is zero based.
        assert frame['focus']['line'] == 1 and frame['focus']['cell'] == 1, frame
    assert source.read_bytes() == original
print('GUI settings reflect core values; custom shortcuts retain authority; cursor, dirty state and bytes preserved')
