#!/usr/bin/env python3
"""Generate native envelope expectations from authored core VM sink assertions.

The upstream compiler owns program/VM fixtures. This native-host stage owns
framing expectations; it never reads execution output or rewrites bytecode.
"""
import argparse, json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');a=p.parse_args()
    folder=ROOT/'conformance/vectors';path=folder/'manifest.json';manifest=json.loads(path.read_text());changed=[]
    for vector in manifest['vectors']:
        expect=vector['expect'];sink=expect.get('sink')
        if sink is None:continue
        error=expect.get('runtime_error_at') or expect.get('call_error_at')
        if error:
            detail={'return-rhs-error':'int division by zero'}.get(vector['name'],error['message'])
            message='dsl line '+str(error['line'])+(':'+str(error['column'])if error.get('column')else'')+': '+detail
            envelope={'ok':False,'phase':'run','code':error['code'],'message':message,'line':error['line']}
            if error.get('column'):envelope['column']=error['column']
        else:envelope={'ok':True,'result':json.loads(sink['json'])}
        sink['frame']='LMCODE_RETURN_BEGIN\n'+json.dumps(envelope,separators=(',',':'),ensure_ascii=False)+'\nLMCODE_RETURN_END\n'
        fixture=folder/(vector['name']+'.expect');lines=fixture.read_text().splitlines();assert lines[-1].startswith('sink ')
        lines[-1]='sink '+json.dumps(sink,separators=(',',':'),ensure_ascii=False)
        raw='\n'.join(lines)+'\n'
        if raw!=fixture.read_text():changed.append(fixture);a.check or fixture.write_text(raw,encoding='utf8',newline='\n')
    manifest['host']['result_sink']='Native sink JSON is asserted independently from one terminal invocation envelope; success includes ok/result and controlled failure includes phase/code/message/location with no payload.'
    manifest['invocation_expectations_generated_by']='scripts/gen-invocation-expectations.py'
    raw=json.dumps(manifest,indent=2,ensure_ascii=False)+'\n'
    if raw!=path.read_text():changed.append(path);a.check or path.write_text(raw,encoding='utf8',newline='\n')
    if a.check and changed:raise SystemExit('stale native envelope fixtures: '+', '.join(p.name for p in changed))
    print('native invocation expectations current' if a.check else 'generated native invocation expectations')
if __name__=='__main__':main()
