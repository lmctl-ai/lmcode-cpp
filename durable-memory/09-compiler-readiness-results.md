# Compiler readiness snapshot (2026-10-24)

This is a frozen input snapshot: other workers continued adding examples during
the survey. No delimiter edits were applied to any live source repository.
The tool and compiler reproduction steps are in
[07-compiler-readiness-sweep.md](07-compiler-readiness-sweep.md).

The dry run inventories 33 repositories and 1,975 text files. It would change
1,015 of 1,018 physical LMScript files and 35 documentation files: 1,050 files,
1,451 blocks and 2,902 delimiter tokens. All 210 remaining marker mentions are
left unchanged. Full file paths and ambiguous line/column/text records are in
[09-compiler-readiness-results.json](09-compiler-readiness-results.json).

The built origin/main compiler checked **1,054/1,054** inputs successfully:
1,018 physical scripts and 36 rewritten documentation snippets. No failures,
timeouts or introduced regressions remain. Inverting only the recorded marker
spans reconstructed the original SHA-256 of all 1,975 staged text files,
verifying that schema names, flags, strings, comments and other text did not
change. The exact lexer transformation self-test passes 19 cases.

The Blender CAD composition is intended to run through `examples/compose/run.sh`,
which supplies the CAD schema. Its raw standalone `--check` initially reports a
missing relative schema. A byte-identical copy of the actual CAD schema was
placed at that expected path in scratch only; the source schema option and
filename were not edited, and the example then passed.

CLI-based blocks used distinct regular schema-only host stubs backed by current
local schema JSON (or read-only metadata queries). Distinct executable paths
matter because the compiler caches queried schemas by real binary identity.
The scripts themselves were never executed; installation and downloads were
not needed. Tool/cache/temp outputs stayed in the scratch directory.

| Repository | Files that would change | LMScript files | Docs | Checks passed | Ambiguous mentions |
| --- | ---: | ---: | ---: | ---: | ---: |
| dot-github | 0 | 0 | 0 | 0 | 2 |
| lmaudio | 29 | 28 | 1 | 29 | 2 |
| lmbio | 30 | 29 | 1 | 30 | 4 |
| lmblender | 157 | 144 | 13 | 158 | 30 |
| lmcad | 39 | 38 | 1 | 39 | 13 |
| lmcell | 24 | 23 | 1 | 24 | 3 |
| lmcloud | 17 | 16 | 1 | 17 | 22 |
| lmcode-cpp | 0 | 0 | 0 | 0 | 6 |
| lmcode-csharp | 0 | 0 | 0 | 0 | 1 |
| lmcode-go | 2 | 2 | 0 | 2 | 4 |
| lmcode-java | 7 | 7 | 0 | 7 | 0 |
| lmcode-rust | 1 | 1 | 0 | 1 | 4 |
| lmdb | 14 | 14 | 0 | 14 | 1 |
| lmdraw | 61 | 59 | 2 | 62 | 6 |
| lmfilament | 39 | 38 | 1 | 39 | 7 |
| lmfinance | 43 | 42 | 1 | 43 | 6 |
| lmfluid | 25 | 24 | 1 | 25 | 8 |
| lmgallery | 42 | 42 | 0 | 42 | 6 |
| lmhtml | 13 | 13 | 0 | 13 | 7 |
| lmimg | 35 | 34 | 1 | 35 | 2 |
| lmmagnum | 37 | 37 | 0 | 37 | 8 |
| lmmolecule | 34 | 33 | 1 | 34 | 5 |
| lmnum | 32 | 31 | 1 | 32 | 9 |
| lmoffice | 75 | 75 | 0 | 75 | 4 |
| lmparticle | 37 | 36 | 1 | 37 | 3 |
| lmphysics | 41 | 40 | 1 | 43 | 10 |
| lmplot | 55 | 53 | 2 | 55 | 15 |
| lmshark | 26 | 25 | 1 | 26 | 1 |
| lmspace | 33 | 32 | 1 | 33 | 1 |
| lmssh | 10 | 9 | 1 | 10 | 5 |
| lmssl | 12 | 11 | 1 | 12 | 6 |
| lmvideo | 42 | 42 | 0 | 42 | 7 |
| lmweather | 38 | 37 | 1 | 38 | 2 |

The duplicate host API inventory and reasons for keeping domain types in hosts
are in [08-host-adoption-inventory.md](08-host-adoption-inventory.md) and the
open-items section of [06-stdlib-catalog.md](06-stdlib-catalog.md).
