#ifdef _WIN32
#include "bodysync/ipc_pipe.h"
#include <windows.h>
#include <sddl.h>
#include <string>

namespace bs::ipc {
static std::wstring widen(const std::string& s) { int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0); std::wstring w(n > 0 ? n - 1 : 0, L'\0'); if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n); return w; }

// One overlapped duplex pipe handle plus message framing.
class Conn {
 public:
  enum class Pump { Ok, Closed, Stop, Corrupt };
  Conn(HANDLE h, HANDLE stop) : h_(h), stop_(stop) { rd_.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr); wr_.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr); }
  ~Conn() {
    if (pending_) { CancelIoEx(h_, &rd_); DWORD n; GetOverlappedResult(h_, &rd_, &n, TRUE); }
    CloseHandle(rd_.hEvent); CloseHandle(wr_.hEvent); CloseHandle(h_);
  }
  Pump pump(DWORD timeoutMs, std::vector<Message>& out, HANDLE extra = nullptr, bool* extraSignaled = nullptr) {
    if (extraSignaled) *extraSignaled = false;
    if (!pending_) {
      ResetEvent(rd_.hEvent);
      if (!ReadFile(h_, tmp_, sizeof tmp_, nullptr, &rd_) && GetLastError() != ERROR_IO_PENDING) return Pump::Closed;
      pending_ = true;
    }
    HANDLE hs[3] = {rd_.hEvent, stop_, extra}; DWORD w = WaitForMultipleObjects(extra ? 3 : 2, hs, FALSE, timeoutMs);
    if (w == WAIT_OBJECT_0 + 1) return Pump::Stop;
    if (extra && w == WAIT_OBJECT_0 + 2 && extraSignaled) *extraSignaled = true;
    if (w == WAIT_OBJECT_0 || (extra && w == WAIT_OBJECT_0 + 2 && HasOverlappedIoCompleted(&rd_))) {
      DWORD got = 0;
      if (!GetOverlappedResult(h_, &rd_, &got, FALSE)) { pending_ = false; return Pump::Closed; }
      pending_ = false; acc_.insert(acc_.end(), tmp_, tmp_ + got);
      for (;;) {
        Message m; size_t used = 0; Result r = tryExtract(acc_.data(), acc_.size(), m, &used);
        if (r == Result::Incomplete) break;
        if (r != Result::Ok) return Pump::Corrupt;
        acc_.erase(acc_.begin(), acc_.begin() + (long)used); out.push_back(std::move(m));
      }
      if (acc_.size() > 2 * kMaxPayload) return Pump::Corrupt;
    } else if (w == WAIT_FAILED) return Pump::Closed;
    return Pump::Ok;
  }
  bool write(const std::vector<uint8_t>& d, DWORD timeoutMs) {
    ResetEvent(wr_.hEvent); DWORD n = 0;
    if (!WriteFile(h_, d.data(), (DWORD)d.size(), &n, &wr_)) {
      if (GetLastError() != ERROR_IO_PENDING) return false;
      HANDLE hs[2] = {wr_.hEvent, stop_}; DWORD w = WaitForMultipleObjects(2, hs, FALSE, timeoutMs);
      if (w != WAIT_OBJECT_0) { CancelIoEx(h_, &wr_); GetOverlappedResult(h_, &wr_, &n, TRUE); return false; }
      if (!GetOverlappedResult(h_, &wr_, &n, FALSE)) return false;
    }
    return n == d.size();
  }
 private:
  HANDLE h_, stop_; OVERLAPPED rd_{}, wr_{}; bool pending_ = false; uint8_t tmp_[4096]; std::vector<uint8_t> acc_;
};

// DACL: current user + SYSTEM + Administrators only.
static bool makeSecurity(SECURITY_ATTRIBUTES& sa, PSECURITY_DESCRIPTOR& sd) {
  HANDLE tok = nullptr; if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return false;
  DWORD n = 0; GetTokenInformation(tok, TokenUser, nullptr, 0, &n); std::vector<uint8_t> buf(n);
  bool ok = n && GetTokenInformation(tok, TokenUser, buf.data(), n, &n); CloseHandle(tok);
  if (!ok) return false;
  LPWSTR sid = nullptr; if (!ConvertSidToStringSidW(((TOKEN_USER*)buf.data())->User.Sid, &sid)) return false;
  std::wstring sddl = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;" + std::wstring(sid) + L")"; LocalFree(sid);
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &sd, nullptr)) return false;
  sa = {sizeof sa, sd, FALSE}; return true;
}

PipeServer::PipeServer(std::string name, ServerCallbacks cb) : name_(std::move(name)), cb_(std::move(cb)) { stopEvt_ = CreateEventW(nullptr, TRUE, FALSE, nullptr); }
PipeServer::~PipeServer() { stop(); CloseHandle((HANDLE)stopEvt_); }
void PipeServer::start() { if (th_.joinable()) return; ResetEvent((HANDLE)stopEvt_); th_ = std::thread([this] { run(); }); }
void PipeServer::stop() { if (!th_.joinable()) return; SetEvent((HANDLE)stopEvt_); th_.join(); }

