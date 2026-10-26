# durable-memory tools

| Path | What it is |
| --- | --- |
| lmcode-sweep.sh, lmcode-sweep.mjs | the lexer-aware _dsl_ to _lmcode_ sweep (07-compiler-readiness-sweep.md) |
| rename-audit/scan-one.sh, rename-audit/one-repo.mjs | the 2026-10-25 rename inventory driver: shallow-clones one lmctl-ai repo into $SCR, runs the sweep in dry-run mode and appends one JSON line per repo (blocks, cpp-dsl-sdk references, std-schema duplicates, workflow lmctl pins) to $SCR/results.ndjson. Put lmcode-sweep.mjs, one-repo.mjs, std.schema.json and a built lmscript lexer (lexer-dist) in $SCR first; LMCTL_SITE points at a local lmctl site checkout for the site repo. |
| site-migrate/html-migrate.cjs | rewrites paired _dsl_ blocks to _lmcode_ inside pre/code on a site checkout (tools/, skills/, autorun/): node html-migrate.cjs SITE_ROOT, with LMCODE_LEXER pointing at lexer-dist/lmscript.js |
| site-migrate/live-links-check.lms | an _lmcode_ cli=lmhtml script that lists base hrefs, anchors and every link/src attribute of the given HTML files as JSON, used to check that the migrated pages still link correctly |

The rename-audit and site-migrate scripts were recovered from /tmp on t4g2 on 2026-10-26 (used for the lmcode rename inventory and the site migration of 2026-10-25); only their hard-coded scratch and site paths were turned into SCR, LMCTL_SITE, LMCODE_LEXER and SITE_ROOT.
