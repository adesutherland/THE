#!/usr/bin/env python3
"""Apply the candidate's argv correction to the pinned CI dependency only."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path)
args = parser.parse_args()
source = args.source.resolve()
expected_commit = 'ae1607b8e145174422cee7f3e73fbcc37a65226c'
commit = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
if commit != expected_commit:
    parser.error('This correction applies only to cREXX ' + expected_commit)
target = source / 'interpreter/crexxsaa.c'
original = target.read_bytes().replace(b'\r\n', b'\n')
if hashlib.sha256(original).hexdigest() != 'c5b0bd5b0f816384d2ae3665b53031614d49c6fed2cd3d6cdb3231172799fbd3':
    parser.error('Pinned crexxsaa.c hash mismatch; refusing to overwrite changes')
text = original.decode('utf-8')
include = '# include <process.h>'
call = 'rc = _spawnvp(_P_WAIT, executable, argv);'
assert text.count(include) == 1 and text.count(call) == 1
text = text.replace(include, include + '\n# include "crexxsaa-spawn.h"')
text = text.replace(call, 'rc = the_crexx_spawnvp(executable, argv);')
shutil.copy2(Path(__file__).resolve().parent.parent / 'packaging/windows/crexxsaa-spawn.h',
             source / 'interpreter/crexxsaa-spawn.h')
target.write_bytes(text.encode('utf-8'))
print('Applied pinned cREXX Windows argv correction')
