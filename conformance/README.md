Canonical core bytecode, sink assertions and invocation envelopes come from mono's
`lmscript/scripts/generate-dsl-conformance.ts`. Regeneration requires no native rewrite.
`python3 scripts/gen-invocation-expectations.py --check` independently recomputes
frame expectations from authored sink/error assertions and checks freshness without
changing compiler provenance or bytecode. The integer-division diagnostic expands
the VM's canonical exact message; the VM assertion remains a substring match.
