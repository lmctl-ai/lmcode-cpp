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
elif command -v lmscript >/dev/null 2>&1 && lmscript --lmcode-signatures "$SCHEMA" > /dev/null 2>&1; then
  COMPILER=(lmscript)
elif command -v npx >/dev/null 2>&1; then
  COMPILER=(npx --yes --package=@lmctl-ai/lmctl -- lmscript)
else
  echo "fail: signature parity needs a compiler with --lmcode-signatures; install @lmctl-ai/lmctl, provide npx, or set LMSCRIPT_BIN" >&2
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
"${COMPILER[@]}" --lmcode-signatures "$SCHEMA" > "$WORK/compiler-signatures-all"
"$HOST" --signatures > "$WORK/host-signatures"
# Since lmscript 0.2.420 std is built into the compiler, so --lmcode-signatures
# merges the built-in std catalog with the schema instead of listing only the
# schema's own functions. Restrict the compiler listing to the names this
# catalog declares so the diff stays catalog-vs-catalog (a no-op for older
# compilers that already list the schema only).
node - "$WORK/compiler-signatures-all" "$WORK/compiler-signatures" "$FRAGMENT" <<'JS_FILTER'
const fs = require('fs');
const [srcPath, dstPath, fragmentPath] = process.argv.slice(2);
const fragment = JSON.parse(fs.readFileSync(fragmentPath));
const fns = new Set(Object.keys(fragment.functions || {}));
const kept = fs.readFileSync(srcPath, 'utf8').split('\n').filter((line) => {
  if (!line) return false;
  let m = line.match(/^fn ([A-Za-z_][A-Za-z0-9_]*)\(/);
  if (m) return fns.has(m[1]);
  m = line.match(/^method ([A-Za-z_][A-Za-z0-9_]*)\.([A-Za-z_][A-Za-z0-9_]*)\(/);
  if (m) return Boolean(fragment.types && fragment.types[m[1]] && fragment.types[m[1]].methods && fragment.types[m[1]].methods[m[2]]);
  return true;
});
fs.writeFileSync(dstPath, kept.join('\n') + '\n');
JS_FILTER
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
