#!/usr/bin/env bash
# Regression test: the block-spelling sweep must treat .lms, .lm and .dsl
# files as scripts (previously only .lms was), and must report — not silently
# skip — files whose extension is not covered even when they mention markers.
set -euo pipefail

TOOL="${LMCODE_SWEEP:-$HOME/tools/lmcode-sweep.sh}"
LEXER="${LMSCRIPT_LEXER:-/tmp/claude-1000/scratch-lmcode/compiler-lmctl-mono/lmscript/dist/lmscript.js}"

if [[ ! -x "$TOOL" ]]; then echo "SKIP: sweep tool not found at $TOOL"; exit 0; fi
if [[ ! -f "$LEXER" ]]; then echo "SKIP: lexer not found at $LEXER"; exit 0; fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
FIXTURE="$TMP/root/sweep-fixture"

git init -q "$FIXTURE"
git -C "$FIXTURE" remote add origin https://github.com/lmctl-ai/sweep-fixture.git

for ext in lms lm dsl; do
  printf 'p=_dsl_;\n1+1\nend _dsl_\n' > "$FIXTURE/block.$ext"
done
# A marker-mentioning file under an uncovered extension: must be counted as a
# skipped-extension marker mention, not silently dropped.
printf 'p=_dsl_;\nend _dsl_\n' > "$FIXTURE/odd.xyz"
printf 'already current\n' > "$FIXTURE/notes.md"

"$TOOL" --dry-run --root "$TMP/root" --lexer "$LEXER" > "$TMP/report.json" 2> "$TMP/stderr.log"

node - "$TMP/report.json" <<'EOF'
const fs = require('fs');
const report = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
const repo = report.repositories.find(r => r.name === 'sweep-fixture');
if (!repo) throw new Error('sweep-fixture repo missing from report');
const byPath = Object.fromEntries(repo.files.map(f => [f.path, f]));
for (const ext of ['lms', 'lm', 'dsl']) {
  const f = byPath[`block.${ext}`];
  if (!f) throw new Error(`block.${ext} was not inventoried as a script`);
  if (f.kind !== 'script') throw new Error(`block.${ext} kind is ${f.kind}, expected script`);
  if (f.blocks !== 1 || f.replacements.length !== 2)
    throw new Error(`block.${ext}: expected 1 block / 2 replacements, got ${f.blocks} / ${f.replacements.length}`);
}
if (!('.xyz' in repo.skipped_extensions)) throw new Error('.xyz not counted in skipped_extensions');
if (repo.skipped_marker_mentions !== 1)
  throw new Error(`expected 1 skipped marker mention, got ${repo.skipped_marker_mentions}`);
if (report.totals.blocks !== 3) throw new Error(`expected 3 blocks total, got ${report.totals.blocks}`);
console.log('sweep extension coverage: OK (.lms/.lm/.dsl inventoried as scripts; skipped extensions counted)');
EOF

grep -q 'odd.xyz' "$TMP/stderr.log" || { echo "FAIL: no loud warning for skipped marker-mentioning file"; exit 1; }

echo "PASS: lmcode-sweep extension coverage regression"
