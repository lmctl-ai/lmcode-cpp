#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../stdlib/containers.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
using namespace dslvm;
using namespace dslvm::stdlib;

int main(int argc, char** argv) {
  if (argc == 2 && std::string(argv[1]) == "--signatures") {
    Registry registry(StdMode::Disabled); Collections arena;
    register_containers(registry, arena);
    for (const auto& signature : registry.signatures()) std::cout << signature << '\n';
    return 0;
  }
  Collections arena;
  Registry r(StdMode::Disabled);
  register_containers(r, arena);
  auto fn = [&](const std::string &s, Args a = {}) {
    auto *x = r.function_by_signature(s);
    assert(x);
    return x->fn(a);
  };
  auto m = [&](const std::string &s, void *p, Args a = {}) {
    auto *x = r.method_by_signature(s);
    assert(x);
    return x->fn(p, a);
  };
  auto fail = [&](auto f, const std::string &w) {
    try {
      f();
      assert(false);
    } catch (const Error &e) {
      assert(e.what() == w);
    }
  };
  auto im = fn("fn int_map() -> IntMap");
  m("method IntMap.set(string, int) -> IntMap", im.obj,
    {Value::string("x"), Value::integer(INT64_MIN)});
  m("method IntMap.set(string, int) -> IntMap", im.obj,
    {Value::string("y"), Value::integer(7)});
  assert(m("method IntMap.len() -> int", im.obj).i == 2);
  assert(
      m("method IntMap.get(string) -> int", im.obj, {Value::string("x")}).i ==
      INT64_MIN);
  assert(
      m("method IntMap.key_at(int) -> string", im.obj, {Value::integer(1)}).s ==
      "y");
  assert(m("method IntMap.remove(string) -> bool", im.obj, {Value::string("x")})
             .b);
  assert(
      !m("method IntMap.has(string) -> bool", im.obj, {Value::string("x")}).b);
  m("method IntMap.clear() -> IntMap", im.obj);
  assert(m("method IntMap.len() -> int", im.obj).i == 0);
  auto fm = fn("fn float_map() -> FloatMap");
  m("method FloatMap.set(string, float) -> FloatMap", fm.obj,
    {Value::string("n"),
     Value::real(std::numeric_limits<double>::quiet_NaN())});
  assert(
      m("method FloatMap.has(string) -> bool", fm.obj, {Value::string("n")}).b);
  assert(std::isnan(
      m("method FloatMap.get(string) -> float", fm.obj, {Value::string("n")})
          .f));
  assert(m("method FloatMap.get_or(string, float) -> float", fm.obj,
           {Value::string("missing"), Value::real(3.5)})
             .f == 3.5);
  assert(m("method FloatMap.key_at(int) -> string", fm.obj, {Value::integer(0)})
             .s == "n");
  assert(
      m("method FloatMap.remove(string) -> bool", fm.obj, {Value::string("n")})
          .b);
  m("method FloatMap.clear() -> FloatMap", fm.obj);
  auto st = fn("fn int_stack() -> IntStack");
  m("method IntStack.push(int) -> IntStack", st.obj, {Value::integer(1)});
  m("method IntStack.push(int) -> IntStack", st.obj, {Value::integer(2)});
  assert(m("method IntStack.at(int) -> int", st.obj, {Value::integer(0)}).i ==
         2);
  assert(m("method IntStack.pop() -> int", st.obj).i == 2);
  assert(m("method IntStack.peek() -> int", st.obj).i == 1);
  fail(
      [&] { m("method IntStack.at(int) -> int", st.obj, {Value::integer(7)}); },
      "IntStack.at: index out of range: 7");
  auto fst = fn("fn float_stack() -> FloatStack");
  m("method FloatStack.push(float) -> FloatStack", fst.obj,
    {Value::real(1.25)});
  assert(m("method FloatStack.peek() -> float", fst.obj).f == 1.25);
  assert(m("method FloatStack.pop() -> float", fst.obj).f == 1.25);
  auto sst = fn("fn str_stack() -> StrStack");
  m("method StrStack.push(string) -> StrStack", sst.obj, {Value::string("s")});
  m("method StrStack.clear() -> StrStack", sst.obj);
  assert(m("method StrStack.empty() -> bool", sst.obj).b);
  auto qu = fn("fn str_queue() -> StrQueue");
  const std::string raw("a\0\xff", 3);
  m("method StrQueue.push(string) -> StrQueue", qu.obj, {Value::string(raw)});
  m("method StrQueue.push(string) -> StrQueue", qu.obj, {Value::string("b")});
  assert(
      m("method StrQueue.at(int) -> string", qu.obj, {Value::integer(0)}).s ==
      raw);
  assert(m("method StrQueue.pop() -> string", qu.obj).s == raw);
  assert(m("method StrQueue.peek() -> string", qu.obj).s == "b");
  auto iq = fn("fn int_queue() -> IntQueue");
  auto fq = fn("fn float_queue() -> FloatQueue");
  m("method IntQueue.push(int) -> IntQueue", iq.obj, {Value::integer(9)});
  m("method FloatQueue.push(float) -> FloatQueue", fq.obj, {Value::real(2.5)});
  assert(m("method IntQueue.pop() -> int", iq.obj).i == 9);
  assert(m("method FloatQueue.pop() -> float", fq.obj).f == 2.5);
  auto pq = fn("fn int_pq() -> IntPQ");
  m("method IntPQ.push(int, float) -> IntPQ", pq.obj,
    {Value::integer(4), Value::real(2)});
  m("method IntPQ.push(int, float) -> IntPQ", pq.obj,
    {Value::integer(1), Value::real(1)});
  m("method IntPQ.push(int, float) -> IntPQ", pq.obj,
    {Value::integer(2), Value::real(1)});
  assert(m("method IntPQ.pop() -> int", pq.obj).i == 1);
  assert(m("method IntPQ.pop() -> int", pq.obj).i == 2);
  assert(m("method IntPQ.pop() -> int", pq.obj).i == 4);
  fail([&] { m("method IntPQ.peek() -> int", pq.obj); }, "IntPQ.peek: empty");
  fail(
      [&] {
        m("method IntPQ.push(int, float) -> IntPQ", pq.obj,
          {Value::integer(1), Value::real(NAN)});
      },
      "IntPQ.push: priority must be finite");
  auto sp = fn("fn str_pq() -> StrPQ");
  auto fp2 = fn("fn float_pq() -> FloatPQ");
  m("method StrPQ.push(string, float) -> StrPQ", sp.obj,
    {Value::string("x"), Value::real(1)});
  m("method FloatPQ.push(float, float) -> FloatPQ", fp2.obj,
    {Value::real(3.25), Value::real(1)});
  assert(m("method StrPQ.pop() -> string", sp.obj).s == "x");
  assert(m("method FloatPQ.pop() -> float", fp2.obj).f == 3.25);
  auto *sequence = static_cast<IntPQ *>(pq.obj);
  sequence->next_sequence = std::numeric_limits<int64_t>::max() - 1;
  m("method IntPQ.push(int, float) -> IntPQ", pq.obj,
    {Value::integer(8), Value::real(0)});
  const auto before_exhaustion = sequence->items.size();
  fail(
      [&] {
        m("method IntPQ.push(int, float) -> IntPQ", pq.obj,
          {Value::integer(9), Value::real(0)});
      },
      "IntPQ.push: insertion sequence exhausted");
  assert(sequence->items.size() == before_exhaustion);
  m("method IntPQ.clear() -> IntPQ", pq.obj);
  assert(sequence->next_sequence == 0);
  auto scale = fn("fn int_pq() -> IntPQ");
  for (int i = 999; i >= 0; --i)
    m("method IntPQ.push(int, float) -> IntPQ", scale.obj,
      {Value::integer(i), Value::real(static_cast<double>(i))});
  for (int i = 0; i < 1000; ++i)
    assert(m("method IntPQ.pop() -> int", scale.obj).i == i);
  auto ties = fn("fn int_pq() -> IntPQ");
  for (int i = 0; i < 1000; ++i)
    m("method IntPQ.push(int, float) -> IntPQ", ties.obj,
      {Value::integer(i), Value::real(1)});
  for (int i = 0; i < 1000; ++i)
    assert(m("method IntPQ.pop() -> int", ties.obj).i == i);
  auto fp = fn("fn float_pq() -> FloatPQ");
  m("method FloatPQ.push(float, float) -> FloatPQ", fp.obj,
    {Value::real(-0.0), Value::real(0)});
  m("method FloatPQ.push(float, float) -> FloatPQ", fp.obj,
    {Value::real(1.5), Value::real(-0.0)});
  assert(std::signbit(m("method FloatPQ.pop() -> float", fp.obj).f));
  while (arena.size() < 65536)
    fn("fn int_stack() -> IntStack");
  fail([&] { fn("fn str_queue() -> StrQueue"); },
       "StrQueue object limit exceeded (65536)");
  std::cout << "containers pass\n";
  return 0;
}
