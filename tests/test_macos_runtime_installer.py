#!/usr/bin/env python3
"""Native Installer choice checks; only run on a disposable hosted macOS runner."""
import argparse
import json
import os
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--installer', required=True, type=Path)
args = parser.parse_args()
assert sys.platform == 'darwin' and os.environ.get('RUNNER_ENVIRONMENT') == 'github-hosted'
prefix = Path('/usr/local/the-crexx')
preference = prefix / 'share/the/runtime-preference'
choices = plistlib.loads(subprocess.check_output(
    ['installer', '-showChoicesXML', '-pkg', str(args.installer), '-target', '/']))


def find(items, name):
    for item in items:
        if item.get('choiceIdentifier') == name:
            return item
        found = find(item.get('childItems', []), name)
        if found:
            return found
    return None


choice = find(choices, 'installed-crexx')
assert choice and not choice['choiceIsSelected'], choices
assert not preference.exists(), 'Default PKG unexpectedly selected an external runtime'
with tempfile.TemporaryDirectory(prefix='THE Installer choices ') as tmp:
    changed = Path(tmp) / 'choices.plist'
    changed.write_bytes(plistlib.dumps([dict(choiceIdentifier='installed-crexx',
                                            choiceAttribute='selected', attributeSetting=1)]))
    subprocess.run(['sudo', 'installer', '-pkg', str(args.installer), '-target', '/',
                    '-applyChoiceChangesXML', str(changed)], check=True, capture_output=True)
    assert preference.read_text().strip() == 'INSTALLED'
    env = {k: v for k, v in os.environ.items() if not k.startswith(('THE_', 'CREXX'))}
    env.update(HOME=tmp, THE_CREXX_HOME=str(Path(tmp) / 'missing cREXX'))
    info = json.loads(subprocess.check_output([str(prefix / 'libexec/the/the-launcher'),
                                              '--info', str(prefix)], env=env, text=True))
    assert info['preference'] == 'INSTALLED' and info['selected'] == 'BUNDLED', info
    subprocess.run([sys.executable, str(Path(__file__).with_name('test_runtime_selection.py')),
                    '--prefix', str(prefix)], check=True)
    # A subsequent default installation must return to the bundled default.
    subprocess.run(['sudo', 'installer', '-pkg', str(args.installer), '-target', '/'],
                   check=True, capture_output=True)
    assert not preference.exists()
print('Native macOS Installer: bundled default, installed choice persisted, missing fallback and default reinstall passed.')
