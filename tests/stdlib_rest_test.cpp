// Tests of the opt-in `rest` std module (stdlib/rest.hpp): router, HTTP subset, registration, exchange rules and the
// shared conformance suite (conformance/rest-suite) replayed through in-memory streams and through a real child process.
//   stdlib_rest_test [REST_SERVE_EXE [SUITE_DIR]]      (defaults: build/rest_serve, conformance/rest-suite)
//   stdlib_rest_test --signatures                      prints the registered rest signatures (schema parity check)
#include "../stdlib/rest.hpp"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

using namespace dslvm;
using namespace dslvm::stdlib::rest;

// ---- a tiny test framework: named tests, CHECK throws with the failing expression ----------------------------------------
struct Failure { std::string text; };
#define CHECK(cond) do { if (!(cond)) throw Failure{std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " + #cond}; } while (0)
#define CHECK_EQ(a, b) do { const auto va_ = (a); const auto vb_ = (b); if (!(va_ == vb_)) throw Failure{std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " + #a + " == " + #b + "\n    left:  " + show(va_) + "\n    right: " + show(vb_)}; } while (0)
static std::string show(const std::string& s) { return "\"" + s + "\""; }
template <class T, class = typename std::enable_if<std::is_integral<T>::value>::type> static std::string show(T v) { return std::to_string(v); }
static std::string show(bool v) { return v ? "true" : "false"; }
static std::string show(const std::vector<std::string>& v) { std::string o = "["; for (auto& s : v) o += s + ","; return o + "]"; }
struct TestCase { const char* name; void (*body)(); };
static std::vector<TestCase>& all_tests() { static std::vector<TestCase> t; return t; }
struct Register { Register(const char* n, void (*b)()) { all_tests().push_back({n, b}); } };
#define TEST(label, id) static void id(); static Register reg_##id(label, id); static void id()

template <class F> static std::string message_of(F f) {
  try { f(); } catch (const Error& e) { CHECK_EQ(e.code, std::string("RUN_HOST")); return e.message; }
  throw Failure{"expected a RUN_HOST error, none thrown"};
}
#define THROWS(expr, text) do { std::string m_ = message_of([&] { (void)(expr); }); if (m_.find(text) == std::string::npos) throw Failure{std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": message \"" + m_ + "\" lacks \"" + (text) + "\""}; } while (0)

static std::string g_serve_exe = "build/rest_serve";
static std::string g_suite_dir = "conformance/rest-suite";

static Program::Entry entry(const std::string& annotation, const std::string& iface = HANDLER_INTERFACE, const std::string& name = "") {
  Program::Entry e; e.annotation = annotation; e.iface = iface; e.name = name.empty() ? annotation : name; return e;
}
static std::shared_ptr<RestController> controller_with(Registry& registry, std::vector<Program::Entry> entries, Options options = {}) {
  auto c = std::make_shared<RestController>(registry, std::move(options));
  c->bind_entries(entries);
  return c;
}

// ---- unit: routes -------------------------------------------------------------------------------------------------------
TEST("route templates: valid shapes parse into literal, param and rest segments", route_valid) {
  Route r = parse_route("GET", "/users/{id}/files/{path...}");
  CHECK_EQ(r.segments.size(), size_t(4));
  CHECK(r.segments[0].kind == Segment::LITERAL && r.segments[1].kind == Segment::PARAM && r.segments[2].kind == Segment::LITERAL && r.segments[3].kind == Segment::REST);
  CHECK_EQ(r.segments[1].text, std::string("id"));
  CHECK_EQ(parse_route("GET", "/").segments.size(), size_t(1));
  CHECK_EQ(parse_route("GET", "/files/").segments.back().text, std::string(""));
  CHECK_EQ(parse_route("GET", "/caf\xc3\xa0").segments[0].text, std::string("caf\xc3\xa0"));   // 0xA0 is an ordinary byte
}

TEST("route templates: invalid shapes are RUN_HOST errors with pinned text", route_invalid) {
  THROWS(parse_route("get", "/x"), "rest: invalid route get /x: method must be one of GET, POST, PUT, PATCH, DELETE, HEAD, OPTIONS");
  THROWS(parse_route("GET", "x"), "path must start with '/'");
  THROWS(parse_route("GET", ""), "path must start with '/'");
  THROWS(parse_route("GET", "/a b"), "whitespace");
  THROWS(parse_route("GET", "/a\tb"), "whitespace");
  THROWS(parse_route("GET", "/a?b"), "whitespace, control characters, ? or #");
  THROWS(parse_route("GET", "/a#b"), "whitespace, control characters, ? or #");
  THROWS(parse_route("GET", "/a//b"), "empty segment ('//')");
  THROWS(parse_route("GET", "/{1x}"), "invalid parameter name '1x'");
  THROWS(parse_route("GET", "/{}"), "invalid parameter name ''");
  THROWS(parse_route("GET", "/{...}"), "invalid parameter name ''");
  THROWS(parse_route("GET", "/{a-b}"), "invalid parameter name 'a-b'");
  THROWS(parse_route("GET", "/{a}/{a}"), "duplicate parameter 'a'");
  THROWS(parse_route("GET", "/{a...}/x"), "'{a...}' must be the last segment");
  THROWS(parse_route("GET", "/a{b}"), "malformed placeholder in segment 'a{b}'");
  THROWS(parse_route("GET", "/{b"), "malformed placeholder");
  THROWS(parse_route("GET", "/b}"), "malformed placeholder");
}

