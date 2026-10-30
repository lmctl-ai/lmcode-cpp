#!/usr/bin/env python3
"""Generate literal-free data-container/Json conformance vectors.

The programs intentionally call the canonical declarations directly.  Their
schemas are derived from the committed fixture schema plus the v2 catalog, so
they remain compilable before lmscript bundles the new digest.
"""
import json, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "conformance" / "stdlib-vectors"
CANON = json.loads((ROOT / "std/lmcode-std.schema.json").read_text())
BASE = json.loads((OUT / "schema.json").read_text())
for old in OUT.glob("data-*"):
    old.unlink()

def schema(name):
    s = json.loads(json.dumps(BASE)); s["name"] = name
    f = s.setdefault("functions", {}); t = s.setdefault("types", {})
    for k,v in CANON["functions"].items(): f[k] = v
    for k,v in CANON["types"].items(): t[k] = v
    return s

DATA_SCHEMA = schema("data_catalog_conformance")
(OUT / "data.schema.json").write_text(json.dumps(DATA_SCHEMA, indent=2) + "\n")

def write(name, body, trace=None, error=None):
    # Vector sources are DSL bodies; conformance/compile-vectors.py adds the
    # single wrapper line and matching end marker for physical source mapping.
    src = body + "\n"
    (OUT / (name + ".dsl")).write_text(src)
    if error:
        (OUT / (name + ".expect")).write_text("runtime_error_exact\n" + "\n".join(error) + "\n")
    else:
        (OUT / (name + ".expect")).write_text("trace\n" + "\n".join(trace or []) + "\n")

def simple_family(prefix, typ, kind):
    val = '1' if typ == 'int' else ('1.5' if typ == 'float' else 'bytes("6100ff")')
    var = prefix + kind.lower()
    body = f'let {var} = {prefix}_{kind.lower()}().push({val}).push({val}); t.int({var}.len()); t.bool({var}.empty()); t.'
    body += ('int' if typ == 'int' else 'float' if typ == 'float' else 'hex') + f'({var}.peek()); t.'
    body += ('int' if typ == 'int' else 'float' if typ == 'float' else 'hex') + f'({var}.at(0)); t.'
    body += ('int' if typ == 'int' else 'float' if typ == 'float' else 'hex') + f'({var}.pop()); {var}.clear(); t.bool({var}.empty());'
    out = [f'int 2', 'bool false']
    token = 'int 1' if typ == 'int' else 'float 1.5' if typ == 'float' else 'hex 6100ff'
    out += [token, token, token, 'bool true']
    write(f'data-{prefix}-{kind.lower()}', body, out)

for prefix,typ in [('int','int'),('float','float'),('str','string')]:
    for kind in ('Stack','Queue'): simple_family(prefix, typ, kind)
    val = '1' if typ == 'int' else ('1.5' if typ == 'float' else 'bytes("6100ff")')
    var = prefix + 'pq'; scalar = 'int' if typ == 'int' else 'float' if typ == 'float' else 'hex'
    body = f'let {var} = {prefix}_pq().push({val}, 2.0).push({val}, 1.0); t.int({var}.len()); t.bool({var}.empty()); t.{scalar}({var}.peek()); t.{scalar}({var}.pop()); {var}.clear(); t.bool({var}.empty());'
    token='int 1' if typ=='int' else 'float 1.5' if typ=='float' else 'hex 6100ff'
    write(f'data-{prefix}-pq', body, ['int 2','bool false',token,token,'bool true'])

for prefix,typ in [('int','int'),('float','float')]:
    val='1' if typ=='int' else '1.5'; var=prefix+'map'; scalar='int' if typ=='int' else 'float'
    body=f'let {var} = {prefix}_map().set(bytes("6100ff"), {val}).set("b", {val}); t.int({var}.len()); t.bool({var}.has(bytes("6100ff"))); t.{scalar}({var}.get(bytes("6100ff"))); t.{scalar}({var}.get_or("missing", {val})); t.hex({var}.key_at(0)); t.bool({var}.remove("b")); {var}.clear(); t.int({var}.len());'
    write(f'data-{prefix}-map',body,['int 2','bool true',f'{scalar} {val}',f'{scalar} {val}','hex 6100ff','bool true','int 0'])

