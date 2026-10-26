#!/usr/bin/env python3
"""Require positive-path bytecode coverage for every post-v1 catalog signature."""
import json
from pathlib import Path
import runpy
ROOT = Path(__file__).resolve().parents[1]
api = runpy.run_path(str(ROOT / 'sample-suite/audit.py'))
canonical = api['signatures'](json.loads((ROOT/'std/lmcode-std.schema.json').read_text()))
legacy = set()
for area in ['string','math','collections','file']:
    legacy |= api['signatures'](json.loads((ROOT/f'stdlib/{area}.schema-fragment.json').read_text()))
manifest = json.loads((ROOT/'conformance/stdlib-vectors/manifest.json').read_text())
covered = set()
for case in manifest['vectors']:
    if case['name'].startswith(('data-', 'v3-')) and 'trace' in case.get('expect',{}):
        covered |= api['imports'](ROOT/'conformance/stdlib-vectors'/case['bytecode'])
missing = (canonical-legacy)-covered
assert not missing, f'new signatures without successful execution coverage: {sorted(missing)}'
print(f'PASS {len(canonical-legacy)}/{len(canonical-legacy)} new signatures in positive vectors')
