// Unit tests of the SDK runtime: a tiny in-test assembler builds valid and invalid bytecode v5; every verifier rejection and
// the run-time checks are exercised, and the absence of limits is pinned. Build and run: make test
#include "dslvm.hpp"
#include <cassert>
#include <cstdio>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

using namespace dslvm;
static int passed = 0;

struct Asm {
  struct EntrySpec {
    std::string name, annotation, iface;
    uint16_t nparams = 0;
    std::vector<std::pair<std::string, uint8_t>> locals;  // (type name, hidden); slots 0..nparams-1 are the parameters
    uint16_t maxStack = 1;
    std::string code;
    std::vector<std::tuple<uint32_t, uint32_t, uint16_t>> lines;
  };
  mutable std::vector<std::string> consts; std::vector<uint32_t> imports; std::vector<std::pair<uint32_t, uint32_t>> contexts;
  std::vector<std::pair<uint32_t, uint8_t>> locals; uint16_t maxStack = 0; std::string code; std::vector<std::tuple<uint32_t, uint32_t, uint16_t>> lines;
  std::vector<EntrySpec> entries;
  uint32_t k(const std::string& s) const { for (size_t i = 0; i < consts.size(); ++i) if (consts[i] == s) return uint32_t(i); consts.push_back(s); return uint32_t(consts.size() - 1); }
  uint16_t imp(const std::string& canonical) { uint32_t c = k(canonical); for (size_t i = 0; i < imports.size(); ++i) if (imports[i] == c) return uint16_t(i); imports.push_back(c); return uint16_t(imports.size() - 1); }
  uint16_t ctxIndex(const std::string& name, const std::string& type) { std::pair<uint32_t, uint32_t> e{k(name), k(type)}; for (size_t i = 0; i < contexts.size(); ++i) if (contexts[i] == e) return uint16_t(i); contexts.push_back(e); return uint16_t(contexts.size() - 1); }
  Asm& local(const std::string& type, uint8_t hidden = 0) { locals.emplace_back(k(type), hidden); return *this; }
  Asm& line(uint32_t pc, uint32_t ln, uint16_t col) { lines.emplace_back(pc, ln, col); return *this; }
  Asm& u8(unsigned v) { code.push_back(char(v)); return *this; }
  Asm& u16(unsigned v) { code.push_back(char(v >> 8)); code.push_back(char(v)); return *this; }
  Asm& u32(uint32_t v) { for (int i = 3; i >= 0; --i) code.push_back(char((v >> (i * 8)) & 0xff)); return *this; }
  Asm& i32(int32_t v) { return u32(uint32_t(v)); }
  Asm& i64(int64_t v) { uint64_t u = uint64_t(v); for (int i = 7; i >= 0; --i) code.push_back(char((u >> (i * 8)) & 0xff)); return *this; }
  Asm& f64(double d) { uint64_t bits; std::memcpy(&bits, &d, 8); for (int i = 7; i >= 0; --i) code.push_back(char((bits >> (i * 8)) & 0xff)); return *this; }
  Asm& pushInt(int64_t v) { return u8(PUSH_INT).i64(v); }
  Asm& pushFloat(double v) { return u8(PUSH_FLOAT).f64(v); }
  Asm& pushStr(const std::string& s) { return u8(PUSH_STR).u32(k(s)); }
  Asm& pushBool(bool b) { return u8(PUSH_BOOL).u8(b ? 1 : 0); }
  Asm& ctx(const std::string& n, const std::string& type) { return u8(LOAD_CTX).u16(ctxIndex(n, type)); }
  Asm& fn(const std::string& canonical, unsigned argc) { return u8(CALL_FN).u16(imp(canonical)).u8(argc); }
  Asm& method(const std::string& canonical, unsigned argc) { return u8(CALL_METHOD).u16(imp(canonical)).u8(argc); }
  Asm& halt() { return u8(HALT); }
  Asm& entry(const EntrySpec& e) { entries.push_back(e); return *this; }
  void patch(size_t at, size_t target) { int32_t rel = int32_t(target) - int32_t(at + 4); for (int k = 0; k < 4; ++k) code[at + k] = char(uint32_t(rel) >> (24 - k * 8)); }
  static void be16(std::string& o, unsigned v) { o.push_back(char(v >> 8)); o.push_back(char(v)); }
  static void be32(std::string& o, uint32_t v) { for (int i = 3; i >= 0; --i) o.push_back(char((v >> (i * 8)) & 0xff)); }
  std::string file(const std::string& schema, uint16_t version = VERSION, const std::string& magic = "DSLB") const {
    for (auto& e : entries) { k(e.name); k(e.annotation); k(e.iface); for (auto& l : e.locals) k(l.first); }  // entries share the pool: intern before it is written
    std::string out = magic; be16(out, version); out += sha256(schema);
    be32(out, uint32_t(consts.size())); for (auto& c : consts) { out.push_back(1); be32(out, uint32_t(c.size())); out += c; }
    be16(out, unsigned(imports.size())); for (auto i : imports) be32(out, i);
    be16(out, unsigned(contexts.size())); for (auto& c : contexts) { be32(out, c.first); be32(out, c.second); }
    be16(out, unsigned(locals.size())); for (auto& l : locals) { be32(out, l.first); out.push_back(char(l.second)); }
    be16(out, maxStack);
    be32(out, uint32_t(code.size())); out += code;
    be32(out, uint32_t(lines.size())); for (auto& l : lines) { be32(out, std::get<0>(l)); be32(out, std::get<1>(l)); be16(out, std::get<2>(l)); }
    be16(out, unsigned(entries.size()));
    for (auto& e : entries) {
      be32(out, k(e.name)); be32(out, k(e.annotation)); be32(out, k(e.iface));
      be16(out, e.nparams);
      be16(out, unsigned(e.locals.size()));
      for (auto& l : e.locals) { be32(out, k(l.first)); out.push_back(char(l.second)); }
      be16(out, e.maxStack);
      be32(out, uint32_t(e.code.size())); out += e.code;
      be32(out, uint32_t(e.lines.size()));
      for (auto& l : e.lines) { be32(out, std::get<0>(l)); be32(out, std::get<1>(l)); be16(out, std::get<2>(l)); }
    }
    return out;
  }
};

static const std::string SCHEMA = "{\"dsl.schema\":1,\"name\":\"t\"}";  // only its bytes matter to the runtime
static const std::string T_INT = "method Trace.int(int) -> void", T_STR = "method Trace.str(string) -> void", THING_ID = "method Thing.id() -> int", TWICE = "fn twice(int) -> int", BOOM = "fn boom() -> void";
struct Thing { int64_t id; };
struct Host {
  std::vector<std::string> trace; Thing root{0};
  Registry reg;
  Host() {
    reg.context("t", "Trace").context("root", "Thing");
    reg.method("Trace", "int", {"int"}, "void", [&](void*, Args& a) { trace.push_back("int " + std::to_string(a[0].i)); return Value::none(); });
    reg.method("Trace", "str", {"string"}, "void", [&](void*, Args& a) { trace.push_back("str " + a[0].s); return Value::none(); });
    reg.method("Thing", "id", {}, "int", [&](void* self, Args&) { return Value::integer(static_cast<Thing*>(self)->id); });
    reg.function("twice", {"int"}, "int", [&](Args& a) { return Value::integer(a[0].i * 2); });
    reg.function("boom", {}, "void", [&](Args&) -> Value { throw Error("boom from the host"); });
  }
  std::vector<std::string> run(const std::string& bytes) { Program p = Program::load(bytes, SCHEMA, reg); Context c; c.set("t", &trace); c.set("root", &root); trace.clear(); p.run(c, reg); return trace; }
};

