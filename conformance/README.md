
Native invocation envelopes have a separate deterministic expectation stage:
`python3 scripts/gen-invocation-expectations.py` after the compiler's core corpus
generator. It derives envelope bytes from the authored sink/error assertions and
never reads execution output or changes bytecode. `--check` verifies freshness.
The one integer-division diagnostic expands the VM's canonical exact message;
the core VM assertion itself intentionally remains a substring match.
