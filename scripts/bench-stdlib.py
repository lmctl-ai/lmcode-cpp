#!/usr/bin/env python3
"""Run deterministic standard-catalog workloads against prebuilt runners."""
import argparse, json, os, shlex, subprocess, tempfile, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def make_schema(path):
    doc = json.loads((ROOT / "std/lmcode-std.schema.json").read_text())
    doc["name"] = "lmcode_std_benchmark"
    doc["context"] = {"t": "Trace"}
    doc["types"]["Trace"] = {"methods": {"int": [{"params":["int"],"returns":"void"}], "bool": [{"params":["bool"],"returns":"void"}], "str": [{"params":["string"],"returns":"void"}]}}
    doc["functions"]["test_temp_dir"] = [{"params":[],"returns":"string"}]
    path.write_text(json.dumps(doc, indent=2, sort_keys=True) + "\n")

def workloads(n):
    return {
        "str-list": (f'let l = str_list(); for (i in range(0, {n})) {{ l.add("x"); }} t.int(l.len()); t.int(str_len(l.at({n-1}))); t.bool(l.contains("x")); t.int(l.index_of("x"));', [f"int {n}", "int 1", "bool true", "int 0"]),
        "str-map": (f'let m = str_map(); for (i in range(0, {n})) {{ m.set("k" + str(i), "v"); }} t.int(m.len()); t.bool(m.has("k{n//2}")); t.str(m.get_or("missing", "fallback")); t.str(m.key_at({n-1})); t.bool(m.remove("k{n//2}"));', [f"int {n}", "bool true", "str fallback", f"str k{n-1}", "bool true"]),
        "string-heavy": (f'let x = str_repeat("ab", {n}); let r = str_replace(x, "ab", "x"); t.int(str_len(r)); let p = str_split(str_repeat("a,", {n}), ","); t.int(p.len()); t.bool(str_match(str_repeat("a", {n}), "a*"));', [f"int {n}", f"int {n+1}", "bool true"]),
        "file-100mb": ('let p = test_temp_dir() + "/bench.bin"; let x = str_repeat("x", 50000000) + str_repeat("y", 50000000); file_write(p, x); t.int(str_len(file_read(p))); file_append(p, "z"); t.int(str_len(file_read(p))); t.int(file_lines(p).len());', ["int 100000000", "int 100000001", "int 1"]),
    }

def compile_vector(root, name, source, expected):
    vector = root / name; vector.mkdir(); schema = vector / "schema.json"; make_schema(schema)
    (vector / (name + ".dsl")).write_text(source + "\n")
    (vector / (name + ".expect")).write_text("trace\n" + "\n".join(expected) + "\n")
    (vector / "manifest.json").write_text(json.dumps({"schema":"schema.json","vectors":[{"name":name,"schema":"schema.json","source":name+".dsl","bytecode":name+".dslb"}]}))
    wrapper = f'program = _dsl_ schema={json.dumps(str(schema))};\n{source}\nend _dsl_\nsavedsl(program, {json.dumps(str(vector/(name+".dslb")))});\n'
    with tempfile.TemporaryDirectory(prefix="lmcode-bench-compile-") as scratch:
        lm = Path(scratch) / "compile.lm"; lm.write_text(wrapper); subprocess.run([os.environ.get("LMSCRIPT_BIN", "lmscript"), str(lm)], check=True, stdout=subprocess.DEVNULL)
    return vector

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--size", type=int, default=1000); ap.add_argument("--repeat", type=int, default=3); ap.add_argument("--runner", action="append", default=[])
    args = ap.parse_args(); runners = {}
    for item in args.runner:
        name, command = item.split("=", 1); runners[name] = shlex.split(command)
    if not runners and (ROOT / "build/conformance").exists(): runners["cpp"] = [str(ROOT / "build/conformance")]
    if not runners: raise SystemExit("no runner supplied; use --runner NAME=COMMAND")
    with tempfile.TemporaryDirectory(prefix="lmcode-stdlib-bench-") as scratch:
        results = []
        for name, (source, expected) in workloads(args.size).items():
            vector = compile_vector(Path(scratch), name, source, expected)
            for runner, command in runners.items():
                warm = subprocess.run(command + [str(vector)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                if warm.returncode: raise SystemExit(f"{runner}/{name} warmup failed: {warm.stderr[-1000:]}")
                samples = []
                for _ in range(args.repeat):
                    start = time.perf_counter(); run = subprocess.run(command + [str(vector)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True); seconds = time.perf_counter() - start
                    if run.returncode or not run.stdout.rstrip().endswith("PASS 1/1 vectors"): raise SystemExit(f"{runner}/{name} failed: {run.stdout[-500:]} {run.stderr[-500:]}")
                    samples.append(seconds)
                results.append({"runner":runner,"workload":name,"size":args.size,"warmup":"pass","repetitions":args.repeat,"seconds":samples})
        print(json.dumps({"results":results}, indent=2))

if __name__ == "__main__": main()
