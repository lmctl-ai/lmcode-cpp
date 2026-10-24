#include "../stdlib/testing.hpp"
#include <cassert>
#include <cmath>
#include <clocale>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

using namespace dslvm::stdlib;

int main(int argc, char** argv) {
  std::ostringstream output;
  dslvm::Registry registry(dslvm::StdMode::Disabled);
  register_testing(registry, &output);
  if (argc == 2 && std::string(argv[1]) == "--signatures") {
    for (const auto& signature : registry.signatures()) std::cout << signature << '\n';
    return 0;
  }

  int passed = 0;
  auto check = [&](bool ok) {
    if (!ok) { std::cerr << "testing check " << passed + 1 << " failed\n"; std::abort(); }
    ++passed;
  };

  auto print = registry.function_by_signature("fn print(string) -> void");
  auto close = registry.function_by_signature("fn assert_close(float, float, float) -> void");
  check(print && close && registry.signatures().size() == 2);
  dslvm::Args bytes{dslvm::Value::string(std::string("a\0b", 3))};
  check(print->fn(bytes).kind == dslvm::Value::VOID && output.str() == std::string("a\0b\n", 4));

  dslvm::Args equal{dslvm::Value::real(1.0), dslvm::Value::real(1.0001), dslvm::Value::real(0.001)};
  check(close->fn(equal).kind == dslvm::Value::VOID);
  dslvm::Args rounded{dslvm::Value::real(1.2345678901234567), dslvm::Value::real(2.0), dslvm::Value::real(0.25)};
  try { close->fn(rounded); check(false); }
  catch (const dslvm::Error& e) { check(e.message == "assert_close failed: 1.23456789012346 vs 2 (tol 0.25)"); }

  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  try { testing_assert_close(std::copysign(nan, -1.0), 1.0, 0.1); check(false); }
  catch (const dslvm::Error& e) { check(e.code == "RUN_HOST" && e.message == "assert_close failed: nan vs 1 (tol 0.1)"); }
  for (const auto& args : {
         dslvm::Args{dslvm::Value::real(nan), dslvm::Value::real(1), dslvm::Value::real(1)},
         dslvm::Args{dslvm::Value::real(1), dslvm::Value::real(nan), dslvm::Value::real(1)},
         dslvm::Args{dslvm::Value::real(1), dslvm::Value::real(1), dslvm::Value::real(nan)},
         dslvm::Args{dslvm::Value::real(1), dslvm::Value::real(1), dslvm::Value::real(-1)},
         dslvm::Args{dslvm::Value::real(inf), dslvm::Value::real(inf), dslvm::Value::real(0)}}) {
    try { close->fn(const_cast<dslvm::Args&>(args)); check(false); }
    catch (const dslvm::Error&) { ++passed; }
  }
  // Native host locales must not alter the shared diagnostic contract.
  const std::string original_numeric = std::setlocale(LC_NUMERIC, nullptr);
  int locales_checked = 0;
  for (const char* locale : {"C", "C.utf8", "de_DE.UTF-8", "fr_FR.UTF-8", "German_Germany.1252"}) {
    if (!std::setlocale(LC_NUMERIC, locale)) continue;
    const std::string active = std::setlocale(LC_NUMERIC, nullptr);
    try { testing_assert_close(1.5, 2.5, 0.25); check(false); }
    catch (const dslvm::Error& e) {
      check(e.code == "RUN_HOST" && e.message == "assert_close failed: 1.5 vs 2.5 (tol 0.25)");
    }
    check(active == std::setlocale(LC_NUMERIC, nullptr));
    ++locales_checked;
  }
  std::setlocale(LC_NUMERIC, original_numeric.c_str());
  std::cout << "assertion formatting checked in " << locales_checked << " locales\n";
  testing_assert_close(inf, -inf, inf); check(true);
  std::cout << "stdlib testing: " << passed << " checks passed\n";
}