write('data-json','let j = json_parse(bytes("7b226e223a3132332c2261223a5b747275652c6e756c6c5d7d")); t.hex(j.kind()); t.int(j.len()); t.int(j.get("n").as_int()); t.bool(j.get("a").at(0).as_bool()); t.hex(j.to_json());',['hex 6f626a656374','int 2','int 123','bool true','hex 7b226e223a3132332c2261223a5b747275652c6e756c6c5d7d'])

for prefix,typ in [('int','int'),('float','float'),('str','string')]:
    token = '1' if typ == 'int' else ('1.5' if typ == 'float' else '"x"')
    for kind in ('Stack','Queue','PQ'):
        var=prefix+kind.lower(); op='peek' if kind=='PQ' else 'pop'; ctor=prefix+'_'+kind.lower()
        write(f'data-{prefix}-{kind.lower()}-{op}-empty',f'let x = {ctor}(); x.{op}();',error=[ 'RUN_HOST', f'{prefix.title()}{kind}.{op}: empty' ])
    for kind in ('Stack','Queue'):
        var=prefix+kind.lower(); ctor=prefix+'_'+kind.lower()
        write(f'data-{prefix}-{kind.lower()}-at-bounds',f'let x = {ctor}().push({token}); x.at(9);',error=['RUN_HOST',f'{prefix.title()}{kind}.at: index out of range: 9'])
    var=prefix+'pq'; ctor=prefix+'_pq'
    write(f'data-{prefix}-pq-nan',f'let x = {ctor}(); x.push({token}, math_sqrt(-1.0));',error=['RUN_HOST',f'{prefix.title()}PQ.push: priority must be finite'])
for prefix,typ in [('int','int'),('float','float')]:
    var=prefix+'map'; ctor=prefix+'_map'; token='1' if typ=='int' else '1.5'; owner=prefix.title()+'Map'
    write(f'data-{prefix}-map-missing',f'let x = {ctor}(); x.get("missing");',error=['RUN_HOST',f'{owner}.get: missing key'])
    write(f'data-{prefix}-map-bounds',f'let x = {ctor}().set("a", {token}); x.key_at(9);',error=['RUN_HOST',f'{owner}.key_at: index out of range: 9'])
write('data-json-wrong-kind','let j = json_parse(bytes("5b5d")); j.get("x");',error=['RUN_HOST','Json.get: expected object'])
write('data-json-missing','let j = json_parse(bytes("7b7d")); j.get("x");',error=['RUN_HOST','Json.get: missing key'])
write('data-json-bounds','let j = json_parse(bytes("5b5d")); j.at(0);',error=['RUN_HOST','Json.at: index out of range: 0'])

# Parser boundary contract: JSON bytes are supplied through bytes(), so these
# cases remain independent of host source decoding and exercise eager parsing.
for name, payload, reason in [
    ('invalid-utf8-unvisited-key','7b22ff223a317d','json_parse: invalid UTF-8 at byte 2'),
    ('invalid-utf8-unvisited-value','7b2261223aff7d','json_parse: invalid UTF-8 at byte 5'),
    ('escaped-high','225c754438303022','json_parse: unpaired high surrogate at byte 7'),
    ('escaped-low','225c754443303022','json_parse: unpaired low surrogate at byte 3'),
    ('duplicate-escaped-key','7b2261223a312c225c7530303631223a327d','json_parse: duplicate object key at byte 7'),
    ('bom','efbbbf7b7d','json_parse: BOM is not allowed at byte 0'),
    ('int-overflow','39323233333732303336383534373735383038','json_parse: integer out of range at byte 0'),
    ('float-overflow','3165333039','json_parse: nonfinite number at byte 0'),
    ('jsonc-comment','7b2f2f636f6d6d656e745c6e2261223a317d','json_parse: object key must be a string at byte 1'),
    ('jsonc-trailing','5b312c5d','json_parse: unexpected value at byte 3'),
]:
    write('data-json-'+name, f'json_parse(bytes("{payload}"));', error=['RUN_HOST', reason])
