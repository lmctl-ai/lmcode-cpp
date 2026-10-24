#!/usr/bin/env python3
"""Regression checks for the generated canonical standard documentation."""
from pathlib import Path
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / "scripts" / "gen-std-docs.py"
DOCS = ROOT / "std" / "lmcode-std.docs.json"

subprocess.run([sys.executable, str(GEN), "--check"], check=True)
schema = json.loads((ROOT / "std" / "lmcode-std.schema.json").read_text())
docs = json.loads(DOCS.read_text())
assert docs["version"] == 1
assert set(docs["signatures"]) == {
    *(f"fn {name}({', '.join(overload['params'])}) -> {overload['returns']}"
      for name, overloads in schema["functions"].items() for overload in overloads),
    *(f"method {typ}.{name}({', '.join(overload['params'])}) -> {overload['returns']}"
      for typ, body in schema["types"].items() for name, overloads in body.get("methods", {}).items() for overload in overloads),
}
assert len(docs["signatures"]) == 72
for signature, meta in docs["signatures"].items():
    assert meta["area"] in {"string", "math", "collections", "file"}, signature
    assert meta["summary"].strip(), signature
    assert isinstance(meta["errors"], list), signature
print(f"PASS std docs ({len(docs['signatures'])} signatures)")
