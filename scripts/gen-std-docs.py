#!/usr/bin/env python3
"""Generate the checked-in documentation for the canonical lmcode std surface."""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = ROOT / "std" / "lmcode-std.schema.json"
JSON_OUT = ROOT / "std" / "lmcode-std.docs.json"
MD_OUT = ROOT / "docs" / "std.md"

STRING = {
    "str_starts_with": ("string", "Byte prefix comparison; an empty prefix matches.", []),
    "str_ends_with": ("string", "Byte suffix comparison; an empty suffix matches.", []),
    "str_contains": ("string", "Byte substring search; an empty needle matches.", []),
    "str_lower": ("string", "Lowercase ASCII A-Z bytes; preserve every other byte.", []),
    "str_upper": ("string", "Uppercase ASCII a-z bytes; preserve every other byte.", []),
    "str_substr": ("string", "Return a byte slice; clamp negative starts and overlong counts, with nonpositive counts empty.", []),
    "str_len": ("string", "Return the raw byte length, including NUL and invalid UTF-8 bytes.", []),
    "str_match": ("string", "Match a whole byte string with *, ?, classes, ranges, negation, and backslash escapes.", ["str_match: pattern exceeds the 2^27 work limit"]),
    "str_find": ("string", "Return the first byte offset or -1; an empty needle returns 0.", []),
    "str_replace": ("string", "Replace nonoverlapping byte matches left to right; an empty search is a no-op.", ["str_replace: result must be at most 67108864 bytes"]),
    "str_trim": ("string", "Remove ASCII space and bytes 0x09 through 0x0d from both ends.", []),
    "str_repeat": ("string", "Repeat raw bytes; reject negative counts and results above 64 MiB.", ["str_repeat: count must be nonnegative and result at most 67108864 bytes"]),
    "str_to_int": ("string", "Parse a complete signed decimal int64 token; return zero on failure.", []),
    "str_is_int": ("string", "Report whether the complete signed decimal token fits int64.", []),
    "str_to_float": ("string", "Parse a finite locale-independent decimal binary64 token; return zero on failure.", []),
    "str_is_float": ("string", "Report whether the complete decimal token is a finite binary64, including representable subnormals.", []),
}
MATH = {
    "math_abs": "Absolute value; integer arithmetic wraps, and float signed zero is cleared.",
    "math_min": "Minimum; integer comparison, or NaN propagation with first-operand ties for floats.",
    "math_max": "Maximum; integer comparison, or NaN propagation with first-operand ties for floats.",
    "math_floor": "Identity for int; floor for binary64 while preserving signed zero.",
    "math_ceil": "Identity for int; ceiling for binary64 while preserving signed zero.",
    "math_round": "Identity for int; binary64 nearest integer with halfway cases away from zero.",
    "math_pow": "Integer exponentiation wraps modulo 2^64; float power follows IEEE/libm special cases.",
    "math_sqrt": "Exact floor integer square root; binary64 square root for float.",
}
COLLECTION_FUNCTIONS = {
    "str_list": "Construct an empty StrList in the active collection arena.",
    "int_list": "Construct an empty IntList in the active collection arena.",
    "float_list": "Construct an empty FloatList in the active collection arena.",
    "str_map": "Construct an empty insertion-ordered StrMap in the active collection arena.",
    "str_split": "Split raw bytes on every nonoverlapping nonempty separator into a StrList.",
}
FILE = {
    "file_read": "Read a complete regular file as raw bytes.",
    "file_write": "Create or truncate a regular file and write all raw bytes.",
    "file_append": "Create or append all raw bytes to a regular file.",
    "file_exists": "Return true only when metadata identifies a regular file; missing and nonregular paths return false.",
    "file_lines": "Read raw bytes and split on LF, preserving CR, empty fields, and the final field.",
}
METHODS = {
    "StrList": {"len": "Return element count.", "at": "Return the element at a zero-based index.", "add": "Append a byte string and return the receiver.", "set": "Replace an indexed byte string and return the receiver.", "contains": "Test byte equality of elements.", "index_of": "Return the first byte-equal index or -1.", "clear": "Remove elements and return the receiver.", "join": "Join byte strings with a byte separator."},
    "IntList": {"len": "Return element count.", "at": "Return the element at a zero-based index.", "add": "Append an int and return the receiver.", "set": "Replace an indexed int and return the receiver.", "contains": "Test integer equality of elements.", "index_of": "Return the first equal index or -1.", "clear": "Remove elements and return the receiver."},
    "FloatList": {"len": "Return element count.", "at": "Return the element at a zero-based index.", "add": "Append a float and return the receiver.", "set": "Replace an indexed float and return the receiver.", "contains": "Test float == equality; NaN never matches.", "index_of": "Return the first float == index or -1; NaN never matches.", "clear": "Remove elements and return the receiver."},
    "StrMap": {"len": "Return insertion-order entry count.", "has": "Test byte-key presence.", "get": "Return a byte-key value.", "get_or": "Return a byte-key value or the fallback.", "set": "Insert or replace a value while preserving an existing key position.", "key_at": "Return the insertion-order key at a zero-based index.", "remove": "Remove a key and report whether it existed.", "clear": "Remove entries and return the receiver."},
}

