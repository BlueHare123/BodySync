// Every user-visible setting must change the behaviour of the algorithm ("no decorative switches").
#include <functional>
#include "test_helpers.h"
using namespace bs;

namespace {
struct Case { const char* name; Scenario sc; std::function<void(Profile&)> mod; std::function<void(Profile&)> prep = nullptr; };
double difference(const Run& a, const Run& b) {
  double d = 0; size_t n = std::min(a.out.size(), b.out.size());
  for (size_t i = 0; i < n; ++i) {
    for (int r = 0; r < kRoleCount; ++r) {
      const auto &x = a.out[i].tr[r], &y = b.out[i].tr[r];
      d += distance(x.pose.p, y.pose.p) + angleBetween(x.pose.q, y.pose.q) * 0.1 + (x.valid != y.valid) + (x.active != y.active);
    }
    d += (a.out[i].posture != b.out[i].posture);
  }
  return d;
}
}
TEST(every_setting_changes_the_output) {
  const std::vector<Case> cases = {
    {"tracking.smoothing", Scenario::Walk, [](Profile& p) { p.tracking.smoothing = 1.0; }},
    {"tracking.pelvisSway", Scenario::StepLeft, [](Profile& p) { p.tracking.pelvisSway = 1.0; }},
    {"tracking.torsoYawDeadzoneDeg", Scenario::TurnInPlace, [](Profile& p) { p.tracking.torsoYawDeadzoneDeg = 80; p.tracking.neckTwistDeg = 100; }},
    {"tracking.torsoYawFollowSec", Scenario::TurnInPlace, [](Profile& p) { p.tracking.torsoYawFollowSec = 0.3; }},
    {"tracking.footLock", Scenario::StepLeft, [](Profile& p) { p.tracking.footLock = 0.0; }},
    {"tracking.stepHeightM", Scenario::WalkInPlace, [](Profile& p) { p.tracking.stepHeightM = 0.35; }},
    {"tracking.stepDurationScale", Scenario::StepLeft, [](Profile& p) { p.tracking.stepDurationScale = 2.2; }},
    {"tracking.walkInPlaceSensitivity", Scenario::WalkInPlace, [](Profile& p) { p.tracking.walkInPlaceSensitivity = 0.0; }},
    {"tracking.postureSensitivity", Scenario::Sit, [](Profile& p) { p.tracking.postureSensitivity = 1.0; }},
    {"tracking.kneeMaxBendDeg", Scenario::Squat, [](Profile& p) { p.tracking.kneeMaxBendDeg = 60; }},
    {"tracking.elbowMaxBendDeg", Scenario::Neutral, [](Profile& p) { p.tracking.elbowMaxBendDeg = 10; }},
    {"tracking.spineMaxLeanDeg", Scenario::StepLeft, [](Profile& p) { p.tracking.spineMaxLeanDeg = 10; }},
    {"tracking.neckTwistDeg", Scenario::TurnInPlace, [](Profile& p) { p.tracking.neckTwistDeg = 20; }},
    {"tracking.spineTwistDeg", Scenario::TurnInPlace, [](Profile& p) { p.tracking.spineTwistDeg = 5; }},
    {"tracking.maxSpeedMps", Scenario::Walk, [](Profile& p) { p.tracking.maxSpeedMps = 2.0; }},
    {"tracking.enabled[waist]", Scenario::Neutral, [](Profile& p) { p.tracking.enabled[kWaist] = false; }},
    {"poses.mode", Scenario::Neutral, [](Profile& p) { p.poses.mode = PostureMode::Sitting; }},
    {"poses.sitEnterRatio", Scenario::Sit, [](Profile& p) { p.poses.sitEnterRatio = 0.5; p.poses.sitExitRatio = 0.6; }},
    {"poses.sitExitRatio", Scenario::Sit, [](Profile& p) { p.poses.sitExitRatio = 0.95; }},
    {"poses.crouchRatio", Scenario::Squat, [](Profile& p) { p.poses.crouchRatio = 0.3; }},
    {"poses.lieHeightRatio", Scenario::LieBack, [](Profile& p) { p.poses.lieHeightRatio = 0.05; }},
    {"poses.lieEnterUp", Scenario::LieBack, [](Profile& p) { p.poses.lieEnterUp = 0.0; p.poses.lieExitUp = 0.1; }},
    {"poses.lieExitUp", Scenario::LieBack, [](Profile& p) { p.poses.lieExitUp = 0.99; }, [](Profile& p) { p.poses.lieEnterUp = 0.3; p.poses.lieExitUp = 0.35; }},
    {"poses.sitStyle[reclined]", Scenario::Sit, [](Profile& p) { p.poses.sitStyle = SitStyle::Reclined; }},
    {"poses.sitStyle[upright]", Scenario::Sit, [](Profile& p) { p.poses.sitStyle = SitStyle::Upright; }},
    {"poses.sitStyle[cross-legged]", Scenario::Sit, [](Profile& p) { p.poses.sitStyle = SitStyle::CrossLegged; }},
    {"poses.holdTimeSec", Scenario::Sit, [](Profile& p) { p.poses.holdTimeSec = 3.0; }},
    {"poses.transitionSec", Scenario::Sit, [](Profile& p) { p.poses.transitionSec = 3.0; }},
    {"poses.standLeanDeg", Scenario::Neutral, [](Profile& p) { p.poses.standLeanDeg = 15; }},
    {"poses.sitLeanDeg", Scenario::Sit, [](Profile& p) { p.poses.sitLeanDeg = 20; }},
    {"poses.sitPelvisHeightOffsetM", Scenario::Sit, [](Profile& p) { p.poses.sitPelvisHeightOffsetM = 0.1; }},
    {"poses.sitPelvisBackM", Scenario::Sit, [](Profile& p) { p.poses.sitPelvisBackM = 0.3; }},
    {"poses.sitLegExtend", Scenario::Sit, [](Profile& p) { p.poses.sitLegExtend = 0.3; }},
    {"poses.sitFeetSpreadM", Scenario::Sit, [](Profile& p) { p.poses.sitFeetSpreadM = 0.2; }},
    {"poses.kneeSplayDeg", Scenario::Neutral, [](Profile& p) { p.poses.kneeSplayDeg = 40; }},
    {"poses.lieKneeBendDeg", Scenario::LieBack, [](Profile& p) { p.poses.lieKneeBendDeg = 70; }},
    {"poses.lieSideKneeBendDeg", Scenario::LieSide, [](Profile& p) { p.poses.lieSideKneeBendDeg = 100; }},
    {"poses.lieFloorOffsetM", Scenario::LieBack, [](Profile& p) { p.poses.lieFloorOffsetM = 0.35; }},
    {"calibration.eyeHeightM", Scenario::Sit, [](Profile& p) { p.calibration.eyeHeightM = 1.9; }},
    {"calibration.headPitchOffsetDeg", Scenario::Neutral, [](Profile& p) { p.calibration.headPitchOffsetDeg = 20; }},
    {"calibration.torsoYawBiasDeg", Scenario::Neutral, [](Profile& p) { p.calibration.torsoYawBiasDeg = 25; }},
    {"calibration.yawOffsetDeg", Scenario::Neutral, [](Profile& p) { p.calibration.yawOffsetDeg = 20; }},
    {"calibration.posOffset", Scenario::Neutral, [](Profile& p) { p.calibration.posOffset = {0.1, 0.1, 0.1}; }},
    {"calibration.floorOffsetM", Scenario::Neutral, [](Profile& p) { p.calibration.floorOffsetM = 0.1; }},
    {"calibration.pelvisOffset", Scenario::Neutral, [](Profile& p) { p.calibration.pelvisOffset = {0.05, 0.05, 0.05}; }},
    {"calibration.footLateralM", Scenario::Neutral, [](Profile& p) { p.calibration.footLateralM = 0.15; }},
    {"calibration.footForwardM", Scenario::Neutral, [](Profile& p) { p.calibration.footForwardM = 0.15; }},
    {"calibration.footToeOutDeg", Scenario::Neutral, [](Profile& p) { p.calibration.footToeOutDeg = 30; }},
    {"body.heightM", Scenario::Neutral, [](Profile& p) { p.body.heightM = 2.0; }},
    {"body.legScale", Scenario::Neutral, [](Profile& p) { p.body.legScale = 1.3; }},
    {"body.spineScale", Scenario::Neutral, [](Profile& p) { p.body.spineScale = 1.3; }},
    {"body.armScale", Scenario::Neutral, [](Profile& p) { p.body.armScale = 1.3; }},
    {"body.widthScale", Scenario::Neutral, [](Profile& p) { p.body.widthScale = 1.4; }},
  };
  for (const auto& c : cases) {
    Profile base = defaultProfile(); if (c.prep) c.prep(base); Profile mod = base; c.mod(mod);
    double d = difference(runScenario(c.sc, base), runScenario(c.sc, mod));
    CHECK_MSG(d > 1e-3, "setting '%s' has no effect on '%s' (diff %.6f)", c.name, scenarioName(c.sc), d);
  }
}
