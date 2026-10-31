#!/usr/bin/env python3
"""Slice 1 return-sink catalog contract checks."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TYPES = ["int", "float", "bool", "string", "Json", "StrList", "IntList", "FloatList", "StrMap", "IntMap", "FloatMap"]
EXPECTED = {f"fn lmcode_return_set({typ}) -> void" for typ in TYPES}
EXPECTED |= {f"fn lmcode_return_put(string, {typ}) -> void" for typ in TYPES}
EXPECTED |= {"fn lmcode_return_null() -> void", "fn lmcode_return_put_null(string) -> void"}


def signatures(schema):
    result = set()
    for name, overloads in schema["functions"].items():
        for overload in overloads:
            result.add(f"fn {name}({', '.join(overload['params'])}) -> {overload['returns']}")
    return result


def test_exact_slice_signatures():
    schema = json.loads((ROOT / "std/lmcode-std.schema.json").read_text())
    actual = signatures(schema) & {s for s in signatures(schema) if "lmcode_return_" in s}
    assert actual == EXPECTED


def test_docs_and_locks_are_regenerated():
    docs = json.loads((ROOT / "std/lmcode-std.docs.json").read_text())
    assert {s for s in docs["signatures"] if "lmcode_return_" in s} == EXPECTED
    lock = dict(line.split("=", 1) for line in (ROOT / "std.lock").read_text().splitlines())
    assert (ROOT / "std.lock").read_bytes() == (ROOT / "std/std.lock").read_bytes()
    assert lock["sha256"] == hashlib.sha256((ROOT / lock["schema"]).read_bytes()).hexdigest()
    assert "ResultSink" in (ROOT / "durable-memory/12-result-sink.md").read_text()


if __name__ == "__main__":
    test_exact_slice_signatures()
    test_docs_and_locks_are_regenerated()
    print("return catalog checks passed")
