#!/usr/bin/env python3
"""Package command defaults for the editor-only browser milestone."""
import json
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parent.parent
destination = Path(sys.argv[1])
destination.mkdir(parents=True, exist_ok=True)
data = json.loads((root / 'profiles/default-profile.json').read_text())
# cREXX ships in the next milestone; avoid bindings to unavailable macros.
commands = [command.format(suffix='') for command in data['commands'] if ' macro ' not in command]
commands += ['define c-f cursor cmdline', 'define c-o cursor cmdline', 'define c-g cursor cmdline',
             'set impos off', 'set impmacro off', 'set macro off',
             'set linend off', 'set synonym off', data['welcome']]
(destination / 'browser.commands').write_text('\n'.join(commands) + '\n')
shutil.copy2(root / 'doc/THE_Help.txt', destination / 'THE_Help.txt')
shutil.copytree(root / 'syntax', destination / 'syntax', dirs_exist_ok=True)
