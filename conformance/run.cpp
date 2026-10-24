// Conformance runner: implements the host stub described in conformance/vectors/manifest.json ("host") and checks every
// NAME.dslb against NAME.expect. An independent runtime (for example, Rust) can implement the same stub and reads the same files.
// Usage: build/conformance conformance/vectors
#include "dslvm.hpp"
#include "../stdlib/string.hpp"
#include "../stdlib/math.hpp"
#include "../stdlib/collections.hpp"
#include "../stdlib/testing.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

struct Thing { int64_t id; };
struct Stub {
  std::vector<std::string> trace; std::vector<std::unique_ptr<Thing>> things; Thing root{0}, variable{0};
  dslvm::stdlib::IntList xs{{0, 1, 2, 3}};
  std::ostringstream printed;
  dslvm::Registry reg;
  Thing* make(int64_t id) { things.push_back(std::make_unique<Thing>(Thing{id})); return things.back().get(); }
  static std::string fmt(double f) { char buf[64]; std::snprintf(buf, sizeof buf, "%.15g", f); return buf; }
  explicit Stub(bool) {
    using namespace dslvm;
    reg.set_std_output(&printed);
    reg.function("printed", {}, "string", [&](Args&) { return Value::string(printed.str()); });
    // Portable fixture adapter: raw bytes are supplied independently of UTF-8
    // source literals, and binary results are traced as unambiguous hex.
    reg.function("bytes", {"string"}, "string", [](Args& a) {
      const std::string& hex = a[0].s;
      auto digit = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return unsigned(c - '0');
        if (c >= 'a' && c <= 'f') return unsigned(c - 'a' + 10);
        throw Error("bytes: expected lowercase hexadecimal pairs");
      };
      if (hex.size() % 2) throw Error("bytes: expected lowercase hexadecimal pairs");
      std::string bytes;
      for (size_t i = 0; i < hex.size(); i += 2) bytes.push_back(char(digit(hex[i]) * 16 + digit(hex[i + 1])));
      return Value::string(std::move(bytes));
    });
    reg.method("Trace", "hex", {"string"}, "void", [&](void*, Args& a) { trace.push_back("hex " + hex(a[0].s)); return Value::none(); });
    reg.context("t", "Trace").context("root", "Thing").context("v", "Var").context("xs", "IntList");
    reg.method("Trace", "int", {"int"}, "void", [&](void*, Args& a) { trace.push_back("int " + std::to_string(a[0].i)); return Value::none(); });
    reg.method("Trace", "float", {"float"}, "void", [&](void*, Args& a) { trace.push_back("float " + fmt(a[0].f)); return Value::none(); });
    reg.method("Trace", "str", {"string"}, "void", [&](void*, Args& a) { trace.push_back("str " + a[0].s); return Value::none(); });
    reg.method("Trace", "bool", {"bool"}, "void", [&](void*, Args& a) { trace.push_back(std::string("bool ") + (a[0].b ? "true" : "false")); return Value::none(); });
    reg.method("Trace", "thing", {"Thing"}, "void", [&](void*, Args& a) { trace.push_back("thing " + std::to_string(static_cast<Thing*>(a[0].obj)->id)); return Value::none(); });
    reg.method("Thing", "len", {}, "int", [&](void* p, Args&) { auto n=static_cast<Thing*>(p)->id; trace.push_back("len " + std::to_string(n)); return Value::integer(n); });
    reg.method("Thing", "at", {"int"}, "Thing", [&](void*, Args& a) { trace.push_back("at " + std::to_string(a[0].i)); return Value::object("Thing", make(a[0].i)); });
    reg.method("Thing", "id", {}, "int", [&](void* self, Args&) { return Value::integer(static_cast<Thing*>(self)->id); });
    reg.method("Thing", "next", {}, "Thing", [&](void* self, Args&) { return Value::object("Thing", make(static_cast<Thing*>(self)->id + 1)); });
    reg.method("Thing", "name", {}, "string", [&](void* self, Args&) { return Value::string("thing-" + std::to_string(static_cast<Thing*>(self)->id)); });
    reg.method("Thing", "scaled", {"float"}, "float", [&](void* self, Args& a) { return Value::real(static_cast<Thing*>(self)->id * a[0].f); });
    reg.method("Trace", "int", {"float"}, "void", [&](void*, Args& a) { trace.push_back("float " + fmt(a[0].f)); return Value::none(); });
    reg.method("Trace", "status", {}, "Status", [](void*, Args&) { return Value::string("OPTIMAL"); });
    reg.method("Thing", "plus", {"Thing"}, "Thing", [&](void* p, Args& a) { return Value::object("Thing", make(from_twos_complement(uint64_t(static_cast<Thing*>(p)->id) + uint64_t(static_cast<Thing*>(a[0].obj)->id)))); });
    reg.method("Thing", "minus", {"Thing"}, "Thing", [&](void* p, Args& a) { return Value::object("Thing", make(from_twos_complement(uint64_t(static_cast<Thing*>(p)->id) - uint64_t(static_cast<Thing*>(a[0].obj)->id)))); });
    reg.method("Thing", "times", {"int"}, "Thing", [&](void* p, Args& a) { return Value::object("Thing", make(from_twos_complement(uint64_t(static_cast<Thing*>(p)->id) * uint64_t(a[0].i)))); });
    reg.method("Thing", "neg", {}, "Thing", [&](void* p, Args&) { return Value::object("Thing", make(from_twos_complement(0 - uint64_t(static_cast<Thing*>(p)->id)))); });
    reg.method("Thing", "rminus", {"int"}, "Thing", [&](void* p, Args& a) { return Value::object("Thing", make(from_twos_complement(uint64_t(a[0].i) - uint64_t(static_cast<Thing*>(p)->id)))); });
    reg.method("Thing", "set", {"int"}, "Thing", [](void* p, Args& a) { static_cast<Thing*>(p)->id = a[0].i; return Value::object("Thing", p); });
    reg.method("Var", "id", {}, "int", [](void* p, Args&) { return Value::integer(static_cast<Thing*>(p)->id); });
    reg.method("Var", "expr", {}, "Thing", [&](void* p, Args&) { auto n = static_cast<Thing*>(p)->id; trace.push_back("expr " + std::to_string(n)); return Value::object("Thing", make(n)); });
    reg.function("var", {"int"}, "Var", [&](Args& a) { trace.push_back("var " + std::to_string(a[0].i)); return Value::object("Var", make(a[0].i)); });
    reg.function("twice", {"float"}, "float", [](Args& a) { return Value::real(a[0].f * 2.0); });
    reg.function("thing", {"int"}, "Thing", [&](Args& a) { return Value::object("Thing", make(a[0].i)); });
    reg.function("twice", {"int"}, "int", [&](Args& a) { return Value::integer(a[0].i * 2); });
    reg.function("bound", {"int"}, "int", [&](Args& a) { trace.push_back("bound " + std::to_string(a[0].i)); return Value::integer(a[0].i); });
    reg.function("half", {"float"}, "float", [&](Args& a) { return Value::real(a[0].f / 2.0); });
    reg.function("bad_int", {}, "int", [](Args&) { return Value::string("x"); });
    reg.function("yes", {}, "bool", [&](Args&) { trace.push_back("yes"); return Value::boolean(true); });
    reg.function("fail", {"string"}, "void", [&](Args& a) -> Value { throw Error(a[0].s); });
  }
};