void PipeServer::run() {
  HANDLE stop = (HANDLE)stopEvt_; std::wstring wname = widen(name_);
  SECURITY_ATTRIBUTES sa{}; PSECURITY_DESCRIPTOR sd = nullptr; bool haveSa = makeSecurity(sa, sd);
  auto log = [&](const std::string& s) { if (cb_.log) cb_.log(s); };
  while (WaitForSingleObject(stop, 0) != WAIT_OBJECT_0) {
    HANDLE pipe = CreateNamedPipeW(wname.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                   1, 16384, 16384, 0, haveSa ? &sa : nullptr);   // 1 instance: a single BodySync controller at a time
    if (pipe == INVALID_HANDLE_VALUE) { log("CreateNamedPipe failed: " + std::to_string(GetLastError())); if (WaitForSingleObject(stop, 1000) == WAIT_OBJECT_0) { break; } continue; }
    OVERLAPPED ov{}; ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    BOOL c = ConnectNamedPipe(pipe, &ov); DWORD err = GetLastError(); bool gotClient = false;
    if (c || err == ERROR_PIPE_CONNECTED) gotClient = true;
    else if (err == ERROR_IO_PENDING) {
      HANDLE hs[2] = {ov.hEvent, stop}; DWORD w = WaitForMultipleObjects(2, hs, FALSE, INFINITE);
      if (w == WAIT_OBJECT_0) gotClient = true; else { CancelIoEx(pipe, &ov); DWORD n; GetOverlappedResult(pipe, &ov, &n, TRUE); }
    }
    CloseHandle(ov.hEvent);
    if (!gotClient) { CloseHandle(pipe); continue; }
    ++sessions_;
    Conn conn(pipe, stop);                 // owns and closes `pipe`
    std::vector<Message> msgs; bool handshake = false, alive = true; uint32_t seq = 1, badRun = 0;
    ULONGLONG t0 = GetTickCount64(), lastRx = t0, lastStatus = t0;
    while (alive) {
      msgs.clear(); Conn::Pump pr = conn.pump(50, msgs);
      if (pr != Conn::Pump::Ok) { if (pr == Conn::Pump::Corrupt) log("corrupt stream from client, dropping connection"); alive = false; }
      ULONGLONG now = GetTickCount64(); if (!msgs.empty()) lastRx = now;
      for (auto& m : msgs) {
        Type t = (Type)m.h.type;
        if (!handshake) {
          HelloMsg h; if (t != Type::Hello || decodeHello(m, h) != Result::Ok || h.protocol != kVersion) { log("bad handshake"); alive = false; break; }
          if (!conn.write(encodeHelloAck(seq++, cb_.driverVersion, cb_.trackerCount, cb_.enabledMask, cb_.runtime), 200)) { alive = false; break; }
          handshake = true; connected_ = true; log(std::string("client connected: ") + h.name + " pid " + std::to_string(h.pid)); if (cb_.onConnected) cb_.onConnected();
          continue;
        }
        if (t == Type::Poses) { PoseFrame f; if (decodePoses(m, f) == Result::Ok) { badRun = 0; if (cb_.onPoses) cb_.onPoses(f); } else { ++bad_; if (++badRun > 50) { log("too many invalid pose messages"); alive = false; } } }
        else if (t == Type::Config) { ConfigMsg c2; if (decodeConfig(m, c2) == Result::Ok) { if (cb_.onConfig) cb_.onConfig(c2.enabledMask); } else ++bad_; }
        else if (t == Type::Ping) { if (!conn.write(encode(Type::Pong, seq++, nullptr, 0), 100)) alive = false; }
        else if (t == Type::Bye) alive = false;
      }
      if (!handshake && now - t0 > 2000) { log("handshake timeout"); alive = false; }
      if (handshake && now - lastRx > 1500) { log("client timeout"); alive = false; }   // client sends pings at least every 250 ms
      if (handshake && alive && now - lastStatus >= 250) { lastStatus = now; StatusMsg s = cb_.getStatus ? cb_.getStatus() : StatusMsg{}; if (!conn.write(encodeStatus(seq++, s), 100)) alive = false; }
    }
    if (handshake) { connected_ = false; log("client disconnected"); if (cb_.onDisconnected) cb_.onDisconnected(); }
  }
  if (sd) LocalFree(sd);
}