TEST("route table: duplicates are detected by shape, parameter names do not matter", route_duplicates) {
  RouteTable t;
  t.add("GET", "/a/{x}", "one");
  t.add("POST", "/a/{x}", "two");
  THROWS(t.add("GET", "/a/{y}", "three"), "rest: duplicate route GET /a/{y}");
  t.add("GET", "/a/b", "literal");
  t.add("GET", "/a/{x...}", "rest");   // a different shape
  CHECK_EQ(t.size(), size_t(4));
}

TEST("route table: specificity is literal > {param} > {rest...}, first difference wins", route_specificity) {
  RouteTable t;
  t.add("GET", "/x/{rest...}", "rest");
  t.add("GET", "/x/{p}/z", "param-z");
  t.add("GET", "/x/{p}/{q}", "param-param");
  t.add("GET", "/x/y/{q}", "literal-param");
  t.add("GET", "/x/y/z", "literal-literal");
  auto pick = [&](const std::string& path) { Match m = t.match("GET", path); return m.route ? m.route->annotation : std::to_string(m.status); };
  CHECK_EQ(pick("/x/y/z"), std::string("literal-literal"));
  CHECK_EQ(pick("/x/y/w"), std::string("literal-param"));
  CHECK_EQ(pick("/x/a/z"), std::string("param-z"));
  CHECK_EQ(pick("/x/a/b"), std::string("param-param"));
  CHECK_EQ(pick("/x/a/b/c"), std::string("rest"));
  CHECK_EQ(pick("/x/"), std::string("rest"));
  CHECK_EQ(pick("/x"), std::string("404"));
  CHECK_EQ(t.match("GET", "/x/a/b/c").params.at("rest"), std::string("a/b/c"));
}

TEST("route table: 404 vs 405, Allow header, HEAD fallback", route_methods) {
  RouteTable t;
  t.add("GET", "/r", "g"); t.add("DELETE", "/r", "d"); t.add("PUT", "/only-put", "p");
  Match m = t.match("POST", "/r");
  CHECK_EQ(m.status, 405); CHECK_EQ(m.allow, std::string("DELETE, GET, HEAD"));
  m = t.match("GET", "/only-put");
  CHECK_EQ(m.status, 405); CHECK_EQ(m.allow, std::string("PUT"));
  CHECK_EQ(t.match("GET", "/missing").status, 404);
  CHECK_EQ(t.match("HEAD", "/r").route->method, std::string("GET"));
  CHECK_EQ(t.match("HEAD", "/only-put").status, 405);
  CHECK_EQ(t.match("OPTIONS", "/r").status, 405);   // OPTIONS is never implicit
  t.add("HEAD", "/r", "h");
  CHECK_EQ(t.match("HEAD", "/r").route->annotation, std::string("h"));
}

TEST("route table: encoded slashes stay in one segment, parameters are decoded, invalid escapes never match", route_decoding) {
  RouteTable t;
  t.add("GET", "/f/{name}", "f");
  CHECK_EQ(t.match("GET", "/f/a%2Fb").params.at("name"), std::string("a/b"));
  CHECK_EQ(t.match("GET", "/f/a%2Fb/c").status, 404);
  CHECK_EQ(t.match("GET", "/f/%zz").status, 404);
  CHECK_EQ(t.match("GET", "/f/").status, 404);   // an empty segment is not a {param}
  CHECK_EQ(t.match("GET", "/f/%C3%B6").params.at("name"), std::string("\xc3\xb6"));
  std::string out;
  CHECK(percent_decode("%C3%B6", out)); CHECK_EQ(out, std::string("\xc3\xb6"));   // decoding yields bytes, not code points
  CHECK(percent_decode("a+b", out, true)); CHECK_EQ(out, std::string("a b"));
  CHECK(percent_decode("a+b", out)); CHECK_EQ(out, std::string("a+b"));
  CHECK(!percent_decode("%4", out)); CHECK(!percent_decode("%", out)); CHECK(!percent_decode("%g0", out));
  CHECK(percent_decode(std::string("%00"), out)); CHECK_EQ(out, std::string("\0", 1));
}

// ---- unit: HTTP subset ----------------------------------------------------------------------------------------------------
TEST("http: request heads", http_heads) {
  Head h = parse_head("GET /a?b=1 HTTP/1.1\r\nHost: x\r\nX-A: \t v \t");
  CHECK_EQ(h.error, 0); CHECK_EQ(h.method, std::string("GET")); CHECK_EQ(h.target, std::string("/a?b=1"));
  CHECK_EQ(h.headers.size(), size_t(2)); CHECK_EQ(h.headers[0].second, std::string("x")); CHECK_EQ(h.headers[1].first, std::string("X-A")); CHECK_EQ(h.headers[1].second, std::string("v"));
  CHECK_EQ(parse_head("HEAD /a HTTP/1.0").error, 0);
  CHECK_EQ(parse_head("GET /caf\xc3\xa0 HTTP/1.1").error, 0);                 // 0xA0 is an ordinary byte in targets
  CHECK_EQ(parse_head("GET /a HTTP/1.1\r\nX-\xa0: v").error, 0);              // ... and in header names
  CHECK_EQ(parse_head("GET /a HTTP/1.1\r\nX: \xa0v\xa0").headers[0].second, std::string("\xa0v\xa0"));   // values trim spaces/tabs only
  for (const char* bad : {"garbage", "GET /a HTTP/2.0", "GET a HTTP/1.1", "GET /a#f HTTP/1.1", "get /a HTTP/1.1", "GET  /a HTTP/1.1", "GET /a b HTTP/1.1",
                          "GET /a HTTP/1.1\r\nNoColon", "GET /a HTTP/1.1\r\n: v", "GET /a HTTP/1.1\r\nBad Name: v", "GET /a HTTP/1.1\r\nBad\tName: v",
                          "GET /a HTTP/1.1\r\nX: a\rb", "GET /a HTTP/1.1\r\nX: a\nb", "GET /a HTTP/1.1\r\nX: a\x01" "b", "GET /a\x7f HTTP/1.1", "GET / HTTP/1.1x", " HTTP/1.1"})
    CHECK_EQ(parse_head(bad).error, 400);
}

