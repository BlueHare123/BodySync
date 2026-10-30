#include "bodysync/calibration.h"
#include <ctime>

namespace bs {
double median(std::vector<double> v) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  return v.size() % 2 ? v[v.size() / 2] : 0.5 * (v[v.size() / 2 - 1] + v[v.size() / 2]);
}
HeightResult calibrateHeight(const std::vector<CalibSample>& s, double floorY) {
  HeightResult r; std::vector<double> y;
  for (auto& c : s) if (c.hmdOk && c.hmd.isFinite()) y.push_back(c.hmd.p.y - floorY);
  if (y.size() < 20) { r.msg = "not enough head tracking data"; return r; }
  double m = median(y), lo = *std::min_element(y.begin(), y.end()), hi = *std::max_element(y.begin(), y.end());
  if (hi - lo > 0.08) { r.msg = "head moved too much - stand still and straight"; return r; }
  if (m < 0.6 || m > 2.3) { r.msg = "eye height out of range - check floor level"; return r; }
  r.ok = true; r.eyeHeight = m; r.height = clampd(m / kEyeHeightRatio, 0.9, 2.4);
  return r;
}
TPoseResult calibrateTPose(const std::vector<CalibSample>& s, const BodyProfile& body) {
  TPoseResult r; std::vector<double> span, bias;
  Proportions P = Proportions::from(body);
  for (auto& c : s) {
    if (!(c.hmdOk && c.lcOk && c.rcOk)) continue;
    Vec3 v = c.rc.p - c.lc.p; double sp = v.length();
    if (sp < 0.5 || std::fabs(v.y) > 0.25 * sp) continue;       // hands must be level and apart
    span.push_back(sp);
    Vec3 f = cross({0, 1, 0}, Vec3{v.x, 0, v.z}.normalized({1, 0, 0}));   // facing direction from the hand axis
    Vec3 hf = c.hmd.q.forward(); hf.y = 0;
    bias.push_back(rad2deg(wrapPi(yawOf(f) - yawOf(hf.normalized({0, 0, -1})))));
  }
  if (span.size() < 20) { r.msg = "both controllers must be tracked with arms held out to the sides"; return r; }
  r.span = median(span);
  r.armLen = std::max(0.3, (r.span - 2 * P.shoulderHalf) / 2);
  double def = (0.186 + 0.200) * body.heightM;                 // default shoulder->controller length at armScale 1
  r.armScale = clampd(r.armLen / def, 0.8, 1.25);
  r.torsoYawBiasDeg = clampd(median(bias), -45, 45);
  r.ok = true; if (std::fabs(median(bias)) > 30) r.msg = "T-pose looks rotated against the head direction - look straight ahead and repeat";
  return r;
}
NeutralResult calibrateNeutral(const std::vector<CalibSample>& s) {
  NeutralResult r; std::vector<double> pitch;
  for (auto& c : s) if (c.hmdOk && c.hmd.isFinite()) pitch.push_back(rad2deg(std::asin(clampd(c.hmd.q.forward().y, -1, 1))));
  if (pitch.size() < 20) { r.msg = "not enough head tracking data"; return r; }
  r.ok = true; r.headPitchOffsetDeg = clampd(median(pitch), -45, 45);
  return r;
}
VerifyReport verifySkeleton(const std::vector<SolverOutput>& outs) {
  VerifyReport v; std::vector<Vec3> w; int bad = 0;
  for (auto& o : outs) {
    v.maxSegErrM = std::max(v.maxSegErrM, o.diag.maxSegErr);
    if (!o.tr[kWaist].valid) { ++bad; continue; }
    w.push_back(o.tr[kWaist].pose.p);
    const auto& t = o.tr;
    double a = std::fabs(distance(t[kWaist].pose.p, t[kLeftKnee].pose.p) - distance(t[kWaist].pose.p, t[kRightKnee].pose.p));
    a = std::max(a, std::fabs(distance(t[kChest].pose.p, t[kLeftElbow].pose.p) - distance(t[kChest].pose.p, t[kRightElbow].pose.p)));
    a = std::max(a, std::fabs(distance(t[kWaist].pose.p, t[kLeftFoot].pose.p) - distance(t[kWaist].pose.p, t[kRightFoot].pose.p)));
    v.asymmetryM = std::max(v.asymmetryM, a);
  }
  if (w.size() < 10) { v.messages.push_back("not enough valid frames"); return v; }
  Vec3 m; for (auto& p : w) m += p; m = m / (double)w.size();
  double ss = 0; for (auto& p : w) ss += dot(p - m, p - m);
  v.jitterRmsM = std::sqrt(ss / w.size());
  if (v.maxSegErrM > 0.01) v.messages.push_back("segment lengths drift - check body proportions");
  if (v.asymmetryM > 0.06) v.messages.push_back("left/right asymmetry - check controller tracking and stance offsets");
  if (v.jitterRmsM > 0.03) v.messages.push_back("pelvis is unstable - stand still, or raise smoothing");
  if (bad) v.messages.push_back("some frames had no valid pose");
  v.pass = v.messages.empty();
  return v;
}
std::string timestampNow() { std::time_t t = std::time(nullptr); char b[32]; std::strftime(b, sizeof b, "%Y-%m-%d %H:%M:%S", std::localtime(&t)); return b; }
void apply(Profile& p, const HeightResult& r) { if (!r.ok) return; p.body.heightM = r.height; p.calibration.eyeHeightM = r.eyeHeight; p.calibration.valid = true; p.calibration.timestamp = timestampNow(); }
void apply(Profile& p, const TPoseResult& r) { if (!r.ok) return; p.body.armScale = r.armScale; p.calibration.torsoYawBiasDeg = r.torsoYawBiasDeg; p.calibration.timestamp = timestampNow(); }
void apply(Profile& p, const NeutralResult& r) { if (!r.ok) return; p.calibration.headPitchOffsetDeg = r.headPitchOffsetDeg; p.calibration.timestamp = timestampNow(); }
}  // namespace bs
