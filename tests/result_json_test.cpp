#include "../stdlib/result_json.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace dslvm;
using namespace dslvm::stdlib;
using namespace dslvm::stdlib::result_detail;
static void check(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
static void rejects(const std::function<void()>& fn,const std::string& path,const char* code) {
  try {fn();}catch(const Error& e){check(e.code=="RUN_HOST","wrong error code");check(e.message=="lmcode return: invalid JSON value at "+path+": "+code,"wrong JSON pointer/reason");return;}
  throw std::runtime_error("expected numeric/value refusal");
}
static JsonNode parsed(const std::string& text) {json_detail::Parser p(text);auto node=p.val();p.ws();check(p.p==text.size(),"bad test token");return node;}
int main(int argc,char** argv) {
 int failures=0,passed=0;
 auto test=[&](const char* name,const std::function<void()>& fn){if(argc>1&&std::string(name).find(argv[1])==std::string::npos)return;try{fn();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failures;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
 test("shortest float and zero",[]{check(encode(copy_value(Value::real(.1+.2)))=="0.30000000000000004","%.15g truncates result");check(encode(copy_value(Value::real(.1)))=="0.1","formatter is round-tripping but not shortest");check(encode(copy_value(Value::real(-0.0)))=="0","negative zero");check(encode(copy_value(Value::real(std::numeric_limits<double>::denorm_min())))=="5e-324","subnormal");});
 test("exact decimal integer classification",[]{
  for(const char* token:{"9007199254740991","-9007199254740991","9007199254740991.0","90071992547409910e-1","9.007199254740991e15","1.0000000000000000","0e-999999999999999999999999999","-0.0e99999999999999999999999999","1e-20","0.00001e20"})check(!decimal_unsafe_integer(token),"safe/fractional token rejected");
  for(const char* token:{"9007199254740992","9007199254740993","-9007199254740993","90071992547409930000e-4","9.007199254740992e15","1e20","1e99999999999999999999999999"})check(decimal_unsafe_integer(token),"unsafe raw integer accepted");
 });
 test("retained raw tokens cannot be rounded away",[]{JsonNode n;n.kind=JsonNode::Kind::Int;n.integer=1;n.raw="9007199254740993";rejects([&]{copy_node(n);},"","unsafe_integer");n.kind=JsonNode::Kind::Float;n.floating=1;n.raw="1e20";rejects([&]{copy_node(n);},"","unsafe_integer");});
 test("actual Json parser boundaries",[]{for(const char* token:{"9007199254740991","9007199254740991.0","90071992547409910e-1","5e-324","-0.0"}){auto n=parsed(token);copy_node(n);}for(const char* token:{"9007199254740992","9007199254740993","9.007199254740992e15","9007199254740991.9"}){auto n=parsed(token);rejects([&]{copy_node(n);},"","unsafe_integer");}});
 test("underflow checks mantissa rather than exponent digits",[]{JsonNode n;n.kind=JsonNode::Kind::Float;n.raw="1e-999";n.floating=0;rejects([&]{copy_node(n);},"","underflow");n.raw="0e-999";check(encode(copy_node(n))=="0","zero exponent");});
 test("nested numeric JSON pointers",[]{auto n=parsed("{\"a/b~c\":[9007199254740993]}");rejects([&]{copy_node(n);},"/a~1b~0c/0","unsafe_integer");});
 test("invalid UTF8 values and keys",[]{JsonNode n;n.kind=JsonNode::Kind::String;n.text=std::string(1,char(0xff));rejects([&]{copy_node(n);},"","utf8");JsonNode object;object.kind=JsonNode::Kind::Object;object.object.push_back({std::string(1,char(0xff)),"",std::make_unique<JsonNode>()});rejects([&]{copy_node(object);},"","utf8");});
 test("owned JSON graph copy",[]{auto n=parsed("{\"a\":[\"owned\",true,null]}");auto owned=copy_value(Value::object("Json",&n));n.object[0].value->array.clear();check(encode(owned)=="{\"a\":[\"owned\",true,null]}","source mutation/Json ABI");});
 test("cycle refused and detached before destruction",[]{auto root=std::make_unique<JsonNode>();root->kind=JsonNode::Kind::Object;root->object.push_back({"self","",std::unique_ptr<JsonNode>(root.get())});bool rejected=false;try{copy_node(*root);}catch(const Error& e){rejected=e.message=="lmcode return: invalid JSON value at /self: cycle";}root->object[0].value.release();check(rejected,"cycle not refused");});
 test("duplicate keys never serialized",[]{auto n=parsed("{\"x\":1}");n.object.push_back({"x","",std::make_unique<JsonNode>()});rejects([&]{copy_node(n);},"/x","handle");IntMap map;map.items={{"x",1},{"x",2}};rejects([&]{copy_value(Value::object("IntMap",&map));},"/x","handle");});
 test("unsupported handles never stringify",[]{int host=3;rejects([&]{copy_value(Value::object("Host",&host));},"","handle");rejects([&]{copy_value(Value::object("Json",nullptr));},"","handle");});
 std::cout<<passed<<" conversion groups passed, "<<failures<<" failed\n";return failures?1:0;
}
