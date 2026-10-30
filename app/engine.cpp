#include "engine.h"
#include <windows.h>
#include <timeapi.h>
#include <algorithm>
#include <chrono>
#include "bodysync/log.h"

namespace bs {
namespace {
double nowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
}

struct Engine::Impl {
  // shared state (guarded by m)
  mutable std::mutex m; Profile profile; bool profileDirty = true;
  std::atomic<int> source{(int)InputSource::Live}; std::atomic<bool> publish{true}, publishSim{false};
  Scenario scen = Scenario::Neutral; bool simLoop = true, simRunning = true; bool simRestart = true;
  Snapshot snap; Capture cap; double capDur = 0, capT0 = 0; bool capActive = false, capDone = false;
  // thread-owned
  std::thread th; std::atomic<bool> stopFlag{false}; std::unique_ptr<PipeClient> pipe;
  Solver solver; Mixer mixer; VrSource vr; SimSource sim{1.62}; double simT = 0, lastT = 0, lastSnap = 0, hzEma = 0, maxDt = 0, maxWinStart = 0, solveAvg = 0; uint64_t ticks = 0;
  bool hmdOk = false, wasPublishing = false, haveT = false; Profile cur; uint32_t lastMask = 0xFFFFFFFF;
  VrFrame frame; SolverInput in;
};

Engine::Engine() : d_(new Impl) {}
Engine::~Engine() { stop(); }
void Engine::setProfile(const Profile& p) { std::lock_guard<std::mutex> lk(d_->m); d_->profile = p; sanitize(d_->profile); d_->profileDirty = true; }
void Engine::setSource(InputSource s) { d_->source = (int)s; std::lock_guard<std::mutex> lk(d_->m); d_->simRestart = true; }
void Engine::setScenario(Scenario s, bool loop) { std::lock_guard<std::mutex> lk(d_->m); d_->scen = s; d_->simLoop = loop; d_->simRestart = true; d_->simRunning = true; }
void Engine::setSimRunning(bool on) { std::lock_guard<std::mutex> lk(d_->m); d_->simRunning = on; }
void Engine::setPublish(bool on) { d_->publish = on; }
void Engine::setPublishSimulation(bool on) { d_->publishSim = on; }
void Engine::startCapture(CaptureKind k, double sec) { std::lock_guard<std::mutex> lk(d_->m); d_->cap = {}; d_->cap.kind = k; d_->capDur = std::max(0.5, sec); d_->capT0 = -1; d_->capActive = true; d_->capDone = false; }
void Engine::cancelCapture() { std::lock_guard<std::mutex> lk(d_->m); d_->capActive = d_->capDone = false; d_->cap = {}; }
bool Engine::takeCapture(Capture& out) { std::lock_guard<std::mutex> lk(d_->m); if (!d_->capDone) return false; out = std::move(d_->cap); d_->cap = {}; d_->capDone = false; return true; }
Snapshot Engine::snapshot() const {
  std::lock_guard<std::mutex> lk(d_->m); Snapshot s = d_->snap; s.ipc = d_->pipe ? d_->pipe->info() : PipeClient::Info{};
  s.capKind = d_->capActive || d_->capDone ? d_->cap.kind : CaptureKind::None; s.capDone = d_->capDone;
  s.capProgress = d_->capDone ? 1.0 : d_->capActive && d_->capT0 >= 0 ? std::min(1.0, (nowS() - d_->capT0) / d_->capDur) : 0.0;
  return s;
}

void Engine::start(const std::string& pipeName) {
  if (d_->th.joinable()) return;
  d_->pipe = std::make_unique<PipeClient>(pipeName); d_->pipe->start(); d_->stopFlag = false;
  d_->th = std::thread([this] { run(); });
}
void Engine::stop() {
  if (!d_->th.joinable()) return;
  d_->stopFlag = true; d_->th.join();
  if (d_->pipe) { d_->pipe->stop(); }      // driver sees the disconnect and makes the trackers disappear
}

void Engine::run() {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
  HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
  bool fine = timer != nullptr; if (!fine) { timer = CreateWaitableTimerW(nullptr, FALSE, nullptr); timeBeginPeriod(1); }   // shortcut: pre-Win10-1803 fallback has ~1 ms granularity
  double next = nowS();
  while (!d_->stopFlag) {
    int hz; { std::lock_guard<std::mutex> lk(d_->m); hz = d_->profile.tracking.updateRateHz; } hz = std::clamp(hz, 30, 240);
    double now = nowS(), dt = d_->haveT ? std::clamp(now - d_->lastT, 0.001, 0.1) : 1.0 / hz; d_->haveT = true; d_->lastT = now;
    tick(now, dt);
    next += 1.0 / hz; double wait = next - nowS();
    if (wait < -0.05) next = nowS();                       // fell behind: do not burst
    else if (wait > 0) { LARGE_INTEGER due; due.QuadPart = -(LONGLONG)(wait * 1e7); SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE); WaitForSingleObject(timer, (DWORD)(wait * 1000) + 20); }
  }
  CloseHandle(timer); if (!fine) timeEndPeriod(1);
}

