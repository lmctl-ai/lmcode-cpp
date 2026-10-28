#include "../include/dslvm.hpp"
#include "../stdlib/collections.hpp"
#include "../stdlib/math.hpp"
#include "../stdlib/string.hpp"
#include "../stdlib/testing.hpp"
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>

using namespace dslvm;
using namespace dslvm::stdlib;

static bool contains(const std::vector<std::string>& values, const std::string& value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

static void be16(std::string& out, unsigned v) { out.push_back(char(v >> 8)); out.push_back(char(v)); }
static void be32(std::string& out, uint32_t v) { for (int i = 3; i >= 0; --i) out.push_back(char(v >> (i * 8))); }

// Small bytecode fixture: str_list() -> capture(StrList) -> halt. Keeping it
// here avoids coupling this regression test to the larger VM test assembler.
static std::string run_fixture(const std::string& schema) {
  const std::string first = "fn str_list() -> StrList";
  const std::string second = "fn capture(StrList) -> void";
  std::string out = "DSLB";
  be16(out, VERSION); out += sha256(schema);
  be32(out, 2);
  for (const auto& constant : {first, second}) {
    out.push_back(1); be32(out, uint32_t(constant.size())); out += constant;
  }
  be16(out, 2); be32(out, 0); be32(out, 1);
  be16(out, 0); // contexts
  be16(out, 0); // locals
  be16(out, 1); // max stack
  be32(out, 9); out.push_back(char(CALL_FN)); be16(out, 0); out.push_back(0); out.push_back(char(CALL_FN)); be16(out, 1); out.push_back(1); out.push_back(char(HALT));
  be32(out, 0); // lines
  be16(out, 0); // entries
  return out;
}

int main(int argc, char** argv) {
  if (argc == 2 && std::string(argv[1]) == "--signatures") {
    Registry standard; for (const auto& signature : standard.signatures()) std::cout << signature << '\n'; return 0;
  }
  // A normal registry owns the complete standard surface, while its host
  // surface remains separately inspectable.
  int checks = 0;
  auto check = [&](bool ok) { if (!ok) { std::cerr << "default std check " << checks + 1 << " failed\n"; std::abort(); } ++checks; };
  Registry registry;
  check(registry.has_std());
  check(registry.std_signatures().size() == 181);
  check(registry.host_signatures().empty());
  check(contains(registry.std_signatures(), "fn file_read(string) -> string"));
  check(contains(registry.std_signatures(), "fn file_lines(string) -> StrList"));
  check(registry.signatures().size() == 181);
  check(contains(registry.std_signatures(), "fn str_split(string, string) -> StrList"));
  check(contains(registry.std_signatures(), "method StrList.at(int) -> string"));
  check(!contains(registry.signatures(), "fn assert_close(float, float, float) -> void"));
  check(!contains(registry.signatures(), "fn print(string) -> void"));

  // Built-in registration is idempotent, and the old explicit group binders
  // remain safe for hosts that have not migrated their registration code yet.
  register_std(registry);
  register_string(registry);
  register_math(registry);
  check(registry.std_signatures().size() == 181);

  // Opt-out leaves a genuinely empty registry until a host opts back in.
  Registry disabled(StdMode::Disabled);
  check(!disabled.has_std());
  check(disabled.signatures().empty());
  register_std(disabled);
  check(disabled.has_std() && disabled.std_signatures().size() == 181);

  // Host-only and merged schema views both validate. Extra, missing, and
  // wrong-return signatures are rejected by the same self-check.
  Registry host(StdMode::Disabled);
  host.function("host_only", {"int"}, "string", [](Args&) { return Value::string("ok"); });
  const auto host_sigs = host.host_signatures();
  check(host.matches_schema_signatures(host_sigs));
  register_std(host);
  check(host.matches_schema_signatures(host_sigs));
  auto std_conflict = host_sigs; std_conflict.push_back("fn str_len(string) -> float");
  check(!host.matches_schema_signatures(std_conflict));
  auto merged = host.signatures();
  check(host.matches_schema_signatures(merged));
  auto missing = merged;
  missing.erase(std::find(missing.begin(), missing.end(), "fn host_only(int) -> string"));
  check(!host.matches_schema_signatures(missing));
  auto extra = merged;
  extra.push_back("fn invented() -> void");
  check(!host.matches_schema_signatures(extra));
  auto wrong = merged;
  *std::find(wrong.begin(), wrong.end(), "fn host_only(int) -> string") = "fn host_only(int) -> int";
  check(!host.matches_schema_signatures(wrong));

  // A host callback receives the exact bytes emitted by print, including NUL.
  std::ostringstream sink;
  Registry output;
  register_testing(output, &sink);
  check(output.std_signatures().size() == 181 && output.host_signatures().size() == 2);
  check(!output.matches_schema_signatures({}));
  check(output.matches_schema_signatures(output.host_signatures()));
  auto print = output.function_by_signature("fn print(string) -> void");
  check(print);
  Args bytes{Value::string(std::string("a\0b", 3))};
  print->fn(bytes);
  check(sink.str() == std::string("a\0b\n", 4));

  // The standard arena belongs to the registry and is directly usable by a
  // callback; a run scope returns it to an empty state for the next run.
  Registry objects;
  auto list = objects.std_collections().make_str_list();
  list->items.push_back("persisted only within this run");
  check(objects.std_collections().size() == 1);
  {
    auto scope = objects.std_run_scope();
    (void)scope;
    check(objects.std_collections().size() == 0);
    objects.std_collections().make_int_list();
  }
  check(objects.std_collections().size() == 0);
  objects.std_collections().reset();
  check(objects.std_collections().size() == 0);

  // Program.run resets the registry arena on every entry and exit, including
  // a host exception, and prevents re-entering a live registry run.
  const std::string schema = "{\"dsl.schema\":1,\"name\":\"std-default-test\"}";
  Registry runtime;
  Registry alternate(StdMode::Disabled);
  Program* active = nullptr;
  int callbacks = 0;
  bool nested_refused = false;
  bool throw_on_callback = false;
  runtime.function("capture", {"StrList"}, "void", [&](Args& args) {
    ++callbacks;
    check(args.size() == 1 && args[0].kind == Value::OBJ);
    check(runtime.std_collections().size() == 1);
    if (active && callbacks == 1) {
      try { Context nested; active->run(nested, alternate); }
      catch (const Error& e) { nested_refused = e.code == "RUN_HOST"; }
    }
    if (throw_on_callback) throw Error("callback failure");
    return Value::none();
  });
  Program fixture = Program::load(run_fixture(schema), schema, runtime);
  active = &fixture;
  Context empty;
  fixture.run(empty, runtime);
  check(callbacks == 1 && nested_refused && runtime.std_collections().size() == 0);
  fixture.run(empty, runtime);
  check(callbacks == 2 && runtime.std_collections().size() == 0);
  throw_on_callback = true;
  bool callback_failed = false;
  try { fixture.run(empty, runtime); } catch (const Error& e) { callback_failed = e.message == "callback failure"; }
  check(callback_failed && runtime.std_collections().size() == 0);

  throw_on_callback = false;
  fixture.run(empty, alternate);
  check(runtime.std_collections().size() == 0);
  Registry copied = runtime;
  {
    auto active_scope = runtime.std_run_scope();
    bool copy_refused = false;
    try { auto overlap = copied.std_run_scope(); (void)overlap; }
    catch (const Error& e) { copy_refused = e.code == "RUN_HOST"; }
    check(copy_refused);
  }

  // Existing duplicate declarations still fail rather than silently replacing
  // a standard implementation.
  Registry duplicate(StdMode::Disabled);
  duplicate.function("str_split", {"string", "string"}, "string", [](Args&) { return Value::string(""); });
  bool refused = false;
  try { register_std(duplicate); } catch (const Error& e) { refused = e.code == "LOAD_UNBOUND"; }
  check(refused);
  check(!duplicate.has_std() && duplicate.signatures().size() == 1);

  std::cout << "stdlib default: " << checks << " checks passed\n";
}
