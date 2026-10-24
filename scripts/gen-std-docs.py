#!/usr/bin/env python3
"""Render canonical lmcode std documentation from checked-in metadata."""
from __future__ import annotations
import argparse, json, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = ROOT / "std" / "lmcode-std.schema.json"
METADATA = ROOT / "std" / "lmcode-std.docs.json"
MD_OUT = ROOT / "docs" / "std.md"

def schema_signatures(schema):
    out = {}
    for name, overloads in schema["functions"].items():
        for item in overloads:
            out[f"fn {name}({', '.join(item['params'])}) -> {item['returns']}"] = item["params"]
    for typ, body in schema["types"].items():
        for name, overloads in body.get("methods", {}).items():
            for item in overloads:
                out[f"method {typ}.{name}({', '.join(item['params'])}) -> {item['returns']}"] = item["params"]
    return dict(sorted(out.items()))

def load_document(schema_path=SCHEMA, metadata_path=METADATA):
    schema = json.loads(Path(schema_path).read_text(encoding="utf-8")); metadata = json.loads(Path(metadata_path).read_text(encoding="utf-8"))
    expected = schema_signatures(schema); actual = metadata.get("signatures", {})
    missing = sorted(set(expected) - set(actual)); extra = sorted(set(actual) - set(expected))
    if missing or extra: raise ValueError(f"metadata signatures differ: missing={missing}, extra={extra}")
    for signature, params in expected.items():
        entry = actual[signature]
        if entry.get("params") != params or not entry.get("area") or not entry.get("summary", "").strip() or not isinstance(entry.get("errors"), list): raise ValueError(f"invalid metadata entry for {signature}")
    return {"schema": "std/lmcode-std.schema.json", "version": 1, "notes": metadata.get("notes", {}), "signatures": actual}

def render(doc):
    lines = ["# Built-in lmcode std v1", "", f"Canonical signatures: {len(doc['signatures'])}.", "", "| Signature | Area | Semantics | Errors |", "| --- | --- | --- | --- |"]
    for signature, meta in sorted(doc["signatures"].items()):
        errors = "; ".join(meta["errors"]) or "—"; lines.append(f"| `{signature}` | {meta['area']} | {meta['summary']} | {errors} |")
    return "\n".join(lines) + "\n"

def main(argv=None):
    parser = argparse.ArgumentParser(); parser.add_argument("--schema", default=str(SCHEMA)); parser.add_argument("--metadata", default=str(METADATA)); parser.add_argument("--output", default=str(MD_OUT)); parser.add_argument("--check", action="store_true"); args = parser.parse_args(argv)
    try: doc = load_document(args.schema, args.metadata)
    except (OSError, ValueError, json.JSONDecodeError) as error: print(f"gen-std-docs: {error}", file=sys.stderr); return 2
    expected_json = json.dumps(doc, indent=2, sort_keys=True) + "\n"; expected_md = render(doc); json_path = Path(args.metadata); md_path = Path(args.output)
    if args.check: return 0 if json_path.read_text() == expected_json and md_path.read_text() == expected_md else 1
    json_path.write_text(expected_json, encoding="utf-8"); md_path.parent.mkdir(parents=True, exist_ok=True); md_path.write_text(expected_md, encoding="utf-8"); return 0

if __name__ == "__main__": raise SystemExit(main())
