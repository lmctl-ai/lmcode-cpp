#pragma once
// Test-only cleanup independent of the file catalog under test. MinGW's
// recursive filesystem removal can stall on extended-length directory paths.
#include <filesystem>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
namespace fixture_fs {
#ifdef _WIN32
inline bool remove_native(const std::wstring& path, std::error_code& error) {
  const auto attributes = GetFileAttributesW(path.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    const auto code = GetLastError();
    if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND) return true;
    error = std::error_code(static_cast<int>(code), std::system_category()); return false;
  }
  if ((attributes & FILE_ATTRIBUTE_DIRECTORY) && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
    WIN32_FIND_DATAW item;
    HANDLE search = FindFirstFileW((path + L"\\*").c_str(), &item);
    if (search == INVALID_HANDLE_VALUE) {
      const auto code = GetLastError();
      if (code != ERROR_FILE_NOT_FOUND) { error = std::error_code(static_cast<int>(code), std::system_category()); return false; }
    } else {
      bool ok = true;
      do {
        const std::wstring name(item.cFileName);
        if (name != L"." && name != L".." && !remove_native(path + L"\\" + name, error)) { ok = false; break; }
      } while (FindNextFileW(search, &item));
      const auto code = GetLastError();
      FindClose(search);
      if (!ok) return false;
      if (code != ERROR_NO_MORE_FILES) { error = std::error_code(static_cast<int>(code), std::system_category()); return false; }
    }
  }
  const bool removed = (attributes & FILE_ATTRIBUTE_DIRECTORY) ? RemoveDirectoryW(path.c_str()) : DeleteFileW(path.c_str());
  if (!removed) error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
  return removed;
}
#endif
inline void remove_tree(const std::filesystem::path& path, std::error_code& error) {
  error.clear();
#ifdef _WIN32
  auto native = std::filesystem::absolute(path, error).make_preferred().native();
  if (error) return;
  if (native.rfind(LR"(\\?\)", 0) != 0) {
    if (native.rfind(LR"(\\)", 0) == 0) native = LR"(\\?\UNC\)" + native.substr(2);
    else native = LR"(\\?\)" + native;
  }
  remove_native(native, error);
#else
  std::filesystem::remove_all(path, error);
#endif
}
} // namespace fixture_fs
