// Tracking engine: one real-time thread (input -> mixer gate -> solver -> mixer blend -> driver pipe) plus a snapshot for the UI.
#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include "bodysync/calibration.h"
#include "bodysync/ipc_pipe.h"
#include "bodysync/mixer.h"
#include "bodysync/sim.h"
#include "vr_source.h"

namespace bs {
using ipc::PipeClient; using ipc::PoseFrame; using ipc::Pose8; using ipc::kPipeName;
enum class InputSource { Live, Simulation };
enum class CaptureKind { None, Floor, Stand, TPose, Verify };
struct Capture { CaptureKind kind = CaptureKind::None; std::vector<CalibSample> samples; std::vector<SolverOutput> outs; double floorY = 0; bool floorOk = false; };

struct Snapshot {
  SolverOutput out; std::array<Mixer::RoleState, kRoleCount> mixer{};
  InputSource source = InputSource::Live; Scenario scenario = Scenario::Neutral; double simT = 0, simDuration = 0; bool simRunning = false;
  VrState vr = VrState::NotRunning; std::string vrStatus; std::vector<PhysDevice> trackers;
  DeviceSample hmd, lc, rc;
  PipeClient::Info ipc; bool publishing = false;
  double loopHz = 0, loopMaxMs = 0, solveUsAvg = 0; uint64_t ticks = 0;
  CaptureKind capKind = CaptureKind::None; double capProgress = 0; bool capDone = false;
};

class Engine {
 public:
  Engine(); ~Engine();
  void start(const std::string& pipeName = kPipeName);   // pipe name is injectable for tests
  void stop();
  void setProfile(const Profile& p);                      // thread-safe, applied on the next tick
  void setSource(InputSource s);
  void setScenario(Scenario s, bool loop);                // restarts the scenario
  void setSimRunning(bool on);
  void setPublish(bool on);                                // user switch "send to SteamVR"
  void setPublishSimulation(bool on);                      // allow simulated poses to reach SteamVR (testing only)
  void startCapture(CaptureKind k, double seconds);
  void cancelCapture();
  bool takeCapture(Capture& out);                          // true once when a capture has finished
  Snapshot snapshot() const;
 private:
  void run(); void tick(double now, double dt);
  struct Impl; std::unique_ptr<Impl> d_;
};
}  // namespace bs
