// One slot spans body and entries. Controlled errors use the same terminal
// envelope boundary as success; host validation happens before publication.
#include "../include/dslvm.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdio>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
static std::string read(const std::string& path){std::ifstream in(std::filesystem::u8path(path),std::ios::binary);if(!in)throw dslvm::Error("LOAD_BAD_BYTECODE","cannot read fixture");std::ostringstream out;out<<in.rdbuf();return out.str();}
static int run_cli(int argc,char**argv){
#ifdef _WIN32
 _setmode(_fileno(stdout),_O_BINARY);
#endif
 using namespace dslvm;
 if(argc<3)return failure_frame("cli",Error("CLI_USAGE","fixture arguments required"));
 bool rounding=false,reset=false,early=false,app_fail=false;int repeat=1;
 for(int i=3;i<argc;++i){std::string flag=argv[i];if(flag=="--mutate-rounding")rounding=true;else if(flag=="--mutate-reset")reset=true;else if(flag=="--mutate-early-frame")early=true;else if(flag=="--app-fail")app_fail=true;else if(flag=="--repeat"&&i+1<argc)repeat=std::stoi(argv[++i]);else return failure_frame("cli",Error("CLI_USAGE","invalid fixture flag"));}
 if(repeat<1)return failure_frame("cli",Error("CLI_USAGE","invalid repeat"));
 auto results=InvocationResults::from_command({argv[1]},std::nullopt);Registry registry;auto& sink=results.select(0,registry,{});std::string phase="load";
 try{
  registry.function("entry_fail",{},"void",[](Args&)->Value{throw Error("entry failure");});
  registry.function("expect_result",{"string"},"void",[&](Args&a){if(sink.json()!=a[0].s)throw Error("return overload payload mismatch");return Value::none();});
  auto program=Program::load(read(argv[1]),read(argv[2]),registry);Context context;phase="run";program.run(context,registry);
  if(early)std::cout<<invocation_detail::framed("{\"ok\":true,\"result\":"+sink.json()+"}"); // deliberate host fault
  for(int n=0;n<repeat;++n)for(size_t i=0;i<program.entries().size();++i){if(reset)sink.reset();const auto value=program.call(i,context,registry,{});if(value.kind!=Value::INT||value.i!=7)throw Error("entry HALT result changed");}
  results.complete(0);
  if(app_fail)results.fail(std::nullopt,"app",Error("APP_FAIL","application validation failed"));
  if(rounding){auto number=stdlib::json_detail::Parser(sink.json()).val();char legacy[64];std::snprintf(legacy,sizeof legacy,"%.15g",number.floating);std::cout<<invocation_detail::framed(std::string("{\"ok\":true,\"result\":")+legacy+"}");return std::cout?0:4;}
 }catch(const Error&e){std::cerr<<e.what()<<'\n';results.fail(0,phase,e);}
 return results.write_frame(std::cout);
}

#include "native_invocation_args.hpp"
