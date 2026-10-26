#include "../stdlib/encoding.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

using namespace dslvm;
using namespace dslvm::stdlib;

static void bad(std::string (*fn)(const std::string&), const std::string& input, const std::string& message) {
  try { (void)fn(input); assert(false); }
  catch (const Error& e) { assert(e.message == message); }
}

int main(int argc, char** argv) {
  Registry signatures(StdMode::Disabled);
  register_encoding(signatures);
  if (argc == 2 && std::string(argv[1]) == "--signatures") {
    for (const auto& signature : signatures.signatures()) std::cout << signature << '\n';
    return 0;
  }
  const size_t max_size = std::numeric_limits<size_t>::max();
  const size_t max_base64_input = (max_size / 4) * 3;
  assert(detail::checked_base64_size(max_base64_input) == (max_size / 4) * 4);
  try { (void)detail::checked_base64_size(max_base64_input + 1); assert(false); }
  catch (const Error& e) { assert(e.message == "str_base64_encode: size overflow"); }
  assert(detail::checked_hex_size(max_size / 2) == (max_size / 2) * 2);
  try { (void)detail::checked_hex_size(max_size / 2 + 1); assert(false); }
  catch (const Error& e) { assert(e.message == "str_hex_encode: size overflow"); }

  assert(str_base64_encode("") == "");
  assert(str_base64_encode("f") == "Zg==");
  assert(str_base64_encode("fo") == "Zm8=");
  assert(str_base64_encode("foo") == "Zm9v");
  assert(str_base64_encode("foobar") == "Zm9vYmFy");
  assert(str_base64_decode("Zg==") == "f");
  assert(str_base64_decode("Zm8=") == "fo");
  assert(str_base64_decode("Zm9v") == "foo");
  assert(str_hex_encode(std::string("\0\x01\x7f\x80\xff", 5)) == "00017f80ff");
  assert(str_hex_decode("00017F80ff") == std::string("\0\x01\x7f\x80\xff", 5));

  std::string all;
  for (int i = 0; i < 256; ++i) all.push_back(static_cast<char>(i));
  assert(str_base64_decode(str_base64_encode(all)) == all);
  assert(str_hex_decode(str_hex_encode(all)) == all);
  std::string large(1024 * 1024 + 17, '\0');
  for (size_t i = 0; i < large.size(); ++i) large[i] = static_cast<char>(i * 37);
  assert(str_base64_decode(str_base64_encode(large)) == large);
  assert(str_hex_decode(str_hex_encode(large)) == large);

  bad(str_base64_decode, "Z", "str_base64_decode: invalid encoding");
  bad(str_base64_decode, "Zg=", "str_base64_decode: invalid encoding");
  bad(str_base64_decode, "Zg==AAAA", "str_base64_decode: invalid encoding");
  bad(str_base64_decode, "Zh==", "str_base64_decode: invalid encoding");
  bad(str_base64_decode, "Zm=8", "str_base64_decode: invalid encoding");
  bad(str_base64_decode, "Zm9=", "str_base64_decode: invalid encoding");
  bad(str_base64_decode, "Zm9v\n", "str_base64_decode: invalid encoding");
  bad(str_base64_decode, "-w==", "str_base64_decode: invalid encoding");
  bad(str_hex_decode, "0", "str_hex_decode: invalid encoding");
  bad(str_hex_decode, "0g", "str_hex_decode: invalid encoding");
  bad(str_hex_decode, "00 ", "str_hex_decode: invalid encoding");
  bad(str_hex_decode, "0x00", "str_hex_decode: invalid encoding");

  assert(signatures.signatures().size() == 4);
  auto b64e = signatures.function_by_signature("fn str_base64_encode(string) -> string");
  auto b64d = signatures.function_by_signature("fn str_base64_decode(string) -> string");
  auto hexe = signatures.function_by_signature("fn str_hex_encode(string) -> string");
  auto hexd = signatures.function_by_signature("fn str_hex_decode(string) -> string");
  assert(b64e && b64d && hexe && hexd);
  Args callback_arg{Value::string(std::string("a\0b", 3))};
  assert(b64e->fn(callback_arg).s == "YQBi");
  callback_arg[0] = Value::string("YQBi");
  assert(b64d->fn(callback_arg).s == std::string("a\0b", 3));
  callback_arg[0] = Value::string(std::string("a\0b", 3));
  assert(hexe->fn(callback_arg).s == "610062");
  callback_arg[0] = Value::string("610062");
  assert(hexd->fn(callback_arg).s == std::string("a\0b", 3));
  std::cout << "encoding stdlib checks passed\n";
}
