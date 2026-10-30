// Deterministic pose scenarios for offline diagnostics. Output is SIMULATION and must always be labelled as such.
#pragma once
#include "solver.h"

namespace bs {
enum class Scenario { Neutral, StepLeft, StepRight, TurnInPlace, Squat, Sit, LieBack, LieSide, WalkInPlace, Walk, LoseController, LoseTracker, Recovery, SuddenMove, InvalidInput };
constexpr int kScenarioCount = 15;
const char* scenarioName(Scenario s);
const char* scenarioNameRu(Scenario s);
double scenarioDuration(Scenario s);

class SimSource {
 public:
  explicit SimSource(double eyeHeight = 1.62) : eye_(eyeHeight) {}
  // local time t in [0, duration]; physical left-foot tracker is provided for the LoseTracker scenario (serial "SIM-LeftFoot")
  SolverInput sample(Scenario s, double t) const;
 private:
  double eye_;
};
}  // namespace bs
