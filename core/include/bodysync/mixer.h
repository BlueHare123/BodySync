// Mixed tracking: decides, per role, whether a physical tracker or the virtual estimate is authoritative,
// with quality checks, loss/recovery debounce and smooth hand-over between the two sources.
#pragma once
#include "solver.h"

namespace bs {
class Mixer {
 public:
  struct RoleState {
    bool assigned = false, healthy = false, usable = false, physInCharge = false;
    double goodT = 0, badT = 0, sameT = 0, progress = 1; std::string reason;
  };
  void configure(const MixedSettings& m) { cfg_ = m; }
  // in.phys[r] holds the raw physical sample for role r (valid=false when absent). Gating rewrites validity for the solver.
  void gate(double dt, SolverInput& in, bool hmdOk);
  void blend(double dt, SolverOutput& out);
  const RoleState& role(int r) const { return st_[r]; }
  void reset() { st_ = {}; priv_ = {}; }
 private:
  struct Priv { Pose last, lastGood, from, lastOut; bool haveLast = false, haveGood = false, prevUsable = false, haveOut = false; };
  MixedSettings cfg_; std::array<RoleState, kRoleCount> st_{}; std::array<Priv, kRoleCount> priv_{};
};
}  // namespace bs
