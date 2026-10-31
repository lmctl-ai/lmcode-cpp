#!/usr/bin/env python3
"""Compile real SDK faults; compile errors/signals never count as catches."""
import os,shlex,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];cxx=shlex.split(os.environ.get('CXX','c++'))
with tempfile.TemporaryDirectory(prefix='invocation-faults-',dir=ROOT/'build')as tmp:
 work=Path(tmp)
 for folder in ['include','stdlib']:shutil.copytree(ROOT/folder,work/folder)
 (work/'tests').mkdir();shutil.copy2(ROOT/'tests/invocation_results_test.cpp',work/'tests/invocation_results_test.cpp')
 paths=['stdlib/invocation.hpp','stdlib/result.hpp','stdlib/result_json.hpp','stdlib/result_contract.hpp']
 original={p:(work/p).read_text()for p in paths}
 def run(case=''):
  binary=work/'unit'
  p=subprocess.run([*cxx,'-std=c++17','-O0','-I',str(work/'include'),str(work/'tests/invocation_results_test.cpp'),'-o',str(binary)],capture_output=True)
  assert p.returncode==0,p.stderr.decode(errors='replace')
  return subprocess.run([str(binary),case],capture_output=True)
 baseline=run();assert baseline.returncode==0,(baseline.stdout,baseline.stderr)
 print('PASS invocation fault baseline',flush=True)
 faults=[
 ('swap-order','stdlib/invocation.hpp','encode(*slots_[i].result)','encode(*slots_[slots_.size()-1-i].result)','reverse execution'),
 ('reset-on-reselect','stdlib/invocation.hpp','registry.set_result_sink(s.sink.get());','s.sink->reset();registry.set_result_sink(s.sink.get());','sealed sinks'),
 ('missing-completion','stdlib/invocation.hpp','if(!failure_)for(size_t i=0;i<slots_.size();++i)if(!slots_[i].result){fail(i,"run",Error("lmcode invocation: slot not completed"));break;}','if(!failure_)for(auto& s:slots_)if(!s.result)s.result=stdlib::JsonNode{};','unfinished slot'),
 ('partial-result-on-error','stdlib/invocation.hpp','if(failure_){payload=','if(failure_)for(auto& s:slots_)if(!s.result)s.result=stdlib::JsonNode{};\n    if(false&&failure_){payload=','first failure'),
 ('missing-seal','stdlib/result.hpp','sealed_=true;','sealed_=false;','sealed sinks'),
 ('wrong-part','stdlib/invocation.hpp','builder_?index:std::nullopt','builder_?std::optional<size_t>(0):std::nullopt','first failure'),
 ('missing-flush','stdlib/invocation.hpp','out.flush();','/* omitted flush */','flush failure'),
 ('dropped-contract','stdlib/result_contract.hpp','if(type_.empty())return;','return; /* dropped contract */','contract immutability'),
 ]
 faults.append(('shared-sink','stdlib/invocation.hpp','s.sink=std::make_unique<ResultSink>(contract);','if(!shared_sink_){shared_sink_=std::make_shared<ResultSink>(contract);}s.sink=shared_sink_;','reverse execution'))
 faults.append(('per-part-frames','stdlib/invocation.hpp','writing_=true;return invocation_detail::emit(out,payload,code);','if(builder_&&!failure_){writing_=true;for(auto& s:slots_)invocation_detail::emit(out,"{\\"ok\\":true,\\"result\\":"+stdlib::result_detail::encode(*s.result)+"}",0);return 0;}\n    writing_=true;return invocation_detail::emit(out,payload,code);','one frame and shortest'))
 for name,path,anchor,replacement,case in faults:
  for p,text in original.items():(work/p).write_text(text)
  text=original[path];assert text.count(anchor)==1,(name,'anchor',text.count(anchor));text=text.replace(anchor,replacement)
  if name=='shared-sink':
   text=text.replace('std::unique_ptr<ResultSink> sink;','std::shared_ptr<ResultSink> sink;').replace('  bool builder_=', '  std::shared_ptr<ResultSink> shared_sink_;\n  bool builder_=')
  (work/path).write_text(text)
  result=run(case);assert result.returncode==1 and b'FAIL 'in result.stderr,(name,result.returncode,result.stdout,result.stderr)
  print('CAUGHT',name,flush=True)
 for p,text in original.items():(work/p).write_text(text)
 path=work/'stdlib/result_json.hpp';text=original['stdlib/result_json.hpp'];start=text.index('auto r=std::to_chars(');end=text.index('return std::string(b,r.ptr);',start)+len('return std::string(b,r.ptr);')
 path.write_text(text[:start]+'std::snprintf(b,sizeof b,"%.15g",v);return std::string(b);'+text[end:])
 result=run('one frame and shortest');assert result.returncode==1 and b'0.3,'in result.stderr,(result.stdout,result.stderr)
 print('CAUGHT actual %.15g envelope formatter',flush=True)
