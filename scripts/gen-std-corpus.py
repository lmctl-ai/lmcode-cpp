#!/usr/bin/env python3
"""Seeded byte-level standard corpus with an independent Python oracle."""
import argparse
import functools
import json
import math
import os
from pathlib import Path
import random
import re
import subprocess
import tempfile
from decimal import Decimal

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SEEDS = [7, 42, 20261024]
I64_MIN, I64_MAX = -(1 << 63), (1 << 63) - 1

def literal(value):
    return 'bytes("' + value.hex() + '")'

def integer(value):
    if value == I64_MAX: return '(9007199254740991 * 1024 + 1023)'
    if value == I64_MIN: return '(-9007199254740991 * 1024 - 1024)'
    return str(value)

def glob(text, pattern):
    """Interpret small patterns as tokens, then use a memoized language matcher."""
    tokens = []; i = 0
    while i < len(pattern):
        c = pattern[i]; i += 1
        if c == 42: tokens.append(('star', None))
        elif c == 63: tokens.append(('any', None))
        elif c == 92:
            if i < len(pattern): c = pattern[i]; i += 1
            tokens.append(('literal', c))
        elif c == 91:
            end = pattern.find(b']', i + (1 if pattern[i:i+1] in (b'!', b'^', b']') else 0))
            if end < 0: tokens.append(('literal', c)); continue
            body = pattern[i:end]; i = end + 1
            neg = body[:1] in (b'!', b'^')
            if neg: body = body[1:]
            choices = set(); j = 0
            while j < len(body):
                if j + 2 < len(body) and body[j+1] == 45:
                    choices.update(range(body[j], body[j+2]+1)); j += 3
                else: choices.add(body[j]); j += 1
            tokens.append(('class', (neg, choices)))
        else: tokens.append(('literal', c))
    @functools.lru_cache(None)
    def accepts(t, p):
        if p == len(tokens): return t == len(text)
        kind, arg = tokens[p]
        if kind == 'star': return accepts(t, p+1) or (t < len(text) and accepts(t+1, p))
        if t == len(text): return False
        matches = kind == 'any' or (kind == 'literal' and text[t] == arg) or (kind == 'class' and ((text[t] in arg[1]) != arg[0]))
        return matches and accepts(t+1, p+1)
    return accepts(0, 0)

def numeric(raw):
    is_int = re.fullmatch(rb'[+-]?[0-9]+', raw) is not None
    number = int(raw) if is_int else 0
    is_int = is_int and I64_MIN <= number <= I64_MAX
    is_float = re.fullmatch(rb'[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?', raw) is not None
    value = float(raw) if is_float else 0.0
    is_float = is_float and math.isfinite(value) and (value != 0 or Decimal(raw.decode()) == 0)
    return is_int, number if is_int else 0, is_float, value if is_float else 0.0

def schema():
    s = json.loads((ROOT / 'std/lmcode-std.schema.json').read_text())
    s['name'] = 'stdlib_seeded_corpus'
    s['types']['Trace'] = {'methods': {name: [{'params':[typ], 'returns':'void'}] for name, typ in [('int','int'), ('float','float'), ('hex','string'), ('bool','bool')]}}
    s['context'] = {'t':'Trace'}
    s['functions'].update({'bytes':[{'params':['string'],'returns':'string'}], 'test_temp_dir':[{'params':[],'returns':'string'}]})
    return json.dumps(s, indent=2) + '\n'

class Case:
    def __init__(self): self.source = []; self.expected = []
    def emit(self, text): self.source.append(text)
    def trace(self, expression, value, kind=None):
        kind = kind or ('bool' if isinstance(value,bool) else 'int' if isinstance(value,int) else 'float' if isinstance(value,float) else 'hex')
        self.emit(f't.{kind}({expression});')
        rendered = value.hex() if kind == 'hex' else str(value).lower() if kind == 'bool' else format(value,'.15g') if kind == 'float' else str(value)
        # A sentinel prevents trailing whitespace being stripped from empty byte traces.
        if kind == 'hex':
            self.source[-1] = f't.hex({expression} + bytes("00"));'
            rendered += '00'
        self.expected.append(kind + ' ' + rendered)

