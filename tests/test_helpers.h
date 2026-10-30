#pragma once
#include <vector>
#include "bodysync/calibration.h"
#include "bodysync/mixer.h"
#include "bodysync/sim.h"
#include "test_framework.h"

struct Run {
  std::vector<bs::SolverOutput> out; double hz = 90;
  const bs::SolverOutput& at(double t) const { size_t i = (size_t)std::min<double>(std::max(0.0, t * hz), (double)out.size() - 1); return out[i]; }
};
// Runs a scenario through Solver + Mixer exactly as the engine does. Optional per-frame hook may mutate the input.
inline Run runScenario(bs::Scenario sc, const bs::Profile& prof, double hz = 90, double eye = 1.62) {
  bs::Solver solver; solver.configure(prof);
  bs::Mixer mixer; mixer.configure(prof.mixed);
  bs::SimSource sim(eye); Run r; r.hz = hz;
  double dur = bs::scenarioDuration(sc), dt = 1.0 / hz; bool hmdOk = true;
  for (double t = 0; t < dur; t += dt) {
    bs::SolverInput in = sim.sample(sc, t); in.t = t;
    mixer.gate(dt, in, hmdOk);
    bs::SolverOutput o = solver.update(in);
    mixer.blend(dt, o);
    hmdOk = o.hmdOk; r.out.push_back(o);
  }
  return r;
}
inline bool finitePose(const bs::Pose& p) { return p.isFinite() && std::fabs(p.q.norm() - 1) < 1e-6; }
inline bs::Profile defaultProfile() { bs::Profile p; p.body.heightM = 1.62 / 0.93; p.calibration.valid = true; p.calibration.eyeHeightM = 1.62; return p; }
