#!/bin/bash
set -euo pipefail
SCR=${SCR:-/tmp/lmcode-rename-audit}   # scratch: needs lmcode-sweep.mjs (../lmcode-sweep.mjs), one-repo.mjs, std.schema.json and a built lmscript lexer under $SCR/lexer-dist
repo="$1"
url="https://github.com/lmctl-ai/${repo}"
git clone --depth 1 "$url" "$SCR/$repo" >/dev/null
if [ "$repo" = "lmctl" ]; then
  cp -a "${LMCTL_SITE:-$HOME/lmctl}"/tools "${LMCTL_SITE:-$HOME/lmctl}"/{skills,lmscript-site,autorun,helloexample} "$SCR/lmctl/"
fi
LMCODE_SWEEP_ROOT="$SCR" LMCODE_SWEEP_REPO="$repo" LMCODE_SWEEP_MODE=dry-run LMCODE_SWEEP_SCRATCH="$SCR" LMCODE_SWEEP_COMPILER="$SCR/lexer-dist/lmscript.js" LMCODE_SWEEP_REPORT="$SCR/sweep.json" node "$SCR/lmcode-sweep.mjs" >"$SCR/sweep.stdout" 2>"$SCR/sweep.stderr" || status=$?
node "$SCR/one-repo.mjs" "$repo" "$SCR/$repo" "$SCR/std.schema.json" "$SCR/sweep.json" >> "$SCR/results.ndjson"
if [ "${status:-0}" != 0 ]; then echo "SWEEP_STATUS $repo ${status}" >> "$SCR/sweep-errors.txt"; fi
rm -r "$SCR/$repo"
rm -f "$SCR/sweep.json" "$SCR/sweep.stdout" "$SCR/sweep.stderr"
