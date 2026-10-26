#include "../stdlib/collections.hpp"
#include <iostream>
#include <limits>
using namespace dslvm;
using namespace dslvm::stdlib;
int main(int argc, char** argv) {
  Collections arena; Registry r(StdMode::Disabled); register_collections(r, arena);
  if (argc == 2 && std::string(argv[1]) == "--signatures") { for (const auto& s : r.signatures()) std::cout << s << '\n'; return 0; }
  int checks = 0;
  auto check = [&](bool ok) { if (!ok) { std::cerr << "collection check " << checks + 1 << " failed\n"; std::abort(); } ++checks; };
  auto call = [&](const std::string& sig, void* self, Args args = {}) { auto entry = r.method_by_signature(sig); if (!entry) throw Error("missing " + sig); return entry->fn(self, args); };
  auto construct = [&](const std::string& sig, Args args = {}) { auto entry = r.function_by_signature(sig); if (!entry) throw Error("missing " + sig); return entry->fn(args); };
  auto refused = [&](auto fn, const std::string& message) { try { fn(); check(false); } catch (const Error& e) { check(e.code == "RUN_HOST" && e.what() == message); } };
  auto s = construct("fn str_list() -> StrList");
  check(call("method StrList.add(string) -> StrList", s.obj, {Value::string(std::string("a\0\xff", 3))}).obj == s.obj);
  check(call("method StrList.at(int) -> string", s.obj, {Value::integer(0)}).s == std::string("a\0\xff", 3));
  check(call("method StrList.set(int, string) -> StrList", s.obj, {Value::integer(0), Value::string("b")}).obj == s.obj);
  check(call("method StrList.contains(string) -> bool", s.obj, {Value::string("b")}).b);
  check(call("method StrList.index_of(string) -> int", s.obj, {Value::string("x")}).i == -1);
  check(call("method StrList.join(string) -> string", s.obj, {Value::string(",")}).s == "b");
  for (int64_t i : {INT64_MIN, int64_t(-1), int64_t(1), INT64_MAX}) {
    refused([&] { call("method StrList.at(int) -> string", s.obj, {Value::integer(i)}); }, "StrList.at: index out of range: " + std::to_string(i));
    refused([&] { call("method StrList.set(int, string) -> StrList", s.obj, {Value::integer(i), Value::string("x")}); }, "StrList.set: index out of range: " + std::to_string(i));
  }
  auto split = construct("fn str_split(string, string) -> StrList", {Value::string(",a,,"), Value::string(",")});
  check(static_cast<StrList*>(split.obj)->items == std::vector<std::string>({"", "a", "", ""}));
  auto empty = construct("fn str_split(string, string) -> StrList", {Value::string(""), Value::string(",")}); check(static_cast<StrList*>(empty.obj)->items == std::vector<std::string>({""}));
  refused([&] { construct("fn str_split(string, string) -> StrList", {Value::string("a"), Value::string("")}); }, "str_split: separator must not be empty");
  auto ints = construct("fn int_list() -> IntList");
  check(call("method IntList.add(int) -> IntList", ints.obj, {Value::integer(INT64_MIN)}).obj == ints.obj);
  check(call("method IntList.at(int) -> int", ints.obj, {Value::integer(0)}).i == INT64_MIN);
  auto floats = construct("fn float_list() -> FloatList");
  const double nan = std::numeric_limits<double>::quiet_NaN();
  call("method FloatList.add(float) -> FloatList", floats.obj, {Value::real(nan)});
  check(!call("method FloatList.contains(float) -> bool", floats.obj, {Value::real(nan)}).b);
  check(call("method FloatList.index_of(float) -> int", floats.obj, {Value::real(nan)}).i == -1);
  auto map = construct("fn str_map() -> StrMap");
  for (const auto& key : {"a", "b", "a"}) check(call("method StrMap.set(string, string) -> StrMap", map.obj, {Value::string(key), Value::string("v")}).obj == map.obj);
  check(call("method StrMap.len() -> int", map.obj).i == 2);
  check(call("method StrMap.key_at(int) -> string", map.obj, {Value::integer(0)}).s == "a");
  check(call("method StrMap.get_or(string, string) -> string", map.obj, {Value::string("x"), Value::string("fallback")}).s == "fallback");
  refused([&] { call("method StrMap.get(string) -> string", map.obj, {Value::string("x")}); }, "StrMap.get: missing key");
  refused([&] { call("method StrMap.key_at(int) -> string", map.obj, {Value::integer(-1)}); }, "StrMap.key_at: index out of range: -1");
  check(call("method StrMap.remove(string) -> bool", map.obj, {Value::string("a")}).b);
  check(!call("method StrMap.remove(string) -> bool", map.obj, {Value::string("a")}).b);
  check(call("method StrMap.key_at(int) -> string", map.obj, {Value::integer(0)}).s == "b");
  check(call("method StrMap.clear() -> StrMap", map.obj).obj == map.obj);
  check(call("method StrList.clear() -> StrList", s.obj).obj == s.obj);
  check(call("method StrList.len() -> int", s.obj).i == 0);
  // Compaction must not resurrect a deleted first slot or lose a surviving key.
  auto churn = construct("fn str_map() -> StrMap");
  auto put = [&](void* obj, const std::string& key) { call("method StrMap.set(string, string) -> StrMap", obj, {Value::string(key), Value::string("v")}); };
  auto drop = [&](void* obj, const std::string& key) { return call("method StrMap.remove(string) -> bool", obj, {Value::string(key)}).b; };
  auto key_at = [&](void* obj, int64_t i) { return call("method StrMap.key_at(int) -> string", obj, {Value::integer(i)}).s; };
  put(churn.obj, "deleted"); put(churn.obj, "stable"); check(drop(churn.obj, "deleted"));
  bool churn_ok = true;
  for (int i = 0; i < 5000; ++i) {
    const auto key = std::to_string(i); put(churn.obj, key);
    churn_ok = churn_ok && key_at(churn.obj, 0) == "stable" && drop(churn.obj, key);
  }
  check(churn_ok && call("method StrMap.len() -> int", churn.obj).i == 1);
  check(!call("method StrMap.has(string) -> bool", churn.obj, {Value::string("deleted")}).b);
  check(static_cast<StrMap*>(churn.obj)->items.size() <= 1028);
  check(static_cast<StrMap*>(churn.obj)->entries() == std::vector<std::pair<std::string, std::string>>({{"stable", "v"}}));
  auto* native_map = arena.make_str_map(); native_map->set("native", "value");
  check(call("method StrMap.get(string) -> string", native_map, {Value::string("native")}).s == "value");
  auto bulk = construct("fn str_map() -> StrMap");
  for (int i = 0; i < 20000; ++i) put(bulk.obj, std::to_string(i));
  bool order_ok = true;
  for (int i = 0; i < 20000; i += 2) {
    order_ok = order_ok && drop(bulk.obj, std::to_string(i));
    order_ok = order_ok && key_at(bulk.obj, 20000 - i / 2 - 2) == "19999";
  }
  for (int i = 0; i < 10000; ++i) order_ok = order_ok && key_at(bulk.obj, i) == std::to_string(i * 2 + 1);
  check(order_ok);
  put(bulk.obj, "0"); put(bulk.obj, "1");
  check(key_at(bulk.obj, 10000) == "0" && key_at(bulk.obj, 0) == "1");
  while (arena.size() < 65536) arena.make_int_list();
  check(static_cast<StrList*>(s.obj)->items.empty());
  refused([&] { arena.make_str_list(); }, "StrList object limit exceeded (65536)");
  refused([&] { arena.make_int_list(); }, "IntList object limit exceeded (65536)");
  refused([&] { arena.make_float_list(); }, "FloatList object limit exceeded (65536)");
  refused([&] { arena.make_str_map(); }, "StrMap object limit exceeded (65536)");
  arena.reset(); check(arena.size() == 0 && arena.make_str_list()->items.empty());
  check(r.signatures().size() == 37);
  std::cout << "stdlib collections: " << checks << " checks passed\n";
}
