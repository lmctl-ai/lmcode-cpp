# Standard REST Endpoints & Stateless CLI Interfaces (`lmcode_std_v3` 226 Signatures)

## Summary

`lmcode-cpp` expands `lmcode_std_v3` from 206 to 226 canonical signatures by integrating `lmcode_std_rest_v1` into `register_std` (`stdlib/std.hpp`, `stdlib/rest.hpp`, `std/lmcode-std.schema.json`):
- **10 Standard Interfaces**: `RestGet`, `RestPost`, `RestPut`, `RestDelete`, `RestPatch`, `RestHead`, `RestOptions`, `RestHandler`, `CliCommand`, `CliOption`.
- **20 Standard Signatures**: `rest_config(string, string) -> void`, `rest_route(string, string, string) -> void`, `rest_serve() -> void`, `rest_stop() -> void`, `rest_cli(StrList) -> string`, and 15 `RestExchange` methods.
- **Route Templates**: Supports both `{param}` / `{rest...}` and `:param` / `:rest...` placeholders.
- **Stateless CLI Mode**: `rest_cli(argv)` dispatches options, commands, and REST routes using `call_entry_isolated` without resetting outer run state.
