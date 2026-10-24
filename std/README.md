# lmcode std v1

`lmcode-std.schema.json` is the canonical, byte-pinned standard surface.
`lmcode.std: 1` is the standard-library version; `dsl.schema: 2` is the schema
language version, and bytecode version 5 is a separate contract. The canonical
file is a fragment, not a standalone application schema. Use the merge helper
before invoking the current compiler; future implicit injection will read the
same declarations.

The file combines string, math, collection and testing declarations: 69 canonical
signatures. The old `stdlib/*.schema-fragment.json` files are compatibility
subsets. Other SDKs copy this file exactly and pin its SHA-256 and source revision
in `std.lock`; they must not regenerate it locally.

Until implicit compiler injection is available, merge it at build time:

```sh
python3 scripts/merge-std-schema.py host.schema.json merged.schema.json
lmscript --dsl-signatures merged.schema.json
```

The helper retains host metadata and accepts identical declarations or additional
overloads/methods. A conflicting standard signature is an error. Use the exact
merged bytes both to compile bytecode and to load it; schema-hash binding does not
change. `--std PATH` selects a vendored canonical copy, and `-` selects stdin or
stdout for the corresponding positional argument.
