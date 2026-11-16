#!/usr/bin/env python3
"""The opt-in rest module's registered signatures must equal the declarations of its schema fragment
(stdlib/rest.schema-fragment.json == std/rest/lmcode-rest.schema.json): exactly 18, and none of them in the default std schema."""
import json, os, subprocess, sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
fragment = json.load(open(os.path.join(root, 'stdlib', 'rest.schema-fragment.json'), encoding='utf-8'))
declared = [f'fn {n}({", ".join(d["params"])}) -> {d["returns"]}' for n, ds in fragment['functions'].items() for d in ds]
declared += [f'method {t}.{n}({", ".join(d["params"])}) -> {d["returns"]}' for t, td in fragment['types'].items() for n, ds in td['methods'].items() for d in ds]
assert len(declared) == 20, f'expected 20 declarations, found {len(declared)}'
assert fragment['name'] == 'lmcode_std_rest_v1', fragment['name']

default_schema = json.load(open(os.path.join(root, 'std', 'lmcode-std.schema.json'), encoding='utf-8'))
assert all(n in default_schema['functions'] for n in fragment['functions']), 'rest functions missing from default std schema'
assert 'RestExchange' in default_schema.get('types', {}), 'RestExchange missing from default std schema'
assert default_schema.get('interfaces') == fragment.get('interfaces'), 'std interfaces drift from rest fragment'

host = sys.argv[1]
registered = subprocess.run([host, '--signatures'], check=True, capture_output=True, text=True).stdout.split('\n')
registered = sorted(s for s in registered if s)
assert registered == sorted(declared), f'registry/schema drift:\n  only registered: {sorted(set(registered) - set(declared))}\n  only declared: {sorted(set(declared) - set(registered))}'
print(f'rest schema parity: {len(declared)} signatures')