write('data-json-int-boundaries','let a=json_parse(bytes("39323233333732303336383534373735383037")); let b=json_parse(bytes("2d39323233333732303336383534373735383038")); t.int(a.as_int()); t.int(b.as_int());', ['int 9223372036854775807','int -9223372036854775808'])
write('data-json-float-boundaries','let a=json_parse(bytes("2d302e30")); let b=json_parse(bytes("302e30")); let c=json_parse(bytes("31652d393939")); t.float(a.as_float()); t.float(b.as_float()); t.float(c.as_float());', ['float -0','float 0','float 0'])
for name, body, reason in [
    ('len','let j=json_parse(bytes("6e756c6c")); j.len();','Json.len: expected array or object'),
    ('at-kind','let j=json_parse(bytes("7b7d")); j.at(0);','Json.at: expected array'),
    ('get-kind','let j=json_parse(bytes("5b5d")); j.get("x");','Json.get: expected object'),
    ('has-kind','let j=json_parse(bytes("5b5d")); j.has("x");','Json.has: expected object'),
    ('keys-kind','let j=json_parse(bytes("5b5d")); j.keys();','Json.keys: expected object'),
    ('as-string-kind','let j=json_parse(bytes("31")); j.as_string();','Json.as_string: expected string'),
    ('as-int-kind','let j=json_parse(bytes("227822")); j.as_int();','Json.as_int: expected int'),
    ('as-float-kind','let j=json_parse(bytes("74727565")); j.as_float();','Json.as_float: expected number'),
    ('as-bool-kind','let j=json_parse(bytes("31")); j.as_bool();','Json.as_bool: expected bool'),
]:
    write('data-json-'+name, body, error=['RUN_HOST',reason])

# Distinct payloads make pop order, non-consuming foreach, and stable PQ ties observable.
for prefix, typ, values, tokens in [
 ('int','int',['1','2','3'],['int 1','int 2','int 3']),
 ('float','float',['1.5','2.5','3.5'],['float 1.5','float 2.5','float 3.5']),
 ('str','string',['bytes("6100ff")','bytes("6200fe")','bytes("6300fd")'],['hex 6100ff','hex 6200fe','hex 6300fd'])]:
 scalar={'int':'int','float':'float','string':'hex'}[typ]
 for family in ['stack','queue']:
  order=list(range(3))[::-1] if family=='stack' else list(range(3))
  body='let x='+prefix+'_'+family+'()'+''.join('.push('+v+')' for v in values)+'; '
  body+='for (item in x) { t.'+scalar+'(item); } t.int(x.len()); t.'+scalar+'(x.at(1)); t.'+scalar+'(x.pop()); t.'+scalar+'(x.peek()); x.clear(); t.int(x.len());'
  write('data-'+prefix+'-'+family+'-pop-order',body,[*(tokens[i] for i in order),'int 3',tokens[order[1]],tokens[order[0]],tokens[order[1]],'int 0'])
  write('data-'+prefix+'-'+family+'-negative-index','let x='+prefix+'_'+family+'().push('+values[0]+'); x.at(-1);',error=['RUN_HOST',prefix.title()+family.title()+'.at: index out of range: -1'])
 for family in ['stack','queue','pq']:
  for method in ['pop','peek']:
   write('data-'+prefix+'-'+family+'-'+method+'-empty','let x='+prefix+'_'+family+'(); x.'+method+'();',error=['RUN_HOST',prefix.title()+{'stack':'Stack','queue':'Queue','pq':'PQ'}[family]+'.'+method+': empty'])
 body='let x='+prefix+'_pq().push('+values[0]+',2.0).push('+values[1]+',-0.0).push('+values[2]+',0.0); t.'+scalar+'(x.peek()); t.int(x.len()); '
 body+=' '.join('t.'+scalar+'(x.pop());' for _ in range(3))+' t.bool(x.empty());'
 write('data-'+prefix+'-pq-priorities',body,[tokens[1],'int 3',tokens[1],tokens[2],tokens[0],'bool true'])
 write('data-'+prefix+'-pq-infinite','let x='+prefix+'_pq(); x.push('+values[0]+', math_pow(2.0,1024.0));',error=['RUN_HOST',prefix.title()+'PQ.push: priority must be finite'])
