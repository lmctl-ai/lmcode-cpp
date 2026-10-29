# Purpose and place

`lmcode-cpp` is the C++ runtime for lmscript `_lmcode_` bytecode. A host embeds `include/dslvm.hpp`, binds the functions and object
methods in its JSON schema through `dslvm::Registry`, loads bytecode against the exact schema bytes, and runs it with host-owned
objects in a `dslvm::Context`.

The runtime hashes schema bytes to bind them to the bytecode; it does not parse schema JSON. The registry provides the concrete
host implementation and is checked against the bytecode imports before execution. The header is vendored by hosts with a pinned
commit and SHA-256 so each host can build without fetching this repository.

The language-neutral files in `conformance/vectors` let independent runtime implementations check that they agree on execution
and errors. They are the cross-runtime test contract; the C++ header is not required by another implementation.