static void expect_error(const std::string& label, const std::function<void()>& f, const std::string& needle) {
  try { f(); } catch (const Error& e) { if (std::string(e.what()).find(needle) == std::string::npos) { std::cerr << "FAIL " << label << ": got '" << e.what() << "', want '" << needle << "'\n"; std::exit(1); } passed++; std::cout << "ok - " << label << ": " << e.what() << "\n"; return; }
  std::cerr << "FAIL " << label << ": no error (wanted '" << needle << "')\n"; std::exit(1);
}
static void expect_trace(const std::string& label, Host& h, const std::string& bytes, const std::vector<std::string>& want) {
  auto got = h.run(bytes); if (got != want) { std::cerr << "FAIL " << label << ":\n"; for (auto& g : got) std::cerr << "  got  " << g << "\n"; for (auto& w : want) std::cerr << "  want " << w << "\n"; std::exit(1); }
  passed++; std::cout << "ok - " << label << "\n";
}

int main() {
  Host h;
  const int64_t MAX = INT64_MAX, MIN = INT64_MIN;
  // Malformed values are VM failures, not host callback failures. Exercise every
  // type-check family that formerly used Error(message)'s RUN_HOST default.
  const std::vector<std::pair<Asm, std::string>> type_errors = {
    {Asm().pushInt(1).pushStr("x").u8(EQ).halt(), "cannot compare int with string"},
    {Asm().pushStr("x").pushInt(1).u8(ADD_I).halt(), "int operator applied to string and int"},
    {Asm().pushInt(1).pushFloat(1).u8(ADD_F).halt(), "float operator applied to int and float"},
    {Asm().pushStr("x").u8(NEG_I).halt(), "int negation applied to string"},
    {Asm().pushInt(1).u8(NEG_F).halt(), "float negation applied to int"},
    {Asm().pushStr("x").u8(I2F).halt(), "float() applied to string"},
    {Asm().pushInt(1).pushStr("x").u8(CONCAT).halt(), "string + applied to int and string"},
    {Asm().pushStr("x").u8(I2S).halt(), "str(int) applied to string"},
    {Asm().pushInt(1).u8(F2S).halt(), "str(float) applied to int"},
    {Asm().pushInt(1).u8(B2S).halt(), "str(bool) applied to int"},
    {Asm().pushInt(1).u8(NOT).halt(), "! applied to a int"},
    {Asm().pushInt(1).u8(JUMP_IF_FALSE).i32(0).halt(), "condition is a int, not a bool"},
  };
  for (const auto& test : type_errors) {
    bool caught = false;
    try { h.run(test.first.file(SCHEMA)); }
    catch (const Error& e) { caught = true; assert(e.code == "RUN_ERROR" && e.message == test.second); }
    assert(caught); ++passed;
    std::cout << "ok - VM type error code: " << test.second << "\n";
  }
  // Valid programs.
  { Asm a; a.ctx("t", "Trace").pushInt(1).pushInt(2).u8(ADD_I).method(T_INT, 1).ctx("t", "Trace").pushStr("a").pushStr("b").u8(CONCAT).method(T_STR, 1).halt(); expect_trace("arithmetic, concat, method calls", h, a.file(SCHEMA), {"int 3", "str ab"}); }
  { Asm a; a.local("int"); a.pushInt(21).fn(TWICE, 1).u8(STORE_LOCAL).u16(0).ctx("t", "Trace").u8(LOAD_LOCAL).u16(0).method(T_INT, 1).halt(); expect_trace("host function and locals", h, a.file(SCHEMA), {"int 42"}); }
  { Asm a; a.pushBool(true).u8(JUMP_IF_FALSE).i32(21).ctx("t", "Trace").pushInt(1).method(T_INT, 1).u8(JUMP).i32(16).ctx("t", "Trace").pushInt(2).method(T_INT, 1).halt(); expect_trace("forward jumps select a branch", h, a.file(SCHEMA), {"int 1"}); }
  { Asm a; a.ctx("t", "Trace").ctx("root", "Thing").method(THING_ID, 0).method(T_INT, 1).ctx("t", "Trace").pushFloat(2.5).u8(F2S).method(T_STR, 1).ctx("t", "Trace").pushInt(7).pushInt(2).u8(DIV_I).u8(I2F).pushFloat(0.5).u8(ADD_F).u8(F2S).method(T_STR, 1).ctx("t", "Trace").pushInt(3).pushInt(3).u8(EQ).u8(B2S).method(T_STR, 1).halt(); expect_trace("context object, floats, conversions, equality", h, a.file(SCHEMA), {"int 0", "str 2.5", "str 3.5", "str true"}); }
  { Asm a; a.ctx("t", "Trace").pushFloat(std::numeric_limits<double>::quiet_NaN()).u8(F2S).method(T_STR, 1).ctx("t", "Trace").pushFloat(-std::numeric_limits<double>::quiet_NaN()).u8(F2S).method(T_STR, 1).halt(); expect_trace("NaN text ignores sign/payload", h, a.file(SCHEMA), {"str nan", "str nan"}); }
  // MATH: all IDs, source-order operands, integer edge semantics, and
  // signed-zero selection for float min/max.
  { Asm a;
    auto fs = [&](double x, uint8_t fn) { a.ctx("t", "Trace").pushFloat(x).u8(MATH).u8(fn).u8(F2S).method(T_STR, 1); };
    auto fb = [&](double x, double y, uint8_t fn) { a.ctx("t", "Trace").pushFloat(x).pushFloat(y).u8(MATH).u8(fn).u8(F2S).method(T_STR, 1); };
    auto fc = [&](double x, double lo, double hi, uint8_t fn) { a.ctx("t", "Trace").pushFloat(x).pushFloat(lo).pushFloat(hi).u8(MATH).u8(fn).u8(F2S).method(T_STR, 1); };
    auto is = [&](int64_t x, uint8_t fn) { a.ctx("t", "Trace").pushInt(x).u8(MATH).u8(fn).method(T_INT, 1); };
    auto ib = [&](int64_t x, int64_t y, uint8_t fn) { a.ctx("t", "Trace").pushInt(x).pushInt(y).u8(MATH).u8(fn).method(T_INT, 1); };
    auto ic = [&](int64_t x, int64_t lo, int64_t hi, uint8_t fn) { a.ctx("t", "Trace").pushInt(x).pushInt(lo).pushInt(hi).u8(MATH).u8(fn).method(T_INT, 1); };
    fs(0.0, MATH_SIN_F); fs(0.0, MATH_COS_F); fs(4.0, MATH_SQRT_F); fs(-2.5, MATH_ABS_F); fs(1.8, MATH_FLOOR_F);
    fb(2.0, 3.0, MATH_POW_F); fb(2.0, 3.0, MATH_MIN_F); fb(3.0, 2.0, MATH_MIN_F); fb(2.0, 3.0, MATH_MAX_F); fb(3.0, 2.0, MATH_MAX_F);
    fc(5.0, 0.0, 3.0, MATH_CLAMP_F); fc(2.0, 5.0, 0.0, MATH_CLAMP_F); a.ctx("t", "Trace").pushFloat(0.0).pushFloat(10.0).pushFloat(0.25).u8(MATH).u8(MATH_LERP_F).u8(F2S).method(T_STR, 1);
    fs(0.5, MATH_EASE_F); fs(0.5, MATH_EASE_IN_F); fs(0.5, MATH_EASE_OUT_F); fs(std::numeric_limits<double>::quiet_NaN(), MATH_EASE_F); a.ctx("t", "Trace").pushFloat(-2.8).u8(MATH).u8(MATH_INT_F).method(T_INT, 1);
    is(MIN, MATH_ABS_I); ib(2, 3, MATH_MIN_I); ib(2, 3, MATH_MAX_I); ic(5, 0, 3, MATH_CLAMP_I); ic(2, 5, 0, MATH_CLAMP_I);
    a.ctx("t", "Trace").pushFloat(0.0).pushFloat(-0.0).u8(MATH).u8(MATH_MIN_F).u8(F2S).method(T_STR, 1);
    a.ctx("t", "Trace").pushFloat(-0.0).pushFloat(0.0).u8(MATH).u8(MATH_MAX_F).u8(F2S).method(T_STR, 1).halt();
    expect_trace("MATH functions and edge semantics", h, a.file(SCHEMA), {"str 0", "str 1", "str 2", "str 2.5", "str 1", "str 8", "str 2", "str 2", "str 3", "str 3", "str 3", "str 0", "str 2.5", "str 0.5", "str 0.25", "str 0.75", "str nan", "int -2", "int -9223372036854775808", "int 2", "int 3", "int 3", "int 0", "str 0", "str -0"}); }
  { std::vector<double> raw;
    Registry reg; reg.context("t", "Trace");
    reg.method("Trace", "capture", {"float"}, "void", [&](void*, Args& args) { raw.push_back(args[0].f); return Value::none(); });
    Asm a;
    a.ctx("t", "Trace").pushFloat(0.0).pushFloat(10.0).pushFloat(0.25).u8(MATH).u8(MATH_LERP_F).method("method Trace.capture(float) -> void", 1);
    a.ctx("t", "Trace").pushFloat(0.25).u8(MATH).u8(MATH_EASE_F).method("method Trace.capture(float) -> void", 1);
    a.ctx("t", "Trace").pushFloat(0.0).pushFloat(-0.0).u8(MATH).u8(MATH_MIN_F).method("method Trace.capture(float) -> void", 1);
    a.ctx("t", "Trace").pushFloat(-0.0).pushFloat(0.0).u8(MATH).u8(MATH_MAX_F).method("method Trace.capture(float) -> void", 1).halt();
    Program p = Program::load(a.file(SCHEMA), SCHEMA, reg); Context c; c.set("t", &raw); p.run(c, reg);
    assert(raw.size() == 4 && raw[0] == 2.5 && raw[1] == 0.15625 && !std::signbit(raw[2]) && std::signbit(raw[3]));
    passed++; std::cout << "ok - MATH exact values and signed zero are preserved\n";
  }
  // Structured range loops: bounds are evaluated once and the hidden end is a snapshot.
  { Asm a; a.local("int").pushInt(0).u8(STORE_LOCAL).u16(0); const size_t head = a.code.size(); a.u8(WHILE_BEGIN).u8(LOAD_LOCAL).u16(0).pushInt(3).u8(LT_I).u8(WHILE_TEST); const size_t exit = a.code.size(); a.i32(0).ctx("t", "Trace").u8(LOAD_LOCAL).u16(0).method(T_INT, 1).u8(LOAD_LOCAL).u16(0).pushInt(1).u8(ADD_I).u8(STORE_LOCAL).u16(0).u8(LOOP_BACK); const size_t back = a.code.size(); a.i32(0); const size_t end = a.code.size(); a.patch(exit, end); a.patch(back, head); a.halt(); expect_trace("while loop runs zero-test condition until false", h, a.file(SCHEMA), {"int 0", "int 1", "int 2"}); }
  { Asm a; a.u8(WHILE_BEGIN).pushBool(false).u8(WHILE_TEST).i32(5).u8(LOOP_BACK).i32(-13).halt(); expect_trace("while loop permits zero iterations", h, a.file(SCHEMA), {}); }
  { Asm a; a.local("int", 1).local("int", 1).pushInt(0).pushInt(3).u8(FOR_RANGE).u16(0).u16(1).i32(17).ctx("t", "Trace").u8(LOAD_LOCAL).u16(0).method(T_INT, 1).u8(FOR_NEXT).u16(1).i32(-17).halt(); expect_trace("range loop runs counter 0..2", h, a.file(SCHEMA), {"int 0", "int 1", "int 2"}); }
  { Asm a; a.local("int", 1).local("int", 1).pushInt(2).pushInt(2).u8(FOR_RANGE).u16(0).u16(1).i32(23).ctx("t", "Trace").pushInt(99).method(T_INT, 1).u8(FOR_NEXT).u16(1).i32(-23).halt(); expect_trace("empty range skips body", h, a.file(SCHEMA), {}); }
  { Asm a; a.local("int", 1).local("int", 1).local("int", 1).local("int", 1).pushInt(0).pushInt(2).u8(FOR_RANGE).u16(0).u16(1).i32(51).pushInt(0).pushInt(2).u8(FOR_RANGE).u16(2).u16(3).i32(17).ctx("t", "Trace").u8(LOAD_LOCAL).u16(2).method(T_INT, 1).u8(FOR_NEXT).u16(3).i32(-17).u8(FOR_NEXT).u16(1).i32(-51).halt(); expect_trace("nested ranges preserve inner counter", h, a.file(SCHEMA), {"int 0", "int 1", "int 0", "int 1"}); }
  { Asm a; a.local("int", 1).local("int", 1).pushInt(INT64_MAX - 1).pushInt(INT64_MAX).u8(FOR_RANGE).u16(0).u16(1).i32(17).ctx("t", "Trace").u8(LOAD_LOCAL).u16(0).method(T_INT, 1).u8(FOR_NEXT).u16(1).i32(-17).halt(); expect_trace("range ending at INT64_MAX does not overflow", h, a.file(SCHEMA), {"int 9223372036854775806"}); }
  { Asm a; a.local("int", 1).local("int", 1).pushInt(INT64_MIN).pushInt(INT64_MIN + 2).u8(FOR_RANGE).u16(0).u16(1).i32(17).ctx("t", "Trace").u8(LOAD_LOCAL).u16(0).method(T_INT, 1).u8(FOR_NEXT).u16(1).i32(-17).halt(); expect_trace("range starting at INT64_MIN does not overflow", h, a.file(SCHEMA), {"int -9223372036854775808", "int -9223372036854775807"}); }
  { Asm a; a.halt(); Program p = Program::load(a.file(SCHEMA), SCHEMA, h.reg); assert(p.constant_count() == 0 && p.code_size() == 1 && p.import_count() == 0 && p.local_count() == 0 && p.lines().empty() && p.entries().empty()); passed++; std::cout << "ok - empty program loads\n"; }
  // v4 integers: 64-bit two's complement, wraparound, MOD_I.
  { Asm a; a.ctx("t", "Trace").pushInt(MAX).method(T_INT, 1).ctx("t", "Trace").pushInt(MIN).method(T_INT, 1).ctx("t", "Trace").pushInt(4611686018427387904).pushInt(2).u8(MUL_I).pushInt(1).u8(SUB_I).method(T_INT, 1).halt(); expect_trace("int64 literals and arithmetic", h, a.file(SCHEMA), {"int 9223372036854775807", "int -9223372036854775808", "int 9223372036854775807"}); }
  { Asm a; a.ctx("t", "Trace").pushInt(MAX).pushInt(1).u8(ADD_I).method(T_INT, 1).ctx("t", "Trace").pushInt(MIN).u8(NEG_I).method(T_INT, 1).ctx("t", "Trace").pushInt(MIN).pushInt(-1).u8(DIV_I).method(T_INT, 1).ctx("t", "Trace").pushInt(MIN).pushInt(-1).u8(MOD_I).method(T_INT, 1).ctx("t", "Trace").pushInt(MIN).pushInt(1).u8(SUB_I).method(T_INT, 1).ctx("t", "Trace").pushInt(MAX).pushInt(MAX).u8(MUL_I).method(T_INT, 1).halt(); expect_trace("wraparound: no overflow trap", h, a.file(SCHEMA), {"int -9223372036854775808", "int -9223372036854775808", "int -9223372036854775808", "int 0", "int 9223372036854775807", "int 1"}); }
  { Asm a; a.ctx("t", "Trace").pushInt(7).pushInt(3).u8(MOD_I).method(T_INT, 1).ctx("t", "Trace").pushInt(-7).pushInt(3).u8(MOD_I).method(T_INT, 1).ctx("t", "Trace").pushInt(7).pushInt(-3).u8(MOD_I).method(T_INT, 1).ctx("t", "Trace").pushInt(-7).pushInt(2).u8(DIV_I).method(T_INT, 1).halt(); expect_trace("MOD_I takes the dividend's sign; DIV_I truncates toward zero", h, a.file(SCHEMA), {"int 1", "int -1", "int 1", "int -3"}); }
  { Asm a; std::vector<size_t> exits, bodies, backs, nexts(17); for (int n = 0; n < 17; ++n) { a.local("int", 1).local("int", 1).pushInt(0).pushInt(1).u8(FOR_RANGE).u16(unsigned(n * 2)).u16(unsigned(n * 2 + 1)); exits.push_back(a.code.size()); a.i32(0); bodies.push_back(a.code.size()); } for (int n = 16; n >= 0; --n) { nexts[n] = a.code.size(); a.u8(FOR_NEXT).u16(unsigned(n * 2 + 1)); backs.push_back(a.code.size()); a.i32(0); } const size_t halt = a.code.size(); a.halt(); auto patch = [&](size_t at, int32_t target) { int32_t rel = target - int32_t(at + 4); for (int k = 0; k < 4; ++k) a.code[at + k] = char(uint32_t(rel) >> (24 - k * 8)); }; for (int n = 0; n < 17; ++n) patch(exits[n], int32_t(nexts[n] + 7)); for (int n = 0; n < 17; ++n) patch(backs[n], int32_t(bodies[16 - n])); (void)halt; Program p = Program::load(a.file(SCHEMA), SCHEMA, h.reg); assert(p.local_count() == 34); passed++; std::cout << "ok - 17 nested range loops have no depth cap\n"; }
  { Asm a; a.local("int", 1).local("int", 1).pushInt(0).pushInt(3).u8(FOR_RANGE).u16(0).u16(1).i32(17).ctx("t", "Trace").u8(LOAD_LOCAL).u16(0).method(T_INT, 1).u8(FOR_NEXT).u16(1).i32(-17).halt(); Program p = Program::load(a.file(SCHEMA), SCHEMA, h.reg); Context c; c.set("t", &h.trace); Program::RunStats stats; p.run(c, h.reg, &stats); assert(stats.instructions == 16); passed++; std::cout << "ok - optional stats count executed loop dispatches, not encoded instructions\n"; }
  // Sections: max_stack is recorded, the line table is kept and queried, hidden locals are flagged.
  { Asm a; a.maxStack = 7; a.local("int").local("Thing", 1).line(0, 3, 5).line(9, 4, 1); a.pushInt(1).u8(STORE_LOCAL).u16(0).halt(); Program p = Program::load(a.file(SCHEMA), SCHEMA, h.reg); assert(p.max_stack() == 7 && p.locals().size() == 2 && p.locals()[1].type == "Thing" && p.locals()[1].hidden && !p.locals()[0].hidden); assert(p.lines().size() == 2 && p.line_of(0)->line == 3 && p.line_of(8)->line == 3 && p.line_of(9)->line == 4 && p.line_of(12)->column == 1); passed++; std::cout << "ok - max_stack, typed locals with hidden flags and the line table are read back\n"; }
  // Loader rejections (before anything runs).
  { Asm a; a.halt(); expect_error("bad magic", [&] { Program::load(a.file(SCHEMA, VERSION, "XXXX"), SCHEMA, h.reg); }, "bad magic"); }
  { Asm a; a.halt(); expect_error("version 3 refused (no backward compatibility)", [&] { Program::load(a.file(SCHEMA, 3), SCHEMA, h.reg); }, "unsupported bytecode version 3 (this runtime reads version 6)"); }
  { Asm a; a.halt(); expect_error("version 4 refused (no backward compatibility)", [&] { Program::load(a.file(SCHEMA, 4), SCHEMA, h.reg); }, "unsupported bytecode version 4 (this runtime reads version 6)"); }
  { Asm a; a.halt(); expect_error("version 5 refused (v6 is the only accepted version)", [&] { Program::load(a.file(SCHEMA, 5), SCHEMA, h.reg); }, "unsupported bytecode version 5 (this runtime reads version 6)"); }
  { Asm a; a.halt(); expect_error("schema mismatch", [&] { Program::load(a.file(SCHEMA), SCHEMA + " ", h.reg); }, "schema mismatch"); }
  { Asm a; a.ctx("t", "Trace").halt(); std::string f = a.file(SCHEMA); for (size_t n = 0; n < f.size(); ++n) { try { Program::load(f.substr(0, n), SCHEMA, h.reg); std::cerr << "FAIL truncation at " << n << " loaded\n"; return 1; } catch (const Error&) {} } passed++; std::cout << "ok - every truncation is refused\n"; expect_error("truncated", [&] { Program::load(f.substr(0, f.size() - 3), SCHEMA, h.reg); }, "bytecode is truncated"); }
  { Asm a; a.halt(); expect_error("trailing bytes", [&] { Program::load(a.file(SCHEMA) + "x", SCHEMA, h.reg); }, "trailing byte(s) after the entries"); }
  { Asm a; a.pushStr("x").halt(); std::string f = a.file(SCHEMA); f[4 + 2 + 32 + 4] = 7; expect_error("unknown constant tag", [&] { Program::load(f, SCHEMA, h.reg); }, "constant 0 has unknown tag 7 (1 = string)"); }
  { Asm a; a.u8(PUSH_STR).u32(9).halt(); expect_error("constant index out of range", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "constant index 9 out of range"); }
  { Asm a; a.u8(LOAD_LOCAL).u16(0).halt(); expect_error("local slot out of range", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "local slot index 0 out of range"); }
  { Asm a; a.u8(LOAD_CTX).u16(0).halt(); expect_error("context index out of range", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "context index 0 out of range"); }
  { Asm a; a.ctx("nope", "Trace").halt(); expect_error("undeclared context", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "context variable 'nope' is not declared"); }
  { Asm a; a.ctx("t", "Thing").halt(); expect_error("context type differs", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "context variable 't' is declared as Thing by the bytecode, the host provides Trace"); }
  { Asm a; a.fn("fn missing() -> int", 0).halt(); expect_error("unbound function import", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "import 'fn missing() -> int' is not provided by the host"); }
  { Asm a; a.pushFloat(1.0).fn("fn twice(float) -> int", 1).halt(); expect_error("import with other types names the host's signature", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "import 'fn twice(float) -> int' is not provided by the host (the host declares fn twice(int) -> int)"); }
  { Asm a; a.ctx("root", "Thing").method("method Thing.name() -> string", 0).halt(); expect_error("unbound method import", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "import 'method Thing.name() -> string' is not provided by the host"); }
  { Asm a; a.ctx("root", "Thing").method("method Thing.id(int) -> int", 1).halt(); expect_error("method import with other types", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "(the host declares method Thing.id() -> int)"); }
  { Asm a; a.pushInt(1).pushInt(2).fn(TWICE, 2).halt(); expect_error("CALL_FN arity differs from the import", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "passes 2 argument(s), the import takes 1"); }
  { Asm a; a.ctx("root", "Thing").fn(THING_ID, 0).halt(); expect_error("CALL_FN on a method import", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "which is a method"); }
  { Asm a; a.pushInt(1).method(TWICE, 1).halt(); expect_error("CALL_METHOD on a function import", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "which is a function"); }
  { Asm a; a.u8(CALL_FN).u16(4).u8(0).halt(); expect_error("import index out of range", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "import index 4 out of range"); }
  { Asm a; a.u8(JUMP).i32(1).halt(); expect_error("jump into an operand", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "not an instruction start"); }
  { Asm a; a.u8(JUMP).i32(-5).halt(); expect_error("backward jump", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "goes backwards"); }
  { Asm a; a.u8(LOOP_BACK).i32(-5).halt(); expect_error("stray LOOP_BACK", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "has no matching WHILE_BEGIN"); }
  { Asm a; a.u8(WHILE_BEGIN); expect_error("unpaired WHILE_BEGIN", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "unclosed WHILE_BEGIN"); }
  { Asm a; a.u8(JUMP).i32(8).u8(WHILE_BEGIN).pushBool(false).u8(WHILE_TEST).i32(5).u8(LOOP_BACK).i32(-13).halt(); expect_error("ordinary jump cannot enter loop body", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "crosses a loop boundary"); }
  { Asm a; a.u8(WHILE_BEGIN).pushInt(1).pushBool(false).u8(WHILE_TEST); const size_t exit = a.code.size(); a.i32(0);
    a.u8(POP).u8(LOOP_BACK); const size_t back = a.code.size(); a.i32(0); const size_t end = a.code.size(); a.halt(); a.patch(exit, end); a.patch(back, 0);
    expect_error("while condition must preserve the head stack below its bool", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "WHILE_TEST must leave the loop-head stack depth");
  }
  { Asm a; a.pushBool(true); const size_t head = a.code.size(); a.u8(WHILE_BEGIN).u8(WHILE_TEST); const size_t exit = a.code.size(); a.i32(0);
    a.u8(JUMP); const size_t stop = a.code.size(); a.i32(0).u8(LOOP_BACK); const size_t back = a.code.size(); a.i32(0);
    const size_t end = a.code.size(); a.halt(); a.patch(exit, end); a.patch(stop, end); a.patch(back, head);
    expect_error("break-only while cannot consume a caller operand as its condition", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "WHILE_TEST must leave the loop-head stack depth");
  }
  for (bool to_exit : {false, true}) {
    Asm a; a.u8(WHILE_BEGIN).u8(JUMP); const size_t skip = a.code.size(); a.i32(0);
    a.pushBool(false).u8(WHILE_TEST); const size_t exit = a.code.size(); a.i32(0);
    const size_t back_pc = a.code.size(); a.u8(LOOP_BACK); const size_t back = a.code.size(); a.i32(0);
    const size_t end = a.code.size(); a.halt(); a.patch(skip, to_exit ? end : back_pc); a.patch(exit, end); a.patch(back, 0);
    expect_error(to_exit ? "condition cannot break before WHILE_TEST" : "condition cannot skip WHILE_TEST", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "skips its WHILE_TEST");
  }
  { Asm a; a.u8(WHILE_BEGIN).pushBool(false).u8(WHILE_TEST); const size_t exit = a.code.size(); a.i32(0).pushInt(1).u8(LOOP_BACK); const size_t back = a.code.size(); a.i32(0); const size_t end = a.code.size(); a.patch(exit, end); a.patch(back, 0); a.halt(); expect_error("loop back requires the head stack depth", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "inconsistent operand stack depth at control-flow join"); }
  { Asm a; a.local("int", 1).local("int", 1).pushInt(0).pushInt(1).u8(FOR_RANGE).u16(0).u16(1).i32(10).halt(); expect_error("HALT cannot occur inside a range loop", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "HALT inside loop"); }
  { Asm a; a.local("int", 1).local("int", 1).pushInt(0).pushInt(1).u8(FOR_RANGE).u16(0).u16(1).i32(10).u8(STORE_LOCAL).u16(0).u8(FOR_NEXT).u16(1).i32(-10).halt(); expect_error("loop counter is read-only", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "read-only range slot"); }
  { Asm a; a.local("int", 1).local("int", 1).pushInt(0).pushInt(1).u8(FOR_RANGE).u16(0).u16(1).i32(7).u8(FOR_NEXT).u16(1).i32(-7).pushInt(0).pushInt(1).u8(FOR_RANGE).u16(0).u16(1).i32(7).u8(FOR_NEXT).u16(1).i32(-7).halt(); expect_error("range slots cannot be reused by a sibling loop", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "already owned by another range loop"); }
  { Asm a; a.local("int", 1).local("int", 1).local("int", 1).local("int", 1).pushInt(0).pushInt(1).u8(FOR_RANGE).u16(0).u16(1).i32(12).u8(JUMP).i32(34).u8(FOR_NEXT).u16(1).i32(-12).pushInt(0).pushInt(1).u8(FOR_RANGE).u16(2).u16(3).i32(7).u8(FOR_NEXT).u16(3).i32(-7).halt(); expect_error("ordinary jump cannot enter a sibling loop body", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "crosses a loop boundary"); }
  { Asm a; a.pushInt(1); expect_error("missing HALT", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "does not end with HALT"); }
  { Asm a; a.u8(0x7f).halt(); expect_error("unknown opcode", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "unknown opcode 127"); }
  { Asm a; a.u8(MATH).u8(18).halt(); expect_error("unknown MATH function", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "unknown MATH function 18"); }
  { Asm a; a.u8(PUSH_BOOL).u8(2).halt(); expect_error("bad bool operand", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "PUSH_BOOL operand must be 0 or 1"); }
  { Asm a; a.local("int", 2); a.halt(); expect_error("bad hidden flag", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "hidden flag must be 0 or 1"); }
  { Asm a; a.line(5, 1, 1).line(2, 2, 1); a.pushInt(1).u8(POP).halt(); expect_error("unsorted line table", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "line table is not sorted"); }
  { Asm a; a.line(99, 1, 1); a.halt(); expect_error("line entry beyond the code", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "beyond the code"); }
  // Run-time checks (a correct compiler never produces these; the VM stays defensive). Overflow is not one of them.
  { Asm a; a.pushInt(1).pushInt(0).u8(DIV_I).halt(); expect_error("int division by zero", [&] { h.run(a.file(SCHEMA)); }, "int division by zero"); }
  { Asm a; a.pushInt(1).pushInt(0).u8(MOD_I).halt(); expect_error("int modulo by zero", [&] { h.run(a.file(SCHEMA)); }, "int modulo by zero"); }
  { Asm a; a.pushInt(1).pushStr("x").u8(EQ).halt(); expect_error("mixed-kind equality", [&] { h.run(a.file(SCHEMA)); }, "cannot compare int with string"); }
  { Asm a; a.pushInt(1).pushFloat(1.0).u8(ADD_F).halt(); expect_error("float op on int", [&] { h.run(a.file(SCHEMA)); }, "float operator applied to int and float"); }
  { Asm a; a.local("int"); a.u8(LOAD_LOCAL).u16(0).halt(); expect_error("local read before store", [&] { h.run(a.file(SCHEMA)); }, "read before assignment"); }
  { Asm a; a.pushInt(1).u8(JUMP_IF_FALSE).i32(0).halt(); expect_error("non-bool condition", [&] { h.run(a.file(SCHEMA)); }, "condition is a int, not a bool"); }
  { Asm a; a.u8(POP).halt(); expect_error("stack underflow", [&] { h.run(a.file(SCHEMA)); }, "stack underflow"); }
  { Asm a; a.fn(BOOM, 0).halt(); expect_error("host error propagates", [&] { h.run(a.file(SCHEMA)); }, "boom from the host"); }
  { Asm a; a.ctx("t", "Trace").halt(); Program p = Program::load(a.file(SCHEMA), SCHEMA, h.reg); Context empty; expect_error("context object missing at run", [&] { p.run(empty, h.reg); }, "context variable 't' was not provided by the host for this run"); }
  { Asm a; a.ctx("t", "Trace").method(THING_ID, 0).halt(); expect_error("method on the wrong object type", [&] { h.run(a.file(SCHEMA)); }, "method Thing.id called on a Trace"); }
  // Registry signatures: signatures, duplicate registrations, error codes, the result line.
  { auto sigs = h.reg.host_signatures(); std::vector<std::string> want = {"context root: Thing", "context t: Trace", "fn boom() -> void", "fn twice(int) -> int", "method Thing.id() -> int", "method Trace.int(int) -> void", "method Trace.str(string) -> void"}; assert(sigs == want); passed++; std::cout << "ok - signatures() lists the sorted canonical strings\n"; }
  { Registry dup; dup.function("f", {}, "void", [](Args&) { return Value::none(); }); expect_error("duplicate registration", [&] { dup.function("f", {}, "void", [](Args&) { return Value::none(); }); }, "registered twice"); }
  { Registry over; over.function("f", {"int"}, "int", [](Args&) { return Value::integer(1); }); over.function("f", {"float"}, "float", [](Args&) { return Value::real(1); }); over.function("f", {"int", "int"}, "int", [](Args&) { return Value::integer(2); }); auto sigs = over.host_signatures(); assert(sigs.size() == 3 && sigs[0] == "fn f(float) -> float" && sigs[1] == "fn f(int) -> int" && sigs[2] == "fn f(int, int) -> int"); expect_error("duplicate overload parameter tuple", [&] { over.function("f", {"int"}, "float", [](Args&) { return Value::real(1); }); }, "registered twice"); passed++; std::cout << "ok - overloads are keyed by parameter types and canonical imports\n"; }
  { Registry over; int picked = 0; over.function("pick", {"int"}, "void", [&](Args&) { picked = 1; return Value::none(); }); over.function("pick", {"float"}, "void", [&](Args&) { picked = 2; return Value::none(); }); over.function("pick", {"int", "int"}, "void", [&](Args&) { picked = 3; return Value::none(); }); Asm a; a.pushInt(1).fn("fn pick(int) -> void", 1).pushFloat(1.5).fn("fn pick(float) -> void", 1).pushInt(2).pushInt(3).fn("fn pick(int, int) -> void", 2).halt(); Program p = Program::load(a.file(SCHEMA), SCHEMA, over); Context c; p.run(c, over); assert(picked == 3); passed++; std::cout << "ok - canonical overload imports dispatch the selected closure\n"; }
  { Registry over; int picked = 0; over.context("thing", "Thing"); over.method("Thing", "pick", {"int"}, "void", [&](void*, Args&) { picked = 1; return Value::none(); }); over.method("Thing", "pick", {"float"}, "void", [&](void*, Args&) { picked = 2; return Value::none(); }); over.method("Thing", "pick", {"int", "int"}, "void", [&](void*, Args&) { picked = 3; return Value::none(); }); Asm a; a.ctx("thing", "Thing").pushInt(1).method("method Thing.pick(int) -> void", 1).ctx("thing", "Thing").pushFloat(1.5).method("method Thing.pick(float) -> void", 1).ctx("thing", "Thing").pushInt(2).pushInt(3).method("method Thing.pick(int, int) -> void", 2).halt(); Program p = Program::load(a.file(SCHEMA), SCHEMA, over); Thing thing{0}; Context c; c.set("thing", &thing); p.run(c, over); assert(picked == 3); passed++; std::cout << "ok - canonical method overload imports dispatch the selected closure\n"; }
  { Asm a; a.fn("fn missing() -> void", 0).halt(); try { Program::load(a.file(SCHEMA), SCHEMA, h.reg); } catch (const Error& e) { assert(e.code == "LOAD_UNBOUND"); passed++; std::cout << "ok - unbound imports carry LOAD_UNBOUND\n"; } }
  { Asm a; a.halt(); try { Program::load(a.file(SCHEMA), SCHEMA + " ", h.reg); } catch (const Error& e) { assert(e.code == "LOAD_SCHEMA_MISMATCH"); passed++; std::cout << "ok - schema mismatch carries LOAD_SCHEMA_MISMATCH\n"; } }
  { Asm a; a.halt(); try { Program::load(a.file(SCHEMA, VERSION, "XXXX"), SCHEMA, h.reg); } catch (const Error& e) { assert(e.code == "LOAD_BAD_BYTECODE"); passed++; std::cout << "ok - malformed files carry LOAD_BAD_BYTECODE\n"; } }
  { Asm a; a.fn(BOOM, 0).halt(); try { h.run(a.file(SCHEMA)); } catch (const Error& e) { assert(e.code == "RUN_HOST"); passed++; std::cout << "ok - host-thrown errors carry RUN_HOST\n"; } }
  { Asm a; a.pushInt(1).pushInt(0).u8(DIV_I).halt(); try { h.run(a.file(SCHEMA)); } catch (const Error& e) { assert(e.code == "RUN_ERROR"); passed++; std::cout << "ok - VM errors carry RUN_ERROR\n"; } }
  { Asm a; a.line(0, 1, 1).pushInt(1).pushInt(0).line(18, 9, 7).u8(DIV_I).line(19, 99, 1).halt(); bool failed = false; try { h.run(a.file(SCHEMA)); } catch (const Error& e) { failed = true; assert(e.code == "RUN_ERROR" && e.has_location && e.line == 9 && e.column == 7 && std::string(e.what()).find("dsl line 9:7: int division by zero") == 0); passed++; std::cout << "ok - VM errors use the faulting opcode pc for line locations\n"; } assert(failed); }
  { Asm a; a.line(0, 4, 2).fn(BOOM, 0).halt(); bool failed = false; try { h.run(a.file(SCHEMA)); } catch (const Error& e) { failed = true; assert(e.code == "RUN_HOST" && e.has_location && e.line == 4 && e.column == 2); passed++; std::cout << "ok - host errors are located at the call opcode\n"; } assert(failed); }
  { Asm a; a.pushInt(1).pushInt(0).u8(DIV_I).halt(); bool failed = false; try { h.run(a.file(SCHEMA)); } catch (const Error& e) { failed = true; assert(!e.has_location); passed++; std::cout << "ok - empty line tables preserve unlocated errors\n"; } assert(failed); }
  { Error e("RUN_ERROR", "x"); e.locate(6, 0); assert(e.has_location && !e.has_column && std::string(e.what()) == "dsl line 6: x"); Error empty("RUN_ERROR", "x"); empty.locate(0, 0); assert(!empty.has_location); passed++; std::cout << "ok - column zero keeps a line-only location and line zero stays unknown\n"; }
  { std::ostringstream output; auto* old = std::cout.rdbuf(output.rdbuf()); Error full("RUN_ERROR", "bad"); full.locate(4, 2); Error line_only("RUN_HOST", "host"); line_only.locate(5, 0); Error plain("APP_FAIL", "app"); assert(report_error("run", full) == 3); assert(report_error("run", line_only) == 3); assert(report_error("app", plain) == 4); std::cout.rdbuf(old); assert(output.str() == "{\"ok\": false, \"phase\": \"run\", \"code\": \"RUN_ERROR\", \"message\": \"dsl line 4:2: bad\", \"line\": 4, \"column\": 2}\n{\"ok\": false, \"phase\": \"run\", \"code\": \"RUN_HOST\", \"message\": \"dsl line 5: host\", \"line\": 5}\n{\"ok\": false, \"phase\": \"app\", \"code\": \"APP_FAIL\", \"message\": \"app\"}\n"); full.locate(9, 9); assert(full.line == 4 && full.column == 2); passed++; std::cout << "ok - JSON results carry full, line-only and absent locations\n"; }
  { assert(report_error("load", Error("LOAD_UNBOUND", "x")) == 3); assert(report_error("run", Error("host said no")) == 3); assert(report_app_failure("nothing to do") == 4); assert(report_ok("42") == 0); assert(json_escape("a\"b\n") == "\"a\\\"b\\n\""); passed++; std::cout << "ok - result lines and exit codes 3 (load/run) and 4 (application)\n"; }
  // No VM resource limits (design decision): deep stacks and large strings are the host's memory to spend, not the VM's to refuse.
  { Asm a; a.maxStack = 1; for (int i = 0; i < 5000; ++i) a.pushInt(1); for (int i = 0; i < 4999; ++i) a.u8(ADD_I); a.ctx("t", "Trace").u8(DUP).u8(POP); a.halt(); Program p = Program::load(a.file(SCHEMA), SCHEMA, h.reg); Context c; c.set("t", &h.trace); p.run(c, h.reg); passed++; std::cout << "ok - 5000 operands on the stack run without a cap (max_stack is a reservation hint only)\n"; }
  { Asm a; a.ctx("t", "Trace").pushStr(std::string(65536, 'x')); for (int i = 0; i < 9; ++i) a.u8(DUP).u8(CONCAT); a.method(T_STR, 1).halt(); auto got = h.run(a.file(SCHEMA)); assert(got.size() == 1 && got[0].size() == 4 + 65536u * 512u); passed++; std::cout << "ok - a 32 MiB concatenation succeeds (no string cap)\n"; }
  // Mutating methods, Context::set_dry, PlanEntry call sites, VmConfig::step_budget, and set_std_output.
  {
    struct ChangeObj { std::string json_str; };
    std::vector<ChangeObj> changes;
    changes.reserve(16);
    int plan_calls = 0, apply_calls = 0;
    Registry reg;
    reg.context("net", "Net");
    reg.method("Change", "json", {}, "string", [](void* s, Args&) {
      return Value::string(static_cast<ChangeObj*>(s)->json_str);
    });
    reg.mutating_method(
        "Net", "link_set", {"string", "int"}, "Change",
        [&](void*, Args& a) {
          ++plan_calls;
          changes.push_back(ChangeObj{"{\"planned\":\"" + a[0].s + "\",\"mtu\":" + std::to_string(a[1].i) + ",\"budget_exceeded\":false}"});
          return Value::object("Change", &changes.back());
        },
        [&](void*, Args& a) {
          ++apply_calls;
          changes.push_back(ChangeObj{"{\"applied\":\"" + a[0].s + "\",\"mtu\":" + std::to_string(a[1].i) + "}"});
          return Value::object("Change", &changes.back());
        });
    std::ostringstream out_sink;
    reg.function("println", {"string"}, "void", [](Args& a) {
      std::cout << a[0].s << "\n";
      return Value::none();
    });
    reg.set_std_output(&out_sink);

    Asm a;
    a.pushStr("hello").fn("fn println(string) -> void", 1);
    const size_t call1_pc = a.code.size() + 3 + 5 + 9; // after ctx + pushStr + pushInt
    a.ctx("net", "Net").pushStr("eth0").pushInt(1400);
    assert(a.code.size() == call1_pc);
    a.line(uint32_t(call1_pc), 12, 5);
    a.method("method Net.link_set(string, int) -> Change", 2).u8(POP);
    const size_t call2_pc = a.code.size() + 3 + 5 + 9;
    a.ctx("net", "Net").pushStr("eth1").pushInt(9000);
    assert(a.code.size() == call2_pc);
    a.line(uint32_t(call2_pc), 15, 3);
    a.method("method Net.link_set(string, int) -> Change", 2).u8(POP);
    a.halt();

    Program p = Program::load(a.file(SCHEMA), SCHEMA, reg);
    Thing net_obj{1};

    // Dry mode: runs plan_fn, records PlanEntry with call-site line/column and detail_json.
    Context dry_ctx;
    dry_ctx.set("net", &net_obj);
    dry_ctx.set_dry(true);
    dry_ctx.set_file("test.dslb");
    p.run(dry_ctx, reg);
    assert(plan_calls == 2 && apply_calls == 0);
    assert(!dry_ctx.truncated() && dry_ctx.plan_entries().size() == 2);
    assert(out_sink.str() == "hello\n");
    const auto& e0 = dry_ctx.plan_entries()[0];
    assert(e0.seq == 1 && e0.kind == "host_effect" && e0.function == "net.link_set");
    assert(e0.arguments_json == "[\"eth0\",1400]" && e0.file == "test.dslb" && e0.line == 12 && e0.column == 5);
    assert(e0.simulated && e0.detail_json == "{\"planned\":\"eth0\",\"mtu\":1400,\"budget_exceeded\":false}" && !e0.budget_exceeded);
    const auto& e1 = dry_ctx.plan_entries()[1];
    assert(e1.seq == 2 && e1.line == 15 && e1.column == 3 && e1.arguments_json == "[\"eth1\",9000]");

    // Step budget / dry loop limit: stops before (N+1)th mutating call.
    plan_calls = 0;
    Context lim_ctx;
    lim_ctx.set("net", &net_obj);
    lim_ctx.set_dry(true);
    VmConfig cfg;
    cfg.step_budget = 1;
    lim_ctx.set_config(cfg);
    p.run(lim_ctx, reg);
    assert(plan_calls == 1 && lim_ctx.truncated() && lim_ctx.stop_reason() == "dry loop limit" && lim_ctx.plan_entries().size() == 1);

    // Apply mode: runs apply_fn, records no plan entries.
    Context app_ctx;
    app_ctx.set("net", &net_obj);
    app_ctx.set_dry(false);
    p.run(app_ctx, reg);
    assert(apply_calls == 2 && app_ctx.plan_entries().empty());
    passed++;
    std::cout << "ok - mutating_method, Context::set_dry, PlanEntry locations, VmConfig::step_budget, and set_std_output\n";
  }
  // ---- v6 rule entries ----
  // A value entry and a void entry sharing the file's pools; the body is HALT only.
  {
    Asm rules;
    Asm inc;  // increment(x) -> x + 1: LOAD_LOCAL 0; PUSH_INT 1; ADD_I; HALT
    inc.u8(LOAD_LOCAL).u16(0).pushInt(1).u8(ADD_I).halt();
    Asm touch;  // touch(thing) -> void: Trace.int(thing.id())
    touch.u8(LOAD_CTX).u16(rules.ctxIndex("t", "Trace"));
    touch.u8(LOAD_LOCAL).u16(0);
    touch.u8(CALL_METHOD).u16(rules.imp(THING_ID)).u8(0);
    touch.u8(CALL_METHOD).u16(rules.imp(T_INT)).u8(1);
    touch.halt();
    rules.halt();
    rules.entry({"increment", "step", "interface Step(int) -> int", 1, {{"int", 0}}, 2, inc.code, {}});
    rules.entry({"touch", "visit", "interface Visit(Thing) -> void", 1, {{"Thing", 0}}, 2, touch.code, {}});
    Program p = Program::load(rules.file(SCHEMA), SCHEMA, h.reg);
    const std::vector<Program::Entry>& entries = p.entries();
    assert(entries.size() == 2);
    assert(entries[0].name == "increment" && entries[0].annotation == "step" && entries[0].iface == "interface Step(int) -> int");
    assert(entries[0].params == std::vector<std::string>{"int"} && entries[0].returns == "int");
    assert(entries[1].name == "touch" && entries[1].annotation == "visit" && entries[1].iface == "interface Visit(Thing) -> void");
    assert(entries[1].params == std::vector<std::string>{"Thing"} && entries[1].returns == "void");
    passed++; std::cout << "ok - entries load with names, annotations, interface text, params and returns\n";
    Context c; c.set("t", &h.trace); c.set("root", &h.root);
    h.root.id = 7; h.trace.clear();
    p.run(c, h.reg);
    assert(h.trace.empty());  // run() executes only the body
    Program::RunStats stats;
    Value v = p.call(0, c, h.reg, {Value::integer(41)}, &stats);
    assert(v.kind == Value::INT && v.i == 42 && stats.instructions == 4);
    Value none = p.call(1, c, h.reg, {Value::object("Thing", &h.root)});
    assert(none.kind == Value::VOID);
    assert(h.trace == std::vector<std::string>{"int 7"});
    passed++; std::cout << "ok - call() runs a value entry and a void entry, run() stays body-only\n";
    try { p.call(0, c, h.reg, {}); std::cerr << "FAIL call arity: no error\n"; return 1; }
    catch (const Error& e) {
      assert(e.code == "RUN_HOST");
      assert(std::string(e.what()) == "call entry 'increment' expects 1 argument(s); found 0");
      passed++; std::cout << "ok - call arity is RUN_HOST naming the entry: " << e.what() << "\n";
    }
    expect_error("call entry index out of range", [&] { p.call(5, c, h.reg, {}); }, "call entry index 5 out of range (2 entries)");
  }
  // Entry loader validation: parameter count/types must match the interface, parameter slots must be unhidden.
  { Asm a; Asm e; e.halt(); a.halt(); a.entry({"r", "k", "interface Flag() -> bool", 2, {{"int", 0}}, 1, e.code, {}});
    expect_error("entry nparams exceeds nlocals", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "entry 'r': nparams 2 exceeds nlocals 1"); }
  { Asm a; Asm e; e.u8(LOAD_LOCAL).u16(0).u8(LOAD_LOCAL).u16(1).u8(ADD_I).halt(); a.halt(); a.entry({"r", "k", "interface Step(int) -> int", 2, {{"int", 0}, {"int", 0}}, 2, e.code, {}});
    expect_error("entry parameter count differs from the interface", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "entry 'r' declares 2 parameter(s); interface Step(int) -> int has 1"); }
  { Asm a; Asm e; e.pushInt(1).halt(); a.halt(); a.entry({"r", "k", "interface Step(int) -> int", 1, {{"string", 0}}, 1, e.code, {}});
    expect_error("entry parameter type differs from the interface", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "entry 'r' parameter 1 has type string; the interface expects int"); }
  { Asm a; Asm e; e.u8(LOAD_LOCAL).u16(0).halt(); a.halt(); a.entry({"r", "k", "interface Step(int) -> int", 1, {{"int", 1}}, 1, e.code, {}});
    expect_error("entry parameter slot is hidden", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "entry 'r' parameter 1 is hidden"); }
  // Entry verification: jumps stay inside the region and HALT depth matches the interface.
  { Asm a; Asm e; e.pushBool(true).u8(JUMP_IF_FALSE).i32(5).halt(); a.halt(); a.entry({"badjump", "k", "interface Flag() -> bool", 0, {}, 1, e.code, {}});
    expect_error("entry jump out of region", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "entry 'badjump': jump at offset 2 targets 12, not an instruction start"); }
  { Asm a; Asm e; e.halt(); a.halt(); a.entry({"flag", "k", "interface Flag() -> bool", 0, {}, 1, e.code, {}});
    expect_error("value entry whose HALT leaves 0 values", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "entry 'flag': HALT leaves 0 values"); }
  { Asm a; Asm e; e.pushInt(1).halt(); a.halt(); a.entry({"noisy", "k", "interface Zap() -> void", 0, {}, 1, e.code, {}});
    expect_error("void entry whose HALT leaves a value", [&] { Program::load(a.file(SCHEMA), SCHEMA, h.reg); }, "entry 'noisy': HALT leaves 1 values"); }
  std::cout << "PASS " << passed << " checks\n";
  return 0;
}
