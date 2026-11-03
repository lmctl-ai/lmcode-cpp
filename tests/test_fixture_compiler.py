"""Resolve a published compiler from both POSIX and Windows npm launchers."""
from pathlib import Path
import json
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class PublishedCompiler(unittest.TestCase):
    def test_windows_cmd_shim_resolves_scoped_package(self):
        with tempfile.TemporaryDirectory(prefix='compiler shim ü ') as directory:
            root = Path(directory)
            shim = root/'node_modules/.bin/lmscript.cmd'
            shim.parent.mkdir(parents=True)
            shim.write_text('@echo off\n')
            package = root/'node_modules/@lmctl-ai/lmctl'
            (package/'dist').mkdir(parents=True)
            (package/'package.json').write_text(json.dumps({'name':'@lmctl-ai/lmctl','type':'module','exports':{'./package.json':'./package.json'}}))
            (package/'dist/dsl.js').write_text('export class CompiledDsl { static bytesOf() { return Uint8Array.from([68,83,76,66,0,6]); } }\n')
            (package/'dist/lmscript.js').write_text('export class LMScript { registerFunction(n, fn) { this.capture=fn; } async runAsync() { this.capture({}); return {success:true}; } }\n')
            source=root/'program.lms';source.write_text('program = _lmcode_ schema="host.json";\nlet x=1;\nend _lmcode_\n')
            output=root/'program.dslb'
            env=dict(os.environ,LMSCRIPT_BIN=str(shim));env.pop('LMCODE_COMPILER_MODULE',None)
            result=subprocess.run(['node',str(ROOT/'sample-suite/compile-fixture.mjs'),str(source),str(output)],env=env,capture_output=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(result.stderr,b'')
            self.assertEqual(output.read_bytes(),b'DSLB\0\6')

if __name__=='__main__':unittest.main()
