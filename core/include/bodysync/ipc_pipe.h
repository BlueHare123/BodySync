// Windows named-pipe transport for the BodySync <-> driver protocol.
// Overlapped I/O everywhere: every wait has a timeout and observes a stop event, so threads always shut down cleanly.
#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include "ipc_protocol.h"

namespace bs::ipc {
struct ServerCallbacks {
  std::function<void()> onConnected, onDisconnected;
  std::function<void(const PoseFrame&)> onPoses;
  std::function<void(uint32_t)> onConfig;
  std::function<StatusMsg()> getStatus;
  std::function<void(const std::string&)> log;
  uint32_t driverVersion = 1, trackerCount = 8, enabledMask = 0xFF; std::string runtime = "SteamVR";
};
class PipeServer {
 public:
  PipeServer(std::string name, ServerCallbacks cb);
  ~PipeServer();
  void start();
  void stop();
  bool connected() const { return connected_; }
  uint64_t badMessages() const { return bad_; }
  uint64_t sessions() const { return sessions_; }
 private:
  void run();
  std::string name_; ServerCallbacks cb_; std::thread th_; void* stopEvt_ = nullptr;
  std::atomic<bool> connected_{false}; std::atomic<uint64_t> bad_{0}, sessions_{0};
};

class PipeClient {
 public:
  struct Info {
    bool connected = false; HelloAckMsg ack{}; StatusMsg status{}; uint64_t statusAgeMs = 0;
    uint64_t framesSent = 0, framesDropped = 0, reconnects = 0, bytesSent = 0; std::string lastError;
  };
  explicit PipeClient(std::string name = kPipeName);
  ~PipeClient();
  void start();
  void stop();
  void submitPoses(PoseFrame f);        // non-blocking; latest frame wins
  void submitConfig(uint32_t enabledMask);
  Info info() const;
 private:
  void run();
  std::string name_; std::thread th_; void* stopEvt_ = nullptr; void* frameEvt_ = nullptr;
  mutable std::mutex m_; PoseFrame latest_; bool hasFrame_ = false; uint32_t cfgMask_ = 0xFF; bool cfgDirty_ = true; Info info_; uint64_t lastStatusTick_ = 0;
};
}  // namespace bs::ipc