// ---------------------------------------------------------------- client
PipeClient::PipeClient(std::string name) : name_(std::move(name)) { stopEvt_ = CreateEventW(nullptr, TRUE, FALSE, nullptr); frameEvt_ = CreateEventW(nullptr, FALSE, FALSE, nullptr); }
PipeClient::~PipeClient() { stop(); CloseHandle((HANDLE)stopEvt_); CloseHandle((HANDLE)frameEvt_); }
void PipeClient::start() { if (th_.joinable()) return; ResetEvent((HANDLE)stopEvt_); th_ = std::thread([this] { run(); }); }
void PipeClient::stop() { if (!th_.joinable()) return; SetEvent((HANDLE)stopEvt_); th_.join(); }
void PipeClient::submitPoses(PoseFrame f) {
  { std::lock_guard<std::mutex> lk(m_); if (hasFrame_) ++info_.framesDropped; latest_ = std::move(f); hasFrame_ = true; }
  SetEvent((HANDLE)frameEvt_);
}
void PipeClient::submitConfig(uint32_t mask) { std::lock_guard<std::mutex> lk(m_); if (mask != cfgMask_) { cfgMask_ = mask; cfgDirty_ = true; } SetEvent((HANDLE)frameEvt_); }
PipeClient::Info PipeClient::info() const {
  std::lock_guard<std::mutex> lk(m_); Info i = info_;
  i.statusAgeMs = lastStatusTick_ ? GetTickCount64() - lastStatusTick_ : 0; return i;
}
void PipeClient::run() {
  HANDLE stop = (HANDLE)stopEvt_, fe = (HANDLE)frameEvt_; std::wstring wname = widen(name_);
  auto setErr = [&](const std::string& s) { std::lock_guard<std::mutex> lk(m_); info_.lastError = s; };
  while (WaitForSingleObject(stop, 0) != WAIT_OBJECT_0) {
    HANDLE h = CreateFileW(wname.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
      DWORD e = GetLastError();
      setErr(e == ERROR_FILE_NOT_FOUND ? "driver not running (pipe not found)" : e == ERROR_PIPE_BUSY ? "driver busy (another controller connected?)" : "cannot open pipe, error " + std::to_string(e));
      if (WaitForSingleObject(stop, 500) == WAIT_OBJECT_0) { break; }
      continue;
    }
    Conn conn(h, stop); std::vector<Message> msgs; uint32_t seq = 1; bool ok = conn.write(encodeHello(seq++, GetCurrentProcessId(), "BodySync"), 500), acked = false;
    ULONGLONG t0 = GetTickCount64(), lastRx = t0, lastTx = t0; { std::lock_guard<std::mutex> lk(m_); cfgDirty_ = true; }
    while (ok) {
      msgs.clear(); bool frameSig = false; Conn::Pump pr = conn.pump(50, msgs, fe, &frameSig);
      if (pr == Conn::Pump::Stop) { conn.write(encode(Type::Bye, seq++, nullptr, 0), 50); ok = false; break; }
      if (pr != Conn::Pump::Ok) { setErr(pr == Conn::Pump::Corrupt ? "corrupt data from driver" : "driver closed the connection"); ok = false; break; }
      ULONGLONG now = GetTickCount64();
      for (auto& m : msgs) {
        lastRx = now; Type t = (Type)m.h.type;
        if (t == Type::HelloAck) { HelloAckMsg a; if (decodeHelloAck(m, a) == Result::Ok) { acked = true; std::lock_guard<std::mutex> lk(m_); info_.ack = a; info_.connected = true; info_.lastError.clear(); } }
        else if (t == Type::Status) { StatusMsg s; if (decodeStatus(m, s) == Result::Ok) { std::lock_guard<std::mutex> lk(m_); info_.status = s; lastStatusTick_ = now; } }
      }
      if (!acked) { if (now - t0 > 1500) { setErr("driver did not answer the handshake"); ok = false; } continue; }
      if (now - lastRx > 2000) { setErr("driver stopped responding"); ok = false; break; }
      PoseFrame f; bool have = false, cfg = false; uint32_t mask = 0;
      { std::lock_guard<std::mutex> lk(m_); if (hasFrame_) { f = std::move(latest_); hasFrame_ = false; have = true; } if (cfgDirty_) { cfg = true; mask = cfgMask_; cfgDirty_ = false; } }
      if (cfg && !conn.write(encodeConfig(seq++, mask), 50)) { setErr("write failed"); ok = false; break; }
      if (have) {
        auto bytes = encodePoses(seq++, f);
        if (!conn.write(bytes, 30)) { setErr("write timeout"); ok = false; break; }
        lastTx = now; std::lock_guard<std::mutex> lk(m_); ++info_.framesSent; info_.bytesSent += bytes.size();
      } else if (now - lastTx > 250) { if (!conn.write(encode(Type::Ping, seq++, nullptr, 0), 50)) { ok = false; break; } lastTx = now; }
    }
    { std::lock_guard<std::mutex> lk(m_); info_.connected = false; ++info_.reconnects; lastStatusTick_ = 0; info_.status = StatusMsg{}; }
    if (WaitForSingleObject(stop, 300) == WAIT_OBJECT_0) break;
  }
}
}  // namespace bs::ipc
#endif