def random_bytes(rng):
    alphabet = [0, 255, 128, 10, 13, 9, 32, 97, 98, 99, 65, 90, 195, 169, 42, 63, 91, 92]
    return bytes(rng.choice(alphabet) for _ in range(rng.randrange(0, 25)))

def success(seed, count):
    rng = random.Random(seed); c = Case()
    c.emit('let xs = str_list(); let ns = int_list(); let fs = float_list(); let m = str_map();')
    c.emit('let path = test_temp_dir() + "/corpus.bin";')
    xs = []; ns = []; fs = []; m = {}
    parse_edges = [b'',b'-0',b'+0',b'9223372036854775807',b'9223372036854775808',b'-9223372036854775808',b'1e309',b'1e-999',b'5e-324',b'.5',b'1.',b' 1',b'1\0',b'nan',b'0x10',b'+.0']
    for i in range(count):
        a, b, repl = random_bytes(rng), random_bytes(rng), random_bytes(rng)
        if i % 3 == 0: b = a[rng.randrange(len(a)+1):] if a else b''
        start = rng.choice([-9,-1,0,len(a),len(a)+1,2147483647,I64_MAX,I64_MIN]); n = rng.choice([-3,0,1,10,I64_MAX])
        s, p, r = literal(a), literal(b), literal(repl)
        for name, value in [('str_len',len(a)),('str_lower',a.lower()),('str_upper',a.upper()),('str_trim',a.strip(b' \t\n\v\f\r'))]: c.trace(f'{name}({s})',value)
        for name, value in [('str_starts_with',a.startswith(b)),('str_ends_with',a.endswith(b)),('str_contains',b in a),('str_find',a.find(b))]: c.trace(f'{name}({s}, {p})',value)
        c.trace(f'str_substr({s}, {integer(start)}, {integer(n)})', a[max(0,start):max(0,start)+max(0,n)])
        c.trace(f'str_replace({s}, {p}, {r})', a.replace(b,repl) if b else a)
        repeat = rng.randrange(4); c.trace(f'str_repeat({s}, {repeat})', a*repeat)
        pattern = rng.choice([b'*',b'?',b'*a*',b'[a-z]*',b'[!ab]*',b'[z-a]',b'\\*',b'[',b'[]a]',b'*?*'])
        c.trace(f'str_match({s}, {literal(pattern)})',glob(a,pattern))
        num = parse_edges[i % len(parse_edges)] if i < len(parse_edges) else (f'{rng.randrange(-999999,999999)}.{rng.randrange(1000):03}e{rng.randrange(-20,20):+}').encode()
        ni, nv, nf, fv = numeric(num)
        for name,value in [('str_is_int',ni),('str_to_int',nv),('str_is_float',nf),('str_to_float',fv)]: c.trace(f'{name}({literal(num)})',value)
        c.emit(f'xs.add({s}).add({p});'); xs.extend([a,b])
        index = rng.randrange(len(xs)); xs[index] = repl; c.emit(f'xs.set({index}, {r});')
        c.trace('xs.len()',len(xs)); c.trace(f'xs.at({index})',xs[index]); c.trace(f'xs.contains({p})',b in xs); c.trace(f'xs.index_of({p})',xs.index(b) if b in xs else -1)
        c.trace(f'xs.join({literal(bytes([0,255]))})',bytes([0,255]).join(xs))
        v = rng.randrange(-100000,100000); ns.append(v); fs.append(v/2)
        c.emit(f'ns.add({v}); fs.add({v}.0 / 2.0);')
        c.trace(f'ns.at({len(ns)-1})',v); c.trace(f'fs.at({len(fs)-1})',v/2)
        c.trace(f'ns.index_of({v})',ns.index(v)); c.trace(f'fs.contains({v}.0 / 2.0)',True)
        # Small key pool deliberately exercises replacement, removal and reinsertion.
        key = rng.choice([b'',b'\0',b'\xff',b'a',b'b',b'\xc3\xa9']); k = literal(key)
        if rng.randrange(3):
            c.emit(f'm.set({k}, {s});'); m[key] = a
        else:
            c.trace(f'm.remove({k})',key in m); m.pop(key,None)
        c.trace(f'm.has({k})',key in m); c.trace(f'm.get_or({k}, {r})',m.get(key,repl)); c.trace('m.len()',len(m))
        for j,(mk,mv) in enumerate(m.items()): c.trace(f'm.key_at({j})',mk); c.trace(f'm.get({literal(mk)})',mv)
        sep = rng.choice([b'\0',b'\n',b'aa',b'\xff']); parts=a.split(sep)
        c.emit(f'let parts{i} = str_split({s}, {literal(sep)});');c.trace(f'parts{i}.len()',len(parts));c.trace(f'parts{i}.join({literal(sep)})',a)
        c.emit(f'file_write(path, {s}); file_append(path, {p});')
        c.trace('file_read(path)',a+b);c.trace('file_exists(path)',True)
        lines=(a+b).split(b'\n');c.emit(f'let lines{i} = file_lines(path);');c.trace(f'lines{i}.len()',len(lines));c.trace(f'lines{i}.join(bytes("0a"))',a+b)
        if i % 7 == 6:
            c.emit('xs.clear(); ns.clear(); fs.clear(); m.clear();');xs.clear();ns.clear();fs.clear();m.clear()
    return c

