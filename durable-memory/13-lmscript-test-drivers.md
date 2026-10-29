# lmscript test drivers

The canonical sample suite now includes `run-suite.lms`, `runner-lib.lms`,
`audit.lms`, `check-recompile.lms`, and `test-runner.lms`. Root `test.lms`
drives SDK checks; the C and C++ sample roots have their own `test.lms`
for build, lock validation, canonical tests and the annotated-rule example.

Install both executables with `npm install -g @lmctl-ai/lmctl`. Run from the
repository root with `lmctl test.lms` or `lmscript test.lms`; CI uses
`lmscript --no-preflight test.lms`. `run-suite.sh` is an lmscript launcher
and supplies an absolute `LMCODE_SUITE` so argv host commands stay intact.

Assertions and test selection are in lmscript. `bytes.mjs` is a small Node
adapter for byte-preserving reads/hashes, bytecode import decoding/mutation,
and process capture. Each process receives fresh `SAMPLE_TMP` fixtures and
cleanup in a finally block. No test expectations are inferred or rewritten.
The runner checks manifest hashes before host execution, exact stdout bytes,
empty stderr, exit codes, CLI protocol and compile rejection cases. Nine
negative controls test real lmscript execution, including paths with spaces.

Source compilation requires a current compiler capable of reproducing the
entire pinned corpus; no partial legacy-compiler pass is reported. Existing
Python corpus authoring, native conformance tooling, and the separate
cross-port matrix are retained. Sample release CI is driven by lmscript;
Python remains needed for lock synchronization and schema embedding.
