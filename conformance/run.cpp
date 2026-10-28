// Conformance runner: implements the host stub described in conformance/vectors/manifest.json ("host") and checks every
// NAME.dslb against NAME.expect. An independent runtime (for example, Rust) can implement the same stub and reads the same files.
// Usage: build/conformance conformance/vectors
#include "dslvm.hpp"
#include "../stdlib/string.hpp"
#include "../stdlib/math.hpp"
#include "../stdlib/collections.hpp"
#include "../stdlib/testing.hpp"
#include "../stdlib/file.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <vector>
#include "../tests/fixture_fs.hpp"

// Fixture setup uses native long paths on Windows independently of the catalog
// under test; the DSL still receives ordinary absolute paths.
static std::filesystem::path fixture_native_path(std::filesystem::path path) {
#ifdef _WIN32
  path.make_preferred();
  auto native = path.native();
  if (native.rfind(LR"(\\?\)", 0) == 0) return path;
  if (native.rfind(LR"(\\)", 0) == 0) return std::filesystem::path(LR"(\\?\UNC\)" + native.substr(2));
  return std::filesystem::path(LR"(\\?\)" + native);
#else
  return path;
#endif
}
struct TempCleanup {
  const std::string& directory;
  bool done = false;
  bool clean() {
    if (done || directory.empty()) return true;
    std::error_code error;
    fixture_fs::remove_tree(std::filesystem::u8path(directory), error);
    done = !error;
    return done;
  }
  ~TempCleanup() { try { clean(); } catch (...) { } }
};

