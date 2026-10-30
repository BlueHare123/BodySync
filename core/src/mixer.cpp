#include "bodysync/mixer.h"

namespace bs {
using Pri = RoleAssign::Priority;
static bool samePose(const Pose& a, const Pose& b) {
  return a.p.x == b.p.x && a.p.y == b.p.y && a.p.z == b.p.z && a.q.w == b.q.w && a.q.x == b.q.x && a.q.y == b.q.y && a.q.z == b.q.z;
}
void Mixer::gate(double dt, SolverInput& in, bool hmdOk) {
  for (int r = 0; r < kRoleCount; ++r) {
    RoleState& s = st_[r]; Priv& p = priv_[r]; const RoleAssign& ra = cfg_.roles[r];
    DeviceSample raw = in.phys[r]; in.phys[r] = DeviceSample{};
    bool assigned = cfg_.enabled && !ra.serial.empty() && ra.priority != Pri::VirtualOnly;
    if (!assigned) { s = RoleState{}; p = Priv{}; continue; }
    s.assigned = true;
    bool ok = raw.valid && raw.pose.isFinite() && raw.quality >= 0.5; s.reason.clear();
    if (!raw.valid) s.reason = "no pose"; else if (!ok) s.reason = "poor quality / invalid";
    if (ok && p.haveLast) {
      if (distance(raw.pose.p, p.last.p) / std::max(dt, 1e-3) > cfg_.jumpSpeedMps) { ok = false; s.reason = "jump"; }
      s.sameT = samePose(raw.pose, p.last) ? s.sameT + dt : 0;
      if (s.sameT > cfg_.staleSeconds) { ok = false; s.reason = "stale (frozen)"; }
    }
    if (raw.valid && raw.pose.isFinite()) { p.last = raw.pose; p.haveLast = true; }
    if (ok) { p.lastGood = raw.pose; p.haveGood = true; s.goodT += dt; s.badT = 0; } else { s.badT += dt; s.goodT = 0; }
    if (!s.healthy && s.goodT >= 0.2) s.healthy = true;                 // debounce recovery
    if (s.healthy && s.badT >= cfg_.lossTimeoutSec) s.healthy = false;  // debounce loss
    s.usable = s.healthy && p.haveGood && (ra.priority != Pri::VirtualPreferred || !hmdOk);
    if (s.usable) { in.phys[r].valid = true; in.phys[r].pose = p.lastGood; in.phys[r].quality = 1; }   // holds the last good pose during short dropouts
  }
}

void Mixer::blend(double dt, SolverOutput& out) {
  double blendSec = std::max(cfg_.blendSeconds, 0.05);
  for (int r = 0; r < kRoleCount; ++r) {
    RoleState& s = st_[r]; Priv& p = priv_[r]; TrackerOut& o = out.tr[r]; const RoleAssign& ra = cfg_.roles[r];
    if (!s.assigned) { o.physWeight = 0; if (o.valid) { p.lastOut = o.pose; p.haveOut = true; } continue; }
    if (s.usable != p.prevUsable) { p.from = p.lastOut; s.progress = p.haveOut ? 0 : 1; p.prevUsable = s.usable; }
    s.progress = std::min(1.0, s.progress + dt / blendSec);
    if (!o.valid && s.usable && ra.priority == Pri::VirtualPreferred && p.haveGood) { o.pose = p.lastGood; o.valid = true; o.src = Source::Physical; o.confidence = 1; }   // HMD lost: relay physical
    if (o.valid) {
      double k = smoothstep(s.progress);
      if (s.progress < 1 && p.haveOut) o.pose = lerpPose(p.from, o.pose, k);
      o.physWeight = s.progress < 1 ? (s.usable ? k : 1 - k) : (s.usable ? 1 : 0);
      o.src = s.progress < 1 ? Source::Blend : (s.usable ? Source::Physical : Source::Virtual);
      p.lastOut = o.pose; p.haveOut = true;
    }
    s.physInCharge = s.usable && s.progress >= 1;
    // a physical tracker already serves this role -> hide the virtual twin (no conflicting devices for one role)
    if (ra.priority == Pri::PhysicalOnly || (ra.priority == Pri::PhysicalPreferred && s.physInCharge)) { o.valid = false; o.active = false; }
  }
}
}  // namespace bs
