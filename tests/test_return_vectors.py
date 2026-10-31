#!/usr/bin/env python3
"""Execute canonical binary fixtures and reject real host/framing mutants."""
import hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
VECTORS=ROOT/'conformance/return-vectors'
BINARY=Path(os.environ.get('LMCODE_RETURN_HARNESS',str(ROOT/'build/return_conformance'))).resolve()
manifest=json.loads((VECTORS/'manifest.json').read_text())
schema=VECTORS/manifest['schema']
for line in (VECTORS/'vectors.lock').read_text().splitlines():
 digest,name=line.split('  ',1)
 assert hashlib.sha256((VECTORS/name).read_bytes()).hexdigest()==digest,('stale vector',name)
assert BINARY.is_file(),f'build the canonical return harness first: {BINARY}'
def run(name,*flags):return subprocess.run([str(BINARY),str(VECTORS/(name+'.dslb')),str(schema),*flags],capture_output=True)
for case in manifest['vectors']:
 raw=(VECTORS/case['bytecode']).read_bytes()
 assert raw[:6]==b'DSLB\0\6' and raw[6:38]==hashlib.sha256(schema.read_bytes()).digest()
 args=['--repeat',str(case['repeat'])]
 result=run(case['name'],*args)
 expected=(VECTORS/case['expect']['file']).read_bytes()
 assert (result.returncode,result.stdout)==(case['expect']['exit'],expected),(case['name'],result.stdout,result.stderr)
 if result.returncode==0:assert expected.endswith(b'LMCODE_RETURN_END\n') and not expected.endswith(b'\n\n')
 if case['name']=='entry-error':assert b'entry failure' in result.stderr and b'dsl line ' in result.stderr
 if case['name']=='unsafe-json-integer':assert b'/amount: unsafe_integer' in result.stderr and b'dsl line ' in result.stderr
 if case['name']=='scalar-field':assert b'field assignment requires object or null' in result.stderr
 if case['name']=='invalid-utf8':assert b': utf8' in result.stderr
 print('PASS canonical return fixture',case['name'])
assert (VECTORS/'float-roundtrip.expect').read_bytes()==b'LMCODE_RETURN_BEGIN\n{"ok":true,"result":0.30000000000000004}\nLMCODE_RETURN_END\n'
for name,flag in [('float-roundtrip','--mutate-rounding'),('repeated-calls','--mutate-reset')]:
 result=run(name,'--repeat','2',flag)
 assert result.returncode==0,result.stderr
 assert result.stdout!=(VECTORS/(name+'.expect')).read_bytes(),f'uncaught {flag}'
 print('CAUGHT',flag)
result=run('entry-error','--mutate-early-frame')
assert result.returncode==3 and result.stdout.count(b'LMCODE_RETURN_BEGIN\n')==2
print('CAUGHT premature frame before entry error')
result=run('entry-sink','--app-fail')
assert result.returncode==4 and b'\"ok\":false' in result.stdout and b'\"result\"' not in result.stdout
print('PASS application failure emits only a failure envelope')