def cases(seeds, rounds):
    out = {}
    for seed in seeds:
        c=success(seed,rounds);out[f'fuzz-{seed:010d}']=(c.source,['trace',*c.expected])
        errors=[('split-empty','str_split(bytes("00ff"), "");','str_split: separator must not be empty'),('repeat-negative','str_repeat(bytes("00ff"), -1);','str_repeat: count must be nonnegative and result at most 67108864 bytes'),('map-missing','let m = str_map(); m.get(bytes("00ff"));','StrMap.get: missing key')]
        for typ,ctor in [('StrList','str_list'),('IntList','int_list'),('FloatList','float_list')]:
            for index in [-1,I64_MAX,I64_MIN]: errors.append((typ.lower()+str(index),f'let x = {ctor}(); x.at({integer(index)});',f'{typ}.at: index out of range: {index}'))
        for name,source,message in errors: out[f'fuzz-{seed:010d}-{name}']=([source],['runtime_error_exact','RUN_HOST',message])
    c=Case();c.emit('let path = test_temp_dir() + "/large.bin"; let chunk = str_repeat(bytes("00ff0a"), 400001); file_write(path, chunk); file_append(path, bytes("ff00"));')
    c.trace('str_len(file_read(path))',1200005);c.trace('str_substr(file_read(path), 1200000, 5)',bytes.fromhex('00ff0aff00'))
    c.emit('let lines = file_lines(path);');c.trace('lines.len()',400002);c.trace('lines.at(400001)',bytes.fromhex('ff00'));c.emit('file_write(path, "");');c.trace('str_len(file_read(path))',0)
    out['fuzz-large-file']=(c.source,['trace',*c.expected])
    c=Case()
    c.emit('let m = str_map(); m.set(bytes("00ff"), "deleted").set("stable", "v"); m.remove(bytes("00ff"));')
    c.emit('for (i in range(0, 4096)) { m.set(str(i), "v"); m.remove(str(i)); }')
    c.trace('m.len()',1);c.trace('m.has(bytes("00ff"))',False);c.trace('m.key_at(0)',b'stable')
    c.emit('m.set("other", bytes("ff00")).set("stable", bytes("00ff"));')
    c.trace('m.key_at(0)',b'stable');c.trace('m.key_at(1)',b'other');c.trace('m.get("stable")',bytes.fromhex('00ff'))
    c.emit('m.clear(); for (i in range(0, 4096)) { m.set(str(i), "v"); } let good = true;')
    c.emit('for (i in range(0, 4096)) { if (i % 2 == 0) { m.remove(str(i)); good = good && m.key_at(m.len() - 1) == "4095"; } }')
    c.trace('good',True);c.trace('m.len()',2048)
    c.emit('let sum = 0; for (i in range(0, 2048)) { sum = sum + str_to_int(m.key_at(i)); }')
    c.trace('sum',2048**2);c.emit('m.set("0", "reinsert");');c.trace('m.key_at(2048)',b'0');c.trace('m.key_at(0)',b'1')
    out['fuzz-map-churn']=(c.source,['trace',*c.expected]);return out

