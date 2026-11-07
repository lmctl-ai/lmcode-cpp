# REST server over stdio (`rest` std module)

Opt-in module of [spec/std-rest.md](https://github.com/lmctl-ai/lmcode/blob/main/spec/std-rest.md), in `stdlib/rest.hpp`.
There is **no network listener**: the host process reads HTTP/1.1 requests from an input stream (default `std::cin`) and
writes responses to an output stream (default `std::cout`). `Registry` and `register_std` do not know about it: a default
registry still has 206 signatures; `register_rest` adds exactly 18 (`stdlib/rest.schema-fragment.json`, merged into the host schema).

```cpp
#include "dslvm.hpp"
#include "stdlib/rest.hpp"            // not included by std.hpp

dslvm::Registry registry;
registry.set_std_output(&std::cerr);  // stdout belongs to the protocol while serving
dslvm::stdlib::rest::Options options;  // input / output streams, max_body_bytes (1 MiB), on_handler_error(entry, message)
auto rest = dslvm::stdlib::rest::register_rest(registry, options);
auto program = dslvm::Program::load(bytecode, hostSchemaBytes, registry);   // host schema = lmcode-rest.schema.json (+ yours)
rest->run(program, context);          // runs the body (rest_route(...); rest_serve();), then serves until EOF, rest_stop() or a framing error
```

* `rest->dispatch(Request{method, target, headers, body})` is the stream-independent core used by the unit tests.
* Handlers run strictly one at a time, never nested in the body's VM run. Everything is single threaded.
* On Windows set stdin/stdout to binary mode first (`_setmode(..., _O_BINARY)`); `examples/rest_serve.cpp` does.
* Try it: `make build/rest_serve && printf 'GET /health HTTP/1.1\r\nHost: x\r\n\r\n' | build/rest_serve conformance/rest-suite/app.dslb conformance/rest-suite/host.schema.json`
* Tests: `make test-rest` (unit tests, the shared suite through in-memory streams and through the `rest_serve` child process, schema parity, pin check).
* The shared suite is copied and pinned by `python3 scripts/sync-rest.py` (`--verify` in CI; lock in `rest-suite.lock`).