def signatures(schema: dict) -> list[tuple[str, str, str, list[str]]]:
    out = []
    for name, overloads in schema["functions"].items():
        for overload in overloads:
            params = overload["params"]
            out.append((f"fn {name}({', '.join(params)}) -> {overload['returns']}", "function", name, params))
    for typ, body in schema["types"].items():
        for name, overloads in body.get("methods", {}).items():
            for overload in overloads:
                params = overload["params"]
                out.append((f"method {typ}.{name}({', '.join(params)}) -> {overload['returns']}", "method", f"{typ}.{name}", params))
    return sorted(out)

def entry(canonical: str, kind: str, name: str, params: list[str]) -> dict:
    if kind == "function" and name in STRING:
        area, summary, errors = STRING[name]
    elif kind == "function" and name in MATH:
        area, summary, errors = "math", MATH[name], (["math_pow: integer exponent must be nonnegative"] if name == "math_pow" else ["math_sqrt: integer argument must be nonnegative"] if name == "math_sqrt" else [])
    elif kind == "function" and name in COLLECTION_FUNCTIONS:
        area, summary, errors = "collections", COLLECTION_FUNCTIONS[name], (["str_split: separator must not be empty"] if name == "str_split" else [])
    elif kind == "function" and name in FILE:
        area, summary, errors = "file", FILE[name], ["FUNCTION: PATH: OS reason"]
    else:
        typ, method = name.split(".", 1)
        area, summary, errors = "collections", METHODS[typ][method], ([f"{typ}.{method}: index out of range: i"] if method in {"at", "set", "key_at"} else ["StrMap.get: missing key"] if name == "StrMap.get" else [])
    return {"area": area, "summary": summary, "errors": errors, "params": params}

def render(doc: dict) -> str:
    lines = ["# Built-in lmcode std v1", "", f"Canonical signatures: {len(doc['signatures'])}.", "", "| Signature | Area | Semantics | Errors |", "| --- | --- | --- | --- |"]
    for signature, meta in doc["signatures"].items():
        errors = "; ".join(meta["errors"]) or "—"
        lines.append(f"| `{signature}` | {meta['area']} | {meta['summary']} | {errors} |")
    return "\n".join(lines) + "\n"

def build() -> dict:
    schema = json.loads(SCHEMA.read_text())
    entries = {canonical: entry(canonical, kind, name, params) for canonical, kind, name, params in signatures(schema)}
    return {"schema": "std/lmcode-std.schema.json", "version": 1, "signatures": entries}

def main() -> int:
    expected_json = json.dumps(build(), indent=2, sort_keys=True) + "\n"
    expected_md = render(json.loads(expected_json))
    check = "--check" in sys.argv
    if check:
        ok = JSON_OUT.exists() and MD_OUT.exists() and JSON_OUT.read_text() == expected_json and MD_OUT.read_text() == expected_md
        if not ok: print("generated std docs are stale; run scripts/gen-std-docs.py", file=sys.stderr); return 1
        return 0
    JSON_OUT.write_text(expected_json)
    MD_OUT.parent.mkdir(parents=True, exist_ok=True); MD_OUT.write_text(expected_md)
    return 0

if __name__ == "__main__": raise SystemExit(main())