TEST("http: content-length rules", http_length) {
  CHECK_EQ(body_length({}).length, uint64_t(0));
  CHECK_EQ(body_length({{"content-length", "12"}}).length, uint64_t(12));
  CHECK_EQ(body_length({{"Content-Length", "1"}, {"content-length", "1"}}).length, uint64_t(1));
  CHECK_EQ(body_length({{"Content-Length", "1"}, {"content-length", "2"}}).error, 400);
  for (const char* v : {"-1", "1.5", "abc", "", "+1", " 1"}) CHECK_EQ(body_length({{"Content-Length", v}}).error, 400);
  CHECK_EQ(body_length({{"Content-Length", std::string(16, '1')}}).error, 400);
  CHECK_EQ(body_length({{"Content-Length", std::string(15, '9')}}).error, 0);
  CHECK_EQ(body_length({{"Transfer-Encoding", "chunked"}, {"Content-Length", "x"}}).error, 501);   // 501 wins over a bad length
}

TEST("http: response encoding (Content-Length framing, no Connection header)", http_encoding) {
  Response r; r.status = 200; r.headers = {{"A", "b"}}; r.body = "hi";
  CHECK_EQ(encode_response(r), std::string("HTTP/1.1 200 OK\r\nA: b\r\nContent-Length: 2\r\n\r\nhi"));
  r.status = 299;
  CHECK_EQ(encode_response(r, true), std::string("HTTP/1.1 299 OK\r\nA: b\r\nContent-Length: 2\r\n\r\n"));
  CHECK_EQ(reason_phrase(404), std::string("Not Found")); CHECK_EQ(reason_phrase(399), std::string("Redirect")); CHECK_EQ(reason_phrase(499), std::string("Client Error")); CHECK_EQ(reason_phrase(599), std::string("Server Error"));
  Response e = error_response(405, {{"Allow", "GET, HEAD"}});
  CHECK_EQ(encode_response(e), std::string("HTTP/1.1 405 Method Not Allowed\r\nContent-Type: application/json\r\nAllow: GET, HEAD\r\nContent-Length: 30\r\n\r\n{\"error\":\"method not allowed\"}"));
}

struct ChunkedBuf : std::streambuf {   // delivers the data a few bytes at a time, like a slow pipe
  std::string data; size_t pos = 0, step; char cell[128];
  ChunkedBuf(std::string d, size_t s) : data(std::move(d)), step(s) {}
  int_type underflow() override {
    if (pos >= data.size()) return traits_type::eof();
    size_t n = std::min(step, data.size() - pos); std::memcpy(cell, data.data() + pos, n); pos += n;
    setg(cell, cell, cell + n); return traits_type::to_int_type(cell[0]);
  }
};

TEST("http: framing off a stream (split reads, limits, truncation)", http_framing) {
  const std::string wire = "POST /s HTTP/1.1\r\nContent-Length: 5\r\n\r\nhelloGET /next HTTP/1.1\r\n\r\n";
  for (size_t step : {size_t(1), size_t(3), size_t(7), size_t(64)}) {
    ChunkedBuf buf(wire, step); std::istream in(&buf);
    Taken a = take_request(in, 100);
    CHECK(a.kind == Taken::REQUEST); CHECK_EQ(a.request.body, std::string("hello")); CHECK_EQ(a.request.method, std::string("POST"));
    Taken b = take_request(in, 100);
    CHECK(b.kind == Taken::REQUEST); CHECK_EQ(b.request.target, std::string("/next"));
    CHECK(take_request(in, 100).kind == Taken::END);
  }
  auto take = [](const std::string& wire, uint64_t max = 100) { std::istringstream in(wire); return take_request(in, max); };
  CHECK(take("").kind == Taken::END);
  CHECK(take("GET /hea").kind == Taken::END);                                              // truncated head: dropped
  CHECK(take("POST /s HTTP/1.1\r\nContent-Length: 10\r\n\r\n{\"v").kind == Taken::END);     // truncated body: dropped
  Taken big = take("POST /s HTTP/1.1\r\nContent-Length: 101\r\n\r\n");
  CHECK(big.kind == Taken::FATAL); CHECK_EQ(big.fatal, 413);                                // decided from the header, body unread
  CHECK_EQ(take("POST /s HTTP/1.1\r\nContent-Length: 100\r\n\r\n" + std::string(100, 'x')).kind == Taken::REQUEST, true);
  CHECK_EQ(take("POST /s HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n").fatal, 501);
  CHECK_EQ(take("garbage\r\n\r\n").fatal, 400);
  const std::string line = "GET / HTTP/1.1\r\nX: ";
  const std::string at_limit = line + std::string(MAX_HEADER_BYTES - line.size(), 'a') + "\r\n\r\n";   // head section of exactly 16 KiB
  CHECK(take(at_limit).kind == Taken::REQUEST);
  CHECK_EQ(take(line + std::string(MAX_HEADER_BYTES - line.size() + 1, 'a') + "\r\n\r\n").fatal, 431);
  CHECK_EQ(take(line + std::string(MAX_HEADER_BYTES * 2, 'a')).fatal, 431);                // detected before the head ends
}

