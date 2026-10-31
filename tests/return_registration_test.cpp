#include "../include/dslvm.hpp"
#include <iostream>
#include <vector>
int main() {
  dslvm::Registry registry;
  std::vector<std::string> expected;
  for (const auto& type : {"int", "float", "bool", "string", "Json", "StrList", "IntList", "FloatList", "StrMap", "IntMap", "FloatMap"}) {
    expected.push_back(std::string("fn lmcode_return_set(") + type + ") -> void");
    expected.push_back(std::string("fn lmcode_return_put(string, ") + type + ") -> void");
  }
  expected.push_back("fn lmcode_return_null() -> void");
  expected.push_back("fn lmcode_return_put_null(string) -> void");
  int failed = 0;
  for (const auto& signature : expected) if (!registry.function_by_signature(signature)) {
    std::cerr << "FAIL missing registration: " << signature << '\n'; ++failed;
  }
  if (failed) return 1;
  std::cout << "PASS 24 exact return registrations\n";
}
