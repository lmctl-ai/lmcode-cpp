#include "../stdlib/string.hpp"
#include <cassert>
#include <cmath>
#include <clocale>
#include <iostream>
#include <locale>

// One-shot allocation failure: error reporting can still allocate after rejection.
static bool reject_allocation = false;
void* operator new(std::size_t n) {
  if (reject_allocation) { reject_allocation = false; throw std::bad_alloc(); }
  if (void* p = std::malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

using namespace dslvm::stdlib;
// Two independent chunks call the same catalog and host callback. No VM changes.
static std::string upper_chunk(const std::string& schema, const std::string& input) {
  auto u16 = [](std::string& b, unsigned n) { b += char(n >> 8); b += char(n); };
  auto u32 = [](std::string& b, uint32_t n) { for (int k = 24; k >= 0; k -= 8) b += char(n >> k); };
  std::string b = "DSLB"; u16(b, dslvm::VERSION); b += dslvm::sha256(schema);
  u32(b, 3);
  for (const auto& c : {input, std::string("fn str_upper(string) -> string"), std::string("fn capture(string) -> void")}) {
    b += char(1); u32(b, uint32_t(c.size())); b += c;
  }
  u16(b, 2); u32(b, 1); u32(b, 2); // imports
  u16(b, 0); u16(b, 0); u16(b, 1); // contexts, locals, max_stack
  std::string code; code += char(dslvm::PUSH_STR); u32(code, 0);
  for (unsigned f : {0u, 1u}) { code += char(dslvm::CALL_FN); u16(code, f); code += char(1); }
  code += char(dslvm::HALT); u32(b, uint32_t(code.size())); b += code; u32(b, 1); u32(b, 0); u32(b, 12); u16(b, 8);
  u16(b, 0); // entries
  return b;
}

int main(int argc, char** argv) {
  dslvm::Registry reg(dslvm::StdMode::Disabled); register_string(reg);
  if (argc == 2 && std::string(argv[1]) == "--signatures") {
    for (const auto& s : reg.signatures()) std::cout << s << '\n';
    return 0;
  }
  // Real allocation rejection, not a policy quota or an oversized integer.
  for (bool replace : {false, true}) {
    const std::string source(128, 'x'), replacement(128, 'y');
    bool structured = false;
    reject_allocation = true;
    try { if (replace) (void)str_replace(source, "x", replacement); else (void)str_repeat(source, 128); }
    catch (const dslvm::Error& error) { structured = error.code == "RUN_HOST" && error.message.find("platform capacity") != std::string::npos; }
    catch (const std::bad_alloc&) {}
    reject_allocation = false;
    if (!structured) { std::cerr << "allocation did not become SDK Error\n"; return 1; }
  }
  int passed = 0;
  auto check = [&](bool ok) { if (!ok) { std::cerr << "failed string check " << passed + 1 << '\n'; std::abort(); } ++passed; };
  if (argc == 2 && std::string(argv[1]) == "--accept-repeat-above-old-cap") {
    try { return str_repeat("x", 64 * 1024 * 1024 + 1).size() == 64u * 1024u * 1024u + 1 ? 0 : 1; }
    catch (const dslvm::Error& e) { std::cerr << e.what() << '\n'; return 1; }
  }
  if (argc == 2 && std::string(argv[1]) == "--accept-replace-above-old-cap") {
    try {
      const auto result = str_replace(std::string(65, 'x'), "x", std::string(1024 * 1024, 'y'));
      return result.size() == 65u * 1024u * 1024u && result.front() == 'y' && result.back() == 'y' ? 0 : 1;
    } catch (const dslvm::Error& e) { std::cerr << e.what() << '\n'; return 1; }
  }
  if (argc == 2 && std::string(argv[1]) == "--accept-glob-work-above-old-cap") {
    const std::string text(135, '['), pattern(1024 * 1024, '[');
    try { return str_match(text, pattern) ? 1 : 0; }
    catch (const dslvm::Error& e) { std::cerr << e.what() << '\n'; return 1; }
  }
  check(str_starts_with("alpha", "al") && str_starts_with("", "") && !str_starts_with("a", "ab"));
  check(str_ends_with("alpha", "ha") && str_ends_with("", "") && !str_ends_with("a", "ba"));
  check(str_contains("abc", "") && str_contains("abc", "b") && !str_contains("abc", "z"));
  check(str_len(std::string("a\0b", 3)) == 3);
  check(str_substr("abcdef", -4, 2) == "ab" && str_substr("abcdef", 3, 100) == "def");
  check(str_substr("abc", 9, 3).empty() && str_substr("abc", 1, -1).empty());
  check(str_substr("abc", INT64_MIN, INT64_MAX) == "abc");
  check(str_substr("abc", INT64_MAX, INT64_MAX).empty() && str_substr("abc", 0, INT64_MIN).empty());
  check(str_substr(std::string("a\0b", 3), 1, INT64_MAX) == std::string("\0b", 2));
  std::string high_bytes; for (int c = 128; c < 256; ++c) high_bytes += char(c);
  check(str_upper(high_bytes) == high_bytes && str_lower(high_bytes) == high_bytes);
  check(str_trim(std::string(" \0\t ", 4)) == std::string(1, '\0'));
  check(str_trim("\t\n\v\f\r ").empty());
  check(str_lower("AbC-\xC3\x89") == "abc-\xC3\x89");
  check(str_upper("aBc-\xC3\xA9") == "ABC-\xC3\xA9");
  check(str_find("abcabc", "bc") == 1 && str_find("abc", "z") == -1 && str_find("abc", "") == 0);
  check(str_replace("a-b-a", "a", "xy") == "xy-b-xy" && str_replace("abc", "", "x") == "abc");
  check(str_replace(std::string("a\0a", 3), std::string("\0", 1), "X") == "aXa");
  {
    const std::string megabyte(1024 * 1024, 'x');
    check(str_replace(std::string(65, 'x'), "x", megabyte).size() == 65u * 1024u * 1024u);
    check(str_replace(std::string(64, 'x'), "x", megabyte).size() == 64u * 1024u * 1024u);
    check(str_replace("aaaaa", "aa", "b") == "bba" && str_replace("aaa", "a", "").empty());
  }
  check(str_trim(" \tvalue\r\n") == "value" && str_trim("\xC2\xA0x\xC2\xA0") == "\xC2\xA0x\xC2\xA0");
  check(str_repeat("xy", 3) == "xyxyxy" && str_repeat("x", 0).empty() && str_repeat("", INT64_MAX).empty());
  check(str_repeat("x", 64 * 1024 * 1024 + 1).size() == 64u * 1024u * 1024u + 1);
  bool repeat_overflow = false;
  try { (void)str_repeat("ab", INT64_MAX); }
  catch (const dslvm::Error& e) { repeat_overflow = e.code == "RUN_HOST" && e.message == "str_repeat: result size exceeds platform capacity"; }
  check(repeat_overflow);
  for (int64_t count : {int64_t(-1), INT64_MIN}) for (const auto& value : {std::string(), std::string("x")}) {
    bool refused = false;
    try { (void)str_repeat(value, count); }
    catch (const dslvm::Error& e) { refused = e.code == "RUN_HOST" && e.message == "str_repeat: count must be nonnegative"; }
    check(refused);
  }
  const std::string old_locale = std::setlocale(LC_NUMERIC, nullptr);
  int locales = 0;
  for (const char* name : {"C", "C.utf8", "en_US.utf8", "de_DE.UTF-8", "fr_FR.UTF-8"}) {
    if (!std::setlocale(LC_NUMERIC, name)) continue;
    ++locales; check(str_is_float("1.25") && str_to_float("1.25") == 1.25 && !str_is_float("1,25"));
  }
  std::setlocale(LC_NUMERIC, old_locale.c_str());
  std::cout << "numeric parsing checked in " << locales << " available locales\n";
  check(str_is_int("-9223372036854775808") && str_to_int("-9223372036854775808") == INT64_MIN && str_to_int("+12") == 12);
  check(!str_is_int("9223372036854775808") && !str_is_int(" 3") && !str_is_int("") && str_to_int("nope") == 0);
  for (const auto& token : {"+-1", "++1", "--1", "-+1", "+", "-", " 1", "1 ", "0x10", "1,5"}) {
    check(!str_is_int(token) && str_to_int(token) == 0);
    check(!str_is_float(token) && str_to_float(token) == 0.0);
  }
  check(!str_is_int(std::string("1\0", 2)) && !str_is_float(std::string("1\0", 2)));
  check(!str_is_int(std::string(10000, '9')) && !str_is_float(std::string(10000, '9')));
  check(str_is_int("9223372036854775807") && !str_is_int("-9223372036854775809"));
  check(!str_is_float("1e99999") && !str_is_float("1e-99999") && !str_is_float("0x1p2"));
  check(str_is_float(".5") && str_is_float("1.") && str_is_float("+1e-2"));
  check(str_is_float("-1.25e2") && std::abs(str_to_float("-1.25e2") + 125.0) < 1e-12 && str_to_float("+2.5") == 2.5);
  check(!str_is_float("nan") && !str_is_float("inf") && !str_is_float("1.2x") && str_to_float("bad") == 0.0);
  for (const auto& token : {".", "1e", "1e+", "1e-", "1..0", "0X1P2", "-inf", "+NaN", "nan(payload)", "\t1", "1\n"})
    check(!str_is_float(token) && str_to_float(token) == 0.0);
  check(str_is_float("4.9406564584124654e-324") && str_to_float("4.9406564584124654e-324") == std::numeric_limits<double>::denorm_min());
  check(!str_is_float("2.4703282292062327e-324") && str_is_float("2.4703282292062328e-324"));
  check(str_to_float("2.2250738585072014e-308") == std::numeric_limits<double>::min());
  check(str_to_float("1.7976931348623157e308") == std::numeric_limits<double>::max() && !str_is_float("1.7976931348623159e308"));
  check(str_to_float("1.00000000000000011102230246251565404236316680908203125") == 1.0);
  check(str_to_float("1.00000000000000011102230246251565404236316680908203126") == std::nextafter(1.0, 2.0));
  check(str_is_float("-0") && std::signbit(str_to_float("-0")) && std::signbit(str_to_float("-0.0e-99999")));
  check(str_is_float("0e99999") && str_is_float("0e-99999"));
  // A custom global C++ locale must not affect decimal syntax or rounding.
  struct CommaPoint : std::numpunct<char> { char do_decimal_point() const override { return ','; } };
  const std::locale previous = std::locale();
  std::locale::global(std::locale(std::locale::classic(), new CommaPoint));
  check(str_is_float("1.25") && str_to_float("1.25") == 1.25 && !str_is_float("1,25"));
  std::locale::global(previous);
  check(str_match("a/b.txt", "a/*.txt") && str_match("a", "[!b]") && str_match("x", "[z-a]") == false);
  check(str_match("?*[]\\", "\\?\\*\\[\\]\\\\") && str_match("[", "["));
  check(str_match("]", R"([\]])") && str_match("-", R"([a\-c])") && !str_match("b", R"([a\-c])"));
  check(str_match("\\", R"([\\])") && str_match("!", R"([\!])"));
  check(str_match("]", "[]]") && str_match("a", "[!]]") && !str_match("]", "[!]]"));
  check(str_match("-", "[-a]") && str_match("-", "[a-]") && !str_match("z", "[z-a]"));
  check(str_match("[]", "[]") && str_match("[!", "[!") && str_match("\\", "\\"));
  check(str_match("", "***") && !str_match("", "?") && !str_match("x", ""));
  check(str_match("b", R"([a-\c])") && str_match(std::string(1, char(0x80)), std::string("[") + char(0x80) + "]"));
  check(str_match(std::string("a\0b", 3), std::string("a?b", 3)));
  std::string adversarial(12000, 'a');
  check(!str_match(adversarial, "*aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab"));
  check(!str_match(std::string(135, '['), std::string(1024 * 1024, '[')));
  // Binder signatures and an actual callback dispatch are covered in the host.
  auto entry = reg.function_by_signature("fn str_upper(string) -> string");
  dslvm::Args args{dslvm::Value::string("mixed")};
  check(entry && entry->fn(args).s == "MIXED");
  args = {dslvm::Value::string(std::string("a\0b", 3))};
  check(entry->fn(args).s == std::string("A\0B", 3));
  {
    dslvm::stdlib::Collections arena;
    bool structured = false;
    reject_allocation = true;
    try { (void)arena.make_object<int>("int", 7); }
    catch (const dslvm::Error& error) { structured = error.code == "RUN_HOST" && error.message.find("allocation") != std::string::npos; }
    catch (const std::bad_alloc&) {}
    reject_allocation = false;
    check(structured && arena.objects.empty());
  }
  for (bool length_error : {false, true}) {
    dslvm::Registry failing(dslvm::StdMode::Disabled); register_string(failing);
    failing.function("capture", {"string"}, "void", [=](dslvm::Args&) -> dslvm::Value {
      if (length_error) throw std::length_error("growth");
      throw std::bad_alloc();
    });
    const std::string schema = R"({"dsl.schema":2,"name":"oom","functions":{"str_upper":[{"params":["string"],"returns":"string"}],"capture":[{"params":["string"],"returns":"void"}]}})";
    auto program = dslvm::Program::load(upper_chunk(schema, "x"), schema, failing);
    bool translated = false;
    try { dslvm::Context context; program.run(context, failing); }
    catch (const dslvm::Error& error) { translated = error.code == "RUN_HOST" && error.line == 12 && error.column == 8 && error.message.find("allocation") != std::string::npos; }
    check(translated);
  }
  std::vector<std::string> captured;
  reg.function("capture", {"string"}, "void", [&](dslvm::Args& a) { captured.push_back(a[0].s); return dslvm::Value::none(); });
  const std::string schema = R"({"dsl.schema":2,"name":"two_chunks","types":{},"context":{},"functions":{"str_upper":[{"params":["string"],"returns":"string"}],"capture":[{"params":["string"],"returns":"void"}]}})";
  auto first = dslvm::Program::load(upper_chunk(schema, std::string("a\0b", 3)), schema, reg);
  auto second = dslvm::Program::load(upper_chunk(schema, "second"), schema, reg);
  dslvm::Context context; first.run(context, reg); second.run(context, reg); first.run(context, reg);
  check(captured == std::vector<std::string>{std::string("A\0B", 3), "SECOND", std::string("A\0B", 3)});
  std::cout << "stdlib string: " << passed << " checks passed\n";
}