// ---- unit: registration and validation ----------------------------------------------------------------------------------
static const std::vector<std::string>& declared_signatures() {
  static const std::vector<std::string> d = [] {
    std::vector<std::string> v{
      "fn rest_route(string, string, string) -> void", "fn rest_serve() -> void", "fn rest_stop() -> void",
      "method RestExchange.method() -> string", "method RestExchange.path() -> string", "method RestExchange.query(string) -> string",
      "method RestExchange.has_query(string) -> bool", "method RestExchange.param(string) -> string", "method RestExchange.header(string) -> string",
      "method RestExchange.has_header(string) -> bool", "method RestExchange.body() -> string", "method RestExchange.status(int) -> void",
      "method RestExchange.set_header(string, string) -> void", "method RestExchange.send(string) -> void", "method RestExchange.json(string) -> void",
      "method RestExchange.get(string) -> string", "method RestExchange.set(string, string) -> void", "method RestExchange.increment(string) -> int"};
    std::sort(v.begin(), v.end()); return v;
  }();
  return d;
}
static std::vector<std::string> rest_signatures(const Registry& r) {
  std::vector<std::string> out;
  for (const auto& s : r.signatures()) if (s.find("rest_") != std::string::npos || s.find("RestExchange") != std::string::npos) out.push_back(s);
  return out;
}

TEST("registration: exactly the 18 declared signatures, absent from the default registry", registration) {
  CHECK_EQ(declared_signatures().size(), size_t(18));
  Registry plain;
  CHECK_EQ(plain.signatures().size(), size_t(206));   // the default catalog is unchanged
  CHECK(rest_signatures(plain).empty());
  auto rest = register_rest(plain);
  CHECK_EQ(rest_signatures(plain), declared_signatures());
  CHECK_EQ(plain.signatures().size(), size_t(224));
  CHECK_EQ(plain.host_signatures().size(), size_t(18));   // they are host signatures, not std ones
  Registry again;
  CHECK(rest_signatures(again).empty());   // nothing leaked into a fresh default registry
  Registry bare(StdMode::Disabled);
  register_rest(bare);
  CHECK_EQ(bare.signatures().size(), size_t(18));
}

TEST("serve validation happens before any byte is read", serve_validation) {
  Registry registry;
  auto unbound = std::make_shared<RestController>(registry, Options{});
  unbound->route("GET", "/a", "a");
  THROWS(unbound->serve(), "rest: no program is bound (run the program through the controller)");

  auto none = controller_with(registry, {});
  THROWS(none->serve(), "rest: no routes registered");
  auto missing = controller_with(registry, {});
  missing->route("GET", "/a", "nope");
  THROWS(missing->serve(), "rest: route GET /a references missing annotation @nope");
  auto wrong = controller_with(registry, {entry("a", "interface Rule(Node) -> void", "check")});
  wrong->route("GET", "/a", "a");
  THROWS(wrong->serve(), "rest: entry check must implement RestHandler(RestExchange) -> void");
  auto dup = controller_with(registry, {entry("a"), entry("a", HANDLER_INTERFACE, "other")});
  dup->route("GET", "/a", "a");
  THROWS(dup->serve(), "rest: duplicate annotation @a");
  for (auto* c : {none.get(), missing.get(), wrong.get(), dup.get(), unbound.get()}) CHECK(!c->serving());
  // a failed serve can be retried once the problem is fixed, and entries of other interfaces are ignored
  auto fine = controller_with(registry, {entry("a"), entry("other", "interface Rule(Node) -> void")});
  fine->route("GET", "/a", "a");
  fine->serve();
  CHECK(fine->serving());
}

TEST("serve validation order: already called, then no program, then routes", serve_order) {
  Registry registry;
  auto c = controller_with(registry, {entry("a")});
  c->route("GET", "/a", "a");
  c->serve();
  THROWS(c->serve(), "rest: rest_serve already called");
  THROWS(c->route("GET", "/b", "a"), "rest: routes are frozen once rest_serve has been called");
  auto unbound = std::make_shared<RestController>(registry, Options{});
  THROWS(unbound->serve(), "no program is bound");   // reported before the missing routes
}

// ---- unit: exchange rules --------------------------------------------------------------------------------------------------
struct Harness {
  Registry registry; std::shared_ptr<RestController> rest; State state; Exchange x;
  Harness() { rest = register_rest(registry); x.state = &state; }
  Value call(const std::string& sig, std::vector<Value> args = {}) {
    const MethodEntry* m = registry.method_by_signature("method RestExchange." + sig);
    if (!m) throw Failure{"no such method " + sig};
    Args a(std::move(args)); return m->fn(&x, a);
  }
};
static Value S(const std::string& s) { return Value::string(s); }

TEST("exchange rules: status range", exchange_status) {
  Harness h;
  THROWS(h.call("status(int) -> void", {Value::integer(199)}), "rest: status 199 out of range 200..599");
  THROWS(h.call("status(int) -> void", {Value::integer(600)}), "out of range 200..599");
  THROWS(h.call("status(int) -> void", {Value::integer(INT64_MIN)}), "out of range");
  h.call("status(int) -> void", {Value::integer(599)}); CHECK_EQ(h.x.status, 599);
  h.call("status(int) -> void", {Value::integer(200)}); CHECK_EQ(h.x.status, 200);
}

