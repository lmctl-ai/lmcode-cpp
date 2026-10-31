#!/usr/bin/env python3
"""Compile isolated runtime faults; only genuine assertion failures count as kills."""
import os,shlex,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
compiler=shlex.split(os.environ.get('CXX','c++'))
with tempfile.TemporaryDirectory(prefix='return-controls-',dir=ROOT/'build') as temporary:
 work=Path(temporary)
 for name in ['include','stdlib']:shutil.copytree(ROOT/name,work/name)
 (work/'tests').mkdir()
 for name in ['result_sink_test.cpp','result_json_test.cpp']:shutil.copy2(ROOT/'tests'/name,work/'tests'/name)
 originals={name:(work/name).read_text() for name in ['stdlib/result.hpp','stdlib/std.hpp','stdlib/result_json.hpp']}
 def run(unit,case=None):
  binary=work/'control'
  compiled=subprocess.run([*compiler,'-std=c++17','-O0','-I',str(work/'include'),str(work/'tests'/unit),'-o',str(binary)],capture_output=True)
  assert compiled.returncode==0,compiled.stderr.decode(errors='replace')
  result=subprocess.run([str(binary),*([case] if case else [])],capture_output=True)
  return result
 # Run the complete unmutated units first, so dependency/compiler errors cannot
 # masquerade as a caught fault, then select a safe single case per mutation.
 for unit in ['result_sink_test.cpp','result_json_test.cpp']:
  baseline=run(unit);assert baseline.returncode==0,(unit,baseline.stdout,baseline.stderr)
 print('PASS return-control baselines')
 faults=[
  ('arena-owned result lost at teardown','stdlib/std.hpp','  std_runtime_->collections.reset();\n  std_runtime_->running.store', '  if(std_runtime_->result_sink) std_runtime_->result_sink->reset();\n  std_runtime_->collections.reset();\n  std_runtime_->running.store','result_sink_test.cpp','captured values outlive'),
  ('reset-between-calls','stdlib/std.hpp','  std_runtime_->collections.reset();\n}\ninline void Registry::end_std_run','  std_runtime_->collections.reset();\n  if(std_runtime_->result_sink) std_runtime_->result_sink->reset();\n}\ninline void Registry::end_std_run','result_sink_test.cpp','captured values outlive'),
  ('source-mutation alias','stdlib/result.hpp','  stdlib::JsonNode root_;','  std::shared_ptr<Value> borrowed_;\n  stdlib::JsonNode root_;','result_sink_test.cpp','source mutation cannot'),
  ('missing configured sink','stdlib/result.hpp','if(!configured) throw Error("lmcode return: no result sink");','if(!configured) return Value::none();','result_sink_test.cpp','missing sink'),
  ('bad retained numeric token','stdlib/result_json.hpp','(!n.raw.empty() && decimal_unsafe_integer(n.raw,path))','false','result_json_test.cpp','retained raw tokens'),
 ]
 for name,path,anchor,replacement,unit,case in faults:
  for file,text in originals.items():(work/file).write_text(text)
  source=originals[path];assert anchor in source,('missing anchor',name)
  source=source.replace(anchor,replacement)
  if name=='source-mutation alias':
   source=source.replace('void set(const Value& value) { root_=', 'void set(const Value& value) { borrowed_=std::make_shared<Value>(value); root_=')
   source=source.replace('return stdlib::result_detail::encode(root_);','return borrowed_ ? stdlib::result_detail::encode(stdlib::result_detail::copy_value(*borrowed_)) : stdlib::result_detail::encode(root_);')
  (work/path).write_text(source)
  result=run(unit,case)
  assert result.returncode==1 and b'FAIL ' in result.stderr,(name,result.returncode,result.stdout,result.stderr)
  print('CAUGHT',name)
 for file,text in originals.items():(work/file).write_text(text)
 path=work/'stdlib/result_json.hpp';source=originals['stdlib/result_json.hpp']
 start=source.index('auto r=std::to_chars(');end=source.index('return std::string(b,r.ptr);',start)+len('return std::string(b,r.ptr);')
 source=source[:start]+'std::snprintf(b,sizeof b,"%.15g",v);return std::string(b);'+source[end:]
 path.write_text(source)
 result=run('result_sink_test.cpp','numeric boundary and shortest')
 assert result.returncode==1 and b'float formatter used %.15g' in result.stderr,(result.returncode,result.stdout,result.stderr)
 print('CAUGHT actual %.15g framing formatter')
