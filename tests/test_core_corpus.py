"""The native gate must not silently shrink its manifest's vector set."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
RUNNER = Path(os.environ.get('LMCODE_CONFORMANCE_RUNNER', ROOT / 'build/conformance')).resolve()

class CoreCorpusTests(unittest.TestCase):
    def run_copy(self, mutate=None):
        with tempfile.TemporaryDirectory() as tmp:
            corpus = Path(tmp) / 'vectors'
            shutil.copytree(ROOT / 'conformance/vectors', corpus)
            if mutate:
                mutate(corpus)
            return subprocess.run([RUNNER, corpus], text=True, capture_output=True)

    def test_baseline(self):
        result = self.run_copy()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('PASS 406/406', result.stdout)

    def test_missing_or_extra_core_files_are_refused(self):
        for filename in ['matrix-int-add-0.expect', 'matrix-int-add-0.dslb', 'matrix-int-add-0.dsl', 'schema.json', 'manifest.json']:
            with self.subTest(missing=filename):
                result = self.run_copy(lambda p: (p / filename).unlink())
                self.assertEqual(result.returncode, 2, filename + '\n' + result.stdout + result.stderr)
                self.assertIn('corpus integrity:', result.stderr)
        for suffix in ['expect', 'dslb', 'dsl']:
            with self.subTest(extra=suffix):
                result = self.run_copy(lambda p: shutil.copyfile(p / ('matrix-int-add-0.' + suffix), p / ('unlisted.' + suffix)))
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn('corpus integrity:', result.stderr)

    def test_duplicate_manifest_name_is_refused(self):
        def duplicate(p):
            manifest = json.loads((p / 'manifest.json').read_text())
            manifest['vectors'].append(manifest['vectors'][0])
            (p / 'manifest.json').write_text(json.dumps(manifest))
        result = self.run_copy(duplicate)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn('corpus integrity:', result.stderr)

    def test_wrong_load_code_is_refused(self):
        def wrong_code(p):
            file = p / 'hostile-bool-operand.expect'
            file.write_text('load_error\nRUN_HOST\nPUSH_BOOL operand\n')
        result = self.run_copy(wrong_code)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('FAIL - hostile-bool-operand', result.stdout)

if __name__ == '__main__':
    unittest.main()
