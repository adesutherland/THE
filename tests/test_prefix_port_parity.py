#!/usr/bin/env python3
"""Compare complete text, cursor, queued work and errors across both engines."""
import json
from pathlib import Path
import sys

root = Path(sys.argv[1])
reference = json.loads((root / 'qualification.json').read_text())
ported = json.loads((root / 'levelg-qualification.json').read_text())
assert len(reference) == len(ported) and len(reference) >= 90, (len(reference), len(ported))
for a, b in zip(reference, ported):
    for key in ('package', 'prefix', 'text', 'focus', 'pending', 'error_prefix'):
        assert a.get(key) == b.get(key), (a['package'], a['prefix'], key, a.get(key), b.get(key))
print('Regina / Level G parity:', len(reference), 'workflows match text, cursor, pending state and error markers')
