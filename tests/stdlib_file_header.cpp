#include "../stdlib/file.hpp"

int main() {
  dslvm::Registry registry(dslvm::StdMode::Disabled);
  dslvm::stdlib::Collections arena;
  dslvm::stdlib::register_file(registry, arena);
  return registry.signatures().size() == 5 ? 0 : 1;
}
