#!/usr/bin/env python3
"""Checks that v3 vectors cover every new signature on successful paths."""
import json
import runpy
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "conformance" / "stdlib-vectors"


class V3Vectors(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads((OUT / "manifest.json").read_text())
        cls.audit = runpy.run_path(str(ROOT / "sample-suite/audit.py"))

    def test_schema_is_canonical_v3_plus_trace(self):
        schema = json.loads((OUT / "v3.schema.json").read_text())
        canonical = json.loads((ROOT / "std/lmcode-std.schema.json").read_text())
        for name in ("str_base64_encode", "str_base64_decode", "str_hex_encode", "str_hex_decode"):
            self.assertIn(name, schema["functions"])
            self.assertEqual(schema["functions"][name], canonical["functions"][name])
        for typ in ("StrMap", "IntMap", "FloatMap"):
            for method in ("keys", "values"):
                self.assertEqual(schema["types"][typ]["methods"][method], canonical["types"][typ]["methods"][method])

    def test_all_ten_signatures_have_successful_calls(self):
        actual = set()
        for item in self.manifest["vectors"]:
            if item["name"].startswith("v3-") and "trace" in item["expect"]:
                bytecode = OUT / item["bytecode"]
                self.assertTrue(bytecode.exists(), item["name"])
                actual |= self.audit["imports"](bytecode)
        canonical = json.loads((ROOT / "std/lmcode-std.schema.json").read_text())
        expected = {
            "fn str_base64_encode(string) -> string", "fn str_base64_decode(string) -> string",
            "fn str_hex_encode(string) -> string", "fn str_hex_decode(string) -> string",
            "method StrMap.keys() -> StrList", "method StrMap.values() -> StrList",
            "method IntMap.keys() -> StrList", "method IntMap.values() -> IntList",
            "method FloatMap.keys() -> StrList", "method FloatMap.values() -> FloatList",
        }
        self.assertEqual(expected, {s for s in actual if s in expected})

    def test_codec_boundaries_and_snapshot_cases_exist(self):
        names = {p.stem for p in OUT.glob("v3-*.dsl")}
        for name in ("v3-codecs-empty", "v3-codecs-all256", "v3-codecs-large", "v3-base64-alphabet", "v3-base64-pad-bits", "v3-hex-odd", "v3-snapshots-str", "v3-snapshots-int", "v3-snapshots-float"):
            self.assertIn(name, names)

    def test_manifest_has_all_generated_vectors(self):
        names = {v["name"] for v in self.manifest["vectors"]}
        self.assertEqual({p.stem for p in OUT.glob("v3-*.dsl")}, {n for n in names if n.startswith("v3-")})
        self.assertTrue(all(v["schema"] == "v3.schema.json" for v in self.manifest["vectors"] if v["name"].startswith("v3-")))


if __name__ == "__main__":
    unittest.main()
