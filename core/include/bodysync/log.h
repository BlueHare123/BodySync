// Asynchronous logger: producers only push to a queue (never touch the disk); a writer thread appends to a size-rotated file.
#pragma once
#include <cstdarg>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <string>
#include <vector>

namespace bs {
enum class LogLevel { Debug, Info, Warn, Error };
struct LogEntry { uint64_t timeMs; LogLevel level; std::string text; };
class Logger {
 public:
  static Logger& instance();
  void start(const std::filesystem::path& file, size_t maxBytes = 1 << 20, int keep = 3);   // idempotent
  void stop();                                                                             // flushes
  void log(LogLevel l, std::string text);
  std::vector<LogEntry> tail(size_t n = 500) const;          // newest last (for the UI / diagnostic report)
  int count(LogLevel l) const;
  std::filesystem::path file() const;
 private:
  Logger(); ~Logger();
  struct Impl; Impl* d_;
};
void logf(LogLevel l, const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;
const char* levelName(LogLevel l);
}  // namespace bs