def generate(directory,seeds,rounds,check=False,compile_bytecode=False):
    generated=cases(seeds,rounds); files={'fuzz.schema.json':schema()}; entries=[]
    for name,(source,expect) in generated.items():
        files[name+'.dsl']='\n'.join(source)+'\n';files[name+'.expect']='\n'.join(expect)+'\n'
        entry={'name':name,'covers':'seeded standard byte/property corpus','schema':'fuzz.schema.json','source':name+'.dsl','bytecode':name+'.dslb','mutated':False,'expect':{'trace':expect[1:]} if expect[0]=='trace' else {'runtime_error_exact':{'code':expect[1],'message':expect[2]}}}
        entries.append(entry)
    manifest_path=directory/'manifest.json'
    manifest=json.loads(manifest_path.read_text()) if manifest_path.exists() else {'format':'dsl-conformance/2','bytecode_version':6,'schema':'fuzz.schema.json','host':{'description':'Canonical std plus bytes(hex), Trace int/float/bool/hex and a per-vector test_temp_dir fixture.'},'vectors':[]}
    manifest['vectors']=sorted([v for v in manifest['vectors'] if not v['name'].startswith('fuzz-')]+entries, key=lambda v:v['name'])
    files['manifest.json']=json.dumps(manifest,indent=2)+'\n'
    for name,content in files.items():
        path=directory/name
        if check:
            if not path.exists() or path.read_text()!=content: raise SystemExit('stale corpus: '+str(path))
        else: directory.mkdir(parents=True,exist_ok=True);path.write_text(content)
    if compile_bytecode:
        compiler=os.environ.get('LMSCRIPT_BIN','lmscript')
        for name in generated:
            wrapper='program = _dsl_ schema='+json.dumps(str((directory/'fuzz.schema.json').resolve()))+';\n'+files[name+'.dsl']+'end _dsl_\nsavedsl(program, '+json.dumps(str((directory/(name+'.dslb')).resolve()))+');\n'
            with tempfile.TemporaryDirectory(prefix='corpus-compile-') as tmp:
                lm=Path(tmp)/'compile.lm';lm.write_text(wrapper);subprocess.run([compiler,str(lm)],check=True,stdout=subprocess.DEVNULL)
    print(f'{len(generated)} corpus vectors; seeds={seeds}; rounds={rounds}')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'conformance/stdlib-vectors');p.add_argument('--seeds',default=','.join(map(str,DEFAULT_SEEDS)));p.add_argument('--rounds',type=int,default=16);p.add_argument('--check',action='store_true');p.add_argument('--compile',action='store_true');a=p.parse_args()
    if a.check and a.compile: p.error('--check and --compile are mutually exclusive')
    if not 1 <= a.rounds <= 64: p.error('rounds must be 1..64 per program')
    generate(a.output.resolve(),[int(s) for s in a.seeds.split(',')],a.rounds,a.check,a.compile)
