# Open items

1. Consider a packaged header distribution if copying and pinning the header becomes inconvenient for additional consumers.
2. Consider static argument-kind verification if untrusted bytecode becomes a supported use case; this requires the verifier to
   inspect schema types.
3. Expand the optional host-side string catalog when a concrete host needs additional ordinary functions. List-backed helpers
   should be declared by hosts that define a collection type.
4. Rule-methods rollout (2026-10-27, R47/R48): `sample-suite` gained the `interfaces.Adjuster` SAM declaration, program
   067-rule-methods, and the compile-errors/rule-method-mismatch diagnostic pin. Bytecode is lmcode bytecode (DSLB v6) since R49 (annotated rules emit entries), but the schema-hash
   change invalidated every pinned `.dslb`; the corpus was recompiled with the lmctl-mono rule-methods compiler. Source
   reproduction needs that compiler via `LMSCRIPT_BIN` until an npm release contains the feature, and the updated suite must
   still be synced (suite.lock) into all eight sample ports with each port's CI rerun. Superseded 2026-10-28 by the
   v6 rule entry table ([12](12-dslb-v6-rule-entries.md)): annotated rules now compile to entries and the whole
   corpus is v6.
