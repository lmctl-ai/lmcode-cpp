"""Active consumer entry points must run without the retired module loader."""
from pathlib import Path
import re
import os
import json
import subprocess
import shlex
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

def removed(text):
    return bool(re.search(r'import\s+"(?:native:|@|github:|[^"\n]*cli\.schema\.)', text))

def retired_leading_option(text):
    return bool(re.search(r'"lmscript"\s*,\s*"--(?:no-preflight|dryrun-timeout)(?:=[^"\n]*)?"', text))

class ExecutionModes(unittest.TestCase):
    def test_harness_flags(self):
        for area in ('scripts', 'tests', 'durable-memory/tools/site-migrate'):
            for file in (ROOT / area).rglob('*.lms'):
                self.assertFalse(retired_leading_option(file.read_text()), str(file.relative_to(ROOT)))

    def test_documented_launches(self):
        for name in ('10-std-migration-validation.md', '13-lmscript-test-drivers.md'):
            self.assertNotRegex((ROOT / 'durable-memory' / name).read_text(),
                                r'lmscript --(?:no-preflight|dryrun-timeout)')

    def test_canonical_launchers(self):
        for name in ('test.lms', 'Makefile', 'README.md', 'sample-suite/README.md',
                     'sample-suite/test-runner.lms', 'sample-suite/run-suite.sh',
                     'sample-suite/check-invocations.py'):
            self.assertNotRegex((ROOT / name).read_text(), r'--(?:no-preflight|dryrun-timeout)', name)

    def test_flag_controls_and_script_tail_data(self):
        self.assertTrue(retired_leading_option('system(["lmscript","--no-preflight",program]);'))
        self.assertTrue(retired_leading_option('system(["lmscript","--dryrun-timeout=1",program]);'))
        self.assertFalse(retired_leading_option('system(["lmscript",program,"--no-preflight"]);'))
        self.assertFalse(retired_leading_option('system(["lmscript",program,"--dry-run"]);'))

    @unittest.skipUnless(os.environ.get('LMSCRIPT_BIN'), 'set LMSCRIPT_BIN to the current built compiler')
    @unittest.skipIf(os.name == 'nt', 'POSIX executable shim; static argv controls run on Windows')
    def test_actual_harness_argv_reaches_migrator(self):
        compiler = str(Path(shutil.which(os.environ['LMSCRIPT_BIN']) or os.environ['LMSCRIPT_BIN']).resolve())
        with tempfile.TemporaryDirectory(prefix='explicit mode argv ') as tmp:
            folder = Path(tmp)
            wrapper = folder / 'lmscript'
            wrapper.write_text('#!/bin/sh\nexec ' + shlex.quote(compiler) + ' "$@"\n')
            wrapper.chmod(0o755)
            migrator = folder / 'migrator.lms'
            migrator.write_text('assert(ARGV[0]=="--repo");assert(ARGV[2]=="--historical");'
                                'assert(ARGV[3]=="--dry-run");println("migrator reached");')
            env = dict(os.environ, PATH=str(folder) + os.pathsep + os.environ.get('PATH', ''))
            for name in ('lmstd-migrate-noop.test.lms', 'lmstd-migrate-output.test.lms'):
                text = (ROOT / 'tests' / name).read_text()
                helper = re.search(r'^fn checked\([^\n]*\) \{\n[\s\S]*?^\}', text, re.M)
                call = re.search(r'checked\(\["env","GIT_ALLOW_PROTOCOL=file","lmscript",[^\n]*\);', text)
                self.assertIsNotNone(helper, name)
                self.assertIsNotNone(call, name)
                values = dict(root=tmp, folder=tmp, repo=tmp, report=str(folder / 'report.json'), program=str(migrator))
                declarations = ''.join(key + '=' + json.dumps(value) + ';' for key, value in values.items())
                probe = folder / 'probe.lms'
                probe.write_text(helper[0] + '\n' + declarations + 'println(' + call[0][:-1] + ');')
                result = subprocess.run([compiler, str(probe)], env=env, text=True, capture_output=True)
                self.assertEqual(result.returncode, 0, name + '\n' + result.stderr)
                self.assertEqual(result.stdout, 'migrator reached\n', name)

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
    @unittest.skipIf(os.name == 'nt', 'newline filename fixture is POSIX-only')
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