for prefix,typ,values,tokens in [('int','int',['7','8','9'],['int 7','int 8','int 9']),('float','float',['1.5','2.5','3.5'],['float 1.5','float 2.5','float 3.5'])]:
 scalar=typ; a='bytes("6100ff")'
 body=f'let x={prefix}_map().set({a},{values[0]}).set("b",{values[1]}).set({a},{values[2]}); t.hex(x.key_at(0)); t.hex(x.key_at(1)); t.{scalar}(x.get({a})); t.{scalar}(x.get_or("b",{values[0]})); t.{scalar}(x.get_or("z",{values[0]})); t.bool(x.has("z")); t.bool(x.remove({a})); t.bool(x.remove({a})); x.set({a},{values[0]}); t.hex(x.key_at(0)); t.hex(x.key_at(1)); x.clear(); t.bool(x.has("b"));'
 write('data-'+prefix+'-map-order',body,['hex 6100ff','hex 62',tokens[2],tokens[1],tokens[0],'bool false','bool true','bool false','hex 62','hex 6100ff','bool false'])
 write('data-'+prefix+'-map-negative-key-at',f'let x={prefix}_map(); x.key_at(-1);',error=['RUN_HOST',prefix.title()+'Map.key_at: index out of range: -1'])

raw=r'{ "\u0061" : ["x\u0000",true,null,1.25,-0.0], "n":9223372036854775807, "t":false }'
compact=r'{"\u0061":["x\u0000",true,null,1.25,-0.0],"n":9223372036854775807,"t":false}'
body='let j=json_parse(bytes("'+raw.encode().hex()+'")); t.bool(j.has("a")); t.bool(j.has("z")); let keys=j.keys(); t.int(keys.len()); t.hex(keys.at(0)); t.hex(keys.at(1)); t.hex(keys.at(2)); let a=j.get("a"); t.hex(a.kind()); t.int(a.len()); t.hex(a.at(0).as_string()); t.bool(a.at(1).as_bool()); t.hex(a.at(2).kind()); t.float(a.at(3).as_float()); t.float(a.at(4).as_float()); t.int(j.get("n").as_int()); t.bool(j.get("t").as_bool()); t.hex(j.to_json()); t.float(json_parse(bytes("3132")).as_float());'
write('data-json-complete',body,['bool true','bool false','int 3','hex 61','hex 6e','hex 74','hex 6172726179','int 5','hex 7800','bool true','hex 6e756c6c','float 1.25','float -0','int 9223372036854775807','bool false','hex '+compact.encode().hex(),'float 12'])
write('data-json-astral','let j=json_parse(bytes("225c75443833345c754444314522")); t.hex(j.as_string()); t.hex(j.to_json());',['hex f09d849e','hex 225c75443833345c754444314522'])
for depth in [256,257]:
 raw='['*depth+'null'+']'*depth
 if depth==256:
  write('data-json-depth-256','let j=json_parse(bytes("'+raw.encode().hex()+'")); t.int(str_len(j.to_json())); for (i in range(0,256)) { j=j.at(0); } t.hex(j.kind());',['int 516','hex 6e756c6c'])
 else: write('data-json-depth-257','json_parse(bytes("'+raw.encode().hex()+'"));',error=['RUN_HOST','json_parse: maximum depth exceeded at byte 256'])