TEST("exchange rules: header validation and reserved names", exchange_headers) {
  Harness h;
  const std::string sig = "set_header(string, string) -> void";
  for (const std::string& name : {std::string(""), std::string("a b"), std::string("a:b"), std::string("a\tb"), std::string("a\nb"), std::string("a\x7f")})
    THROWS(h.call(sig, {S(name), S("v")}), "invalid response header");
  THROWS(h.call(sig, {S("X"), S("a\r\nInjected: 1")}), "invalid response header");
  THROWS(h.call(sig, {S("X"), S("a\nb")}), "invalid response header");
  for (const char* name : {"Content-Length", "connection", "TRANSFER-ENCODING"}) THROWS(h.call(sig, {S(name), S("1")}), "managed by the server");
  h.call(sig, {S("X-A"), S("1")});
  h.call(sig, {S("X-B"), S("b")});
  h.call(sig, {S("x-a"), S("2")});
  CHECK_EQ(h.x.headers.size(), size_t(2));
  CHECK_EQ(h.x.headers[0].name, std::string("x-a")); CHECK_EQ(h.x.headers[0].value, std::string("2"));   // replaced in place, case-insensitively
  h.call(sig, {S("X-\xa0"), S("\xa0")});   // 0xA0 is an ordinary byte
  CHECK_EQ(h.x.headers.size(), size_t(3));
  h.call("json(string) -> void", {S("{}")});
  CHECK_EQ(h.x.body, std::string("{}")); CHECK(h.x.has_response_header("content-type"));
  h.call("send(string) -> void", {S(std::string("a\0b", 3))});
  CHECK_EQ(h.x.body, std::string("a\0b", 3));
}

TEST("exchange rules: counters and strings are separate maps, increment wraps as int64", exchange_state) {
  Harness h;
  CHECK_EQ(h.call("increment(string) -> int", {S("k")}).i, int64_t(1));
  CHECK_EQ(h.call("increment(string) -> int", {S("k")}).i, int64_t(2));
  CHECK_EQ(h.call("get(string) -> string", {S("k")}).s, std::string(""));
  h.call("set(string, string) -> void", {S("k"), S("v")});
  CHECK_EQ(h.call("get(string) -> string", {S("k")}).s, std::string("v"));
  CHECK_EQ(h.call("increment(string) -> int", {S("k")}).i, int64_t(3));
  h.state.counts["m"] = INT64_MAX;
  CHECK_EQ(h.call("increment(string) -> int", {S("m")}).i, INT64_MIN);
  CHECK_EQ(h.call("get(string) -> string", {S("nope")}).s, std::string(""));
}

TEST("exchange rules: request accessors", exchange_request) {
  Harness h;
  h.x.request.method = "POST"; h.x.request.body = "b\0y"; h.x.path = "/p";
  h.x.request.headers = {{"X-Tok", "a"}, {"x-tok", "b"}};
  h.x.params["id"] = "7"; h.x.query["q"] = ""; 
  CHECK_EQ(h.call("method() -> string").s, std::string("POST"));
  CHECK_EQ(h.call("header(string) -> string", {S("X-TOK")}).s, std::string("a, b"));
  CHECK_EQ(h.call("header(string) -> string", {S("none")}).s, std::string(""));
  CHECK_EQ(h.call("has_header(string) -> bool", {S("x-tok")}).b, true);
  CHECK_EQ(h.call("has_header(string) -> bool", {S("none")}).b, false);
  CHECK_EQ(h.call("param(string) -> string", {S("id")}).s, std::string("7"));
  CHECK_EQ(h.call("param(string) -> string", {S("zzz")}).s, std::string(""));
  CHECK_EQ(h.call("has_query(string) -> bool", {S("q")}).b, true);
  CHECK_EQ(h.call("query(string) -> string", {S("q")}).s, std::string(""));
  CHECK_EQ(h.call("path() -> string").s, std::string("/p"));
  const MethodEntry* m = h.registry.method_by_signature("method RestExchange.path() -> string");
  Args none; THROWS(m->fn(nullptr, none), "invalid exchange");
}