static std::string unescape(const std::string& s) { std::string out; for (size_t i = 0; i < s.size(); ++i) { if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'n') { out += '\n'; ++i; } else out += s[i]; } return out; }

// A vector may use "~ prefix number" for libm results.  The prefix is still
// exact; only its final numeric token gets the cross-runtime tolerance.
static bool trace_line_matches(const std::string& actual, const std::string& expected) {
  if (expected.compare(0, 2, "~ ") != 0) return actual == expected;
  const std::string body = expected.substr(2);
  const size_t split = body.rfind(' ');
  if (split == std::string::npos || split + 1 == body.size()) return false;
  const std::string prefix = body.substr(0, split + 1);
  if (actual.compare(0, prefix.size(), prefix) != 0) return false;
  char* expected_end = nullptr;
  char* actual_end = nullptr;
  const double expected_value = std::strtod(body.c_str() + split + 1, &expected_end);
  const double actual_value = std::strtod(actual.c_str() + prefix.size(), &actual_end);
  if (expected_end == body.c_str() + split + 1 || *expected_end != '\0' || actual_end == actual.c_str() + prefix.size() || *actual_end != '\0') return false;
  if (!std::isfinite(expected_value) || !std::isfinite(actual_value)) return false;
  const double tolerance = 1e-12 * std::max(std::abs(expected_value), 1e-300);
  return std::abs(actual_value - expected_value) <= tolerance;
}