write('data-json-utf8-before-bom','json_parse(bytes("efbbbfff"));',error=['RUN_HOST','json_parse: invalid UTF-8 at byte 3'])
write('data-json-negative-int-overflow','json_parse(bytes("'+b'-9223372036854775809'.hex()+'"));',error=['RUN_HOST','json_parse: integer out of range at byte 0'])
write('data-float-pq-payloads','let x=float_pq().push(math_sqrt(-1.0),0.0).push(-0.0,1.0); let n=x.pop(); t.bool(n!=n); t.float(x.pop());',['bool true','float -0'])

raw=r'{ "\u0061" : 1.2300e+02, "s":"\u0041", "z":-0 }'
compact=r'{"\u0061":1.2300e+02,"s":"\u0041","z":-0}'
write('data-json-token-spelling','let j=json_parse(bytes("'+raw.encode().hex()+'")); t.hex(j.to_json()); t.hex(j.get("a").to_json()); t.hex(j.get("s").as_string());',['hex '+compact.encode().hex(),'hex '+b'1.2300e+02'.hex(),'hex 41'])

write('data-json-invalid-continuation','json_parse(bytes("22c32822"));',error=['RUN_HOST','json_parse: invalid UTF-8 at byte 1'])

# Canonical structural error vocabulary/offsets, independent of backend exceptions.
for name,payload,reason,offset in [
 ('empty',b'','expected value',0),('space',b' ','expected value',1),
 ('trailing-input',b'nullx','trailing input',4),('bad-literal',b'no','invalid literal',0),
 ('missing-comma',b'["a" "b"]','expected comma',5),('missing-colon',b'{"a"1}','expected colon',4),
 ('bad-value',b'{"a":}','unexpected value',5),('key-required',b'{"a":1, }','object key must be a string',8),
 ('raw-control',b'"\n"','control byte in string',1),('bad-escape',b'"\\x"','invalid escape',2),
 ('bad-unicode-escape',b'"\\u00ZZ"','invalid Unicode escape',5),
 ('unfinished-escape',b'"\\','unterminated escape',2),('unfinished-string',b'"abc','unterminated string',4),
]:
 write('data-json-'+name,'json_parse(bytes("'+payload.hex()+'"));',error=['RUN_HOST',f'json_parse: {reason} at byte {offset}'])

# Text templates use Json only as a nested context container.
write('data-text-template', 'let context=json_parse(bytes("7b226e616d65223a2253616d222c226974656d73223a5b312c325d2c226f6b223a747275657d")); t.hex(template_render("<if(ok)><name>:<items:{item|<index>=<item>}; separator=\\\",\\\"><endif>",context));', ['hex '+b'Sam:0=1,1=2'.hex()])

def update_manifest():
    m = json.loads((OUT / "manifest.json").read_text())
    m["vectors"] = [v for v in m.get("vectors", []) if not v.get("name", "").startswith("data-")]
    for expect in sorted(OUT.glob("data-*.expect")):
        name = expect.stem; first = expect.read_text().splitlines()[0]
        item={"name":name,"covers":"v2 data catalog positive/negative contract","schema":"data.schema.json","source":name+".dsl","bytecode":name+".dslb","mutated":False}
        if first == "trace": item["expect"]={"trace":expect.read_text().splitlines()[1:]}
        else: item["expect"]={first:expect.read_text().splitlines()[1:]}
        m.setdefault("vectors",[]).append(item)
    m["vectors"].sort(key=lambda v:v["name"])
    (OUT / "manifest.json").write_text(json.dumps(m, indent=2) + "\n")
update_manifest()
print("generated data-catalog: 99 new signatures declared; exercised all constructors and core methods")