struct Thing { int64_t id; };
struct Stub {
  std::vector<std::string> trace; std::vector<std::unique_ptr<Thing>> things; Thing root{0}, variable{0};
  dslvm::stdlib::IntList xs{{0, 1, 2, 3}};
  std::ostringstream printed;
  std::string temp_dir;
  dslvm::Registry reg;
  Thing* make(int64_t id) { things.push_back(std::make_unique<Thing>(Thing{id})); return things.back().get(); }
  static std::string fmt(double f) { char buf[64]; std::snprintf(buf, sizeof buf, "%.15g", f); return buf; }
  explicit Stub(bool) {
    using namespace dslvm;
    dslvm::stdlib::register_testing(reg, &printed);
    reg.function("test_temp_dir", {}, "string", [&](Args&) { return Value::string(temp_dir); });
    reg.function("test_long_path", {}, "string", [&](Args&) {
      auto directory = std::filesystem::u8path(temp_dir);
      for (int i = 0; i < 7; ++i) directory /= "segment-" + std::string(40, 'a') + std::to_string(i);
      std::error_code error;
      std::filesystem::create_directories(fixture_native_path(directory), error);
      if (error) throw Error("fixture: cannot create long-path directory: " + error.message());
      return Value::string((directory / std::filesystem::u8path(u8"résumé-文件-😀.bin")).u8string());
    });
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

// dsl-conformance/3: a call vector's line 2 is "ENTRY ARGS-JSON". Args are a JSON array read against the entry's
// declared parameter types: numbers become int or float, strings and bools stay as they are, and {"type":"Thing","id":N}
// uses the fixture Thing (id N, name "thing-N").
static std::vector<dslvm::Value> parse_call_args(const std::string& json, const dslvm::Program::Entry& entry, Stub& stub) {
  using dslvm::Value;
  size_t p = 0;
  auto fail = [&](const std::string& m) -> void { throw dslvm::Error("RUN_HOST", "conformance: bad call args: " + m + " in " + json); };
  auto ws = [&] { while (p < json.size() && (json[p] == ' ' || json[p] == '\t')) ++p; };
  auto parse_string = [&]() -> std::string {
    if (p >= json.size() || json[p] != '"') fail("expected a string");
    ++p; std::string out;
    while (p < json.size() && json[p] != '"') {
      if (json[p] == '\\' && p + 1 < json.size()) { char c = json[++p]; out += c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c; ++p; }
      else out += json[p++];
    }
    if (p >= json.size()) fail("unterminated string");
    ++p; return out;
  };
  auto parse_number = [&]() -> std::string {
    const size_t start = p;
    while (p < json.size() && (isdigit(json[p]) || json[p] == '-' || json[p] == '+' || json[p] == '.' || json[p] == 'e' || json[p] == 'E')) ++p;
    if (start == p) fail("expected a number");
    return json.substr(start, p - start);
  };
  ws(); if (p >= json.size() || json[p] != '[') fail("expected [");
  ++p; ws();
  std::vector<Value> args;
  while (p < json.size() && json[p] != ']') {
    if (args.size() >= entry.params.size()) fail("more arguments than the entry takes");
    const std::string& type = entry.params[args.size()];
    if (json[p] == '{') {
      ++p; ws();
      const std::string key = parse_string(); ws();
      if (p >= json.size() || json[p] != ':') fail("expected :");
      ++p; ws();
      const std::string objtype = parse_string(); ws();
      if (p >= json.size() || json[p] != ',') fail("expected ,");
      ++p; ws();
      const std::string key2 = parse_string(); ws();
      if (p >= json.size() || json[p] != ':') fail("expected :");
      ++p; ws();
      const int64_t id = std::stoll(parse_number()); ws();
      if (p >= json.size() || json[p] != '}') fail("expected }");
      ++p;
      if (key != "type" || key2 != "id" || objtype != type) fail("object argument does not match the parameter type");
      if (objtype != "Thing") fail("unknown object type " + objtype);
      args.push_back(Value::object("Thing", stub.make(id)));
    } else if (json[p] == '"') {
      args.push_back(Value::string(parse_string()));
    } else if (json.compare(p, 4, "true") == 0) { p += 4; args.push_back(Value::boolean(true)); }
    else if (json.compare(p, 5, "false") == 0) { p += 5; args.push_back(Value::boolean(false)); }
    else {
      const std::string num = parse_number();
      args.push_back(type == "float" ? Value::real(std::stod(num)) : Value::integer(std::stoll(num)));
    }
    ws();
    if (p < json.size() && json[p] == ',') { ++p; ws(); }
  }
  if (p >= json.size()) fail("unterminated array");
  ++p; ws();
  if (p != json.size()) fail("trailing text after ]");
  return args;
}

// The "result ..." line of a call vector: "result int N|float X|string S|bool true|false|thing ID|void".
// An enum result arrives as `string NAME`; an object result is the fixture Thing with the given id.
static bool result_matches(const std::string& line, const dslvm::Value& v) {
  using dslvm::Value;
  if (line.compare(0, 7, "result ") != 0) return false;
  const std::string spec = line.substr(7);
  if (spec == "void") return v.kind == Value::VOID;
  const size_t sp = spec.find(' ');
  if (sp == std::string::npos) return false;
  const std::string type = spec.substr(0, sp), text = spec.substr(sp + 1);
  if (type == "int") return v.kind == Value::INT && std::to_string(v.i) == text;
  if (type == "float") return v.kind == Value::FLOAT && Stub::fmt(v.f) == text;
  if (type == "string") return v.kind == Value::STR && v.s == text;
  if (type == "bool") return v.kind == Value::BOOL && (v.b ? "true" : "false") == text;
  if (type == "thing") return v.kind == Value::OBJ && v.type == "Thing" && std::to_string(static_cast<Thing*>(v.obj)->id) == text;
  return false;
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
    Stub stub(schema.find("\"str_list\"") != std::string::npos);
    TempCleanup temp_cleanup{stub.temp_dir};
    if (schema.find("\"test_temp_dir\"") != std::string::npos) {
      std::error_code temp_error;
      const auto base = std::filesystem::temp_directory_path(temp_error);
      if (temp_error) { std::cerr << name << ": cannot find temp directory: " << temp_error.message() << "\n"; return 2; }
      for (unsigned attempt = 0; attempt != 100; ++attempt) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto temp = base / ("lmcode-file-vector-" + name + "-" + std::to_string(stamp) + "-" + std::to_string(attempt));
        if (std::filesystem::create_directory(temp, temp_error)) { stub.temp_dir = temp.u8string(); break; }
        if (temp_error && temp_error != std::errc::file_exists) break;
      }
      if (stub.temp_dir.empty()) { std::cerr << name << ": cannot create fresh temp directory: " << temp_error.message() << "\n"; return 2; }
    }
    std::string result, error, error_code, bare_error; bool loaded = false, ran = false, called = false, error_has_location = false, error_has_column = false; uint32_t error_line = 0; uint16_t error_column = 0;
    dslvm::Value call_result;
    const bool is_call = kind == "call" || kind == "call_error" || kind == "call_error_at";
    std::unique_ptr<dslvm::Program> program;
    try {
      program = std::make_unique<dslvm::Program>(dslvm::Program::load(bytes, schema, stub.reg));
      loaded = true;
      dslvm::Context c; c.set("t", &stub.trace); c.set("root", &stub.root); c.set("v", &stub.variable); c.set("xs", &stub.xs);
      if (kind != "entries") { program->run(c, stub.reg); ran = true; }  // call kinds run the body once, then the entry
      if (is_call) {
        const std::string& selector = lines.at(0);
        const size_t sp = selector.find(' ');
        const std::string entry_name = selector.substr(0, sp);
        size_t entry_index = program->entries().size();
        for (size_t i = 0; i < program->entries().size(); ++i) if (program->entries()[i].name == entry_name) { entry_index = i; break; }
        if (entry_index == program->entries().size()) throw dslvm::Error("RUN_HOST", "conformance: entry '" + entry_name + "' not found");
        call_result = program->call(entry_index, c, stub.reg, parse_call_args(sp == std::string::npos ? "[]" : selector.substr(sp + 1), program->entries()[entry_index], stub));
        called = true;
      }
    }
    catch (const dslvm::Error& e) { error = e.what(); bare_error = error;
      if (e.has_location) {
        std::string prefix = "dsl line " + std::to_string(e.line) + (e.has_column ? ":" + std::to_string(e.column) : "") + ": ";
        if (bare_error.compare(0, prefix.size(), prefix) == 0) bare_error.erase(0, prefix.size());
      }
      error_code = e.code; error_has_location = e.has_location; error_has_column = e.has_column; error_line = e.line; error_column = e.column; }
    if (std::getenv("LMCODE_TRACE")) {
      for (const auto& line : stub.trace) std::cout << "TRACE\t" << name << "\t" << line << "\n";
      if (!error_code.empty()) std::cout << "ERROR\t" << name << "\t" << error_code << "\t" << bare_error << "\n";
    }
    bool ok;
    if (kind == "trace") {
      ok = ran && stub.trace.size() == lines.size();
      if (ok) for (size_t i = 0; i < lines.size(); ++i) if (!trace_line_matches(stub.trace[i], lines[i])) { ok = false; break; }
    }
    else if (kind == "runtime_error_exact") ok = loaded && !ran && lines.size() == 2 && error_code == lines[0] && bare_error == lines[1];
    else if (kind == "runtime_error_file") {
      // File paths are deliberately dynamic and OS reason text is localized.
      // The vector pins the code, function, and relative path while requiring
      // the implementation to retain a nonempty OS reason suffix.
      const std::string expected_path = lines.size() >= 3 && !lines[2].empty() ? stub.temp_dir + "/" + lines[2] : stub.temp_dir;
      const std::string expected_prefix = lines.size() >= 3 ? lines[1] + ": " + expected_path + ": " : "";
      ok = loaded && !ran && lines.size() == 3 && error_code == lines[0] && bare_error.compare(0, expected_prefix.size(), expected_prefix) == 0 && bare_error.find_first_not_of(" \t\r\n", expected_prefix.size()) != std::string::npos;
    }
    else if (kind == "runtime_error") ok = loaded && !ran && error.find(lines.at(0)) != std::string::npos;
    else if (kind == "runtime_error_at") {
      uint32_t line = lines.size() > 1 ? uint32_t(std::stoul(lines[1])) : 0; uint16_t column = lines.size() > 2 ? uint16_t(std::stoul(lines[2])) : 0;
      ok = loaded && !ran && lines.size() >= 4 && error_code == lines[0] && ((line == 0 && !error_has_location) || (line != 0 && error_has_location && error_line == line)) && ((column == 0 && !error_has_column) || (column != 0 && error_has_column && error_column == column)) && error.find(lines[3]) != std::string::npos;
    }
    else if (kind == "entries") {
      std::vector<std::string> actual;
      if (loaded) for (const auto& e : program->entries()) actual.push_back(e.name + " " + e.annotation + " " + e.iface);
      ok = loaded && actual == lines;
    }
    else if (kind == "call") {
      ok = loaded && ran && called && lines.size() >= 2 && result_matches(lines[1], call_result) && stub.trace.size() == lines.size() - 2;
      if (ok) for (size_t i = 2; i < lines.size(); ++i) if (!trace_line_matches(stub.trace[i - 2], lines[i])) { ok = false; break; }
    }
    else if (kind == "call_error") ok = loaded && !called && lines.size() == 3 && error_code == lines[1] && error.find(lines[2]) != std::string::npos;
    else if (kind == "call_error_at") {
      uint32_t line = lines.size() > 2 ? uint32_t(std::stoul(lines[2])) : 0; uint16_t column = lines.size() > 3 ? uint16_t(std::stoul(lines[3])) : 0;
      ok = loaded && !called && lines.size() >= 5 && error_code == lines[1] && ((line == 0 && !error_has_location) || (line != 0 && error_has_location && error_line == line)) && ((column == 0 && !error_has_column) || (column != 0 && error_has_column && error_column == column)) && error.find(lines[4]) != std::string::npos;
    }
    else if (kind == "load_error") ok = !loaded && error.find(lines.at(0)) != std::string::npos;
    else { std::cerr << name << ": unknown expectation kind '" << kind << "'\n"; return 2; }
    if (ok) std::cout << "ok - " << name << " (" << kind << ")\n";
    else { failed++; std::cout << "FAIL - " << name << " (" << kind << ")\n"; if (!error.empty()) std::cout << "    error: " << error << "\n";
      if (kind == "entries" && loaded) for (const auto& e : program->entries()) std::cout << "    got:  " << e.name << " " << e.annotation << " " << e.iface << "\n";
      if (kind == "call" && called) std::cout << "    result: " << (call_result.kind == dslvm::Value::VOID ? "void" : call_result.kind == dslvm::Value::INT ? "int " + std::to_string(call_result.i) : call_result.kind == dslvm::Value::STR ? "string " + call_result.s : call_result.kind_name()) << "\n";
      for (auto& t : stub.trace) std::cout << "    got:  " << t << "\n";
      for (auto& l : lines) std::cout << "    want: " << l << "\n"; }
    if (!temp_cleanup.clean()) { if (ok) ++failed; std::cerr << "FAIL - " << name << ": temporary directory cleanup failed\n"; }
  }
  std::cout << (failed ? "FAIL " : "PASS ") << (names.size() - failed) << "/" << names.size() << " vectors\n";
  return failed ? 1 : 0;
}