int main(int argc, char** argv) {
  if (argc != 2) { std::cerr << "usage: conformance VECTOR_DIR\n"; return 2; }
  // Keep the approximate matcher itself covered: tolerance accepts a close
  // finite result, while drift, non-finite values, and prefix changes fail.
  if (!trace_line_matches("float 1.0000000000001", "~ float 1") ||
      trace_line_matches("float 1.00000000001", "~ float 1") ||
      trace_line_matches("float nan", "~ float 1") ||
      trace_line_matches("double 1", "~ float 1")) {
    std::cerr << "approximate trace matcher self-test failed\n";
    return 2;
  }
  std::string dir = argv[1];
  std::vector<std::string> names;
  if (DIR* d = ::opendir(dir.c_str())) { while (dirent* e = ::readdir(d)) { std::string n = e->d_name; if (n.size() > 7 && n.compare(n.size() - 7, 7, ".expect") == 0) names.push_back(n.substr(0, n.size() - 7)); } ::closedir(d); }
  std::sort(names.begin(), names.end());
  if (names.empty()) { std::cerr << "no vectors in " << dir << "\n"; return 2; }
  int failed = 0;
  for (const std::string& name : names) {
    std::ifstream ex(dir + "/" + name + ".expect"); std::string kind; std::getline(ex, kind); std::vector<std::string> lines; for (std::string l; std::getline(ex, l);) lines.push_back(unescape(l));
    std::string schemaFile = dir + "/schema.json";
    { std::ifstream m(dir + "/manifest.json"); std::stringstream ss; ss << m.rdbuf(); std::string mf = ss.str(); size_t p = mf.find("\"name\": \"" + name + "\"");
      if (p != std::string::npos) { size_t q = mf.find("\"schema\": \"", p), next = mf.find("\"name\": \"", p + 1); if (q != std::string::npos && q < next) { q += 11; schemaFile = dir + "/" + mf.substr(q, mf.find('"', q) - q); } } }
    std::string bytes = dslvm::read_file(dir + "/" + name + ".dslb"), schema = dslvm::read_file(schemaFile);
    Stub stub(schema.find("\"str_list\"") != std::string::npos); std::string result, error, error_code, bare_error; bool loaded = false, ran = false, error_has_location = false, error_has_column = false; uint32_t error_line = 0; uint16_t error_column = 0;
    try { dslvm::Program p = dslvm::Program::load(bytes, schema, stub.reg); loaded = true; dslvm::Context c; c.set("t", &stub.trace); c.set("root", &stub.root); c.set("v", &stub.variable); c.set("xs", &stub.xs); p.run(c, stub.reg); ran = true; }
    catch (const dslvm::Error& e) { error = e.what(); bare_error = error;
      if (e.has_location) {
        std::string prefix = "dsl line " + std::to_string(e.line) + (e.has_column ? ":" + std::to_string(e.column) : "") + ": ";
        if (bare_error.compare(0, prefix.size(), prefix) == 0) bare_error.erase(0, prefix.size());
      }
      error_code = e.code; error_has_location = e.has_location; error_has_column = e.has_column; error_line = e.line; error_column = e.column; }
    bool ok;
    if (kind == "trace") {
      ok = ran && stub.trace.size() == lines.size();
      if (ok) for (size_t i = 0; i < lines.size(); ++i) if (!trace_line_matches(stub.trace[i], lines[i])) { ok = false; break; }
    }
    else if (kind == "runtime_error_exact") ok = loaded && !ran && lines.size() == 2 && error_code == lines[0] && bare_error == lines[1];
    else if (kind == "runtime_error") ok = loaded && !ran && error.find(lines.at(0)) != std::string::npos;
    else if (kind == "runtime_error_at") {
      uint32_t line = lines.size() > 1 ? uint32_t(std::stoul(lines[1])) : 0; uint16_t column = lines.size() > 2 ? uint16_t(std::stoul(lines[2])) : 0;
      ok = loaded && !ran && lines.size() >= 4 && error_code == lines[0] && ((line == 0 && !error_has_location) || (line != 0 && error_has_location && error_line == line)) && ((column == 0 && !error_has_column) || (column != 0 && error_has_column && error_column == column)) && error.find(lines[3]) != std::string::npos;
    }
    else if (kind == "load_error") ok = !loaded && error.find(lines.at(0)) != std::string::npos;
    else { std::cerr << name << ": unknown expectation kind '" << kind << "'\n"; return 2; }
    if (ok) std::cout << "ok - " << name << " (" << kind << ")\n";
    else { failed++; std::cout << "FAIL - " << name << " (" << kind << ")\n"; if (!error.empty()) std::cout << "    error: " << error << "\n"; for (auto& t : stub.trace) std::cout << "    got:  " << t << "\n"; for (auto& l : lines) std::cout << "    want: " << l << "\n"; }
  }
  std::cout << (failed ? "FAIL " : "PASS ") << (names.size() - failed) << "/" << names.size() << " vectors\n";
  return failed ? 1 : 0;
}
