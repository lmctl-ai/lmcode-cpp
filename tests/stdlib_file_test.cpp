#include "../stdlib/file.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace dslvm;
using namespace dslvm::stdlib;

int main(int argc, char** argv) {
  Registry registry(StdMode::Disabled);
  Collections arena;
  register_file(registry, arena);
  if (argc == 2 && std::string(argv[1]) == "--signatures") {
    for (const auto& signature : registry.signatures()) std::cout << signature << "\n";
    return 0;
  }
  int checks = 0;
  auto check = [&](bool ok) { if (!ok) { std::cerr << "file check " << checks+1 << " failed\n"; std::abort(); } ++checks; };
  const auto root = std::filesystem::temp_directory_path() / ("lmcode-file-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::error_code ec;
  check(std::filesystem::create_directory(root));
  const auto path = root / "bytes.bin";
  unsigned operations = 0;
  auto call = [&](const std::string& signature, Args args) {
#ifdef _WIN32
    std::cerr << "file operation " << ++operations << ": " << signature << std::endl;
#else
    (void)operations;
#endif
    return registry.function_by_signature(signature)->fn(args);
  };
  const std::string binary("a\0\xff\nb\r\n", 7);
  call("fn file_write(string, string) -> void", {Value::string(path.string()), Value::string(binary)});
  check(call("fn file_exists(string) -> bool", {Value::string(path.string())}).b);
  check(call("fn file_read(string) -> string", {Value::string(path.string())}).s == binary);
  auto lines = call("fn file_lines(string) -> StrList", {Value::string(path.string())});
  check(static_cast<StrList*>(lines.obj)->items == std::vector<std::string>({std::string("a\0\xff", 3), "b\r", ""}));
  call("fn file_append(string, string) -> void", {Value::string(path.string()), Value::string("tail")});
  check(call("fn file_read(string) -> string", {Value::string(path.string())}).s == binary + "tail");
  std::string large(1024 * 1024 + 17, '\x7f');
  call("fn file_write(string, string) -> void", {Value::string(path.string()), Value::string(large)});
  check(call("fn file_read(string) -> string", {Value::string(path.string())}).s == large);
  check(!call("fn file_exists(string) -> bool", {Value::string("")}).b);
  const auto appended = root / "append-created";
  call("fn file_append(string, string) -> void", {Value::string(appended.u8string()), Value::string("created")});
  check(call("fn file_read(string) -> string", {Value::string(appended.u8string())}).s == "created");
  check(!call("fn file_exists(string) -> bool", {Value::string((path / "child").u8string())}).b);
  const auto previous_cwd = std::filesystem::current_path();
  std::filesystem::current_path(root);
  const auto relative_content = call("fn file_read(string) -> string", {Value::string("append-created")}).s;
  std::filesystem::current_path(previous_cwd);
  check(relative_content == "created");
#ifndef _WIN32
  const auto byte_path = root / std::string("name-\xff", 6);
  bool byte_path_created = false;
  try {
    call("fn file_write(string, string) -> void", {Value::string(byte_path.native()), Value::string(binary)});
    byte_path_created = true;
  } catch (const Error& e) {
    // Native filesystems may reject byte paths (APFS reports EILSEQ).
    const auto prefix = "file_write: " + byte_path.native() + ": ";
    check(e.code == "RUN_HOST" && e.message.rfind(prefix, 0) == 0 && e.message.size() > prefix.size());
  }
  if (byte_path_created) check(call("fn file_read(string) -> string", {Value::string(byte_path.native())}).s == binary);
#endif
  const auto empty = root / "empty";
  call("fn file_write(string, string) -> void", {Value::string(empty.string()), Value::string("")});
  check(call("fn file_read(string) -> string", {Value::string(empty.string())}).s.empty());
  auto empty_lines = call("fn file_lines(string) -> StrList", {Value::string(empty.string())});
  check(static_cast<StrList*>(empty_lines.obj)->items == std::vector<std::string>{""});
  const auto newline = root / "newline";
  call("fn file_write(string, string) -> void", {Value::string(newline.string()), Value::string("\n")});
  auto newline_lines = call("fn file_lines(string) -> StrList", {Value::string(newline.string())});
  check(static_cast<StrList*>(newline_lines.obj)->items == std::vector<std::string>({"", ""}));
  check(!call("fn file_exists(string) -> bool", {Value::string((root / "missing").string())}).b);
  const auto missing = (root / "missing").string();
  bool failed = false;
  try { call("fn file_read(string) -> string", {Value::string(missing)}); }
  catch (const Error& e) { failed = e.code == "RUN_HOST" && e.message.rfind("file_read: " + missing + ": ", 0) == 0; }
  check(failed);
  const auto directory = root / "directory";
  std::filesystem::create_directory(directory);
  check(!call("fn file_exists(string) -> bool", {Value::string(directory.string())}).b);
  failed = false;
  try { call("fn file_read(string) -> string", {Value::string(directory.string())}); }
  catch (const Error& e) { failed = e.code == "RUN_HOST"; }
  check(failed);
  failed = false;
  try { call("fn file_write(string, string) -> void", {Value::string(std::string("bad\0path", 8)), Value::string("x")}); }
  catch (const Error& e) { failed = e.code == "RUN_HOST"; }
  check(failed);
  failed = false;
  try { call("fn file_write(string, string) -> void", {Value::string(directory.string()), Value::string("x")}); }
  catch (const Error& e) { failed = e.code == "RUN_HOST"; }
  check(failed);
  failed = false;
  try { call("fn file_write(string, string) -> void", {Value::string((root / "missing-parent" / "file").string()), Value::string("x")}); }
  catch (const Error& e) { failed = e.code == "RUN_HOST"; }
  check(failed);
#ifdef _WIN32
  const auto unicode = root / L"\u03bb-long-name";
  call("fn file_write(string, string) -> void", {Value::string(unicode.u8string()), Value::string("unicode")});
  check(call("fn file_read(string) -> string", {Value::string(unicode.u8string())}).s == "unicode");
  std::filesystem::path long_path = root;
  for (int i = 0; i < 20; ++i) long_path /= "long-directory-name";
  std::cerr << "file long-directory setup" << std::endl;
  std::filesystem::create_directories(file_detail::native_path("test_setup", long_path.u8string()));
  std::cerr << "file long-directory ready" << std::endl;
  long_path /= "long-file-name";
  const auto long_utf8 = long_path.u8string();
  call("fn file_write(string, string) -> void", {Value::string(long_utf8), Value::string("long")});
  check(call("fn file_read(string) -> string", {Value::string(long_utf8)}).s == "long");
  failed = false;
  try { call("fn file_exists(string) -> bool", {Value::string(std::string("\xff", 1))}); }
  catch (const Error& e) { failed = e.code == "RUN_HOST" && e.message.rfind("file_exists: ", 0) == 0; }
  check(failed);
#endif
#ifdef _WIN32
  std::cerr << "file cleanup" << std::endl;
#endif
  auto cleanup_root = root;
#ifdef _WIN32
  auto cleanup_wide = std::filesystem::absolute(root).make_preferred().native();
  if (cleanup_wide.rfind(L"\\\\?\\", 0) != 0) {
    if (cleanup_wide.rfind(L"\\\\", 0) == 0) cleanup_wide = L"\\\\?\\UNC" + cleanup_wide.substr(1);
    else cleanup_wide = L"\\\\?\\" + cleanup_wide;
  }
  cleanup_root = std::filesystem::path(cleanup_wide);
#endif
  std::filesystem::remove_all(cleanup_root, ec);
  check(!ec);
  std::cout << "stdlib file: " << checks << " checks passed\n";
}
