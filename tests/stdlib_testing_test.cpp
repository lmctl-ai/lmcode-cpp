#include "../stdlib/testing.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

using namespace dslvm::stdlib;

int main(int argc, char** argv) {
  std::ostringstream output;
  dslvm::Registry registry;
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
  for (const auto& args : {
         dslvm::Args{dslvm::Value::real(nan), dslvm::Value::real(1), dslvm::Value::real(1)},
         dslvm::Args{dslvm::Value::real(1), dslvm::Value::real(nan), dslvm::Value::real(1)},
         dslvm::Args{dslvm::Value::real(1), dslvm::Value::real(1), dslvm::Value::real(nan)},
         dslvm::Args{dslvm::Value::real(1), dslvm::Value::real(1), dslvm::Value::real(-1)},
         dslvm::Args{dslvm::Value::real(inf), dslvm::Value::real(inf), dslvm::Value::real(0)}}) {
    try { close->fn(const_cast<dslvm::Args&>(args)); check(false); }
    catch (const dslvm::Error&) { ++passed; }
  }
  std::cout << "stdlib testing: " << passed << " checks passed\n";
}
