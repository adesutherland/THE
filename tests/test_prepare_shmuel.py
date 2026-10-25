#!/usr/bin/env python3
"""Regression: EOF additions must not disappear from error-handler patches."""
import importlib.util
from pathlib import Path

path = Path(__file__).resolve().parent.parent / 'scripts/prepare-shmuel-macros.py'
spec = importlib.util.spec_from_file_location('prepare_shmuel', path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
patch = '--- a/x\n+++ b/x\n@@ -1,2 +1,3 @@\n first\n-last\n+correct\n+exit 1\n'
assert module.apply_patch('first\nlast\n', patch) == 'first\ncorrect\nexit 1\n'
try:
    module.apply_patch('first\nchanged by user\n', patch)
except ValueError:
    pass
else:
    raise AssertionError('Changed originals must not be patched silently')
print('Strict patch context and EOF additions passed')
