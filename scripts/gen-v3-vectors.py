#!/usr/bin/env python3
"""Generate the literal-free v3 codec and map-snapshot vectors."""
from __future__ import annotations
import base64
import base64
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "conformance" / "stdlib-vectors"

def schema() -> None:
    value = json.loads((OUT / "schema.json").read_text())
    canonical = json.loads((ROOT / "std/lmcode-std.schema.json").read_text())
    value["name"] = "std_v3_conformance"
    value.setdefault("functions", {}).update(canonical["functions"])
    value.setdefault("types", {}).update(canonical["types"])
    (OUT / "v3.schema.json").write_text(json.dumps(value, indent=2) + "\n")

def write(name: str, body: str, trace: list[str] | None = None, error: list[str] | None = None) -> None:
    (OUT / f"v3-{name}.dsl").write_text(body.rstrip() + "\n")
    lines = error if error is not None else trace or []
    head = "runtime_error_exact" if error is not None else "trace"
    (OUT / f"v3-{name}.expect").write_text(head + "\n" + "\n".join(lines) + "\n")

def add_manifest() -> None:
    path = OUT / "manifest.json"
    manifest = json.loads(path.read_text())
    manifest["vectors"] = [v for v in manifest["vectors"] if not v["name"].startswith("v3-")]
    for source in sorted(OUT.glob("v3-*.dsl")):
        name = source.stem
        expected = (OUT / f"{name}.expect").read_text().splitlines()
        item = {"name": name, "covers": "v3 byte codecs and map snapshots", "schema": "v3.schema.json", "source": source.name, "bytecode": name + ".dslb", "mutated": False}
        item["expect"] = {expected[0]: expected[1:]}
        manifest["vectors"].append(item)
    manifest["vectors"].sort(key=lambda v: v["name"])
    path.write_text(json.dumps(manifest, indent=2) + "\n")

