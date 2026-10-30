// Calibration math (pure functions over recorded samples) used by the wizard.
#pragma once
#include <string>
#include <vector>
#include "config.h"
#include "solver.h"

namespace bs {
struct CalibSample { Pose hmd, lc, rc; bool hmdOk = false, lcOk = false, rcOk = false; };
struct HeightResult { bool ok = false; double eyeHeight = 0, height = 0; std::string msg; };
struct TPoseResult { bool ok = false; double span = 0, armLen = 0, armScale = 1, torsoYawBiasDeg = 0; std::string msg; };
struct NeutralResult { bool ok = false; double headPitchOffsetDeg = 0; std::string msg; };
struct VerifyReport { bool pass = false; double maxSegErrM = 0, asymmetryM = 0, jitterRmsM = 0; std::vector<std::string> messages; };

constexpr double kEyeHeightRatio = 0.93;   // eye height / stature
double median(std::vector<double> v);
HeightResult calibrateHeight(const std::vector<CalibSample>& s, double floorY);
TPoseResult calibrateTPose(const std::vector<CalibSample>& s, const BodyProfile& body);
NeutralResult calibrateNeutral(const std::vector<CalibSample>& s);
VerifyReport verifySkeleton(const std::vector<SolverOutput>& outs);
void apply(Profile& p, const HeightResult& r);
void apply(Profile& p, const TPoseResult& r);
void apply(Profile& p, const NeutralResult& r);
std::string timestampNow();
}  // namespace bs
