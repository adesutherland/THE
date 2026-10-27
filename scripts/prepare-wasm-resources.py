#!/usr/bin/env python3
"""Package shared browser defaults; compile optional cREXX bytecode on the host."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('destination', type=Path)
parser.add_argument('--crexx-bin', type=Path)
args = parser.parse_args()
destination = args.destination.resolve()
destination.mkdir(parents=True, exist_ok=True)
# Do not retain generated macros when switching to an editor-only build.
for name in ['browser', 'quickfind', 'quickopen', 'quickline', 'paneldemo', 'library']:
    for suffix in ['.commands', '.the', '.rxas', '.rxbin']:
        (destination / (name + suffix)).unlink(missing_ok=True)
data = json.loads((root / 'profiles/default-profile.json').read_text())
commands = [command.format(suffix='') for command in data['commands']]
restricted = ['set impos off', 'set impmacro off', 'set macro off', 'set linend off', 'set synonym off']
if args.crexx_bin:
    bin_dir = args.crexx_bin.resolve()
    commands += ['set macroext rxbin', 'define f7 macro paneldemo.rxbin'] + restricted

    def quote(text):
        return "'" + text.replace("'", "''") + "';"

    source = destination / 'browser.the'
    source.write_text('options levelb\nimport rxfnsb\naddress the\n' + '\n'.join(map(quote, commands)) +
                      '\nsize = .string[]\naddress the "extract /size/" expose size[]\n' +
                      "if size[1] > 0 then 'cursor goto 1 1';\nelse 'bottom';\n" + quote(data['welcome']) + '\n')
    macros = [('browser', source)] + [(name, root / (name + '.the')) for name in
                                     ['quickfind', 'quickopen', 'quickline', 'paneldemo']]
    for name, path in macros:
        output = destination / name
        subprocess.run([str(bin_dir / 'rxc'), '-i', str(bin_dir), '-o', str(output), str(path)], check=True)
        subprocess.run([str(bin_dir / 'rxas'), '-o', str(output), str(output)], check=True)
        output.with_suffix('.rxas').unlink()
    source.unlink()
    shutil.copy2(bin_dir / 'library.rxbin', destination / 'library.rxbin')
else:
    commands = [command for command in commands if ' macro ' not in command]
    commands += ['define c-f cursor cmdline', 'define c-o cursor cmdline', 'define c-g cursor cmdline']
    (destination / 'browser.commands').write_text('\n'.join(commands + restricted + [data['welcome']]) + '\n')
shutil.copy2(root / 'doc/THE_Help.txt', destination / 'THE_Help.txt')
shutil.copytree(root / 'syntax', destination / 'syntax', dirs_exist_ok=True)
