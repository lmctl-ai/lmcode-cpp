#!/usr/bin/env python3
"""Recompile unmutated vectors with lmscript; never rewrite expectations."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    directory = Path(sys.argv[1]).resolve()
    manifest = json.loads((directory / 'manifest.json').read_text())
    for vector in manifest['vectors']:
        if vector.get('mutated', False):
            continue
        schema = directory / vector.get('schema', manifest['schema'])
        source = (directory / vector['source']).read_text()
        output = directory / vector['bytecode']
        # Keep exactly one wrapper line before the DSL, preserving source positions.
        wrapper = ('program = _lmcode_ schema=' + json.dumps(str(schema)) + ';\n'
                   + source + 'end _lmcode_\n'
                   + 'savedsl(program, ' + json.dumps(str(output)) + ');\n')
        with tempfile.TemporaryDirectory(prefix='lmcode-compile-') as scratch:
            path = Path(scratch) / 'compile.lm'
            path.write_text(wrapper)
            subprocess.run([os.environ.get('LMSCRIPT_BIN', 'lmscript'), str(path)], check=True)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit('usage: compile-vectors.py VECTOR_DIR')
    main()
