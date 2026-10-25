# Re-runnable standard migration

`scripts/lmstd-migrate.lms` is the source copy of `~/tools/lmstd-migrate.lms`.
The transformation logic is lmscript, using its file/regex/process helpers and
`native:git`. Run with released lmscript >= 0.2.420:

```sh
lmscript --no-preflight ~/tools/lmstd-migrate.lms --repo /path/to/host \
  --target 0.2.420 --dry-run --report /tmp/claude-1000/scratch-lmcode/host.json
```

The script defaults to its own transactional dry run. `--no-preflight` disables
lmscript's separate effect simulation; it does not authorize an apply.
`--apply` explicitly copies the staged changes, commits with the fixed noreply
identity and model trailer, rebases before writing, and pushes existing main.
It never creates repositories/remotes or changes visibility. Exclusions are
checked against origin identity, so renamed worktrees cannot bypass them.
Dirty, behind/ahead/diverged, non-main and remote-less repositories are refused.
Dry runs do not fetch or change refs, the index, or source files.

Each preview archives HEAD into a fresh scratch directory, rewrites literal
`@lmctl-ai/lmctl@0.2.NNN` pins, and prints JSON containing an exact per-file Git
diff, changed paths, detected patterns, compiled examples and unknown patterns.
`--sdk DIR`, `--compiler PATH`, and `--tests auto|always|never` select tools.
Reports and scratch trees stay under `/tmp/claude-1000/scratch-lmcode/`.
Report parents must already exist; report paths into the source tree, symlinks,
or parents resolving outside scratch are refused before any apply effects.

## Supported C++ patterns

* `set(PREFIX_HOST_SCHEMA .../schema/NAME.host.json)` plus the standard
  `PREFIX_STD_SCHEMA`/`PREFIX_STD_MERGER` execute_process block becomes a
  `configure_file(... COPYONLY)` into the existing ignored `.dsl.json`.
* Host declarations are derived with the SDK's `merge-std.py --host-only`.
  Canonical signatures are independently compared with compiler
  `--dsl-signatures ... --without-std`. Existing host-only byte formatting is
  retained; raw schema bytes are never replaced merely to pretty-print them.
* Registry-dump `gen-schema.py` scripts with the known final `print(dump(schema))`
  become host-only generators. Existing `bridge.remove_standard` generators
  are retained. Other generators are reported.
* Known v5 selectors gain the implicit std probe, stable minimum version, both
  block spellings, raw heredoc probe and exact raw-schema bytecode hash checks.
* Known three-line signature normalization checks become full/host-only parity
  and byte-identity checks. Existing migrated checks are retained.
* Existing `sync-sdk.sh` is invoked against the local SDK checkout. A stale
  runtime that cannot vendor the default catalogs is reported rather than
  concealed by a compiler pin. Native default-registry parity checks all 72
  standard signatures before an apply.

Already migrated repositories with current pins are no-ops. Pin-only repos
need no host pattern. Unknown schema/build/generator patterns block the entire
apply; proposed scratch diffs remain reviewable. Optional testing and host
adapters remain host-owned. No host C++ source adapter is synthesized.

## Compilation and tests

Only recognized single-block compile wrappers are executed: comments, an
optional ARGV output-path assignment, a schema-backed block, and savedsl.
Before execution, savedsl is redirected to a controlled artifact in the staged
copy and its exact source text is restored afterward. Output argv alone cannot
contain literal destinations. `tests/lmstd-migrate-output.test.lms` covers
absolute destinations outside staging and inside the source checkout.
Checked-in bytecode is rebuilt only when exactly one tracked compile wrapper
identifies its name/output; ambiguous recipes are reported. All other examples
are reported, never guessed or executed. The installed lmscript has no
general compile-only bytecode-output command; `--check` alone does not emit
bytecode. This is the exact missing capability for arbitrary effectful wrappers.

`test.sh` runs automatically for cheap repositories without CMake/third-party
builds; expensive native builds are explicitly reported as not run. `--tests
always` requests the full suite in the archive (its dependencies must be
available). Compiler and SDK probes still run when native host tests are skipped.
The script does not call `health.py --fix-pins` inside a preview because that
helper independently commits and pushes. It reuses the same host SDK sync entry
point; `lmgallery/scripts/health.py --json --repo NAME` supplies the existing
read-only ecosystem audit. The migrator invokes `~/tools/lmcode-sweep.sh/.mjs`
in read-only staging mode and copies only its exact planned replacements into
the migration archive. Ambiguous mentions are included in the JSON report.
`--sweep FILE` and `--lexer FILE` override the helper and Lexer module. The
published CLI bundle does not export Lexer; the default selects a compatible read-only
source/built module from existing mono worktrees using the sweep self-test (or
`LMSCRIPT_LEXER`). No mono worktree is edited or built. A missing helper/module is an explicit
capability refusal, never a fallback regex delimiter rewrite.

## Historical validation

`--historical` permits detached pre-migration worktrees only under scratch and
never permits apply/push. SSL baseline is `eb2cab77` (hand migration `e4a7f46`);
SSH baseline is `16423398` (hand migration `2af22cd`). Both have unchanged exact
host-only schema bytes. SSL compiles five examples; SSH compiles nine, with
72-signature native registry probes. SDK lock commits may be newer than the
hand migration while catalog behavior/schema bytes remain equivalent. Full
native test.sh suites are expensive and are not claimed by these preview checks.
See [fixture controls](10-std-migration-validation.md) for independent negative
and apply/idempotence controls; setup-only scaffold success is not migration
coverage.

## Local-only apply fixtures

`--local-fixture` accepts an already-existing scratch repository whose existing
origin resolves to a local path under scratch. It enforces file-only Git
transports and never performs a GitHub CI query. The program still creates no
repository or remote. This mode exercises commit/push and idempotence without
publishing; it cannot target any real host or GitHub origin. The published-repo
delimiter discovery helper is intentionally not run on local fixture origins.

The configured fixture harness passes eight no-write/refusal controls and four
apply/idempotence checks (pin-only and checked-in-bytecode fixtures). Its JSON
reports actual migration checks separately from scaffold setup. An unknown
schema pipeline containing std is a refusal, even if its compiler pin is current.
The five already-migrated SSL/SSH/HTML/Shark/Space checkouts are no-ops; Magnum's
single-header SDK sync and direct `.dsl.json` embedding are reported as
unrecognized, so no real host apply is attempted for that runtime.
