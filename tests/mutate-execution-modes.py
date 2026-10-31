"""Restore retired launch advice in owned copies; require assertion failures."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FILES = [
    'tests/lmstd-migrate-noop.test.lms', 'tests/lmstd-migrate-output.test.lms',
    'durable-memory/10-std-migration-validation.md', 'durable-memory/13-lmscript-test-drivers.md',
    'test.lms', 'Makefile', 'README.md', 'sample-suite/README.md',
    'sample-suite/test-runner.lms', 'sample-suite/run-suite.sh', 'sample-suite/check-invocations.py',
]

with tempfile.TemporaryDirectory(prefix='explicit-mode-controls-') as tmp:
    scratch = Path(tmp)
    for name in FILES + ['tests/removed-imports.test.py']:
        target = scratch / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / name, target)

    def run():
        return subprocess.run([sys.executable, str(scratch / 'tests/removed-imports.test.py'),
                               'ExecutionModes'], text=True, capture_output=True)

    baseline = run()
    assert baseline.returncode == 0, baseline.stdout + baseline.stderr
    print('BASELINE PASS', flush=True)
    for name in FILES:
        target = scratch / name
        original = target.read_text()
        if name.startswith('tests/'):
            assert '"lmscript",program' in original, name
            mutant = original.replace('"lmscript",program', '"lmscript","--no-preflight",program')
        else:
            mutant = original + '\n# lmscript --no-preflight\n'
        try:
            target.write_text(mutant)
            result = run()
            output = result.stdout + result.stderr
            assert result.returncode != 0, 'SURVIVED ' + name
            assert 'AssertionError' in output and 'SyntaxError' not in output, output
            print('CAUGHT ' + name, flush=True)
        finally:
            target.write_text(original)
    print('PASS ' + str(len(FILES)) + ' mutants caught')
