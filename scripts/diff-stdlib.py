#!/usr/bin/env python3
"""Run a seeded corpus and compare the four SDKs' actual byte traces exactly."""
import argparse
import difflib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def execute(command, cwd, **kwargs):
    return subprocess.run(list(map(str,command)), cwd=cwd, check=True, **kwargs)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--siblings',type=Path,default=ROOT.parent)
    p.add_argument('--large',action='store_true',help='16 seeds x 64 rounds, generated and compiled locally')
    p.add_argument('--skip-build',action='store_true')
    p.add_argument('--output',type=Path,help='retain corpus, raw traces, and comparison logs here')
    a=p.parse_args(); siblings=a.siblings.resolve()
    roots={s:Path(os.environ.get('LMCODE_'+s.upper(),str(siblings/('lmcode-'+s)))).resolve() for s in ['cpp','rust','go','csharp']}
    output=a.output.resolve() if a.output else Path(tempfile.mkdtemp(prefix='lmcode-differential-'))
    output.mkdir(parents=True,exist_ok=True); corpus=output/'vectors';corpus.mkdir(exist_ok=True)
    if a.large:
        execute(['python3',ROOT/'scripts/gen-std-corpus.py','--output',corpus,'--seeds',','.join(map(str,range(16))),'--rounds','64','--compile'],ROOT)
    else:
        source=roots['cpp']/'conformance/stdlib-vectors'
        manifest=json.loads((source/'manifest.json').read_text())
        manifest['vectors']=[v for v in manifest['vectors'] if v['name'].startswith('fuzz-')]
        if not manifest['vectors']: raise SystemExit('no committed fuzz vectors')
        for entry in manifest['vectors']:
            for filename in [entry['schema'],entry['source'],entry['bytecode'],entry['name']+'.expect']:
                shutil.copyfile(source/filename,corpus/filename)
        (corpus/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    # Build commands have separate logs and never enter the trace comparison.
    builds={
        'cpp':[['make','build/conformance']],
        'rust':[['cargo','build','--release','-p','dsl-conformance','--bin','dsl-conformance']],
        'go':[['go','build','-o',output/'go-conformance','./cmd/conformance']],
        'csharp':[['dotnet','build','-c','Release','-warnaserror','conformance/LmCode.Conformance']],
    }
    runners={'cpp':[roots['cpp']/'build/conformance'], 'rust':[roots['rust']/'target/release/dsl-conformance'], 'go':[output/'go-conformance'], 'csharp':['dotnet',roots['csharp']/'conformance/LmCode.Conformance/bin/Release/net8.0/LmCode.Conformance.dll']}
    traces={}; env=dict(os.environ,LMCODE_TRACE='1')
    for sdk in roots:
        if not a.skip_build:
            with (output/(sdk+'-build.log')).open('w') as log:
                for command in builds[sdk]: execute(command,roots[sdk],stdout=log,stderr=subprocess.STDOUT)
        result=subprocess.run(list(map(str,runners[sdk]+[corpus])),cwd=roots[sdk],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        (output/(sdk+'-run.log')).write_bytes(result.stdout)
        if result.returncode: raise SystemExit(f'{sdk} corpus failed; see {output}/{sdk}-run.log')
        lines=[line for line in result.stdout.splitlines() if line.startswith((b'TRACE\t',b'ERROR\t'))]
        if not lines: raise SystemExit(f'{sdk}: runner emitted no actual traces; rebuild the runner')
        traces[sdk]=b'\n'.join(lines)+b'\n';(output/(sdk+'.trace')).write_bytes(traces[sdk])
        print(f'{sdk}: {len(lines)} exact observations passed')
    for sdk in ['rust','go','csharp']:
        if traces[sdk]!=traces['cpp']:
            diff=''.join(difflib.unified_diff(traces['cpp'].decode().splitlines(True),traces[sdk].decode().splitlines(True),fromfile='cpp',tofile=sdk))
            (output/(sdk+'.diff')).write_text(diff)
            raise SystemExit(f'exact differential mismatch: {output}/{sdk}.diff')
    print(f'PASS exact differential: four SDKs, {len(traces["cpp"].splitlines())} observations; logs {output}')

if __name__=='__main__': main()
