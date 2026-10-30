// Unicode-safe file helpers. On Windows std::ofstream/getenv go through the ANSI code page and break on non-Latin user names
// (C:\Users\Иван\...), so everything here uses the wide CRT/WinAPI. Files are always closed before returning.
#pragma once
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

namespace bs::fsutil {
inline FILE* openFile(const std::filesystem::path& p, const char* mode) {
#ifdef _WIN32
  std::wstring m(mode, mode + std::char_traits<char>::length(mode));
  return _wfopen(p.c_str(), m.c_str());
#else
  return std::fopen(p.c_str(), mode);
#endif
}
inline std::optional<std::string> readFile(const std::filesystem::path& p) {
  FILE* f = openFile(p, "rb"); if (!f) return std::nullopt;
  std::string s; char buf[8192]; size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
  std::fclose(f); return s;
}
inline bool writeFile(const std::filesystem::path& p, const std::string& text, bool append = false) {
  FILE* f = openFile(p, append ? "ab" : "wb"); if (!f) return false;
  bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size(); ok = std::fflush(f) == 0 && ok;
  return std::fclose(f) == 0 && ok;
}
// environment variable as a path (wide on Windows)
inline std::optional<std::filesystem::path> envPath(const char* name) {
#ifdef _WIN32
  std::wstring w(name, name + std::char_traits<char>::length(name));
  if (const wchar_t* v = _wgetenv(w.c_str())) if (*v) return std::filesystem::path(v);
#else
  if (const char* v = std::getenv(name)) if (*v) return std::filesystem::path(v);
#endif
  return std::nullopt;
}
inline std::filesystem::path fromUtf8(const std::string& s) { return std::filesystem::path(std::u8string(s.begin(), s.end())); }
inline std::string toUtf8(const std::filesystem::path& p) { auto u = p.u8string(); return std::string(u.begin(), u.end()); }
}  // namespace bs::fsutil
