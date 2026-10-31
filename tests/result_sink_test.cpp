#include "../include/dslvm.hpp"
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>
using namespace dslvm;
static void check(bool yes, const char* why) { if (!yes) throw std::runtime_error(why); }
static Value invoke(Registry& r, const std::string& signature, Args args={}) {
  auto fn=r.function_by_signature(signature);
  check(fn!=nullptr,"missing return callback"); return fn->fn(args);
}
static void fails(const std::function<void()>& f, const std::string& expected) {
  try {f();} catch(const Error& e) {check(e.code=="RUN_HOST","wrong error code");check(e.message==expected,"wrong error diagnostic");return;}
  throw std::runtime_error("expected return error");
}
static std::string unit_envelope(const ResultSink& sink) {
  return invocation_detail::framed("{\"ok\":true,\"result\":"+sink.json()+"}");
}
int main(int argc,char** argv) {
  int failed=0,passed=0;
  auto test=[&](const char* name,const std::function<void()>& f){if(argc>1&&std::string(name).find(argv[1])==std::string::npos)return;try{f();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
  test("unset null framing and reset",[]{ResultSink s;check(unit_envelope(s)=="LMCODE_RETURN_BEGIN\n{\"ok\":true,\"result\":null}\nLMCODE_RETURN_END\n","null frame");s.set(Value::integer(4));s.reset();check(s.json()=="null","reset sink");});
  test("field promotion overwrite and insertion order",[]{ResultSink s;s.put("a",Value::integer(1));s.put("b",Value::boolean(false));s.put("a",Value::string("last"));s.put_null("nil");check(s.json()=="{\"a\":\"last\",\"b\":false,\"nil\":null}","field order");s.set_null();s.put("",Value::integer(2));check(s.json()=="{\"\":2}","empty key");});
  test("scalar field error leaves prior sink",[]{ResultSink s;s.set(Value::integer(5));fails([&]{s.put("x",Value::integer(1));},"lmcode return: field assignment requires object or null");check(s.json()=="5","field error changed sink");});
  test("invalid conversion has atomic writes",[]{ResultSink s;s.put("keep",Value::integer(3));fails([&]{s.set(Value::real(std::numeric_limits<double>::infinity()));},"lmcode return: invalid JSON value at : nonfinite");fails([&]{s.put("bad/key~",Value::integer(9007199254740992LL));},"lmcode return: invalid JSON value at /bad~1key~0: unsafe_integer");check(s.json()=="{\"keep\":3}","bad write changed sink");});
  test("missing sink has no silent global fallback",[]{Registry r;fails([&]{invoke(r,"fn lmcode_return_set(int) -> void",{Value::integer(2)});},"lmcode return: no result sink");});
  test("source mutation cannot alias captured list",[]{ResultSink s;stdlib::IntList list{{1,2}};s.set(Value::object("IntList",&list));list.items[0]=99;list.items.push_back(4);check(s.json()=="[1,2]","arena-alias/source-mutation");});
  test("captured values outlive arena reset and per-call reset",[]{Registry r;ResultSink s;r.set_result_sink(&s);{auto scope=r.std_run_scope();auto* list=r.std_collections().make_int_list({7,8});invoke(r,"fn lmcode_return_put(string, IntList) -> void",{Value::string("saved"),Value::object("IntList",list)});list->items.clear();}check(s.json()=="{\"saved\":[7,8]}","arena teardown changed captured value");{auto scope=r.std_run_scope();invoke(r,"fn lmcode_return_put(string, bool) -> void",{Value::string("entry"),Value::boolean(true)});}check(s.json()=="{\"saved\":[7,8],\"entry\":true}","reset-between-calls");});
  test("Json root and borrowed child owned copy",[]{Registry r;ResultSink s,child_copy;r.set_result_sink(&s);{auto scope=r.std_run_scope();auto value=invoke(r,"fn json_parse(string) -> Json",{Value::string("{\"x\":[1,true,null],\"message\":\"yes\"}")});invoke(r,"fn lmcode_return_set(Json) -> void",{value});Args key{Value::string("x")};auto child=r.method_by_signature("method Json.get(string) -> Json")->fn(value.obj,key);child_copy.set(child);static_cast<stdlib::JsonNode*>(value.obj)->object[0].value->array.clear();}check(s.json()=="{\"x\":[1,true,null],\"message\":\"yes\"}","Json not deep owned");check(child_copy.json()=="[1,true,null]","borrowed Json child not owned");});
  test("all list and map kinds convert with owned entries",[]{ResultSink s;stdlib::StrList a{{"one"}};stdlib::FloatList b{{0.5,-0.0}};stdlib::StrMap c;c.set("s","v");stdlib::IntMap d;d.set("i",8);stdlib::FloatMap e;e.set("f",0.25);s.put("a",Value::object("StrList",&a));s.put("b",Value::object("FloatList",&b));s.put("c",Value::object("StrMap",&c));s.put("d",Value::object("IntMap",&d));s.put("e",Value::object("FloatMap",&e));a.items.clear();b.items.clear();c.set("s","changed");d.clear();e.clear();check(s.json()=="{\"a\":[\"one\"],\"b\":[0.5,0],\"c\":{\"s\":\"v\"},\"d\":{\"i\":8},\"e\":{\"f\":0.25}}","map/list ownership");});
  test("numeric boundary and shortest float framing",[]{ResultSink s;s.set(Value::integer(9007199254740991LL));check(s.json()=="9007199254740991","safe integer");s.set(Value::real(0.1+0.2));check(unit_envelope(s)=="LMCODE_RETURN_BEGIN\n{\"ok\":true,\"result\":0.30000000000000004}\nLMCODE_RETURN_END\n","float formatter used %.15g");s.set(Value::real(-0.0));check(s.json()=="0","negative zero");s.set(Value::real(std::numeric_limits<double>::denorm_min()));check(s.json()=="5e-324","subnormal lost");fails([&]{s.set(Value::real(9007199254740992.0));},"lmcode return: invalid JSON value at : unsafe_integer");});
  test("UTF8 values and keys are validated before copy",[]{ResultSink valid;valid.put(u8"café",Value::string(u8"水\nLMCODE_RETURN_END"));check(valid.json()==u8"{\"café\":\"水\\nLMCODE_RETURN_END\"}","valid UTF8 or escaped marker data changed");const auto framed=unit_envelope(valid);check(std::count(framed.begin(),framed.end(),'\n')==3,"marker/newline data broke framing");ResultSink s;s.set(Value::string("kept"));fails([&]{s.set(Value::string(std::string(1,char(0xff))));},"lmcode return: invalid JSON value at : utf8");fails([&]{s.put("key",Value::string("v"));},"lmcode return: field assignment requires object or null");s.set_null();fails([&]{s.put(std::string(1,char(0xff)),Value::string("v"));},"lmcode return: invalid JSON value at : utf8");check(s.json()=="null","invalid key promoted null root");});
  test("arbitrary handles rejected",[]{ResultSink s;int native=1;fails([&]{s.set(Value::object("Widget",&native));},"lmcode return: invalid JSON value at : handle");});
  test("distinct sinks isolated across overlapping registry scopes",[]{Registry a,b;ResultSink x,y;a.set_result_sink(&x);b.set_result_sink(&y);auto sa=a.std_run_scope();auto sb=b.std_run_scope();invoke(a,"fn lmcode_return_set(string) -> void",{Value::string("a")});invoke(b,"fn lmcode_return_set(string) -> void",{Value::string("b")});check(x.json()=="\"a\""&&y.json()=="\"b\"","global/shared invocation sink");});
  test("framing helper surfaces deferred stream failure",[]{
    struct DeferredFailure:std::streambuf {
      std::streamsize xsputn(const char*,std::streamsize n) override {return n;}
      int sync() override {return -1;}
    } buffer;
    std::ostream output(&buffer);auto r=InvocationResults::from_command({"only"},std::nullopt);Registry registry;r.select(0,registry,{});r.complete(0);
    check(r.write_frame(output)==4,"deferred flush failure accepted");
  });
  test("callbacks null overloads and library silence",[]{Registry r;ResultSink s;r.set_result_sink(&s);std::ostringstream diagnostic;r.set_std_output(&diagnostic);invoke(r,"fn lmcode_return_put_null(string) -> void",{Value::string("n")});check(s.json()=="{\"n\":null}","null field import");invoke(r,"fn lmcode_return_null() -> void");check(s.json()=="null"&&diagnostic.str().empty(),"native library printed result");r.set_result_sink(nullptr);fails([&]{invoke(r,"fn lmcode_return_null() -> void");},"lmcode return: no result sink");});
  std::cout<<passed<<" result-sink groups passed, "<<failed<<" failed\n";return failed?1:0;
}
