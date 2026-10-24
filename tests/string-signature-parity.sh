#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HOST="${1:-$ROOT/build/stdlib_string_test}"
CATALOG="${2:-string}"
SCHEMA="$ROOT/tests/$CATALOG.schema.json"
FRAGMENT="$ROOT/stdlib/$CATALOG.schema-fragment.json"
if [[ "$CATALOG" == file ]]; then SCHEMA="$ROOT/tests/file.schema.json"; fi
if [[ "$CATALOG" == std ]]; then SCHEMA="$ROOT/std/lmcode-std.schema.json"; FRAGMENT="$SCHEMA"; fi
WORK="$(mktemp -d "${TMPDIR:-/tmp}/dsl-string-signatures-XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
if [[ -n "${LMSCRIPT_BIN:-}" ]]; then
  COMPILER=("$LMSCRIPT_BIN")
elif command -v lmscript >/dev/null 2>&1 && lmscript --dsl-signatures "$SCHEMA" > /dev/null 2>&1; then
  COMPILER=(lmscript)
elif command -v npx >/dev/null 2>&1; then
  COMPILER=(npx --yes --package=@lmctl-ai/lmctl -- lmscript)
else
  echo "fail: signature parity needs a compiler with --dsl-signatures; install @lmctl-ai/lmctl, provide npx, or set LMSCRIPT_BIN" >&2
  exit 2
fi
node - "$SCHEMA" "$FRAGMENT" <<'JS_CHECK'
const fs = require('fs'), assert = require('assert');
const schema = process.argv[2], fragmentPath = process.argv[3];
const fixture = JSON.parse(fs.readFileSync(schema));
const fragment = JSON.parse(fs.readFileSync(fragmentPath));
assert.deepStrictEqual(fixture.functions, fragment.functions, 'test schema differs from shipped catalog fragment');
assert.deepStrictEqual(fixture.types || {}, fragment.types || {}, 'test types differ from shipped catalog fragment');
JS_CHECK
"${COMPILER[@]}" --dsl-signatures "$SCHEMA" > "$WORK/compiler-signatures"
"$HOST" --signatures > "$WORK/host-signatures"
# Native Windows C++ stdout uses CRLF; Node emits LF. Compare logical lines,
# preserving every signature byte (including lone CRs) and the final newline.
node - "$WORK/compiler-signatures" "$WORK/host-signatures" <<'JS_EOL'
const fs = require('fs');
for (const path of process.argv.slice(2)) {
  const raw = fs.readFileSync(path, 'utf8');
  fs.writeFileSync(path + '.lf', raw.replace(/\r\n/g, '\n'));
}
JS_EOL
diff -u "$WORK/compiler-signatures.lf" "$WORK/host-signatures.lf"
echo "$CATALOG catalog compiler/registry parity passed"
