#!/usr/bin/env python3
"""Offline orchestration tests for run-all-samples.py."""

import importlib.util
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
TEST_RUNNER = ROOT / "sample-suite" / "test_runner.py"
spec = importlib.util.spec_from_file_location("sample_runner_fixture", TEST_RUNNER)
fixture_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture_module)
runner_spec = importlib.util.spec_from_file_location("sample_matrix_runner", ROOT / "sample-suite" / "run-all-samples.py")
runner = importlib.util.module_from_spec(runner_spec)
runner_spec.loader.exec_module(runner)


class MatrixTests(unittest.TestCase):
    languages = ("c", "cpp", "rust", "go", "csharp", "java", "kotlin", "scala")

    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="sample-matrix-test-"))
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.corpus = self.tmp / "corpus"
        shutil.copytree(ROOT / "sample-suite", self.corpus)
        manifest = json.loads((self.corpus / "manifest.json").read_text())
        manifest["cases"] = manifest["cases"][:2]
        (self.corpus / "manifest.json").write_text(json.dumps(manifest))
        self.repos = self.tmp / "repos"
        self.repos.mkdir()
        self.host = self.tmp / "host with spaces"
        self.host.write_text(fixture_module.FAKE_HOST)
        self.host.chmod(self.host.stat().st_mode | stat.S_IXUSR)
        for lang in self.languages:
            repo = self.repos / f"lmcode-{lang}-sample"
            (repo / "build").mkdir(parents=True)
            build = repo / "build.sh"
            build.write_text("#!/usr/bin/env bash\nexit ${SAMPLE_BUILD_FAIL:-0}\n")
            build.chmod(build.stat().st_mode | stat.S_IXUSR)
            executable = repo / "build" / f"lmcode-{lang}-sample"
            shutil.copy2(self.host, executable)
            executable.chmod(executable.stat().st_mode | stat.S_IXUSR)
        self.state = self.tmp / "state"
        self.env = dict(
            os.environ,
            SAMPLE_REPO_ROOT=str(self.repos),
            SAMPLE_CORPUS=str(self.corpus),
            SAMPLE_STATE=str(self.state),
            FAKE_SUITE=str(self.corpus),
            PATH=os.environ["PATH"],
        )

    def run_matrix(self, **overrides):
        return subprocess.run(
            [str(ROOT / "sample-suite" / "run-all-samples.sh"), "--all"],
            env=dict(self.env, **overrides), text=True, capture_output=True, timeout=60,
        )

    def test_eight_ports_pass_canonical_and_cross_comparison(self):
        result = self.run_matrix()
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads((self.state / "report.json").read_text())
        self.assertEqual(set(report["ports"]), set(self.languages))
        self.assertTrue(all(port["ok"] for port in report["ports"].values()))
        self.assertNotIn("cross_mismatches", report)

    def test_command_argv_with_spaces_and_one_cross_port_difference_fails(self):
        bad = self.tmp / "bad host with spaces"
        bad.write_text(fixture_module.FAKE_HOST.replace('mode = os.environ.get("FAKE_MODE", "ok")', 'mode = "wrong-output"'))
        bad.chmod(bad.stat().st_mode | stat.S_IXUSR)
        env = dict(self.env, SAMPLE_GO_COMMAND=json.dumps([str(bad)]))
        result = self.run_matrix(**env)
        self.assertNotEqual(result.returncode, 0)
        report = json.loads((self.state / "report.json").read_text())
        self.assertTrue(report["cross_mismatches"])

    def test_cross_compare_uses_raw_bytes_for_invalid_utf8(self):
        bad = self.tmp / "invalid host"
        bad.write_text(fixture_module.FAKE_HOST.replace(
            "output = expected.read_bytes()",
            "output = b'\\xff\\xc3' if mode == 'invalid-bytes' else expected.read_bytes()",
        ).replace('mode = os.environ.get("FAKE_MODE", "ok")', 'mode = "invalid-bytes"'))
        bad.chmod(bad.stat().st_mode | stat.S_IXUSR)
        result = self.run_matrix(SAMPLE_GO_COMMAND=json.dumps([str(bad)]))
        self.assertNotEqual(result.returncode, 0)
        report = json.loads((self.state / "report.json").read_text())
        self.assertTrue(report["cross_mismatches"])

    def test_build_failure_does_not_stop_other_ports(self):
        result = self.run_matrix(SAMPLE_BUILD_FAIL="7")
        self.assertNotEqual(result.returncode, 0)
        report = json.loads((self.state / "report.json").read_text())
        self.assertTrue(all("build_exit" in value for value in report["ports"].values()))

    def test_missing_port_is_a_failure_and_is_not_skipped(self):
        missing = self.repos / "lmcode-rust-sample"
        shutil.rmtree(missing)
        result = self.run_matrix()
        self.assertNotEqual(result.returncode, 0)
        report = json.loads((self.state / "report.json").read_text())
        self.assertEqual(report["ports"]["rust"]["error"], "missing repository")

    def test_unsafe_dotenv_is_rejected_without_shell_evaluation(self):
        env_file = self.tmp / "unsafe.env"
        env_file.write_text("SAMPLE_GO_COMMAND=$(touch hacked)\n")
        result = subprocess.run(
            [str(ROOT / "sample-suite" / "run-all-samples.sh"), "--all", "--env-file", str(env_file)],
            env=self.env, text=True, capture_output=True, timeout=30,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.tmp / "hacked").exists())

    def test_json_environment_file_accepts_argv_arrays(self):
        config = self.tmp / "sample.json"
        config.write_text(json.dumps({"SAMPLE_GO_COMMAND": [str(self.host)]}))
        result = subprocess.run(
            [str(ROOT / "sample-suite" / "run-all-samples.sh"), "--all", "--env-file", str(config)],
            env=self.env, text=True, capture_output=True, timeout=60,
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_empty_manifest_is_a_setup_failure(self):
        manifest = json.loads((self.corpus / "manifest.json").read_text())
        manifest["cases"] = []
        (self.corpus / "manifest.json").write_text(json.dumps(manifest))
        result = self.run_matrix()
        self.assertNotEqual(result.returncode, 0)

    def test_native_port_build_and_command_defaults_are_selected(self):
        cases = {
            "rust": ("Cargo.toml", "cargo"),
            "csharp": ("lmcode.csproj", "dotnet"),
            "kotlin": ("pom.xml", "mvn"),
            "scala": ("build.sbt", "sbt"),
        }
        for lang, (marker, tool) in cases.items():
            repo = self.tmp / f"native-{lang}"
            repo.mkdir()
            (repo / marker).write_text("fixture")
            build = runner.build_for(lang, repo, {})
            command = runner.command_for({}, lang, repo)
            self.assertEqual(build[0], tool, lang)
            self.assertTrue(command, lang)

    def test_explicit_native_build_and_command_argv_override_defaults(self):
        repo = self.tmp / "override-rust"
        repo.mkdir()
        env = {
            "SAMPLE_RUST_BUILD": json.dumps(["custom", "build", "--release"]),
            "SAMPLE_RUST_COMMAND": json.dumps(["custom host", "--profile", "release"]),
        }
        self.assertEqual(runner.build_for("rust", repo, env), ["custom", "build", "--release"])
        self.assertEqual(runner.command_for(env, "rust", repo), ["custom host", "--profile", "release"])

    def test_legacy_sdk_catalog_is_pending_and_never_green(self):
        rust = self.repos / "lmcode-rust-sample"
        (rust / "sdk.lock").write_text(json.dumps({"sha256": {"std/lmcode-std.schema.json": runner.LEGACY_STD_SHA256}}))
        result = self.run_matrix()
        self.assertNotEqual(result.returncode, 0)
        report = json.loads((self.state / "report.json").read_text())
        self.assertTrue(report["ports"]["rust"]["pending"])
        self.assertIn("rust", report["pending"])


if __name__ == "__main__":
    unittest.main()
