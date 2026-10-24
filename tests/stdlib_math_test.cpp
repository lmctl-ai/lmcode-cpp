#include "../stdlib/math.hpp"
#include <cassert>
#include <iostream>

using namespace dslvm::stdlib;
int main(int argc, char** argv) {
  dslvm::Registry registry(dslvm::StdMode::Disabled); register_math(registry);
  if (argc == 2 && std::string(argv[1]) == "--signatures") {
    for (const auto& signature : registry.signatures()) std::cout << signature << '\n';
    return 0;
  }
  int passed = 0;
  auto check = [&](bool ok) { if (!ok) { std::cerr << "math check " << passed + 1 << " failed\n"; std::abort(); } ++passed; };
  const int64_t lo = INT64_MIN, hi = INT64_MAX;
  for (int64_t x : {lo, int64_t(-1), int64_t(0), int64_t(1), hi}) {
    check(math_floor(x) == x && math_ceil(x) == x && math_round(x) == x);
  }
  check(math_abs(lo) == lo && math_abs(int64_t(-7)) == 7);
  check(math_min(lo, hi) == lo && math_max(lo, hi) == hi);
  check(math_pow(int64_t(0), int64_t(0)) == 1);
  check(math_pow(int64_t(2), int64_t(63)) == lo && math_pow(int64_t(2), int64_t(64)) == 0);
  check(math_pow(int64_t(-2), int64_t(3)) == -8 && math_pow(int64_t(-1), hi) == -1);
  check(math_pow(lo, int64_t(1)) == lo && math_pow(lo, int64_t(2)) == 0);
  for (int64_t x : {int64_t(0), int64_t(1), int64_t(2), int64_t(15), int64_t(16), int64_t(17), hi}) {
    const int64_t root = math_sqrt(x);
    check((root == 0 || root <= x / root) && root + 1 > x / (root + 1));
  }
  // A float-based sqrt rounds this value up to 3037000499 before truncation.
  check(math_sqrt(INT64_C(9223372030926249000)) == INT64_C(3037000498));
  for (int64_t x : {lo, int64_t(-1)}) {
    try { (void)math_sqrt(x); check(false); }
    catch (const dslvm::Error& e) { check(e.code == "RUN_HOST" && std::string(e.what()) == "math_sqrt: integer argument must be nonnegative"); }
    try { (void)math_pow(int64_t(2), x); check(false); }
    catch (const dslvm::Error& e) { check(e.code == "RUN_HOST" && std::string(e.what()) == "math_pow: integer exponent must be nonnegative"); }
  }
  const double nan = std::numeric_limits<double>::quiet_NaN(), inf = std::numeric_limits<double>::infinity();
  check(!std::signbit(math_abs(-0.0)) && math_abs(-inf) == inf && std::isnan(math_abs(nan)));
  for (double x : {-inf, -0.0, 0.0, inf, nan}) {
    check(std::isnan(math_min(x, nan)) && std::isnan(math_max(nan, x)));
    check(std::isnan(math_min(nan, x)) && std::isnan(math_max(x, nan)));
  }
  check(std::signbit(math_min(-0.0, 0.0)) && !std::signbit(math_min(0.0, -0.0)));
  check(std::signbit(math_max(-0.0, 0.0)) && !std::signbit(math_max(0.0, -0.0)));
  check(math_min(-inf, inf) == -inf && math_max(-inf, inf) == inf);
  check(math_floor(-1.2) == -2.0 && math_ceil(-1.2) == -1.0);
  check(math_round(0.5) == 1.0 && math_round(-0.5) == -1.0);
  check(math_round(std::nextafter(0.5, 0.0)) == 0.0 && math_round(std::nextafter(-0.5, 0.0)) == 0.0);
  check(std::signbit(math_round(-0.1)) && std::signbit(math_ceil(-0.1)));
  check(math_round(4503599627370496.0) == 4503599627370496.0);
  for (double x : {-0.0, -inf, inf, nan}) {
    if (std::isnan(x)) check(std::isnan(math_floor(x)) && std::isnan(math_ceil(x)) && std::isnan(math_round(x)));
    else check(math_floor(x) == x && math_ceil(x) == x && math_round(x) == x && std::signbit(math_round(x)) == std::signbit(x));
  }
  check(math_pow(nan, 0.0) == 1.0 && math_pow(1.0, nan) == 1.0 && math_pow(-1.0, inf) == 1.0);
  check(std::isnan(math_pow(nan, 2.0)) && std::isnan(math_pow(2.0, nan)) && std::isnan(math_pow(-2.0, 0.5)));
  check(math_pow(-0.0, -3.0) == -inf && std::signbit(math_pow(-0.0, 3.0)));
  check(math_pow(1e308, 2.0) == inf && math_pow(1e-308, 2.0) == 0.0);
  check(std::isnan(math_sqrt(-1.0)) && std::isnan(math_sqrt(nan)) && math_sqrt(inf) == inf && std::signbit(math_sqrt(-0.0)));
  check(math_sqrt(4.0) == 2.0 && math_pow(2.0, 3.0) == 8.0);
  // Canonical imports resolve both overloads; registration stays explicit and optional.
  auto integer = registry.function_by_signature("fn math_pow(int, int) -> int");
  auto floating = registry.function_by_signature("fn math_pow(float, float) -> float");
  dslvm::Args ints{dslvm::Value::integer(2), dslvm::Value::integer(63)};
  dslvm::Args floats{dslvm::Value::real(2.0), dslvm::Value::real(3.0)};
  check(integer && floating && integer->fn(ints).i == lo && floating->fn(floats).f == 8.0);
  check(registry.signatures().size() == 16);
  std::cout << "stdlib math: " << passed << " checks passed\n";
}
