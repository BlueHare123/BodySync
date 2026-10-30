// Real named-pipe IPC between threads of this process (server = driver side, client = BodySync side).
#include <windows.h>
#include <chrono>
#include <thread>
#include "bodysync/ipc_pipe.h"
#include "test_framework.h"
using namespace bs::ipc;
using namespace std::chrono_literals;

static std::string pipeName(const char* tag) { return std::string(R"(\\.\pipe\BodySync.Test.)") + tag + "." + std::to_string(GetCurrentProcessId()); }
template <class F> static bool waitFor(F f, int ms = 4000) { auto t0 = std::chrono::steady_clock::now(); while (std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds(ms)) { if (f()) return true; std::this_thread::sleep_for(5ms); } return f(); }
static PoseFrame frame(int n, double x) { PoseFrame f; f.timestampUs = (uint64_t)n; f.timeOffsetSec = -0.005f; for (int i = 0; i < 8; ++i) { Pose8 p; p.id = (uint8_t)i; p.active = p.valid = true; p.pos[0] = x + i; p.pos[1] = 1; p.rot[0] = 1; f.poses.push_back(p); } return f; }

namespace {
struct Rec {
  std::atomic<int> connected{0}, disconnected{0}, poses{0}; std::atomic<uint32_t> mask{0xFF}; std::atomic<uint64_t> lastTs{0}; std::atomic<double> lastX{0};
  ServerCallbacks cb() {
    ServerCallbacks c; c.onConnected = [this] { ++connected; }; c.onDisconnected = [this] { ++disconnected; };
    c.onPoses = [this](const PoseFrame& f) { ++poses; lastTs = f.timestampUs; lastX = f.poses.empty() ? 0 : f.poses[0].pos[0]; };
    c.onConfig = [this](uint32_t m) { mask = m; };
    c.getStatus = [this] { StatusMsg s{}; s.flags = kClientConnected | kDevicesRegistered; s.framesReceived = (uint64_t)poses.load(); s.activeMask = 0xFF; return s; };
    c.trackerCount = 8; c.driverVersion = 7; c.runtime = "TestRuntime"; return c;
  }
};
}

