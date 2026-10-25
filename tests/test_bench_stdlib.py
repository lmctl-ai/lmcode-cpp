#!/usr/bin/env python3
import importlib.util
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("bench", root / "scripts" / "bench-stdlib.py")
bench = importlib.util.module_from_spec(spec); spec.loader.exec_module(bench)

for size in (1, 2, 3, 1000):
    workloads = bench.workloads(size)
    assert len(workloads["str-list"][1]) == 5
    assert workloads["str-list"][1][0] == f"int {size}"
    assert workloads["str-map"][1][0] == f"int {size + 1}"
    assert workloads["str-map"][1][-2] == ("str reinsert" if size == 1 else "str k1")
    assert workloads["str-map"][1][-1] == ("str z" if size == 1 else "str updated" if size // 2 == 1 else "str v")
print("PASS benchmark workload expectations")
