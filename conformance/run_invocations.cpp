// Canonical native adapter for existing untyped v6 programs. Host R6 adapters
// adopt this lifecycle; this fixture runner deliberately has no public ABI flags.
#include "../include/dslvm.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
static std::string read(const std::string& file){std::ifstream f(std::filesystem::u8path(file),std::ios::binary);if(!f)throw dslvm::Error("LOAD_BAD_BYTECODE","cannot read fixture");std::ostringstream o;o<<f.rdbuf();return o.str();}
static int run_cli(int argc,char**argv){
#ifdef _WIN32
 _setmode(_fileno(stdout),_O_BINARY);
#endif
 using namespace dslvm;
 if(argc<4)return failure_frame("cli",Error("CLI_USAGE","fixture arguments required"));
 try{
  auto config=invocation_detail::parse(read(argv[2]));
  std::vector<std::string> files;for(int i=3;i<argc;++i)files.emplace_back(argv[i]);
  auto results=InvocationResults::from_command(files);
  Registry registry;Context context;int64_t counter=0;
  registry.function("next_counter",{},"int",[&](Args&){return Value::integer(++counter);});
  registry.function("entry_fail",{},"void",[](Args&)->Value{throw Error("native entry failure");});
  const std::string schema=read(argv[1]);const auto contract=ResultContract::from_schema(schema);
  const auto* order=invocation_detail::member(config,"execution");
  for(const auto& entry:order->array){
   const auto index=static_cast<size_t>(entry.integer);std::string phase="load";
   try{
    const auto& registrations=results.registrations();auto registration=std::find_if(registrations.begin(),registrations.end(),[&](const auto&r){return r.index==index;});
    if(registration==registrations.end())throw Error("CLI_USAGE","fixture index absent");
    auto program=Program::load(read(registration->file),schema,registry);
    results.select(index,registry,contract);phase="run";program.run(context,registry);
    for(size_t i=0;i<program.entries().size();++i){auto value=program.call(i,context,registry,{});if(value.kind!=Value::INT||value.i!=7)throw Error("entry HALT result changed");}
    results.complete(index);
   }catch(const Error&e){results.fail(index,phase,e);break;}
  }
  if(const auto* app=invocation_detail::member(config,"app_fail");app&&app->boolean)results.fail(std::nullopt,"app",Error("APP_FAIL","native validation failed"));
  return results.write_frame(std::cout);
 }catch(const Error&e){return failure_frame("cli",e);}
}

#include "native_invocation_args.hpp"
