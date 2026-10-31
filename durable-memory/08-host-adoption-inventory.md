# Host adoption audit

Scope: 25 lmcode host repositories and 30 schema/programmatic profiles under `the sibling checkouts directory`; lmproxy and SDK repositories are excluded. Corresponding `*.host.json` is preferred as the declaration set; older hosts use their primary `<host>.dsl.json`. lmdb has no schema directory, so `HostApi.java` is parsed as the declarative source and cross-checked against the emitted `Main --dsl-schema` JSON. lmgallery is a gallery/non-host repository; lmvideo is outside the lmcode host adoption set. No in-scope host lacks an `origin` remote.

## Canonical standard overlap

The effective generated profiles overlap canonical std in exactly these five repositories: `lmhtml`, `lmshark`, `lmspace`, `lmssh`, and `lmssl`. The source declaration profiles of all other hosts, including programmatic lmdb, have no canonical std overlap.

Canonical groups in effective profiles:
- function `('file_append', ('string', 'string'), 'void')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('file_exists', ('string',), 'bool')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('file_lines', ('string',), 'StrList')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('file_read', ('string',), 'string')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('file_write', ('string', 'string'), 'void')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('float_list', (), 'FloatList')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('int_list', (), 'IntList')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('math_abs', ('float',), 'float')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('math_abs', ('int',), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('math_ceil', ('float',), 'float')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('math_ceil', ('int',), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('math_floor', ('float',), 'float')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('math_floor', ('int',), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('math_max', ('float', 'float'), 'float')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('math_max', ('int', 'int'), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('math_min', ('float', 'float'), 'float')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('math_min', ('int', 'int'), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('math_pow', ('float', 'float'), 'float')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('math_pow', ('int', 'int'), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('math_round', ('float',), 'float')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('math_round', ('int',), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('math_sqrt', ('float',), 'float')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('math_sqrt', ('int',), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_contains', ('string', 'string'), 'bool')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_ends_with', ('string', 'string'), 'bool')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_find', ('string', 'string'), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_is_float', ('string',), 'bool')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_is_int', ('string',), 'bool')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_len', ('string',), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_list', (), 'StrList')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_lower', ('string',), 'string')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_map', (), 'StrMap')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_match', ('string', 'string'), 'bool')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_repeat', ('string', 'int'), 'string')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_replace', ('string', 'string', 'string'), 'string')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_split', ('string', 'string'), 'StrList')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_starts_with', ('string', 'string'), 'bool')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_substr', ('string', 'int', 'int'), 'string')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_to_float', ('string',), 'float')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_to_int', ('string',), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_trim', ('string',), 'string')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- function `('str_upper', ('string',), 'string')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('FloatList', 'add', ('float',), 'FloatList')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('FloatList', 'at', ('int',), 'float')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('FloatList', 'clear', (), 'FloatList')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('FloatList', 'contains', ('float',), 'bool')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('FloatList', 'index_of', ('float',), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('FloatList', 'len', (), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('FloatList', 'set', ('int', 'float'), 'FloatList')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('IntList', 'add', ('int',), 'IntList')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('IntList', 'at', ('int',), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('IntList', 'clear', (), 'IntList')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('IntList', 'contains', ('int',), 'bool')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('IntList', 'index_of', ('int',), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('IntList', 'len', (), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('IntList', 'set', ('int', 'int'), 'IntList')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('StrList', 'add', ('string',), 'StrList')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('StrList', 'at', ('int',), 'string')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('StrList', 'clear', (), 'StrList')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('StrList', 'contains', ('string',), 'bool')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('StrList', 'index_of', ('string',), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('StrList', 'join', ('string',), 'string')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('StrList', 'len', (), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('StrList', 'set', ('int', 'string'), 'StrList')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('StrMap', 'clear', (), 'StrMap')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('StrMap', 'get', ('string',), 'string')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('StrMap', 'get_or', ('string', 'string'), 'string')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('StrMap', 'has', ('string',), 'bool')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('StrMap', 'key_at', ('int',), 'string')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('StrMap', 'len', (), 'int')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('StrMap', 'remove', ('string',), 'bool')`: lmhtml, lmshark, lmspace, lmssh, lmssl
- method `('StrMap', 'set', ('string', 'string'), 'StrMap')`: lmhtml, lmshark, lmspace, lmssh, lmssl

## Shared host declarations

Optional testing functions are excluded from the main candidate table. Exact shared free function:
- `('material', (), 'Material')`: lmcad, lmmagnum

Exact shared methods:
- `('Camera', 'look_at', ('float', 'float', 'float', 'float', 'float', 'float'), 'Camera')`: lmcad, lmmagnum
- `('Key', 'bits', (), 'int')`: lmssh, lmssl
- `('Key', 'fingerprint', (), 'string')`: lmssh, lmssl
- `('Key', 'type', (), 'string')`: lmssh, lmssl
- `('Light', 'intensity', ('float',), 'Light')`: lmcad, lmmagnum
- `('Material', 'color', ('float', 'float', 'float'), 'Material')`: lmcad, lmmagnum
- `('Material', 'name', (), 'string')`: lmfilament, lmparticle
- `('Material', 'roughness', ('float',), 'Material')`: lmcad, lmmagnum
- `('Material', 'shininess', ('float',), 'Material')`: lmcad, lmmagnum
- `('Material', 'specular', ('float',), 'Material')`: lmcad, lmmagnum
- `('Matrix', 'cols', (), 'int')`: lmcell, lmfluid, lmnum, lmspace
- `('Matrix', 'get', ('int', 'int'), 'float')`: lmcell, lmfluid, lmnum, lmspace
- `('Matrix', 'rows', (), 'int')`: lmcell, lmfluid, lmnum, lmspace
- `('Matrix', 'save_csv', ('string',), 'void')`: lmcell, lmfluid, lmnum, lmspace
- `('Matrix', 'set', ('int', 'int', 'float'), 'Matrix')`: lmfluid, lmnum
- `('Mesh', 'triangle_count', (), 'int')`: lmblender, lmfilament
- `('Mesh', 'vertex_count', (), 'int')`: lmblender, lmfilament
- `('Report', 'add', ('string', 'string', 'int', 'string'), 'void')`: lmhtml, lmssh
- `('Report', 'fail_if_any', (), 'void')`: lmhtml, lmssh
- `('Report', 'write_json', ('string',), 'void')`: lmhtml, lmssh
- `('Report', 'write_text', ('string',), 'void')`: lmhtml, lmssh
- `('Result', 'column', ('int',), 'string')`: lmbio, lmdb
- `('Result', 'rows', (), 'int')`: lmbio, lmdb
- `('Scene', 'ambient', ('float',), 'Scene')`: lmcad, lmmagnum
- `('Scene', 'background', ('float', 'float', 'float'), 'Scene')`: lmcad, lmmagnum
- `('Scene', 'box', ('float', 'float', 'float', 'Material'), 'Shape')`: lmcad, lmmagnum
- `('Scene', 'camera', ('float', 'float', 'float', 'float', 'float', 'float', 'float'), 'Camera')`: lmcad, lmmagnum
- `('Scene', 'cylinder', ('float', 'float', 'Material'), 'Shape')`: lmcad, lmmagnum
- `('Scene', 'directional_light', ('float', 'float', 'float', 'float'), 'Light')`: lmcad, lmmagnum
- `('Scene', 'export_obj', ('string',), 'void')`: lmcad, lmmagnum
- `('Scene', 'export_stl', ('string',), 'void')`: lmcad, lmmagnum
- `('Scene', 'group', (), 'Group')`: lmdraw, lmmagnum
- `('Scene', 'render_png', ('string', 'int', 'int'), 'void')`: lmcad, lmfilament, lmmagnum
- `('Scene', 'render_png', ('string', 'int', 'int', 'int'), 'void')`: lmblender, lmdraw
- `('Scene', 'render_ppm', ('string', 'int', 'int'), 'void')`: lmcad, lmmagnum
- `('Scene', 'shadows', ('bool',), 'Scene')`: lmfilament, lmmagnum
- `('Scene', 'sphere', ('float', 'Material'), 'Shape')`: lmcad, lmmagnum
- `('Scene', 'tube_between', ('float', 'float', 'float', 'float', 'float', 'float', 'float', 'Material'), 'Shape')`: lmcad, lmmagnum
- `('Shape', 'color', ('float', 'float', 'float'), 'Shape')`: lmcad, lmmagnum
- `('Shape', 'name', ('string',), 'Shape')`: lmcad, lmmagnum
- `('Shape', 'rotate', ('float', 'float', 'float'), 'Shape')`: lmcad, lmmagnum
- `('Shape', 'scale', ('float', 'float', 'float'), 'Shape')`: lmcad, lmmagnum
- `('Shape', 'translate', ('float', 'float', 'float'), 'Shape')`: lmcad, lmmagnum
- `('Shape', 'visible', ('bool',), 'Shape')`: lmcad, lmmagnum
- `('System', 'energy', (), 'float')`: lmphysics, lmspace
- `('Texture', 'filter', ('string',), 'Texture')`: lmfilament, lmmagnum
- `('Texture', 'height', (), 'int')`: lmfilament, lmmagnum
- `('Texture', 'pixel', ('int', 'int', 'float', 'float', 'float'), 'Texture')`: lmfilament, lmmagnum
- `('Texture', 'repeat', ('float', 'float'), 'Texture')`: lmfilament, lmmagnum
- `('Texture', 'save_png', ('string',), 'void')`: lmfilament, lmmagnum
- `('Texture', 'width', (), 'int')`: lmfilament, lmmagnum
- `('Texture', 'wrap', ('string',), 'Texture')`: lmfilament, lmmagnum

Function names with multiple signature variants or profiles are in the JSON report. Type names declared in at least two distinct repositories:
- `Animation`: lmcad, lmimg
- `Body`: lmphysics, lmspace
- `Camera`: lmcad, lmmagnum
- `Curve`: lmfinance, lmimg
- `FloatList`: lmhtml, lmshark, lmspace, lmssh, lmssl
- `Group`: lmdraw, lmmagnum
- `IntList`: lmhtml, lmshark, lmspace, lmssh, lmssl
- `Key`: lmssh, lmssl
- `Light`: lmcad, lmmagnum
- `Material`: lmblender, lmcad, lmfilament, lmmagnum, lmparticle
- `Matrix`: lmcell, lmfluid, lmnum, lmspace
- `Mesh`: lmblender, lmfilament, lmfluid
- `Model`: lmbio, lmfinance
- `Node`: lmdraw, lmfilament, lmhtml
- `Report`: lmhtml, lmssh
- `Result`: lmbio, lmcell, lmdb, lmfinance, lmparticle, lmssh
- `Scene`: lmblender, lmcad, lmdraw, lmfilament, lmmagnum
- `Shape`: lmcad, lmdraw, lmmagnum
- `Sim`: lmcell, lmmolecule, lmparticle
- `StrList`: lmhtml, lmshark, lmspace, lmssh, lmssl
- `StrMap`: lmhtml, lmshark, lmspace, lmssh, lmssl
- `System`: lmmolecule, lmnum, lmphysics, lmspace
- `Texture`: lmfilament, lmmagnum

Programmatic lmdb schema: `Connection` and `Result` are the only lmcode object types; `connect` and `connect_file` are the free functions. `Database` and `Database.QueryResult` are Java implementation types, and rows are `List<List<Object>>`; there is no `Row` lmcode type.

## Classification and migration guidance

- `StrList`, `IntList`, `FloatList`, `StrMap` and canonical `str_*`, `math_*`, `file_*` signatures are already SDK std overlap in the five migrated C++ hosts. No source-only host outside those five declares canonical std signatures.
- The shared `material() -> Material` function in lmcad/lmmagnum is renderer-specific construction and is not common std.
- `Key` methods in lmssh/lmssl depend on different crypto object ownership and backends; keep host-owned.
- `Report` methods in lmhtml/lmssh have host-specific findings, diagnostics, ordering, and output policy; adapters would be complicated.
- `Matrix` is used by biology, fluid, numerical, and orbital domains with different storage, mutation, numerical rules, and lifetimes; keep domain-owned.
- `Scene`, `Shape`, `Camera`, `Light`, `Texture`, `Mesh`, `Material`, `Animation`, `Curve`, `Model`, `Body`, `Group`, `Result`, `Sim`, and `System` are domain object names reused across unrelated products. Same names do not imply compatible ownership or semantics.
- Optional testing functions `print(string)` and `assert_close(float,float,float)` occur in lmhtml, lmshark, lmspace, lmssh, and lmssl; they remain explicit host/testing APIs.
- No new std additions are recommended.

## Source registration drift

The JSON `source_registration_lines_missing_from_schema` section records likely registry registration lines whose literal names do not appear in the declaration schema. Dynamic registrations in lmblender, lmphysics, lmplot, and lmssh need manual review; generic wrappers and test strings are heuristic leads.

Reproduce with:

```sh
python3 /tmp/claude-1000/scratch-lmcode/host-adoption-audit/run-audit.py
```

Source registration search leads remain in the scratch audit. They are not confirmed schema drift: dynamic binders, generated names, internal helpers and tests require runtime signature comparison before a change.
