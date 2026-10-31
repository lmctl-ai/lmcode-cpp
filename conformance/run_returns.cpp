// Canonical host contract: one sink across body + selected entry calls. The
// library never prints; only this host's final success branch emits a frame.
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
static std::string read(const std::string& path) {
  std::ifstream in(std::filesystem::u8path(path),std::ios::binary);
  if(!in)throw dslvm::Error("LOAD_BAD_BYTECODE","cannot read fixture");
  std::ostringstream out;out<<in.rdbuf();return out.str();
}
int main(int argc,char** argv) {
#ifdef _WIN32
  _setmode(_fileno(stdout),_O_BINARY); // Protocol frames require LF on every OS.
#endif
  if(argc<3)return 2;
  bool rounding=false,reset=false,early=false,app_fail=false;int repeat=1;
  for(int i=3;i<argc;++i) {
    std::string flag=argv[i];
    if(flag=="--mutate-rounding")rounding=true;
    else if(flag=="--mutate-reset")reset=true;
    else if(flag=="--mutate-early-frame")early=true;
    else if(flag=="--app-fail")app_fail=true;
    else if(flag=="--repeat"&&i+1<argc)repeat=std::stoi(argv[++i]);
    else return 2;
  }
  if(repeat<1)return 2;
  try {
    dslvm::Registry registry;dslvm::ResultSink sink;registry.set_result_sink(&sink);
    registry.function("entry_fail",{},"void",[](dslvm::Args&)->dslvm::Value{throw dslvm::Error("entry failure");});
    registry.function("expect_result",{"string"},"void",[&](dslvm::Args& a){
      if(sink.json()!=a[0].s)throw dslvm::Error("return overload payload mismatch");
      return dslvm::Value::none();
    });
    auto program=dslvm::Program::load(read(argv[1]),read(argv[2]),registry);
    dslvm::Context context;sink.reset();program.run(context,registry);
    if(early)dslvm::write_result_frame(std::cout,sink); // mutation: before dispatch
    for(int n=0;n<repeat;++n)for(size_t i=0;i<program.entries().size();++i) {
      if(reset)sink.reset(); // mutation: erase body/prior-entry writes
      const auto value=program.call(i,context,registry,{});
      if(value.kind!=dslvm::Value::INT||value.i!=7)throw dslvm::Error("entry HALT result changed");
    }
    if(app_fail)return 4; // application validation must precede framing too
    if(rounding) {
      const std::string payload=sink.json();
      auto number=dslvm::stdlib::json_detail::Parser(payload).val();
      char legacy[64];std::snprintf(legacy,sizeof legacy,"%.15g",number.floating);
      std::cout<<"LMCODE_RETURN_BEGIN\n"<<legacy<<"\nLMCODE_RETURN_END\n";
    } else dslvm::write_result_frame(std::cout,sink);
    return std::cout?0:3;
  }catch(const dslvm::Error& e){std::cerr<<e.what()<<'\n';return 3;}
}
