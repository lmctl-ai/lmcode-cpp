# Block spelling readiness sweep

`tools/lmcode-sweep.sh` and its adjacent `lmcode-sweep.mjs` are the installed
`~/tools/lmcode-sweep.sh` implementation. Node and the compiler's exported
`Lexer` determine structural block tokens; no substring replacement is used.
The default mode is `--dry-run`. `--apply` is an explicit write mode and cannot
be combined with `--stage`. This readiness audit used staging only.

## Transformation contract

The tool changes the opener `_dsl_` and its matched `end _dsl_` marker to
`_lmcode_` / `end _lmcode_`. It preserves every other byte, including newlines,
indentation, body strings, comments and other raw-block formats. Existing
`_lmcode_` blocks are unchanged. Lexer errors leave the affected code region
unchanged. Invalid UTF-8 text is reported rather than decoded lossily; a UTF-8
BOM is preserved, including when the selected lexer rejects that source.

Names such as `.dsl.json`, `.dslb`, `dslvm`, `savedsl`, `dsl_cli`,
`--dsl-schema` and `--dsl-signatures` are not renamed. Marker text inside
strings, comments, raw body data or prose stays text. Remaining `_dsl_`
mentions are listed with file, line and column for manual classification.
Markdown LMScript fences, indented code and LMScript shell heredocs are
handled separately from prose; incomplete snippets remain unchanged.

The repository inventory uses existing GitHub `lmctl-ai` remotes under
`/home/mma/repos/dsl`, plus lmvideo (currently
`/home/mma/repos/lmctlhq/lmvideo`). lmproxy and lmctl-mono are excluded before
source inspection. Repository discovery never creates repositories or changes
remotes. Nonignored `.lms` files, READMEs and documentation are inventoried,
including untracked examples; build/vendor directories are excluded.

## Compiler and reproduction

The audit compiler is lmctl-mono origin/main
`17cc7da7474f194d4c254020750c4853afb43f9f`, which contains alias commit
`c055c3029faeac0f92a867781ec37ce44ef9991c`. The original checkout is not
edited. A detached worktree and all build/check artifacts live below the
scratch directory. The published compiler lacks the new spelling at the time
of this snapshot, so a current built lexer is required.

```sh
compiler_worktree=/tmp/claude-1000/scratch-lmcode/compiler-lmctl-mono
# Register a detached scratch worktree if it does not already exist:
git -C /home/mma/repos/lmctlhq/lmctl-mono worktree add --detach "$compiler_worktree" origin/main
# In the scratch worktree:
npm ci --ignore-scripts
npm run build --workspace @lmctl-ai/native-git
npm run build --workspace lmctl
npm run build --workspace lmscript
```

The build used Node 26.8.2, npm 11.19.1, Bun 1.3.14 and TypeScript 5.6.3.
The built CLI reports lmscript 0.2.38. Both spellings passed a schema-backed
`--check` smoke case. The CLI binary is `lmscript/bin/lmscript` in that worktree.

```sh
~/tools/lmcode-sweep.sh --self-test
~/tools/lmcode-sweep.sh --dry-run \
  --lexer /tmp/claude-1000/scratch-lmcode/compiler-lmctl-mono/lmscript/dist/lmscript.js \
  --stage /tmp/claude-1000/scratch-lmcode/lmcode-sweep-stage \
  --report-json /tmp/claude-1000/scratch-lmcode/sweep-report.json
```

`--root DIR`, `--repo NAME`, `--lexer FILE` and `LMSCRIPT_LEXER` select the
inventory/compiler. Staging and JSON report destinations are restricted to
`/tmp/claude-1000/scratch-lmcode/`. Staging copies rewritten scripts and schema
JSON dependencies without writing source files. Changed documentation code
regions become scratch-only checkable snippets with paired legacy baselines.
Use a fresh stage directory for a new snapshot so removed input files cannot
leave stale checks behind.

The self-test checks actual transformed bytes for 19 cases: both delimiters,
other raw formats, multiple queued blocks, strings/comments, interpolation,
existing aliases, malformed/indented closers, CR/LF/CRLF, BOMs, Markdown and
heredocs. This does not require creating a fixture Git repository or remote.
