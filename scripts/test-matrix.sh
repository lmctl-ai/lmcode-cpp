#!/usr/bin/env bash
# Isolate orchestration tests from SDK/toolchain availability.
set -euo pipefail
matrix=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/matrix.sh
scratch=$(mktemp -d "${TMPDIR:-/tmp}/matrix-test.XXXXXX")
trap 'rm -rf -- "$scratch"' EXIT
root="$scratch/sibling checkouts"
mkdir -p "$scratch/bin" "$root"/lmcode-{cpp,rust,go,csharp}
mkdir -p "$root/lmcode-cpp/build" "$root/lmcode-cpp/conformance/"{vectors,stdlib-vectors}
cat > "$scratch/bin/tool" <<'TOOL'
#!/usr/bin/env bash
printf '%s %s\n' "$(basename -- "$0")" "$*" >> "$MATRIX_TEST_CALLS"
if [[ ${MATRIX_TEST_FAIL:-} == yes && $(basename -- "$0") == cargo && $1 == test ]]; then exit 42; fi
exit 0
TOOL
chmod +x "$scratch/bin/tool"
for tool in make cargo go dotnet; do ln -s tool "$scratch/bin/$tool"; done
cat > "$root/lmcode-cpp/build/conformance" <<'RUNNER'
#!/usr/bin/env bash
[[ $# == 1 && -d $1 ]]
RUNNER
chmod +x "$root/lmcode-cpp/build/conformance"
export PATH="$scratch/bin:$PATH" MATRIX_TEST_CALLS="$scratch/calls"
unset LMCODE_CPP LMCODE_RUST LMCODE_GO LMCODE_CSHARP
export MATRIX_LOG_DIR="$scratch/logs"
"$matrix" "$root" > "$scratch/pass" 2> "$scratch/progress"
[[ $(grep -c 'PASS.*PASS.*PASS' "$scratch/pass") == 4 ]]
[[ $(wc -l < "$scratch/calls") == 13 ]]
: > "$scratch/calls"
export MATRIX_TEST_FAIL=yes
if "$matrix" "$root" > "$scratch/fail" 2> "$scratch/progress"; then
  echo 'matrix accepted a failing Rust test command' >&2; exit 1
else
  [[ $? == 1 ]]
fi
grep -Eq '^rust +\| FAIL +\| PASS +\| PASS' "$scratch/fail"
[[ $(grep -c 'PASS.*PASS.*PASS' "$scratch/fail") == 3 ]]
[[ $(wc -l < "$scratch/calls") == 13 ]] # Did not stop at the failed test.
unset MATRIX_TEST_FAIL
export LMCODE_GO="$scratch/missing Go checkout"
if "$matrix" "$root" > "$scratch/missing" 2> "$scratch/progress"; then
  echo 'matrix accepted a missing checkout' >&2; exit 1
else
  [[ $? == 1 ]]
fi
grep -Eq '^go +\| FAIL +\| FAIL +\| FAIL' "$scratch/missing"
[[ $(grep -c 'PASS.*PASS.*PASS' "$scratch/missing") == 3 ]]
printf 'PASS 3 matrix orchestration cases (success, failing command, missing checkout; paths with spaces)\n'
