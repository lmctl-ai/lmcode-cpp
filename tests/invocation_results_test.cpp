#include "../include/dslvm.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <functional>
using namespace dslvm;
static int passed=0,failed=0;
static void check(bool v,const std::string& m){if(!v)throw std::runtime_error(m);}
static void rejects(const std::function<void()>& f,const std::string& text){try{f();}catch(const Error& e){check(e.message.find(text)!=std::string::npos,e.message);return;}throw std::runtime_error("missing refusal: "+text);}
static std::string frame(const std::string& s){return "LMCODE_RETURN_BEGIN\n"+s+"\nLMCODE_RETURN_END\n";}
static void test(const std::string& name,const std::function<void()>& fn,const std::string& select){if(!select.empty()&&name.find(select)==std::string::npos)return;try{fn();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception&e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}}
int main(int argc,char**argv){const std::string filter=argc>1?argv[1]:"";
 test("reverse execution retains add order and distinct sinks",[]{
  auto r=InvocationResults::from_command({"A","B"},std::nullopt);Registry reg;int counter=0;
  for(size_t index:{size_t(1),size_t(0)}){auto& s=r.select(index,reg,{});s.put("counter",Value::integer(++counter));s.put("part",Value::string(index?"B":"A"));r.complete(index);}
  std::ostringstream out;check(r.write_frame(out)==0,"exit");check(out.str()==frame("{\"ok\":true,\"result\":[{\"counter\":2,\"part\":\"A\"},{\"counter\":1,\"part\":\"B\"}],\"parts\":2}"),out.str());
 },filter);
 test("duplicate program occurrences are separate slots",[]{auto r=InvocationResults::from_command({"same","same"},std::nullopt);Registry reg;r.select(1,reg,{}).set(Value::integer(2));r.complete(1);r.select(0,reg,{}).set(Value::integer(1));r.complete(0);std::ostringstream out;check(r.write_frame(out)==0,"exit");check(out.str()==frame("{\"ok\":true,\"result\":[1,2],\"parts\":2}"),out.str());},filter);
 test("single list distinct from one slot builder",[]{
  auto one=InvocationResults::from_command({"only"},std::nullopt);Registry reg;stdlib::IntList list;list.items={1,2};one.select(0,reg,{}).set(Value::object("IntList",&list));one.complete(0);std::ostringstream out;check(one.write_frame(out)==0,"single");check(out.str()==frame("{\"ok\":true,\"result\":[1,2]}"),out.str());
  auto builder=InvocationResults::from_metadata("{\"version\":1,\"mode\":\"builder\",\"parts\":[{\"index\":0,\"file\":\""+std::filesystem::absolute("only").generic_string()+"\"}]}",{std::filesystem::absolute("only").generic_string()});builder.select(0,reg,{}).set(Value::object("IntList",&list));builder.complete(0);std::ostringstream b;check(builder.write_frame(b)==0,"builder");check(b.str()==frame("{\"ok\":true,\"result\":[[1,2]],\"parts\":1}"),b.str());
 },filter);
 test("unfinished slot never becomes silent null",[]{auto r=InvocationResults::from_command({"A","B"},std::nullopt);Registry reg;r.select(0,reg,{}).set(Value::integer(9));r.complete(0);std::ostringstream out;check(r.write_frame(out)==3,"missing completion exit");check(out.str().find("\"ok\":false")!=std::string::npos&&out.str().find("\"result\"")==std::string::npos&&out.str().find("\"part\":1")!=std::string::npos,out.str());},filter);
 test("first failure association and no partial result",[]{auto r=InvocationResults::from_command({"A","B"},std::nullopt);Registry reg;r.select(0,reg,{}).set(Value::integer(9));r.complete(0);Error e("RUN_HOST","bad");e.locate(4,2);r.fail(1,"run",e);r.fail(std::nullopt,"app",Error("APP_FAIL","later"));std::ostringstream out;check(r.write_frame(out)==3,"first failure exit");check(out.str()==frame("{\"ok\":false,\"phase\":\"run\",\"code\":\"RUN_HOST\",\"message\":\"dsl line 4:2: bad\",\"line\":4,\"column\":2,\"part\":1}"),out.str());},filter);
 test("global app and single failures omit part",[]{auto r=InvocationResults::from_command({"A"},std::nullopt);Registry reg;r.select(0,reg,{}).set(Value::integer(4));r.complete(0);r.fail(0,"app",Error("APP_FAIL","denied"));std::ostringstream out;check(r.write_frame(out)==4,"app");check(out.str()==frame("{\"ok\":false,\"phase\":\"app\",\"code\":\"APP_FAIL\",\"message\":\"denied\"}"),out.str());std::ostringstream cli;check(failure_frame(cli,"cli",Error("CLI_USAGE","bad args"))==2,"cli");},filter);
 test("one frame and shortest round trip",[]{auto r=InvocationResults::from_command({"A","B"},std::nullopt);Registry reg;r.select(0,reg,{}).set(Value::real(0.1+0.2));r.complete(0);r.select(1,reg,{});r.complete(1);std::ostringstream out;check(r.write_frame(out)==0,"exit");check(out.str()==frame("{\"ok\":true,\"result\":[0.30000000000000004,null],\"parts\":2}"),out.str());const auto before=out.str();check(r.write_frame(out)==4&&out.str()==before,"duplicate frame");},filter);
 test("sealed sinks refuse writes resets select and completion",[]{auto r=InvocationResults::from_command({"A"},std::nullopt);Registry reg;auto&s=r.select(0,reg,{});s.put("body",Value::integer(1));check(&r.select(0,reg,{})==&s,"active select reset/replaced sink");r.complete(0);rejects([&]{s.set_null();},"sealed");rejects([&]{s.reset();},"sealed");rejects([&]{r.select(0,reg,{});},"sealed");rejects([&]{r.complete(0);},"sealed");},filter);
 test("optional contracts atomic writes and completion",[]{
  auto c=ResultContract::from_schema("{\"dsl.schema\":2,\"result\":{\"type\":\"IntMap\"}}");ResultSink s(c);s.put("n",Value::integer(7));rejects([&]{s.put("bad",Value::string("x"));},"expected int; got string");check(s.json()=="{\"n\":7}","failed write changed root");rejects([&]{s.put_null("bad");},"expected int; got null");rejects([&]{s.set_null();},"expected IntMap; got null");ResultSink unfinished(c);rejects([&]{unfinished.seal();},"expected IntMap; got null");
  ResultSink scalar(ResultContract::from_schema("{\"dsl.schema\":2,\"result\":{\"type\":\"float\"}}"));scalar.set(Value::integer(1));check(scalar.json()=="1","coerced int");
  for(const std::string field:{"{\"type\":\"void\"}","{\"type\":\"null\",\"nullable\":false}","{\"type\":\"int\",\"extra\":1}","{\"nullable\":true}"})rejects([&]{ResultContract::from_schema("{\"dsl.schema\":2,\"result\":"+field+"}");},"result contract");
 },filter);
 test("contract immutability and nested JSON runtime check",[]{auto r=InvocationResults::from_command({"A"},std::nullopt);Registry reg;auto c=ResultContract::from_schema("{\"dsl.schema\":2,\"result\":{\"type\":\"IntList\"}}");auto&s=r.select(0,reg,c);rejects([&]{r.select(0,reg,{});},"contract");auto node=stdlib::json_detail::Parser("[1,null]").val();rejects([&]{s.set(Value::object("Json",&node));},"/1: expected int; got null");},filter);
 test("manifest strict registration validation",[]{
  const auto a=std::filesystem::absolute("A").generic_string(),b=std::filesystem::absolute("B").generic_string();
  const auto meta="{\"version\":1,\"mode\":\"builder\",\"parts\":[{\"index\":0,\"file\":\""+a+"\"},{\"index\":1,\"file\":\""+b+"\"}]}";
  auto r=InvocationResults::from_metadata(meta,{b,a});check(r.registrations()[0].index==1&&r.registrations()[1].index==0,"registration order");
  rejects([&]{InvocationResults::from_metadata(meta,{a});},"registration");rejects([&]{InvocationResults::from_metadata(meta,{a,a});},"registration");
  for(auto text:{"{}","{\"version\":2,\"mode\":\"single\",\"parts\":[]}","{\"version\":1,\"mode\":\"single\",\"parts\":[{\"index\":0,\"file\":\"relative\"}]}","{\"version\":1,\"mode\":\"single\",\"parts\":[],\"x\":1}"})rejects([&]{InvocationResults::from_metadata(text,{a});},"invocation");
 },filter);
 test("flush failure cannot trigger a second frame",[]{struct Broken:std::stringbuf{int sync()override{return -1;}}buffer;std::ostream out(&buffer);auto r=InvocationResults::from_command({"A"},std::nullopt);Registry reg;r.select(0,reg,{});r.complete(0);check(r.write_frame(out)==4,"flush failure");const auto before=buffer.str();check(r.write_frame(out)==4&&buffer.str()==before,"second write after failure");},filter);
 std::cout<<passed<<" groups, "<<failed<<" failures\n";return failed?1:0;}
