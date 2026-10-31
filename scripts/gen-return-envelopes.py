#!/usr/bin/env python3
"""Author envelope bytes for the original untyped native fixtures, no execution."""
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];HERE=ROOT/'conformance/return-vectors'
manifest=json.loads((HERE/'manifest.json').read_text())
errors={
 'entry-error':('run','RUN_HOST','dsl line 5:3: entry failure',5,3),
 'unsafe-json-integer':('run','RUN_HOST','dsl line 3:1: lmcode return: invalid JSON value at /amount: unsafe_integer',3,1),
 'scalar-field':('run','RUN_HOST','dsl line 3:1: lmcode return: field assignment requires object or null',3,1),
 'invalid-utf8':('run','RUN_HOST','dsl line 2:1: lmcode return: invalid JSON value at : utf8',2,1),
}
# Preserve the original authored success payloads as source data, not parsed host output.
payloads=json.loads((HERE/'payloads.json').read_text())
for case in manifest['vectors']:
 path=HERE/case['expect']['file'];name=case['name']
 if name in errors:
  phase,code,message,line,column=errors[name];envelope={'ok':False,'phase':phase,'code':code,'message':message,'line':line,'column':column}
 else:
  envelope={'ok':True,'result':payloads[name]}
 path.write_bytes(('LMCODE_RETURN_BEGIN\n'+json.dumps(envelope,ensure_ascii=False,separators=(',',':'))+'\nLMCODE_RETURN_END\n').encode())
manifest['host']['frames']='one invocation envelope on success or controlled failure, after body/entries/application validation; no partial payload on error'
(HERE/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',newline='\n')
paths=[HERE/'schema.json',HERE/'manifest.json',HERE/'payloads.json']+[HERE/c[k]for c in manifest['vectors']for k in ['source','bytecode']]+[HERE/c['expect']['file']for c in manifest['vectors']]
(HERE/'vectors.lock').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n'for p in sorted(paths)),newline='\n')