// ---- the suite: JSON reader, request composition, replay ------------------------------------------------------------------
struct Json {
  enum T { NUL, BOOL, NUM, STR, ARR, OBJ } t = NUL; bool b = false; double n = 0; std::string s; std::vector<Json> a; std::vector<std::pair<std::string, Json>> o;
  const Json* get(const std::string& k) const { for (auto& kv : o) if (kv.first == k) return &kv.second; return nullptr; }
  const Json& at(const std::string& k) const { auto* p = get(k); if (!p) throw Failure{"missing json key " + k}; return *p; }
};
struct JsonReader {
  const std::string& s; size_t i = 0;
  void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i; }
  static void utf8(std::string& out, unsigned cp) {
    if (cp < 0x80) out += char(cp); else if (cp < 0x800) { out += char(0xC0 | cp >> 6); out += char(0x80 | (cp & 63)); }
    else if (cp < 0x10000) { out += char(0xE0 | cp >> 12); out += char(0x80 | ((cp >> 6) & 63)); out += char(0x80 | (cp & 63)); }
    else { out += char(0xF0 | cp >> 18); out += char(0x80 | ((cp >> 12) & 63)); out += char(0x80 | ((cp >> 6) & 63)); out += char(0x80 | (cp & 63)); }
  }
  unsigned hex4() { unsigned v = std::stoul(s.substr(i, 4), nullptr, 16); i += 4; return v; }
  std::string str() {
    std::string out; ++i;
    while (s[i] != '"') {
      if (s[i] != '\\') { out += s[i++]; continue; }
      char c = s[++i]; ++i;
      switch (c) {
        case 'n': out += '\n'; break; case 'r': out += '\r'; break; case 't': out += '\t'; break; case 'b': out += '\b'; break; case 'f': out += '\f'; break;
        case 'u': { unsigned cp = hex4(); if (cp >= 0xD800 && cp < 0xDC00 && s[i] == '\\') { i += 2; cp = 0x10000 + ((cp - 0xD800) << 10) + (hex4() - 0xDC00); } utf8(out, cp); break; }
        default: out += c;
      }
    }
    ++i; return out;
  }
  Json value() {
    ws(); Json j;
    if (s[i] == '{') { j.t = Json::OBJ; ++i; ws(); if (s[i] == '}') { ++i; return j; } for (;;) { ws(); std::string k = str(); ws(); ++i; j.o.push_back({k, value()}); ws(); if (s[i++] == '}') break; } }
    else if (s[i] == '[') { j.t = Json::ARR; ++i; ws(); if (s[i] == ']') { ++i; return j; } for (;;) { j.a.push_back(value()); ws(); if (s[i++] == ']') break; } }
    else if (s[i] == '"') { j.t = Json::STR; j.s = str(); }
    else if (s.compare(i, 4, "true") == 0) { j.t = Json::BOOL; j.b = true; i += 4; }
    else if (s.compare(i, 5, "false") == 0) { j.t = Json::BOOL; i += 5; }
    else if (s.compare(i, 4, "null") == 0) { i += 4; }
    else { j.t = Json::NUM; size_t used = 0; j.n = std::stod(s.substr(i), &used); i += used; }
    return j;
  }
};
static std::string latin1_of(const std::string& utf8) {   // JSON text -> bytes, one byte per code point (the suite's `raw` is latin1)
  std::string out;
  for (size_t i = 0; i < utf8.size(); ++i) {
    unsigned char c = utf8[i];
    if (c < 0x80) out += char(c);
    else if ((c & 0xE0) == 0xC0) { out += char(((c & 0x1F) << 6) | (utf8[i + 1] & 0x3F)); ++i; }
    else throw Failure{"raw scenario text above U+00FF"};
  }
  return out;
}

static Json load_suite() { std::string text = read_file(g_suite_dir + "/scenarios.json"); JsonReader r{text}; return r.value(); }
static std::string request_bytes(const Json& s) {
  if (const Json* raw = s.get("raw")) return latin1_of(raw->s);
  std::string body;
  if (const Json* rep = s.get("body_repeat")) { for (int i = 0; i < int(rep->at("count").n); ++i) body += rep->at("text").s; }
  else if (const Json* b = s.get("body")) body = b->s;
  const std::string method = s.at("method").s;
  std::string out = method + " " + s.at("target").s + " HTTP/1.1\r\nHost: localhost\r\n";
  if (const Json* h = s.get("headers")) for (auto& kv : h->o) out += kv.first + ": " + kv.second.s + "\r\n";
  if (!body.empty() || method == "POST" || method == "PUT" || method == "PATCH") out += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  return out + "\r\n" + body;
}

struct Parsed { int status = 0; std::map<std::string, std::string> headers; std::string body; bool has_length = false; size_t length = 0; };
static std::vector<Parsed> parse_responses(const std::string& wire, const std::vector<std::string>& methods) {
  std::vector<Parsed> out; size_t pos = 0;
  while (pos < wire.size()) {
    size_t end = wire.find("\r\n\r\n", pos);
    if (end == std::string::npos) throw Failure{"incomplete response head: " + wire.substr(pos)};
    Parsed p; std::string head = wire.substr(pos, end - pos);
    size_t eol = head.find("\r\n");
    if (head.compare(0, 9, "HTTP/1.1 ") != 0) throw Failure{"bad status line: " + head};
    p.status = std::stoi(head.substr(9, 3));
    while (eol != std::string::npos) {
      size_t next = head.find("\r\n", eol + 2);
      std::string line = head.substr(eol + 2, next == std::string::npos ? std::string::npos : next - eol - 2);
      size_t colon = line.find(':');
      std::string v = line.substr(colon + 1); size_t a = v.find_first_not_of(" \t"); v = a == std::string::npos ? "" : v.substr(a);
      p.headers[lower(line.substr(0, colon))] = v;
      eol = next;
    }
    auto cl = p.headers.find("content-length");
    p.has_length = cl != p.headers.end();
    if (p.has_length) p.length = std::stoul(cl->second);
    const bool head_only = out.size() < methods.size() && methods[out.size()] == "HEAD";
    size_t start = end + 4;
    if (!head_only) { if (start + p.length > wire.size()) throw Failure{"short response body"}; p.body = wire.substr(start, p.length); }
    pos = head_only ? start : start + p.length;
    out.push_back(std::move(p));
  }
  return out;
}

