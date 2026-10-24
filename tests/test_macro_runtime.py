#!/usr/bin/env python3
"""Verify engine selection and missing runtime handling in separate processes."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())
regina = os.environ.get('THE_REGINA_LIBRARY')
if not regina:
    print('Regina runtime not available for dual-engine loader checks')
    sys.exit(77)

with tempfile.TemporaryDirectory() as tmp:
    sample = Path(tmp) / 'sample.txt'
    sample.write_text('seed\n')

    def check(engine, available, **overrides):
        env = dict(os.environ, THE_REXX_INTERPRETER=engine, TERM='', **overrides)
        result = subprocess.run([binary, '--driver', 'llm', '-n', str(sample)],
                                input='capabilities\nquit\n', text=True,
                                capture_output=True, env=env, timeout=10, check=True)
        caps = next(json.loads(line) for line in result.stdout.splitlines()
                    if '"macro_available"' in line)
        assert caps['macro_interpreter'] == engine.upper(), caps
        assert caps['macro_available'] == available, (caps, result.stderr)
        return result.stderr

    missing = str(Path(tmp) / 'missing-runtime')
    assert 'Unable to load' not in check('CREXX', True, THE_REGINA_LIBRARY=missing)
    assert 'Unable to load' not in check('regina', True, THE_CREXX_LIBRARY=missing)
    assert 'THE_CREXX_LIBRARY' in check('CREXX', False, THE_CREXX_LIBRARY=missing)
    assert 'THE_REGINA_LIBRARY' in check('REGINA', False, THE_REGINA_LIBRARY=missing)
    assert 'Unable to load' not in check('NONE', False, THE_CREXX_LIBRARY=missing,
                                       THE_REGINA_LIBRARY=missing)
    assert 'must be CREXX, REGINA or NONE' in check('INVALID', False)
    # A loadable library with the wrong interface must be rejected before use.
    assert 'missing crexxsaa_create' in check('CREXX', False, THE_CREXX_LIBRARY=regina)
print('Dual-engine runtime loader checks passed')
