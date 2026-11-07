// Serve a compiled program's REST routes over stdin/stdout (the opt-in `rest` std module; no network).
//   rest_serve PROGRAM.dslb HOST.schema.json [--max-body-bytes N]
// stdout carries only HTTP responses; `print` and handler failures go to stderr. Exit 0 after serving, 2 on usage, 3 on error.
#include "../include/dslvm.hpp"
#include "../stdlib/rest.hpp"
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
  if (argc < 3 || (argc != 3 && !(argc == 5 && std::string(argv[3]) == "--max-body-bytes"))) {
    std::cerr << "usage: rest_serve PROGRAM.dslb HOST.schema.json [--max-body-bytes N]\n";
    return 2;
  }
#ifdef _WIN32
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  try {
    dslvm::stdlib::rest::Options options;
    if (argc == 5) options.max_body_bytes = std::stoull(argv[4]);
    dslvm::Registry registry;
    registry.set_std_output(&std::cerr);   // stdout belongs to the protocol
    auto rest = dslvm::stdlib::rest::register_rest(registry, options);
    auto program = dslvm::Program::load(dslvm::read_file(argv[1]), dslvm::read_file(argv[2]), registry);
    rest->run(program);
  } catch (const std::exception& e) {
    std::cerr << "rest_serve: " << e.what() << "\n";
    return 3;
  }
  return 0;
}