static int check_response(const std::string& label, const Parsed& r, const Json& expect) {
  CHECK_EQ(r.status, int(expect.at("status").n));
  if (const Json* hs = expect.get("headers")) for (auto& kv : hs->o) {
    if (kv.second.t == Json::NUL) CHECK(r.headers.count(kv.first) == 0);
    else { auto it = r.headers.find(kv.first); if (it == r.headers.end() || it->second != kv.second.s) throw Failure{label + ": header " + kv.first + " expected \"" + kv.second.s + "\" got \"" + (it == r.headers.end() ? "<absent>" : it->second) + "\""}; }
  }
  if (r.headers.count("connection")) throw Failure{label + ": responses must not carry a Connection header"};
  if (!r.has_length) throw Failure{label + ": Content-Length missing"};
  if (const Json* b = expect.get("body")) if (r.body != b->s) throw Failure{label + ": body expected \"" + b->s + "\" got \"" + r.body + "\""};
  return 1;
}

using ServeFn = std::string (*)(const std::string& input);

static uint64_t suite_max_body() { return uint64_t(load_suite().at("options").at("max_body_bytes").n); }

/** Runs one fresh controller over `input` and returns everything written (in-memory streams). */
static std::string serve_in_memory(const std::string& input, std::function<void(const std::string&, const std::string&)> on_error = nullptr) {
  Registry registry;
  std::istringstream in(input); std::ostringstream out;
  Options options; options.input = &in; options.output = &out; options.max_body_bytes = suite_max_body();
  options.on_handler_error = on_error ? on_error : [](const std::string&, const std::string&) {};
  auto rest = register_rest(registry, options);
  Program program = Program::load(read_file(g_suite_dir + "/app.dslb"), read_file(g_suite_dir + "/host.schema.json"), registry);
  rest->run(program);
  return out.str();
}
static std::string serve_in_memory_default(const std::string& input) { return serve_in_memory(input); }

/** The same through the real process stdin/stdout: examples/rest_serve.cpp as a child process (file-backed pipes). */
static std::string serve_process(const std::string& input) {
  namespace fs = std::filesystem;
  static int counter = 0;
  const fs::path dir = fs::temp_directory_path();
  const std::string stem = "lmcode-rest-" + std::to_string(reinterpret_cast<uintptr_t>(&counter) % 100000) + "-" + std::to_string(counter++) + "-";
  const fs::path in_path = dir / (stem + "in"), out_path = dir / (stem + "out");
  { std::ofstream f(in_path, std::ios::binary); f.write(input.data(), std::streamsize(input.size())); }
  std::string exe = g_serve_exe;
#ifdef _WIN32
  for (char& c : exe) if (c == '/') c = '\\';
#endif
  std::string command = "\"" + exe + "\" \"" + g_suite_dir + "/app.dslb\" \"" + g_suite_dir + "/host.schema.json\" --max-body-bytes " + std::to_string(suite_max_body()) +
                        " < \"" + in_path.string() + "\" > \"" + out_path.string() + "\" 2>" + (
#ifdef _WIN32
                        "nul"
#else
                        "/dev/null"
#endif
                        );
#ifdef _WIN32
  command = "\"" + command + "\"";   // cmd.exe /c strips one pair of outer quotes
#endif
  int rc = std::system(command.c_str());
  std::string output = read_file(out_path.string());
  std::error_code ignored; fs::remove(in_path, ignored); fs::remove(out_path, ignored);
  if (rc != 0) throw Failure{"rest_serve exited with status " + std::to_string(rc) + " (" + command + ")"};
  return output;
}

/** The three suite groups, each scenario on a fresh controller; returns the number of checks. */
static int replay(std::string (*serve)(const std::string&)) {
  Json suite = load_suite(); int count = 0;
  const std::string good = "GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n";
  std::vector<const Json*> answered; std::vector<std::string> methods; std::string input; const Json* unanswered = nullptr;
  for (auto& s : suite.at("stream").a) {
    input += request_bytes(s);
    if (s.get("unanswered")) { unanswered = &s; continue; }
    answered.push_back(&s); methods.push_back(s.at("method").s);
  }
  CHECK(unanswered != nullptr);
  auto responses = parse_responses(serve(input), methods);
  CHECK_EQ(responses.size(), answered.size());   // one response per answered request, none after rest_stop()
  for (size_t i = 0; i < answered.size(); ++i) count += check_response(answered[i]->at("name").s, responses[i], answered[i]->at("expect"));
  for (auto& s : suite.at("fatal").a) {
    auto r = parse_responses(serve(request_bytes(s) + good), {s.at("method").s});
    CHECK_EQ(r.size(), size_t(1));                  // exactly one error response, then the end
    count += check_response(s.at("name").s, r[0], s.at("expect"));
  }
  for (auto& s : suite.at("silent").a) { CHECK_EQ(serve(request_bytes(s)), std::string("")); ++count; }
  return count;
}

// ---- integration: stdio -----------------------------------------------------------------------------------------------------
TEST("conformance suite: stream, fatal and silent groups through in-memory streams", suite_in_memory) {
  CHECK_EQ(replay(serve_in_memory_default), 39);
}

TEST("conformance suite: the same groups through the real process stdin/stdout (rest_serve child)", suite_process) {
  CHECK_EQ(replay(serve_process), 39);
}

TEST("serve: output is identical in memory and through the child process, byte for byte", serve_parity) {
  Json suite = load_suite(); std::string input;
  for (auto& s : suite.at("stream").a) input += request_bytes(s);
  CHECK_EQ(serve_in_memory_default(input), serve_process(input));
}

