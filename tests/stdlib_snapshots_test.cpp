#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../stdlib/std.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
using namespace dslvm;
using namespace dslvm::stdlib;
static uint64_t bits(double value) { uint64_t out; std::memcpy(&out, &value, sizeof out); return out; }

int main() {
  Collections arena;
  Registry r(StdMode::Disabled);
  register_collections(r, arena);
  register_containers(r, arena);
  auto fn = [&](const std::string &s, Args a = {}) {
    auto *x = r.function_by_signature(s); assert(x); return x->fn(a);
  };
  auto m = [&](const std::string &s, void *p, Args a = {}) {
    auto *x = r.method_by_signature(s); assert(x); return x->fn(p, a);
  };
  auto im = fn("fn int_map() -> IntMap");
  m("method IntMap.set(string, int) -> IntMap", im.obj, {Value::string("a"), Value::integer(INT64_MIN)});
  m("method IntMap.set(string, int) -> IntMap", im.obj, {Value::string("b"), Value::integer(INT64_MAX)});
  auto ik = m("method IntMap.keys() -> StrList", im.obj);
  auto iv = m("method IntMap.values() -> IntList", im.obj);
  assert(m("method StrList.at(int) -> string", ik.obj, {Value::integer(0)}).s == "a");
  assert(m("method IntList.at(int) -> int", iv.obj, {Value::integer(1)}).i == INT64_MAX);
  m("method IntMap.set(string, int) -> IntMap", im.obj, {Value::string("a"), Value::integer(7)});
  assert(m("method IntList.at(int) -> int", iv.obj, {Value::integer(0)}).i == INT64_MIN);
  m("method IntMap.remove(string) -> bool", im.obj, {Value::string("b")});
  m("method IntMap.set(string, int) -> IntMap", im.obj, {Value::string("b"), Value::integer(9)});
  auto ik2 = m("method IntMap.keys() -> StrList", im.obj);
  assert(m("method StrList.at(int) -> string", ik2.obj, {Value::integer(0)}).s == "a");
  assert(m("method StrList.at(int) -> string", ik2.obj, {Value::integer(1)}).s == "b");
  m("method IntMap.clear() -> IntMap", im.obj);
  assert(m("method StrList.len() -> int", ik2.obj).i == 2);

  // Removal followed by reinsertion appends the key; prior snapshots remain independent.
  m("method IntMap.set(string, int) -> IntMap", im.obj, {Value::string("a"), Value::integer(1)});
  m("method IntMap.set(string, int) -> IntMap", im.obj, {Value::string("b"), Value::integer(2)});
  m("method IntMap.set(string, int) -> IntMap", im.obj, {Value::string("c"), Value::integer(3)});
  auto ordered_keys = m("method IntMap.keys() -> StrList", im.obj);
  auto ordered_values = m("method IntMap.values() -> IntList", im.obj);
  m("method IntMap.remove(string) -> bool", im.obj, {Value::string("b")});
  m("method IntMap.set(string, int) -> IntMap", im.obj, {Value::string("b"), Value::integer(22)});
  auto reordered = m("method IntMap.keys() -> StrList", im.obj);
  assert(m("method StrList.at(int) -> string", reordered.obj, {Value::integer(0)}).s == "a");
  assert(m("method StrList.at(int) -> string", reordered.obj, {Value::integer(1)}).s == "c");
  assert(m("method StrList.at(int) -> string", reordered.obj, {Value::integer(2)}).s == "b");
  auto reordered_values = m("method IntMap.values() -> IntList", im.obj);
  assert(m("method IntList.at(int) -> int", reordered_values.obj, {Value::integer(2)}).i == 22);
  assert(m("method StrList.at(int) -> string", ordered_keys.obj, {Value::integer(1)}).s == "b");
  assert(m("method IntList.at(int) -> int", ordered_values.obj, {Value::integer(1)}).i == 2);
  m("method StrList.set(int, string) -> StrList", ordered_keys.obj, {Value::integer(0), Value::string("changed")});
  m("method IntList.set(int, int) -> IntList", ordered_values.obj, {Value::integer(0), Value::integer(99)});
  assert(m("method IntMap.key_at(int) -> string", im.obj, {Value::integer(0)}).s == "a");
  assert(m("method IntMap.get(string) -> int", im.obj, {Value::string("a")}).i == 1);

  auto sm = fn("fn str_map() -> StrMap");
  const std::string raw("k\0\xff", 3), val("v\0\xfe", 3);
  m("method StrMap.set(string, string) -> StrMap", sm.obj, {Value::string(raw), Value::string(val)});
  auto sk = m("method StrMap.keys() -> StrList", sm.obj);
  auto sv = m("method StrMap.values() -> StrList", sm.obj);
  assert(m("method StrList.at(int) -> string", sk.obj, {Value::integer(0)}).s == raw);
  assert(m("method StrList.at(int) -> string", sv.obj, {Value::integer(0)}).s == val);
  m("method StrMap.clear() -> StrMap", sm.obj);
  assert(m("method StrList.len() -> int", sk.obj).i == 1);

  auto fm = fn("fn float_map() -> FloatMap");
  const uint64_t nan_bits = 0x7ff8000000001234ULL;
  double nan; std::memcpy(&nan, &nan_bits, sizeof nan);
  m("method FloatMap.set(string, float) -> FloatMap", fm.obj, {Value::string("n"), Value::real(nan)});
  auto fv = m("method FloatMap.values() -> FloatList", fm.obj);
  const double copied = m("method FloatList.at(int) -> float", fv.obj, {Value::integer(0)}).f;
  assert(bits(copied) == nan_bits);
  m("method FloatMap.set(string, float) -> FloatMap", fm.obj, {Value::string("z"), Value::real(-0.0)});
  assert(m("method FloatList.len() -> int", fv.obj).i == 1);
  auto fv2 = m("method FloatMap.values() -> FloatList", fm.obj);
  assert(std::signbit(m("method FloatList.at(int) -> float", fv2.obj, {Value::integer(1)}).f));
  auto fk = m("method FloatMap.keys() -> StrList", fm.obj);
  assert(m("method StrList.at(int) -> string", fk.obj, {Value::integer(0)}).s == "n");
  assert(m("method StrList.at(int) -> string", fk.obj, {Value::integer(1)}).s == "z");

  auto empty = fn("fn str_map() -> StrMap");
  auto empty_keys = m("method StrMap.keys() -> StrList", empty.obj);
  auto empty_values = m("method StrMap.values() -> StrList", empty.obj);
  assert(m("method StrList.len() -> int", empty_keys.obj).i == 0);
  assert(m("method StrList.len() -> int", empty_values.obj).i == 0);
  m("method StrList.add(string) -> StrList", empty_keys.obj, {Value::string("local")});
  assert(m("method StrMap.len() -> int", empty.obj).i == 0);
  assert(m("method StrList.len() -> int", empty_values.obj).i == 0);

  auto cap_check = [&](const std::string &constructor, const std::string &method,
                       const std::string &length, const std::string &error,
                       const std::function<void(Registry &, Value)> &seed) {
    Collections capped;
    Registry cr(StdMode::Disabled);
    register_collections(cr, capped); register_containers(cr, capped);
    Args no_args;
    auto object = cr.function_by_signature(constructor)->fn(no_args);
    seed(cr, object);
    auto call_method = [&](const std::string &sig, void *p, Args args = {}) {
      auto *entry = cr.method_by_signature(sig); assert(entry); return entry->fn(p, args);
    };
    const auto before = call_method(length, object.obj).i;
    while (capped.size() < 65536) { Args none; cr.function_by_signature("fn int_stack() -> IntStack")->fn(none); }
    try { call_method(method, object.obj); assert(false); }
    catch (const Error &e) { assert(e.what() == error); }
    assert(call_method(length, object.obj).i == before);
    capped.reset();
    Args fresh_args;
    auto fresh = cr.function_by_signature(constructor)->fn(fresh_args);
    auto snapshot = call_method(method, fresh.obj);
    assert(call_method("method " + snapshot.type + ".len() -> int", snapshot.obj).i == 0);
  };
  cap_check("fn str_map() -> StrMap", "method StrMap.keys() -> StrList", "method StrMap.len() -> int", "StrList object limit exceeded (65536)",
    [](Registry &rr, Value o) { auto *x = rr.method_by_signature("method StrMap.set(string, string) -> StrMap"); Args a{Value::string("k"), Value::string("v")}; x->fn(o.obj, a); });
  cap_check("fn str_map() -> StrMap", "method StrMap.values() -> StrList", "method StrMap.len() -> int", "StrList object limit exceeded (65536)",
    [](Registry &rr, Value o) { auto *x = rr.method_by_signature("method StrMap.set(string, string) -> StrMap"); Args a{Value::string("k"), Value::string("v")}; x->fn(o.obj, a); });
  cap_check("fn int_map() -> IntMap", "method IntMap.keys() -> StrList", "method IntMap.len() -> int", "StrList object limit exceeded (65536)",
    [](Registry &rr, Value o) { auto *x = rr.method_by_signature("method IntMap.set(string, int) -> IntMap"); Args a{Value::string("k"), Value::integer(1)}; x->fn(o.obj, a); });
  cap_check("fn int_map() -> IntMap", "method IntMap.values() -> IntList", "method IntMap.len() -> int", "IntList object limit exceeded (65536)",
    [](Registry &rr, Value o) { auto *x = rr.method_by_signature("method IntMap.set(string, int) -> IntMap"); Args a{Value::string("k"), Value::integer(1)}; x->fn(o.obj, a); });
  cap_check("fn float_map() -> FloatMap", "method FloatMap.keys() -> StrList", "method FloatMap.len() -> int", "StrList object limit exceeded (65536)",
    [](Registry &rr, Value o) { auto *x = rr.method_by_signature("method FloatMap.set(string, float) -> FloatMap"); Args a{Value::string("k"), Value::real(1)}; x->fn(o.obj, a); });
  cap_check("fn float_map() -> FloatMap", "method FloatMap.values() -> FloatList", "method FloatMap.len() -> int", "FloatList object limit exceeded (65536)",
    [](Registry &rr, Value o) { auto *x = rr.method_by_signature("method FloatMap.set(string, float) -> FloatMap"); Args a{Value::string("k"), Value::real(1)}; x->fn(o.obj, a); });

  std::cout << "snapshot pass\n";
}