def make_vectors() -> None:
    raw256 = bytes(range(256)); rawhex = raw256.hex()
    write("codecs-rfc", 't.hex(str_base64_encode("f")); t.hex(str_base64_encode("fo")); t.hex(str_base64_encode("foo")); t.hex(str_base64_encode("foob")); t.hex(str_base64_encode("fooba")); t.hex(str_base64_encode("foobar")); t.hex(str_base64_decode("Zg==")); t.hex(str_base64_decode("Zm8=")); t.hex(str_base64_decode("Zm9v")); t.hex(str_base64_decode("Zm9vYg==")); t.hex(str_base64_decode("Zm9vYmE=")); t.hex(str_base64_decode("Zm9vYmFy"));', ["hex 5a673d3d", "hex 5a6d383d", "hex 5a6d3976", "hex 5a6d397659673d3d", "hex 5a6d3976596d453d", "hex 5a6d3976596d4679", "hex 66", "hex 666f", "hex 666f6f", "hex 666f6f62", "hex 666f6f6261", "hex 666f6f626172"])
    write("codecs-basic", 'let raw=bytes("000102ff"); let b=str_base64_encode(raw); let h=str_hex_encode(raw); t.hex(b); t.hex(h); t.bool(str_len(str_base64_decode(b))==str_len(raw)); t.bool(str_find(str_base64_decode(b),raw)==0); t.bool(str_len(str_hex_decode(h))==str_len(raw)); t.bool(str_find(str_hex_decode(h),raw)==0);', ["hex 414145432f773d3d", "hex 3030303130326666", "bool true", "bool true", "bool true", "bool true"])
    write("codecs-empty", 't.int(str_len(str_base64_encode(""))); t.int(str_len(str_hex_encode(""))); t.int(str_len(str_base64_decode(""))); t.int(str_len(str_hex_decode("")));', ["int 0"] * 4)
    write("codecs-all256", f'let raw=bytes("{rawhex}"); let b=str_base64_encode(raw); let h=str_hex_encode(raw); t.int(str_len(b)); t.int(str_len(h)); t.bool(str_len(str_base64_decode(b))==str_len(raw)); t.bool(str_find(str_base64_decode(b),raw)==0); t.bool(str_len(str_hex_decode(h))==str_len(raw)); t.bool(str_find(str_hex_decode(h),raw)==0);', ["int 344", "int 512", "bool true", "bool true", "bool true", "bool true"])
    write("codecs-all256-oracle", f'let raw=bytes("{rawhex}"); t.hex(str_base64_encode(raw)); t.hex(str_hex_encode(raw)); t.hex(str_base64_decode("{base64.b64encode(raw256).decode()}")); t.hex(str_hex_decode("{rawhex.upper()}"));', ['hex ' + base64.b64encode(raw256).hex(), 'hex ' + rawhex.encode().hex(), 'hex ' + rawhex, 'hex ' + rawhex])
    large = 1048593
    write("codecs-large", f'let raw=str_repeat("a", {large}); let b=str_base64_encode(raw); let h=str_hex_encode(raw); t.int(str_len(b)); t.int(str_len(h)); t.bool(str_len(str_base64_decode(b))==str_len(raw)); t.bool(str_find(str_base64_decode(b),raw)==0); t.bool(str_len(str_hex_decode(h))==str_len(raw)); t.bool(str_find(str_hex_decode(h),raw)==0);', [f"int {(large + 2) // 3 * 4}", f"int {large * 2}", "bool true", "bool true", "bool true", "bool true"])
    for name, fn, arg in [("base64-alphabet", "str_base64_decode", "!@#$"), ("base64-missing-padding", "str_base64_decode", "Zg"), ("base64-extra-padding", "str_base64_decode", "Zg==="), ("base64-interior-padding", "str_base64_decode", "Z=g="), ("base64-pad-bits", "str_base64_decode", "AB=="), ("base64-whitespace", "str_base64_decode", "Zg== "),  ("base64-nonascii", "str_base64_decode", "é"), ("base64-url", "str_base64_decode", "-_8="), ("hex-odd", "str_hex_decode", "0"), ("hex-whitespace", "str_hex_decode", "00 01"), ("hex-prefix", "str_hex_decode", "0x00")]: write(name, f'{fn}("{arg}");', error=["RUN_HOST", f"{fn}: invalid encoding"])
    write("snapshots-str", 'let m=str_map().set(bytes("6100ff"),bytes("7800fe")).set("b","y"); let ks=m.keys(); let vs=m.values(); t.hex(ks.at(0)); t.hex(ks.at(1)); t.hex(vs.at(0)); t.hex(vs.at(1)); m.remove("b"); m.set("c","z"); t.hex(ks.at(1)); t.hex(vs.at(1)); t.hex(m.keys().at(1));', ["hex 6100ff", "hex 62", "hex 7800fe", "hex 79", "hex 62", "hex 79", "hex 63"])
    lo = "(-9007199254740991 * 1024 - 1024)"; hi = "(9007199254740991 * 1024 + 1023)"
    write("snapshots-int", f'let m=int_map().set("a",{lo}).set("b",{hi}); let ks=m.keys(); let vs=m.values(); t.hex(ks.at(0)); t.hex(ks.at(1)); t.int(vs.at(0)); t.int(vs.at(1)); m.clear(); t.int(ks.len()); t.int(vs.len());', ["hex 61", "hex 62", "int -9223372036854775808", "int 9223372036854775807", "int 2", "int 2"])
    write("snapshots-float", 'let m=float_map().set("nan",math_sqrt(-1.0)).set("z",-0.0); let ks=m.keys(); let vs=m.values(); t.hex(ks.at(0)); t.hex(ks.at(1)); t.bool(vs.at(0)!=vs.at(0)); t.float(vs.at(1));', ["hex 6e616e", "hex 7a", "bool true", "float -0"])
    for typ, ctor in [("str", "str_map"), ("int", "int_map"), ("float", "float_map")]: write(f"snapshots-{typ}-empty", f'let m={ctor}(); let k=m.keys(); let v=m.values(); t.int(k.len()); t.int(v.len());', ["int 0", "int 0"])

    for name,fn,raw in [("base64-raw-nul","str_base64_decode",b"AA\x00A"),("base64-raw-high","str_base64_decode",b"AA\xffA"),("base64-two-pad-bits","str_base64_decode",b"Zm9="),("base64-mid-quartet","str_base64_decode",b"Zg==AAAA"),("hex-raw-high","str_hex_decode",b"0\xff"),("hex-digit","str_hex_decode",b"gg")]:
        write(name, f'{fn}(bytes("{raw.hex()}"));', error=["RUN_HOST",f"{fn}: invalid encoding"])
    for prefix,vals,trace in [('str',['"one"','"two"','"three"','"NEW"'],['hex 6f6e65','hex 74776f','hex 4e4557']),('int',['1','2','3','9'],['int 1','int 2','int 9']),('float',['1.25','2.5','3.75','9.5'],['float 1.25','float 2.5','float 9.5'])]:
        scalar={'str':'hex','int':'int','float':'float'}[prefix]
        body=f'let m={prefix}_map().set("a",{vals[0]}).set("b",{vals[1]}).set("c",{vals[2]}); let keys=m.keys(); let values=m.values(); m.set("a",{vals[3]}); m.remove("b"); m.set("b",{vals[3]}); t.hex(m.keys().join(",")); t.{scalar}(values.at(0)); t.{scalar}(values.at(1)); keys.set(0,"changed"); values.set(0,{vals[1]}); keys.add("extra"); values.add({vals[0]}); t.hex(m.keys().join(",")); t.{scalar}(m.get("a")); m.clear(); t.int(keys.len()); t.int(values.len());'
        write('snapshots-'+prefix+'-independence',body,['hex 612c632c62',trace[0],trace[1],'hex 612c632c62',trace[2],'int 4','int 4'])
        for method,ret in [('keys','StrList'),('values',{'str':'StrList','int':'IntList','float':'FloatList'}[prefix])]:
            write('snapshots-'+prefix+'-'+method+'-limit',f'let m={prefix}_map(); for (i in range(0,65535)) {{ str_list(); }} m.{method}();',error=['RUN_HOST',f'{ret} object limit exceeded (65536)'])

def main() -> None:
    schema()
    for path in OUT.glob("v3-*.dsl"): path.unlink()
    for path in OUT.glob("v3-*.expect"): path.unlink()
    make_vectors(); add_manifest()
    print("generated", len(list(OUT.glob("v3-*.dsl"))), "v3 vectors")

if __name__ == "__main__": main()