TEST(pipe_handshake_poses_config_status) {
  Rec rec; std::string n = pipeName("a"); PipeServer srv(n, rec.cb()); srv.start(); PipeClient cli(n); cli.start();
  CHECK(waitFor([&] { return cli.info().connected && rec.connected == 1; }));
  CHECK(cli.info().ack.trackerCount == 8 && cli.info().ack.driverVersion == 7 && std::string(cli.info().ack.runtime) == "TestRuntime");
  cli.submitConfig(0x0F); CHECK(waitFor([&] { return rec.mask == 0x0F; }));
  for (int i = 1; i <= 200; ++i) { cli.submitPoses(frame(i, i * 0.001)); std::this_thread::sleep_for(3ms); }
  CHECK(waitFor([&] { return rec.lastTs == 200; }));
  CHECK_MSG(rec.poses > 100, "server got %d poses", rec.poses.load());                 // latest-wins mailbox may drop, never reorders
  CHECK(waitFor([&] { return cli.info().status.framesReceived > 0 && cli.info().status.activeMask == 0xFF; }));   // driver status flows back
  CHECK(cli.info().framesSent > 0 && cli.info().statusAgeMs < 1000);
  cli.stop(); CHECK(waitFor([&] { return rec.disconnected == 1; })); srv.stop();
}
TEST(pipe_client_survives_driver_restart_and_missing_driver) {
  Rec rec; std::string n = pipeName("b"); PipeClient cli(n); cli.start();
  CHECK(waitFor([&] { return cli.info().lastError.find("not running") != std::string::npos; }));   // no driver yet: clear error, no crash
  { PipeServer srv(n, rec.cb()); srv.start(); CHECK(waitFor([&] { return cli.info().connected; })); cli.submitPoses(frame(1, 0)); CHECK(waitFor([&] { return rec.poses > 0; })); }   // server destroyed = "SteamVR quit"
  CHECK(waitFor([&] { return !cli.info().connected; })); CHECK(cli.info().reconnects >= 1);
  { Rec rec2; PipeServer srv2(n, rec2.cb()); srv2.start(); CHECK(waitFor([&] { return cli.info().connected && rec2.connected == 1; })); cli.submitPoses(frame(2, 0)); CHECK(waitFor([&] { return rec2.poses > 0; })); }   // "SteamVR restarted"
  cli.stop();
}
TEST(pipe_server_survives_client_death_and_garbage) {
  Rec rec; std::string n = pipeName("c"); PipeServer srv(n, rec.cb()); srv.start();
  std::wstring wn(n.begin(), n.end());
  auto raw = [&]() { HANDLE h = INVALID_HANDLE_VALUE; for (int i = 0; i < 100 && h == INVALID_HANDLE_VALUE; ++i) { h = CreateFileW(wn.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr); if (h == INVALID_HANDLE_VALUE) std::this_thread::sleep_for(20ms); } return h; };
  { HANDLE h = raw(); CHECK(h != INVALID_HANDLE_VALUE); const char junk[] = "this is definitely not a BodySync handshake, just random bytes........"; DWORD w; WriteFile(h, junk, sizeof junk, &w, nullptr); std::this_thread::sleep_for(150ms); CloseHandle(h); }   // garbage -> dropped, never "connected"
  CHECK(rec.connected == 0);
  { HANDLE h = raw(); CHECK(h != INVALID_HANDLE_VALUE); auto hello = encodeHello(1, 1, "evil"); hello[4] = 77; DWORD w; WriteFile(h, hello.data(), (DWORD)hello.size(), &w, nullptr); std::this_thread::sleep_for(150ms); CloseHandle(h); }   // wrong protocol version
  CHECK(rec.connected == 0);
  { HANDLE h = raw(); CHECK(h != INVALID_HANDLE_VALUE); DWORD w; auto hello = encodeHello(1, 1, "half"); WriteFile(h, hello.data(), (DWORD)hello.size(), &w, nullptr);   // valid handshake then client crashes mid-stream
    std::this_thread::sleep_for(150ms); CHECK(rec.connected == 1); auto pf = encodePoses(2, frame(1, 0)); WriteFile(h, pf.data(), (DWORD)pf.size() / 2, &w, nullptr); CloseHandle(h); }
  CHECK(waitFor([&] { return rec.disconnected == 1; }));
  PipeClient cli(n); cli.start(); CHECK(waitFor([&] { return cli.info().connected; })); cli.submitPoses(frame(5, 1)); CHECK(waitFor([&] { return rec.lastTs == 5; }));   // healthy client still works afterwards
  { HANDLE h = CreateFileW(wn.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr); CHECK(h == INVALID_HANDLE_VALUE);   // second controller is refused while one is attached
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
  cli.stop(); srv.stop();
}
TEST(pipe_bad_pose_messages_are_counted_and_ignored) {
  Rec rec; std::string n = pipeName("d"); PipeServer srv(n, rec.cb()); srv.start(); std::wstring wn(n.begin(), n.end());
  HANDLE h = INVALID_HANDLE_VALUE; for (int i = 0; i < 100 && h == INVALID_HANDLE_VALUE; ++i) { h = CreateFileW(wn.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr); if (h == INVALID_HANDLE_VALUE) std::this_thread::sleep_for(20ms); }
  CHECK(h != INVALID_HANDLE_VALUE); DWORD w; auto hello = encodeHello(1, 1, "raw"); WriteFile(h, hello.data(), (DWORD)hello.size(), &w, nullptr);
  PoseFrame f = frame(1, 0); f.poses[2].pos[1] = NAN; auto bad = encodePoses(2, f); for (int i = 0; i < 5; ++i) WriteFile(h, bad.data(), (DWORD)bad.size(), &w, nullptr);
  auto good = encodePoses(3, frame(9, 3)); WriteFile(h, good.data(), (DWORD)good.size(), &w, nullptr);
  CHECK(waitFor([&] { return rec.lastTs == 9; })); CHECK(srv.badMessages() == 5); CHECK(rec.connected == 1 && rec.disconnected == 0);   // stream stays usable
  CloseHandle(h); srv.stop();
}
TEST(pipe_threads_stop_promptly) {
  Rec rec; std::string n = pipeName("e"); auto t0 = std::chrono::steady_clock::now();
  { PipeServer srv(n, rec.cb()); srv.start(); PipeClient cli(n); cli.start(); waitFor([&] { return cli.info().connected; }); for (int i = 0; i < 50; ++i) cli.submitPoses(frame(i, 0)); }
  CHECK(std::chrono::steady_clock::now() - t0 < 3s);
  { PipeServer idle(n, rec.cb()); idle.start(); }          // destroyed while blocked in ConnectNamedPipe
  { PipeClient idle(n); idle.start(); std::this_thread::sleep_for(50ms); }   // destroyed while retrying
  CHECK(std::chrono::steady_clock::now() - t0 < 6s);
}
