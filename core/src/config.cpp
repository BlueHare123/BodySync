#include "bodysync/config.h"
#include <cctype>

namespace bs {
static const char* kNames[kRoleCount] = {"Waist", "Chest", "Left elbow", "Right elbow", "Left knee", "Right knee", "Left foot", "Right foot"};
static const char* kNamesRu[kRoleCount] = {"Таз", "Грудь", "Левый локоть", "Правый локоть", "Левое колено", "Правое колено", "Левая стопа", "Правая стопа"};
static const char* kSerials[kRoleCount] = {"BodySync_Waist", "BodySync_Chest", "BodySync_LeftElbow", "BodySync_RightElbow", "BodySync_LeftKnee", "BodySync_RightKnee", "BodySync_LeftFoot", "BodySync_RightFoot"};
static const char* kSvr[kRoleCount] = {"TrackerRole_Waist", "TrackerRole_Chest", "TrackerRole_LeftElbow", "TrackerRole_RightElbow", "TrackerRole_LeftKnee", "TrackerRole_RightKnee", "TrackerRole_LeftFoot", "TrackerRole_RightFoot"};
const char* roleName(int r) { return (r >= 0 && r < kRoleCount) ? kNames[r] : "?"; }
const char* roleNameRu(int r) { return (r >= 0 && r < kRoleCount) ? kNamesRu[r] : "?"; }
const char* roleSerial(int r) { return (r >= 0 && r < kRoleCount) ? kSerials[r] : ""; }
const char* roleSteamVRName(int r) { return (r >= 0 && r < kRoleCount) ? kSvr[r] : ""; }

// anthropometric fractions of stature (standard ergonomic tables, rounded)
Proportions Proportions::from(const BodyProfile& b) {
  Proportions p; double H = b.heightM;
  p.height = H;
  p.eyeHeight = 0.93 * H;
  p.upperLeg = 0.245 * H * b.legScale;
  p.lowerLeg = 0.246 * H * b.legScale;
  p.ankleHeight = 0.039 * H;
  p.spine = 0.288 * H * b.spineScale;
  p.hipHalf = 0.055 * H * b.widthScale;
  p.shoulderHalf = 0.129 * H * b.widthScale;
  p.upperArm = 0.186 * H * b.armScale;
  p.foreArm = 0.200 * H * b.armScale;      // elbow -> controller grip
  p.neckToEyeUp = 0.118 * H;
  p.neckToEyeBack = 0.065 * H;
  return p;
}

static int fix(double& v, double lo, double hi, double def) {
  if (!std::isfinite(v)) { v = def; return 1; }
  double c = clampd(v, lo, hi);
  if (c != v) { v = c; return 1; }
  return 0;
}
int sanitize(BodyProfile& b) {
  int n = fix(b.heightM, 0.9, 2.4, 1.75);
  for (double* s : {&b.legScale, &b.spineScale, &b.armScale, &b.widthScale}) n += fix(*s, 0.6, 1.5, 1.0);
  return n;
}
int sanitize(Calibration& c) {
  int n = fix(c.eyeHeightM, 0, 2.4, 0) + fix(c.headPitchOffsetDeg, -45, 45, 0) + fix(c.torsoYawBiasDeg, -45, 45, 0) + fix(c.yawOffsetDeg, -180, 180, 0) +
          fix(c.floorOffsetM, -1, 1, 0) + fix(c.footLateralM, -0.2, 0.3, 0.02) + fix(c.footForwardM, -0.3, 0.3, 0) + fix(c.footToeOutDeg, -30, 45, 8);
  for (double& v : c.posOffset) n += fix(v, -2, 2, 0);
  for (double& v : c.pelvisOffset) n += fix(v, -0.3, 0.3, 0);
  return n;
}
int sanitize(PoseSettings& p) {
  int n = fix(p.sitEnterRatio, 0.4, 0.95, 0.80) + fix(p.sitExitRatio, 0.5, 1.0, 0.88) + fix(p.crouchRatio, 0.2, 0.9, 0.62) +
          fix(p.lieHeightRatio, 0.1, 0.8, 0.70) + fix(p.lieEnterUp, 0.0, 0.95, 0.70) + fix(p.lieExitUp, 0.05, 0.99, 0.85) +
          fix(p.holdTimeSec, 0.05, 5, 0.6) + fix(p.transitionSec, 0.05, 5, 0.7) + fix(p.standLeanDeg, -30, 30, 0) + fix(p.sitLeanDeg, -30, 45, 0) +
          fix(p.sitPelvisHeightOffsetM, -0.3, 0.3, 0) + fix(p.sitPelvisBackM, -0.2, 0.4, 0.08) + fix(p.sitLegExtend, 0.2, 1.0, 0.75) +
          fix(p.sitFeetSpreadM, -0.1, 0.4, 0) + fix(p.kneeSplayDeg, -20, 50, 10) + fix(p.lieKneeBendDeg, 0, 90, 15) + fix(p.lieSideKneeBendDeg, 0, 120, 40) +
          fix(p.lieFloorOffsetM, 0, 0.4, 0.10);
  if (p.sitExitRatio < p.sitEnterRatio + 0.02) { p.sitExitRatio = std::min(1.0, p.sitEnterRatio + 0.02); ++n; }   // hysteresis must exist
  if (p.lieExitUp < p.lieEnterUp + 0.02) { p.lieExitUp = std::min(0.99, p.lieEnterUp + 0.02); ++n; }
  int m = (int)p.mode; if (m < 0 || m > 3) { p.mode = PostureMode::Auto; ++n; }
  int ss = (int)p.sitStyle; if (ss < 0 || ss > 3) { p.sitStyle = SitStyle::Chair; ++n; }
  return n;
}
void migratePoseDefaults(PoseSettings& p) {
  if (p.lieHeightRatio == 0.45 && p.lieEnterUp == 0.5 && p.lieExitUp == 0.7) { PoseSettings d; p.lieHeightRatio = d.lieHeightRatio; p.lieEnterUp = d.lieEnterUp; p.lieExitUp = d.lieExitUp; }
}
static int sanitize(TrackingSettings& t) {
  int n = fix(t.smoothing, 0, 1, 0.45) + fix(t.pelvisSway, 0, 1, 0.5) + fix(t.torsoYawDeadzoneDeg, 5, 90, 35) + fix(t.torsoYawFollowSec, 0.2, 20, 3) +
          fix(t.footLock, 0, 1, 0.6) + fix(t.stepHeightM, 0, 0.4, 0.12) + fix(t.stepDurationScale, 0.4, 2.5, 1) + fix(t.walkInPlaceSensitivity, 0, 1, 0.5) +
          fix(t.postureSensitivity, 0, 1, 0.5) + fix(t.kneeMaxBendDeg, 60, 170, 145) + fix(t.elbowMaxBendDeg, 60, 170, 145) + fix(t.spineMaxLeanDeg, 10, 85, 60) +
          fix(t.neckTwistDeg, 20, 120, 75) + fix(t.spineTwistDeg, 5, 90, 40) + fix(t.maxSpeedMps, 2, 50, 12) + fix(t.poseTimeOffsetMs, -100, 100, 0);
  int hz = t.updateRateHz; if (hz < 30 || hz > 240) { t.updateRateHz = 90; ++n; }
  return n;
}
static int sanitize(MixedSettings& m) {
  int n = fix(m.blendSeconds, 0.05, 5, 0.4) + fix(m.lossTimeoutSec, 0.05, 5, 0.25) + fix(m.jumpSpeedMps, 1, 50, 8) + fix(m.staleSeconds, 0.2, 30, 1);
  for (auto& r : m.roles) if (r.serial.size() > 64) { r.serial.clear(); ++n; }
  // one physical tracker may serve only one role: later duplicates are dropped
  for (int i = 0; i < kRoleCount; ++i)
    for (int k = 0; k < i; ++k)
      if (!m.roles[i].serial.empty() && m.roles[i].serial == m.roles[k].serial) { m.roles[i].serial.clear(); ++n; }
  return n;
}
int sanitize(Profile& p) {
  int n = sanitize(p.body) + sanitize(p.calibration) + sanitize(p.tracking) + sanitize(p.poses) + sanitize(p.mixed);
  p.name = safeName(p.name); if (p.name.empty()) { p.name = "Default"; ++n; }
  p.version = kConfigVersion;
  return n;
}
int sanitize(AppSettings& a) {
  int n = 0;
  if (a.language != "en" && a.language != "ru" && a.language != "de" && a.language != "fr" && a.language != "es") { a.language = "en"; ++n; }
  n += fix(a.uiScale, 0.75, 1.75, 1.0);
  a.activeProfile = safeName(a.activeProfile); if (a.activeProfile.empty()) { a.activeProfile = "Default"; ++n; }
  a.version = kConfigVersion;
  return n;
}
std::string safeName(const std::string& s) {
  std::string o;
  for (unsigned char c : s) {
    if (std::isalnum(c) || c >= 0x80 || c == ' ' || c == '-' || c == '_' || c == '.') o += (char)c;   // UTF-8 bytes kept
    if (o.size() >= 48) break;
  }
  size_t b = o.find_first_not_of(" ."), e = o.find_last_not_of(" .");
  return b == std::string::npos ? "" : o.substr(b, e - b + 1);
}
}  // namespace bs
