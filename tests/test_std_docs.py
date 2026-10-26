#!/usr/bin/env python3
"""Regression checks for the generated canonical standard documentation."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / "scripts" / "gen-std-docs.py"
DOCS = ROOT / "std" / "lmcode-std.docs.json"

subprocess.run([sys.executable, str(GEN), "--check"], check=True)
schema = json.loads((ROOT / "std" / "lmcode-std.schema.json").read_text())
docs = json.loads(DOCS.read_text())
assert docs["version"] == 2
assert set(docs["signatures"]) == {
    *(f"fn {name}({', '.join(overload['params'])}) -> {overload['returns']}"
      for name, overloads in schema["functions"].items() for overload in overloads),
    *(f"method {typ}.{name}({', '.join(overload['params'])}) -> {overload['returns']}"
      for typ, body in schema["types"].items() for name, overloads in body.get("methods", {}).items() for overload in overloads),
}
assert len(docs["signatures"]) == 171
assert docs["notes"]["bytes"]
assert "StrMap.set" not in docs["signatures"]["method StrMap.set(string, string) -> StrMap"]["errors"]
assert docs["signatures"]["fn math_pow(float, float) -> float"]["errors"] == []
assert "StrList object limit exceeded (65536)" in docs["signatures"]["fn file_lines(string) -> StrList"]["errors"]
for signature, meta in docs["signatures"].items():
    assert meta["area"] in {"string", "math", "collections", "file", "json"}, signature
    assert meta["summary"].strip(), signature
    assert isinstance(meta["errors"], list), signature
print(f"PASS std docs ({len(docs['signatures'])} signatures)")

with tempfile.TemporaryDirectory() as temp:
    temp = Path(temp)
    schema_copy = temp / "schema.json"; metadata_copy = temp / "metadata.json"; output_copy = temp / "std.md"
    schema_copy.write_text((ROOT / "std/lmcode-std.schema.json").read_text())
    metadata_copy.write_text((ROOT / "std/lmcode-std.docs.json").read_text())
    metadata = json.loads(metadata_copy.read_text()); metadata["signatures"].pop(next(iter(metadata["signatures"])))
    metadata_copy.write_text(json.dumps(metadata))
    missing = subprocess.run([sys.executable, str(GEN), "--schema", str(schema_copy), "--metadata", str(metadata_copy), "--output", str(output_copy)], capture_output=True, text=True)
    assert missing.returncode == 2 and "metadata signatures differ" in missing.stderr
    metadata_copy.write_text((ROOT / "std/lmcode-std.docs.json").read_text())
    output_copy.write_text("stale")
    stale = subprocess.run([sys.executable, str(GEN), "--schema", str(schema_copy), "--metadata", str(metadata_copy), "--output", str(output_copy), "--check"])
    assert stale.returncode == 1

    original_metadata = metadata_copy.read_bytes()
    subprocess.run([sys.executable, str(GEN), "--schema", str(schema_copy), "--metadata", str(metadata_copy), "--output", str(output_copy)], check=True)
    assert metadata_copy.read_bytes() == original_metadata, "generation must not rewrite source metadata"
    assert docs["notes"]["bytes"] in output_copy.read_text()