TEST("serve: rest_stop() ends serving, later input is never read", serve_stop_unread) {
  Registry registry;
  const std::string stop = "POST /shutdown HTTP/1.1\r\nHost: x\r\n\r\n", health = "GET /health HTTP/1.1\r\nHost: x\r\n\r\n";
  std::istringstream in(stop + health); std::ostringstream out;
  Options options; options.input = &in; options.output = &out;
  auto rest = register_rest(registry, options);
  Program program = Program::load(read_file(g_suite_dir + "/app.dslb"), read_file(g_suite_dir + "/host.schema.json"), registry);
  rest->run(program);
  CHECK(rest->stopped());
  CHECK_EQ(parse_responses(out.str(), {"POST"}).size(), size_t(1));
  std::string rest_of_input; std::getline(in, rest_of_input, '\0');
  CHECK_EQ(rest_of_input, health);   // not a byte of it was consumed
}

TEST("serve: a request split across reads is reassembled", serve_split) {
  Registry registry;
  const std::string body = "{\"values\":[10,20,12]}";
  ChunkedBuf buf("POST /sum HTTP/1.1\r\nHost: x\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body, 5);
  std::istream in(&buf); std::ostringstream out;
  Options options; options.input = &in; options.output = &out;
  auto rest = register_rest(registry, options);
  Program program = Program::load(read_file(g_suite_dir + "/app.dslb"), read_file(g_suite_dir + "/host.schema.json"), registry);
  rest->run(program);
  auto r = parse_responses(out.str(), {"POST"});
  CHECK_EQ(r.size(), size_t(1)); CHECK_EQ(r[0].status, 201); CHECK_EQ(r[0].body, std::string("{\"sum\":42}"));
}

TEST("serve: many requests keep order and counters increment 1..N", serve_many) {
  std::string input; std::vector<std::string> methods;
  for (int i = 0; i < 40; ++i) { input += "GET /count HTTP/1.1\r\nHost: x\r\n\r\n"; methods.push_back("GET"); }
  auto r = parse_responses(serve_in_memory_default(input), methods);
  CHECK_EQ(r.size(), size_t(40));
  for (int i = 0; i < 40; ++i) CHECK_EQ(r[i].body, std::to_string(i + 1));
}

TEST("serve: handler errors reach on_handler_error with the entry name and never the client", serve_handler_error) {
  std::vector<std::pair<std::string, std::string>> seen;
  std::string out = serve_in_memory("GET /boom HTTP/1.1\r\nHost: x\r\n\r\nGET /health HTTP/1.1\r\n\r\n", [&](const std::string& e, const std::string& m) { seen.push_back({e, m}); });
  auto r = parse_responses(out, {"GET", "GET"});
  CHECK_EQ(r.size(), size_t(2));   // the server keeps serving
  CHECK_EQ(r[0].status, 500); CHECK_EQ(r[0].body, std::string("{\"error\":\"internal error\"}")); CHECK_EQ(r[1].status, 200);
  CHECK_EQ(seen.size(), size_t(1)); CHECK_EQ(seen[0].first, std::string("boom"));
  CHECK(seen[0].second.find("zero") != std::string::npos);
  CHECK(out.find("zero") == std::string::npos);
}

TEST("dispatch: percent escapes are validated before routing; serving continues", dispatch_escapes) {
  Registry registry;
  std::istringstream in(""); std::ostringstream out;
  Options options; options.input = &in; options.output = &out; options.on_handler_error = [](const std::string&, const std::string&) {};
  auto rest = register_rest(registry, options);
  Program program = Program::load(read_file(g_suite_dir + "/app.dslb"), read_file(g_suite_dir + "/host.schema.json"), registry);
  rest->run(program);
  CHECK(rest->serving());
  auto get = [&](const std::string& target, const std::string& method = "GET") { return rest->dispatch(Request{method, target, {}, ""}).status; };
  CHECK_EQ(get("/nope%zz"), 400);            // 400 although no route would match
  CHECK_EQ(get("/nope?x=%zz"), 400);         // invalid query escape
  CHECK_EQ(get("/health?x=%4"), 400);
  CHECK_EQ(get("/nope"), 404);
  CHECK_EQ(get("/health", "POST"), 405);
  CHECK_EQ(get("/health?&&a&=b"), 200);      // empty pairs are skipped
  CHECK_EQ(get("/hello/%zz"), 400);
  Response r = rest->dispatch(Request{"HEAD", "/health", {}, ""});
  CHECK_EQ(r.status, 200); CHECK_EQ(r.body, std::string("{\"status\":\"ok\"}"));   // the body is dropped only when written
}

int main(int argc, char** argv) {
  if (argc == 2 && std::string(argv[1]) == "--signatures") {
    Registry registry; register_rest(registry);
    for (const auto& s : rest_signatures(registry)) std::cout << s << '\n';
    return 0;
  }
  if (argc > 1) g_serve_exe = argv[1];
  if (argc > 2) g_suite_dir = argv[2];
  int failed = 0;
  for (const auto& t : all_tests()) {
    try { t.body(); std::cout << "ok   " << t.name << "\n"; }
    catch (const Failure& f) { ++failed; std::cout << "FAIL " << t.name << "\n     " << f.text << "\n"; }
    catch (const std::exception& e) { ++failed; std::cout << "FAIL " << t.name << "\n     exception: " << e.what() << "\n"; }
  }
  std::cout << (all_tests().size() - failed) << "/" << all_tests().size() << " rest tests passed\n";
  return failed ? 1 : 0;
}
