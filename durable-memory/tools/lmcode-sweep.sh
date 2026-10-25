#!/usr/bin/env bash
set -euo pipefail

# Read-only by default. The Node implementation below deliberately receives
# only file paths and produces a deterministic report; it never invokes git
# mutation commands or a repository build.
SELF_DIR="$(cd "$(dirname "$0")" && pwd)"
COMPILER="${LMSCRIPT_LEXER:-/tmp/claude-1000/scratch-lmcode/compiler-lmctl-mono/lmscript/dist/lmscript.js}"
ROOT="${LMCODE_SWEEP_ROOT:-/home/mma/repos/dsl}"
MODE=dry-run
SELFTEST=0
REPO_FILTER=""
REPORT=""
STAGE=""

usage() {
  cat <<'EOF'
usage: lmcode-sweep.sh [--dry-run|--apply] [--root DIR] [--repo NAME]
                        [--stage DIR] [--report-json FILE] [--lexer FILE]
EOF
}
while (($#)); do
  case "$1" in
    --dry-run) MODE=dry-run; shift;;
    --self-test) SELFTEST=1; shift;;
    --apply) MODE=apply; shift;;
    --root) ROOT=$2; shift 2;;
    --repo) REPO_FILTER=$2; shift 2;;
    --stage) STAGE=$2; shift 2;;
    --report-json) REPORT=$2; shift 2;;
    --lexer) COMPILER=$2; shift 2;;
    -h|--help) usage; exit 0;;
    *) echo "unknown option: $1" >&2; usage >&2; exit 2;;
  esac
done
if [[ -n "$STAGE" && "$MODE" == apply ]]; then
  echo "--apply and --stage are mutually exclusive; stage is always dry-run" >&2
  exit 2
fi

export LMCODE_SWEEP_MODE="$MODE" LMCODE_SWEEP_ROOT="$ROOT" LMCODE_SWEEP_REPO="$REPO_FILTER" LMCODE_SWEEP_SELFTEST="$SELFTEST"
export LMCODE_SWEEP_STAGE="$STAGE" LMCODE_SWEEP_COMPILER="$COMPILER" LMCODE_SWEEP_REPORT="$REPORT"
exec node "$SELF_DIR/lmcode-sweep.mjs"
