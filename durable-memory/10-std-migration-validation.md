# Standard migration validation

`tests/lmstd-migrate-selftest.lms` is a CI-independent fixture harness written in
lmscript. Run it from the lmcode-cpp root, or pass `sourceRoot` in its JSON
configuration. Its fixture origins are local bare clones under scratch; no
GitHub repository, remote or publication is created. Empty private hook
folders prevent user Git hooks from participating in fixture runs.

The fixture set includes plain compiler pins, a CMake merge, a schema generator,
a pure compilation source with checked-in bytecode, an unknown build pattern,
a dirty tree and a separately clean but behind tree. Snapshots compare raw blob
hashes, complete porcelain status, local refs and bare-origin refs. Read-only
checks must preserve all four. A configured migration command is an argv
template, so shell evaluation is not involved.

The initial scaffold configuration is `{"program":null,"target":"0.2.420"}`.
It reports seven fixture controls and **zero migration checks**. This setup mode
is not evidence that a migrator works. A new, unused scratch directory is
required on each run:

```sh
lmscript --check tests/lmstd-migrate-selftest.lms
lmscript --no-preflight tests/lmstd-migrate-selftest.lms CONFIG.json SCRATCH_DIR
```

For historical validation, each pre-migration commit and expected post-migration
commit lives in a separate scratch worktree backed by private local origins.
The SSL and SSH cases precede the remaining hosts. The raw comparison records
all tracked-tree differences, exact schema bytes, derived schema presence and
checked-in bytecode. Missing generated artifacts stay unverified. `--check`
validates source but does not emit bytecode; arbitrary self-running examples
need an identified compilation recipe before they can be reported as rebuilt.

The comparison oracle itself is checked with unmigrated baselines (must differ),
expected trees compared with themselves (must match), and a valid-JSON
whitespace mutation (must change schema-byte identity).
