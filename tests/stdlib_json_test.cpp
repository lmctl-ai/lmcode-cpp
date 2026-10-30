#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../stdlib/json.hpp"
#include "dslvm.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace dslvm;
using namespace dslvm::stdlib;
static Value fn(const Registry &r, const char *s, Args a) {
  auto *x = r.function_by_signature(s);
  assert(x && x->fn);
  return x->fn(a);
}
static Value meth(const Registry &r, const char *s, void *p, Args a) {
  auto *x = r.method_by_signature(s);
  assert(x && x->fn);
  return x->fn(p, a);
}
static void bad(const Registry &r, const char *s, Args a, const char *needle) {
  try {
    (void)fn(r, s, std::move(a));
    assert(false);
  } catch (const Error &e) {
    assert(std::string(e.what()).find(needle) != std::string::npos);
  }
}
static void bad_at(const Registry &r, const char *s, Args a, const char *needle,
                   size_t offset) {
  try {
    (void)fn(r, s, std::move(a));
    assert(false);
  } catch (const Error &e) {
    const std::string message = e.what();
    assert(message.find(needle) != std::string::npos);
    assert(message.find("at byte " + std::to_string(offset)) !=
           std::string::npos);
  }
}
static std::string nested(size_t depth) {
  std::string out;
  for (size_t i = 0; i < depth; ++i)
    out.push_back('[');
  out += "0";
  for (size_t i = 0; i < depth; ++i)
    out.push_back(']');
  return out;
}
static void templates() {
  std::ifstream input("std/template-cases.json");
  std::stringstream buffer;
  buffer << input.rdbuf();
  auto data = buffer.str();
  json_detail::Parser parser(data);
  auto vectors = parser.val();
  assert(!vectors.array.empty());
  auto field = [](JsonNode &n, const std::string &key) -> JsonNode * {
    for (auto &m : n.object)
      if (m.key == key)
        return m.value.get();
    return nullptr;
  };
  Registry r;
  for (auto &v : vectors.array) {
    auto context = field(v, "context");
    auto error = field(v, "error");
    auto before = json_detail::emit(*context);
    try {
      auto result = fn(r, "fn template_render(string, Json) -> string",
                       {Value::string(field(v, "template")->text),
                        Value::object("Json", context)});
      assert(!error && result.s == field(v, "result")->text);
    } catch (const Error &e) {
      assert(error && e.what() == error->text);
    }
    assert(before == json_detail::emit(*context));
  }
  std::string empty = "{}";
  json_detail::Parser ep(empty);
  auto context = ep.val();
  std::string bytes("\xff\0", 2);
  assert(fn(r, "fn template_render(string, Json) -> string",
            {Value::string(bytes), Value::object("Json", &context)})
             .s == bytes);
  bad(r, "fn template_render(string, Json) -> string",
      {Value::string(std::string("<items:{x|x}; separator=\\\"") + char(255) +
                     "\\\">"),
       Value::object("Json", &context)},
      "template_render: invalid loop separator");
  std::cout << "PASS template vectors and raw bytes\n";
}
int main(int argc, char **argv) {
  if (argc == 2 && std::string(argv[1]) == "--signatures") {
    Registry registry(StdMode::Disabled);
    Collections arena;
    register_json(registry, arena);
    for (const auto &signature : registry.signatures())
      std::cout << signature << '\n';
    return 0;
  }
  templates();
  Registry r(StdMode::Disabled);
  Collections arena;
  register_json(r, arena);
  auto root = fn(r, "fn json_parse(string) -> Json",
                 {Value::string("{\"a\":1,\"b\":[true,\"x\"]}")});
  assert(arena.size() == 1); // Json root; navigation borrows its children.
  assert(meth(r, "method Json.kind() -> string", root.obj, {}).s == "object");
  assert(meth(r, "method Json.len() -> int", root.obj, {}).i == 2);
  assert(
      meth(r, "method Json.has(string) -> bool", root.obj, {Value::string("a")})
          .b);
  auto a = meth(r, "method Json.get(string) -> Json", root.obj,
                {Value::string("a")});
  assert(meth(r, "method Json.as_int() -> int", a.obj, {}).i == 1);
  auto b = meth(r, "method Json.get(string) -> Json", root.obj,
                {Value::string("b")});
  assert(meth(r, "method Json.len() -> int", b.obj, {}).i == 2);
  auto at = meth(r, "method Json.at(int) -> Json", b.obj, {Value::integer(1)});
  assert(meth(r, "method Json.as_string() -> string", at.obj, {}).s == "x");
  auto keys = meth(r, "method Json.keys() -> StrList", root.obj, {});
  assert(static_cast<StrList *>(keys.obj)->items[0] == "a");
  assert(arena.size() == 2); // keys allocates one StrList.
  assert(meth(r, "method Json.to_json() -> string", root.obj, {}).s ==
         "{\"a\":1,\"b\":[true,\"x\"]}");
  auto nums = fn(r, "fn json_parse(string) -> Json",
                 {Value::string("[1,1.0,1e+2,-0]")});
  assert(meth(r, "method Json.to_json() -> string", nums.obj, {}).s ==
         "[1,1.0,1e+2,-0]");
  auto escaped = fn(r, "fn json_parse(string) -> Json",
                    {Value::string("{\"s\":\"\\u0061\\n\"}")});
  assert(meth(r, "method Json.to_json() -> string", escaped.obj, {}).s ==
         "{\"s\":\"\\u0061\\n\"}");
  auto escaped_key = fn(r, "fn json_parse(string) -> Json",
                        {Value::string("{\"\\u0061\":1}")});
  assert(meth(r, "method Json.has(string) -> bool", escaped_key.obj,
              {Value::string("a")})
             .b);
  assert(meth(r, "method Json.to_json() -> string", escaped_key.obj, {}).s ==
         "{\"\\u0061\":1}");
  auto floating =
      fn(r, "fn json_parse(string) -> Json", {Value::string("-1.25e2")});
  assert(meth(r, "method Json.as_float() -> float", floating.obj, {}).f ==
         -125.0);
  auto integer = fn(r, "fn json_parse(string) -> Json",
                    {Value::string("9223372036854775807")});
  assert(meth(r, "method Json.as_float() -> float", integer.obj, {}).f ==
         static_cast<double>(INT64_MAX));
  auto boolean =
      fn(r, "fn json_parse(string) -> Json", {Value::string("true")});
  assert(meth(r, "method Json.as_bool() -> bool", boolean.obj, {}).b);
  auto null = fn(r, "fn json_parse(string) -> Json", {Value::string("null")});
  assert(meth(r, "method Json.kind() -> string", null.obj, {}).s == "null");
  auto empty_array =
      fn(r, "fn json_parse(string) -> Json", {Value::string("[]")});
  assert(meth(r, "method Json.len() -> int", empty_array.obj, {}).i == 0);
  auto empty_object =
      fn(r, "fn json_parse(string) -> Json", {Value::string("{}")});
  assert(meth(r, "method Json.len() -> int", empty_object.obj, {}).i == 0);
  bad(r, "fn json_parse(string) -> Json",
      {Value::string("9223372036854775808")}, "integer out of range");
  bad(r, "fn json_parse(string) -> Json",
      {Value::string("-9223372036854775809")}, "integer out of range");
  bad(r, "fn json_parse(string) -> Json", {Value::string("1e9999")},
      "nonfinite number");
  auto underflow =
      fn(r, "fn json_parse(string) -> Json", {Value::string("1e-9999")});
  assert(meth(r, "method Json.as_float() -> float", underflow.obj, {}).f ==
         0.0);
  auto max_depth =
      fn(r, "fn json_parse(string) -> Json", {Value::string(nested(256))});
  assert(meth(r, "method Json.kind() -> string", max_depth.obj, {}).s ==
         "array");
  const size_t arena_before_errors = arena.size();
  bad(r, "fn json_parse(string) -> Json", {Value::string(nested(257))},
      "maximum depth exceeded");
  bad(r, "fn json_parse(string) -> Json", {Value::string("\"\\uD800\"")},
      "unpaired high surrogate");
  bad(r, "fn json_parse(string) -> Json", {Value::string("\"\\uDC00\"")},
      "unpaired low surrogate");
  const std::string invalid_utf8("\xC3\x28", 2);
  bad(r, "fn json_parse(string) -> Json",
      {Value::string(std::string("\"") + invalid_utf8 + "\"")},
      "invalid UTF-8");
  assert(arena.size() == arena_before_errors);
  auto nul =
      fn(r, "fn json_parse(string) -> Json", {Value::string("\"\\u0000\"")});
  assert(meth(r, "method Json.as_string() -> string", nul.obj, {}).s.size() ==
         1);
  bad_at(r, "fn json_parse(string) -> Json",
         {Value::string(std::string("\xEF\xBB\xBF\xC3\x28", 5))},
         "invalid UTF-8", 3);
  std::string many = "{";
  for (int i = 0; i < 4096; ++i) {
    if (i)
      many += ',';
    many += "\"k" + std::to_string(i) + "\":" + std::to_string(i);
  }
  many += "}";
  auto large = fn(r, "fn json_parse(string) -> Json", {Value::string(many)});
  assert(meth(r, "method Json.len() -> int", large.obj, {}).i == 4096);
  bad(r, "fn json_parse(string) -> Json",
      {Value::string("{\"\\u0061\":1,\"a\":2}")}, "duplicate object key");
  for (const char *bad :
       {"\xEF\xBB\xBFnull", "{\"a\":1,\"a\":2}", "[", "1e9999"}) {
    try {
      (void)fn(r, "fn json_parse(string) -> Json", {Value::string(bad)});
      assert(false);
    } catch (const Error &) {
    }
  }
  try {
    (void)meth(r, "method Json.as_string() -> string", root.obj, {});
    assert(false);
  } catch (const Error &e) {
    assert(std::string(e.what()).find("expected string") != std::string::npos);
  }
  try {
    (void)meth(r, "method Json.at(int) -> Json", root.obj, {Value::integer(0)});
    assert(false);
  } catch (const Error &e) {
    assert(std::string(e.what()).find("expected array") != std::string::npos);
  }
  try {
    (void)meth(r, "method Json.get(string) -> Json", root.obj,
               {Value::string("missing")});
    assert(false);
  } catch (const Error &e) {
    assert(std::string(e.what()).find("missing key") != std::string::npos);
  }
  std::cout << "json std tests passed\n";
}
