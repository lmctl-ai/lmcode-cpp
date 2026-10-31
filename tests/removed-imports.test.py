"""Active consumer entry points must run without the retired module loader."""
from pathlib import Path
import re
import os
import json
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

def removed(text):
    return bool(re.search(r'import\s+"(?:native:|@|github:|[^"\n]*cli\.schema\.)', text))

class ImportBoundary(unittest.TestCase):
    def test_consumers(self):
        for area in ('scripts', 'tests', 'durable-memory/tools/site-migrate'):
            for file in (ROOT / area).rglob('*.lms'):
                self.assertFalse(removed(file.read_text()), str(file.relative_to(ROOT)))

    def test_control(self):
        self.assertTrue(removed('import "native:git";'))
        self.assertTrue(removed('import "./tool.cli.schema.v1.json";'))
        self.assertFalse(removed('import "./helpers.lms";'))

class GitConsumers(unittest.TestCase):
    @unittest.skipUnless(os.environ.get('LMSCRIPT_BIN'), 'set LMSCRIPT_BIN to the current built compiler')
    def test_real_git_queries(self):
        compiler = os.environ['LMSCRIPT_BIN']
        with tempfile.TemporaryDirectory(prefix='d3-git ') as tmp:
            repo = Path(tmp) / 'repository with spaces'
            subprocess.run(['git', 'init', '-q', '-b', 'main', str(repo)], check=True)
            subprocess.run(['git', '-C', str(repo), '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid', 'commit', '-qm', 'Initial', '--allow-empty'], check=True)
            for name, functions in [('scripts/lmstd-migrate.lms', ('refuse', 'run', 'gitcmd', 'clean')), ('tests/lmstd-migrate-selftest.lms', ('checked', 'command', 'sha', 'status', 'clean'))]:
                text = (ROOT / name).read_text()
                helpers = []
                for fn in functions:
                    match = re.search(r'^fn ' + fn + r'\([^\n]*\) \{\n[\s\S]*?^\}', text, re.M)
                    self.assertIsNotNone(match, fn)
                    helpers.append(match[0])
                source = Path(tmp) / 'probe.lms'
                source.write_text('\n'.join(helpers) + '\nprintln(clean(' + json.dumps(str(repo)) + '));')
                result = subprocess.run([compiler, str(source)], text=True, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, 'true\n')
                (repo / 'untracked with spaces\nand newline').write_text('data')
                result = subprocess.run([compiler, str(source)], text=True, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, 'false\n')
                (repo / 'untracked with spaces\nand newline').unlink()

if __name__ == '__main__':
    unittest.main()
