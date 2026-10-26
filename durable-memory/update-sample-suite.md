# Sample-suite update rule

The canonical Shop/Ledger sample suite lives in `sample-suite/` of lmcode-cpp.
Any change to the language, VM, standard catalog, schema binding, bytecode
format, host protocol, or sample contract requires updating the canonical
sample suite first, then regenerating its schema/bytecode/expected fixtures.
Run `sample-suite/run-all-samples.sh --all` with every configured SDK; missing
ports, failed builds, protocol failures, and case failures are errors, never
skips. Keep companion SDK checkouts at the sibling paths expected by the runner
or supply the reviewed environment file/path overrides.
