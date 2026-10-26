#!/usr/bin/env bash
# Run every SDK against the same C++ core and optional string-catalog fixtures.
# Deliberately continue after failures so the summary covers every SDK/stage.
set -uo pipefail

if [[ ${1:-} == --help || ${1:-} == -h ]]; then
  cat <<'USAGE'
Usage: scripts/matrix.sh [SIBLING_ROOT]

SIBLING_ROOT contains lmcode-cpp, lmcode-rust, lmcode-go, lmcode-csharp.
Defaults to this checkout's parent. Override individual paths with LMCODE_CPP,
LMCODE_RUST, LMCODE_GO, LMCODE_CSHARP. Tests/builds run in those checkouts.
MATRIX_LOG_DIR selects the log directory; otherwise one is created under TMPDIR.
MATRIX_CORPUS=large adds a 16-seed x 64-round exact differential run;
MATRIX_CORPUS=small compares the committed reduced corpus traces.
Requires the SDK toolchains and a matching lmscript for compiler parity checks.
Exit 0 means executed stages passed; SKIP means an SDK catalog has not caught up.
Exit 1 means at least one executed stage failed.
USAGE
  exit 0
fi
if (( $# > 1 )); then
  printf 'usage: %s [SIBLING_ROOT]\n' "$0" >&2
  exit 2
fi
script_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd) || exit 2
sibling_root=${1:-$(dirname -- "$script_root")}
# Resolve before entering any SDK directory; paths may contain spaces.
sibling_root=$(cd -- "$sibling_root" && pwd) || exit 2
cpp_dir=${LMCODE_CPP:-$sibling_root/lmcode-cpp}
rust_dir=${LMCODE_RUST:-$sibling_root/lmcode-rust}
go_dir=${LMCODE_GO:-$sibling_root/lmcode-go}
csharp_dir=${LMCODE_CSHARP:-$sibling_root/lmcode-csharp}
# Normalize overrides relative to the invocation directory, including missing paths.
for path_var in cpp_dir rust_dir go_dir csharp_dir; do
  if [[ ${!path_var} != /* ]]; then printf -v "$path_var" '%s/%s' "$PWD" "${!path_var}"; fi
done
if [[ -n ${MATRIX_LOG_DIR:-} ]]; then
  mkdir -p -- "$MATRIX_LOG_DIR" || exit 2
  log_dir=$(cd -- "$MATRIX_LOG_DIR" && pwd) || exit 2
else
  log_dir=$(mktemp -d "${TMPDIR:-/tmp}/lmcode-matrix.XXXXXX") || exit 2
fi
core_vectors=$cpp_dir/conformance/vectors
stdlib_vectors=$cpp_dir/conformance/stdlib-vectors
failed=0
rows=()
skipped=0

std_compatible() {
  local directory=$1
  [[ ! -f "$cpp_dir/std.lock" ]] && return 0 # synthetic/legacy matrix fixtures
  [[ -f "$directory/std.lock" ]] || return 1
  [[ $(sed -n 's/^sha256=//p' "$directory/std.lock" | tr -d '\r') == $(sed -n 's/^sha256=//p' "$cpp_dir/std.lock" | tr -d '\r') ]]
}

stdlib_stage() {
  local sdk=$1 directory=$2
  shift 2
  if std_compatible "$directory"; then
    run_stage "$sdk" stdlib "$directory" "$@"
  else
    stdlib_result=SKIP
    skipped=$((skipped+1))
    printf 'Skipping %s stdlib: canonical std digest differs; SDK port pending\n' "$sdk" >&2
  fi
}

run_stage() {
  local sdk=$1 stage=$2 directory=$3
  shift 3
  local log=$log_dir/$sdk-$stage.log
  printf 'Running %s %s (log: %s)\n' "$sdk" "$stage" "$log" >&2
  if (cd -- "$directory" && "$@") >"$log" 2>&1; then
    printf -v "${stage}_result" '%s' PASS
  else
    printf -v "${stage}_result" '%s' FAIL
    failed=1
  fi
}
cpp_core() { make build/conformance && build/conformance "$core_vectors"; }
cpp_stdlib() { make build/conformance && build/conformance "$stdlib_vectors"; }
csharp_tests() { dotnet build -c Release -warnaserror && dotnet test -c Release --no-build; }

for sdk in cpp rust go csharp; do
  case $sdk in
    cpp)
      run_stage "$sdk" tests "$cpp_dir" make test
      run_stage "$sdk" core "$cpp_dir" cpp_core
      run_stage "$sdk" stdlib "$cpp_dir" cpp_stdlib
      ;;
    rust)
      run_stage "$sdk" tests "$rust_dir" cargo test --workspace
      run_stage "$sdk" core "$rust_dir" cargo run --quiet -p dsl-conformance --bin dsl-conformance -- "$core_vectors"
      stdlib_stage "$sdk" "$rust_dir" cargo run --quiet -p dsl-conformance --bin dsl-conformance -- "$stdlib_vectors"
      ;;
    go)
      run_stage "$sdk" tests "$go_dir" go test -count=1 ./...
      run_stage "$sdk" core "$go_dir" go run ./cmd/conformance "$core_vectors"
      stdlib_stage "$sdk" "$go_dir" go run ./cmd/conformance "$stdlib_vectors"
      ;;
    csharp)
      run_stage "$sdk" tests "$csharp_dir" csharp_tests
      run_stage "$sdk" core "$csharp_dir" dotnet run -c Release --project conformance/LmCode.Conformance -- "$core_vectors"
      stdlib_stage "$sdk" "$csharp_dir" dotnet run -c Release --project conformance/LmCode.Conformance -- "$stdlib_vectors"
      ;;
  esac
  rows+=("$sdk $tests_result $core_result $stdlib_result")
done
case ${MATRIX_CORPUS:-skip} in
  skip) ;;
  small|large)
    corpus_args=()
    if [[ $MATRIX_CORPUS == large ]]; then corpus_args+=(--large); fi
    run_stage corpus differential "$cpp_dir" python3 scripts/diff-stdlib.py --siblings "$sibling_root" --output "$log_dir/corpus" "${corpus_args[@]}"
    printf 'Corpus exact differential: %s\n' "$differential_result"
    ;;
  *) printf 'invalid MATRIX_CORPUS: %s\n' "$MATRIX_CORPUS" >&2; exit 2 ;;
esac
printf '\n%-10s | %-6s | %-6s | %-6s\n' SDK Tests Core Stdlib
printf '%s\n' '-----------+--------+--------+-------'
for row in "${rows[@]}"; do
  read -r sdk tests_result core_result stdlib_result <<< "$row"
  printf '%-10s | %-6s | %-6s | %-6s\n' "$sdk" "$tests_result" "$core_result" "$stdlib_result"
done
if (( skipped )); then printf '\n%d SDK stdlib stage(s) SKIPPED: cross-SDK current-catalog parity remains pending.\n' "$skipped"; fi
printf '\nLogs: %s\n' "$log_dir"
exit "$failed"