void Engine::tick(double now, double dt) {
  Impl& d = *d_;
  // ---- adopt a new profile ----
  bool simMode = d.source == (int)InputSource::Simulation; Scenario scen; bool loop, running, restart;
  { std::lock_guard<std::mutex> lk(d.m);
    if (d.profileDirty) { d.cur = d.profile; d.profileDirty = false; d.solver.configure(d.cur); d.sim = SimSource(Proportions::from(d.cur.body).eyeHeight); }
    scen = d.scen; loop = d.simLoop; running = d.simRunning; restart = d.simRestart; d.simRestart = false; }
  MixedSettings mixed = d.cur.mixed;
  if (restart) { d.simT = 0; d.solver.reset(); d.mixer.reset(); d.haveT = false; }
  // ---- inputs ----
  SolverInput& in = d.in; in = {}; in.t = now;
  bool vrUp = false;
  if (simMode) {                                            // simulation never touches real devices
    for (auto& r : mixed.roles) r.serial.clear();
    if (scen == Scenario::LoseTracker) mixed.roles[kLeftFoot].serial = "SIM-LeftFoot";
    double dur = scenarioDuration(scen);
    if (running) { d.simT += dt; if (d.simT > dur) { if (loop) d.simT = 0; else { d.simT = dur; running = false; std::lock_guard<std::mutex> lk(d.m); d.simRunning = false; } } }
    in = d.sim.sample(scen, std::min(d.simT, dur)); in.t = now; d.frame = {};
  } else {
    vrUp = d.vr.poll(d.frame);
    if (vrUp) {
      in.hmd = d.frame.hmd; in.lc = d.frame.lc; in.rc = d.frame.rc;
      for (int r = 0; r < kRoleCount && mixed.enabled; ++r) {
        const std::string& ser = mixed.roles[r].serial; if (ser.empty()) continue;
        for (auto& t : d.frame.trackers) if (t.serial == ser) { in.phys[r].pose = t.pose; in.phys[r].valid = t.poseValid; in.phys[r].quality = t.quality; break; }
      }
    }
  }
  if (!mixed.enabled) for (auto& s : in.phys) s.valid = false;
  // ---- capture (raw inputs) ----
  bool capActive; CaptureKind capKind; { std::lock_guard<std::mutex> lk(d.m); capActive = d.capActive; capKind = d.cap.kind; if (capActive && d.capT0 < 0) d.capT0 = now; }
  // ---- solve ----
  d.mixer.configure(mixed);
  auto t0 = std::chrono::steady_clock::now();
  d.mixer.gate(dt, in, d.hmdOk);
  SolverOutput out = d.solver.update(in);
  d.mixer.blend(dt, out);
  d.hmdOk = out.hmdOk;
  d.solveAvg += (std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() - d.solveAvg) * 0.02;
  if (capActive) {
    std::lock_guard<std::mutex> lk(d.m);
    if (d.capActive) {
      CalibSample cs; cs.hmd = in.hmd.pose; cs.lc = in.lc.pose; cs.rc = in.rc.pose; cs.hmdOk = in.hmd.valid; cs.lcOk = in.lc.valid; cs.rcOk = in.rc.valid;
      d.cap.samples.push_back(cs); if (capKind == CaptureKind::Verify && d.cap.outs.size() < 1200) d.cap.outs.push_back(out);
      if (now - d.capT0 >= d.capDur) { d.capActive = false; d.capDone = true; }
    }
  }
  // ---- publish ----
  bool want = d.publish && (!simMode || d.publishSim);
  if (d.pipe && want) {
    PoseFrame f; f.timestampUs = (uint64_t)(now * 1e6); f.timeOffsetSec = (float)(d.cur.tracking.poseTimeOffsetMs / 1000.0);
    for (int r = 0; r < kRoleCount; ++r) {
      const TrackerOut& o = out.tr[r]; Pose8 p; p.id = (uint8_t)r; p.active = o.active && d.cur.tracking.enabled[r]; p.valid = o.valid && p.active;
      p.pos[0] = o.pose.p.x; p.pos[1] = o.pose.p.y; p.pos[2] = o.pose.p.z; p.rot[0] = o.pose.q.w; p.rot[1] = o.pose.q.x; p.rot[2] = o.pose.q.y; p.rot[3] = o.pose.q.z;
      p.vel[0] = (float)o.vel.x; p.vel[1] = (float)o.vel.y; p.vel[2] = (float)o.vel.z; p.angVel[0] = (float)o.angVel.x; p.angVel[1] = (float)o.angVel.y; p.angVel[2] = (float)o.angVel.z;
      f.poses.push_back(p);
    }
    d.pipe->submitPoses(std::move(f)); d.wasPublishing = true;
    uint32_t mask = 0; for (int r = 0; r < kRoleCount; ++r) if (d.cur.tracking.enabled[r]) mask |= 1u << r;
    if (mask != d.lastMask) { d.pipe->submitConfig(mask); d.lastMask = mask; }
  } else if (d.pipe && d.wasPublishing) {                 // publishing was switched off: make the trackers disappear once
    PoseFrame f; f.timestampUs = (uint64_t)(now * 1e6); for (int r = 0; r < kRoleCount; ++r) { Pose8 p; p.id = (uint8_t)r; f.poses.push_back(p); }
    d.pipe->submitPoses(std::move(f)); d.wasPublishing = false;
  }
  // ---- timing + snapshot ----
  d.maxDt = std::max(d.maxDt, dt); if (now - d.maxWinStart > 2.0) { d.maxWinStart = now; d.snap.loopMaxMs = d.maxDt * 1000; d.maxDt = 0; }
  d.hzEma += (1.0 / dt - d.hzEma) * 0.05; ++d.ticks;
  if (now - d.lastSnap >= 1.0 / 30) {
    d.lastSnap = now; std::lock_guard<std::mutex> lk(d.m);
    Snapshot& s = d.snap; s.out = out; for (int r = 0; r < kRoleCount; ++r) s.mixer[r] = d.mixer.role(r);
    s.source = (InputSource)d.source.load(); s.scenario = scen; s.simT = d.simT; s.simDuration = scenarioDuration(scen); s.simRunning = running;
    s.vr = d.vr.state(); s.vrStatus = d.vr.status(); s.trackers = d.frame.trackers; s.hmd = in.hmd; s.lc = in.lc; s.rc = in.rc;
    s.publishing = want; s.loopHz = d.hzEma; s.solveUsAvg = d.solveAvg; s.ticks = d.ticks;
  }
}
}  // namespace bs
