#include "bodysync/log.h"
#include "bodysync/fsutil.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

namespace fs = std::filesystem;
namespace bs {
static uint64_t nowMs() { return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
const char* levelName(LogLevel l) { static const char* n[] = {"DEBUG", "INFO", "WARN", "ERROR"}; return n[(int)l]; }

struct Logger::Impl {
  mutable std::mutex m; std::condition_variable cv;
  std::deque<LogEntry> pending, ring; fs::path path; size_t maxBytes = 1 << 20; int keep = 3;
  std::thread th; bool running = false, quit = false; std::atomic<int> counts[4] = {0, 0, 0, 0};
  void rotate() {
    std::error_code ec;
    if (!fs::exists(path, ec) || fs::file_size(path, ec) < maxBytes) return;
    for (int i = keep; i >= 1; --i) {
      fs::path from = i == 1 ? path : fs::path(path.string() + "." + std::to_string(i - 1)), to = path.string() + "." + std::to_string(i);
      if (fs::exists(from, ec)) { fs::remove(to, ec); fs::rename(from, to, ec); }
    }
  }
  void writer() {
    std::unique_lock<std::mutex> lk(m);
    while (true) {
      cv.wait_for(lk, std::chrono::milliseconds(500), [&] { return quit || !pending.empty(); });
      std::deque<LogEntry> batch; batch.swap(pending); fs::path p = path; bool q = quit;
      lk.unlock();
      if (!batch.empty() && !p.empty()) {
        std::error_code ec; if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
        rotate();
        std::string text;
        for (auto& e : batch) {
          std::time_t t = (std::time_t)(e.timeMs / 1000); char ts[32]; std::strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", std::localtime(&t));
          char ms[8]; std::snprintf(ms, sizeof ms, ".%03d", (int)(e.timeMs % 1000));
          text += std::string(ts) + ms + " [" + levelName(e.level) + "] " + e.text + "\n";
        }
        fsutil::writeFile(p, text, true);
      }
      lk.lock();
      if (q && pending.empty()) return;
    }
  }
};
Logger::Logger() : d_(new Impl) {}
Logger::~Logger() { stop(); delete d_; }
Logger& Logger::instance() { static Logger l; return l; }
void Logger::start(const fs::path& file, size_t maxBytes, int keep) {
  std::lock_guard<std::mutex> lk(d_->m);
  d_->path = file; d_->maxBytes = maxBytes; d_->keep = keep;
  if (d_->running) return;
  d_->running = true; d_->quit = false; d_->th = std::thread([this] { d_->writer(); });
}
void Logger::stop() {
  { std::lock_guard<std::mutex> lk(d_->m); if (!d_->running) return; d_->quit = true; }
  d_->cv.notify_all(); if (d_->th.joinable()) d_->th.join();
  std::lock_guard<std::mutex> lk(d_->m); d_->running = false;
}
void Logger::log(LogLevel l, std::string text) {
  d_->counts[(int)l]++;
  LogEntry e{nowMs(), l, std::move(text)};
  { std::lock_guard<std::mutex> lk(d_->m);
    d_->ring.push_back(e); if (d_->ring.size() > 2000) d_->ring.pop_front();   // bounded memory
    if (d_->running) { if (d_->pending.size() < 5000) d_->pending.push_back(std::move(e)); } }   // bounded queue: drop instead of growing
  d_->cv.notify_one();
}
std::vector<LogEntry> Logger::tail(size_t n) const {
  std::lock_guard<std::mutex> lk(d_->m); size_t s = d_->ring.size() > n ? d_->ring.size() - n : 0;
  return std::vector<LogEntry>(d_->ring.begin() + (long)s, d_->ring.end());
}
int Logger::count(LogLevel l) const { return d_->counts[(int)l]; }
fs::path Logger::file() const { std::lock_guard<std::mutex> lk(d_->m); return d_->path; }
void logf(LogLevel l, const char* fmt, ...) {
  char buf[1024]; va_list ap; va_start(ap, fmt); std::vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
  Logger::instance().log(l, buf);
}
}  // namespace bs
