#!/usr/bin/env python3
"""Regression tests for the canonical std schema bridge."""

import importlib.util
import json
from pathlib import Path
import tempfile
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("merge_std_schema", ROOT / "scripts/merge-std-schema.py")
merge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(merge)
CANONICAL = json.loads((ROOT / "std/lmcode-std.schema.json").read_text())


class StdSchemaTests(unittest.TestCase):
    def host(self):
        return {"dsl.schema": 2, "name": "domain", "context": {"x": 7}, "types": {}, "functions": {}}

    def test_signature_normalization_includes_host_contexts(self):
        host = self.host()
        host["context"] = {"domain": "Domain"}
        self.assertIn("context domain: Domain", merge.signature_strings(host))

    def test_canonical_metadata_and_count(self):
        self.assertEqual(CANONICAL["name"], "lmcode_std_v1")
        self.assertEqual(set(CANONICAL), {"dsl.schema", "name", "types", "functions"})
        self.assertEqual(len(merge.canonical_signatures(CANONICAL)), 72)

    def test_canonical_digest_lock(self):
        import hashlib
        lock = dict(line.split("=", 1) for line in (ROOT / "std.lock").read_text().splitlines())
        self.assertEqual(lock["schema"], "std/lmcode-std.schema.json")
        self.assertEqual(lock["version"], "1")
        self.assertEqual(lock["sha256"], hashlib.sha256((ROOT / lock["schema"]).read_bytes()).hexdigest())

    def test_testing_stays_in_host_schema(self):
        host = self.host()
        host["functions"]["print"] = [{"params": ["string"], "returns": "void"}]
        merged = merge.merge_schema(host, CANONICAL)
        self.assertNotIn("print", CANONICAL["functions"])
        self.assertEqual(merge.remove_standard(merged, CANONICAL)["functions"], host["functions"])

    def test_merge_preserves_metadata_and_does_not_mutate_input(self):
        host = self.host()
        original = json.loads(json.dumps(host))
        result = merge.merge_schema(host, CANONICAL)
        self.assertEqual(host, original)
        self.assertEqual(result["context"], {"x": 7})
        self.assertIn("str_len", result["functions"])

    def test_merged_schema_is_idempotent(self):
        once = merge.merge_schema(self.host(), CANONICAL)
        self.assertEqual(once, merge.merge_schema(once, CANONICAL))

    def test_self_return_is_normalized_for_methods(self):
        host = self.host()
        host["types"]["StrList"] = {"methods": {"clear": [{"params": [], "returns": "self"}], "domain": [{"params": [], "returns": "void"}]}}
        result = merge.merge_schema(host, CANONICAL)
        self.assertEqual(result["types"]["StrList"]["methods"]["clear"][0]["returns"], "self")
        self.assertIn("domain", result["types"]["StrList"]["methods"])

    def test_conflicting_return_is_rejected(self):
        host = self.host()
        host["functions"]["str_len"] = [{"params": ["string"], "returns": "float"}]
        with self.assertRaisesRegex(ValueError, "conflicting function str_len"):
            merge.merge_schema(host, CANONICAL)

    def test_host_only_keeps_domain_extensions(self):
        host = self.host()
        merged = merge.merge_schema(host, CANONICAL)
        merged["functions"]["str_len"].append({"params": ["bytes"], "returns": "int"})
        merged["types"]["StrList"]["methods"]["domain"] = [{"params": [], "returns": "void"}]
        result = merge.remove_standard(merged, CANONICAL)
        self.assertIn("str_len", result["functions"])
        self.assertIn("bytes", [x["params"][0] for x in result["functions"].get("str_len", [])])
        self.assertIn("domain", result["types"]["StrList"]["methods"])

    def test_signature_normalization_is_sorted_unique_and_adds_canonical(self):
        signatures = merge.signature_strings({"functions": {"z": [{"params": [], "returns": "void"}], "a": [{"params": [], "returns": "void"}]}})
        signatures.update(merge.canonical_signatures(CANONICAL))
        lines = sorted(signatures)
        self.assertEqual(lines, sorted(set(lines)))
        self.assertIn("fn str_len(string) -> int", lines)

    def test_fragments_are_subsets_of_canonical(self):
        fragment_signatures = set()
        for name in ("string", "math", "collections", "file"):
            path = ROOT / "stdlib" / (name + ".schema-fragment.json")
            fragment = json.loads(path.read_text())
            fragment_signatures.update(merge.signature_strings(fragment))
        self.assertEqual(fragment_signatures, merge.canonical_signatures(CANONICAL))

    def test_cli_does_not_mutate_input_and_accepts_modes(self):
        import subprocess
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source = directory / "host.json"
            source.write_text(json.dumps(self.host()))
            merged = directory / "merged.json"
            subprocess.run([sys.executable, str(ROOT / "scripts/merge-std-schema.py"), str(source), str(merged)], check=True)
            self.assertEqual(json.loads(source.read_text()), self.host())
            normalized = directory / "signatures.txt"
            subprocess.run([sys.executable, str(ROOT / "scripts/merge-std-schema.py"), "--normalize-signatures", str(source), str(normalized)], check=True)
            self.assertEqual(normalized.read_text().splitlines(), sorted(set(normalized.read_text().splitlines())))
            from_stdin = subprocess.run([sys.executable, str(ROOT / "scripts/merge-std-schema.py"), "--normalize-signatures", "-", "-"], input="z()->void\n", text=True, capture_output=True, check=True)
            self.assertIn("z()->void", from_stdin.stdout.splitlines())

    def test_compiler_emits_the_canonical_signatures(self):
        import os
        import shutil
        import subprocess
        selected = os.environ.get("LMSCRIPT_BIN", "lmscript")
        compiler = shutil.which(selected) or selected
        # npm installs a .cmd launcher on Windows, which needs the command shell.
        batch = os.name == "nt" and Path(compiler).suffix.lower() in (".cmd", ".bat")
        output = subprocess.check_output([compiler, "--dsl-signatures", str(ROOT / "std/lmcode-std.schema.json")], text=True, shell=batch)
        compiler_signatures = set(output.splitlines())
        self.assertEqual(compiler_signatures, merge.canonical_signatures(CANONICAL))

    def test_host_only_removes_empty_builtin_types(self):
        result = merge.remove_standard(merge.merge_schema(self.host(), CANONICAL), CANONICAL)
        self.assertNotIn("StrList", result["types"])


if __name__ == "__main__":
    unittest.main()
