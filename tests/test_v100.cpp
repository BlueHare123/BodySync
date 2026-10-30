// v1.0.0: lying detection on realistic data (beds, pillows), squat geometry, seat styles.
#include "bodysync/config_json.h"
#include "test_helpers.h"
using namespace bs;

namespace {
// A person lying on a bed of height `bed` (m): head on a pillow, neck flexed by `flexDeg`. kind: 0 back, 1 front, 2 left side, 3 right side. Returns the final output.
SolverOutput lieDown(Solver& s, const Profile& p, double bed, double flexDeg, int kind, double handDrop = 0.3, double seconds = 4.0, double jitterDeg = 0) {
  (void)p; SolverOutput o; Vec3 a{0, 0, -1}; const double E = 1.62;
  Vec3 f = kind == 0 ? Vec3{0, 1, 0} : kind == 1 ? Vec3{0, -1, 0} : kind == 2 ? cross(a, Vec3{0, 1, 0}) : cross(Vec3{0, 1, 0}, a);
  Quat base = frameFromUpFwd(a, f);
  Quat flex = axisAngle(base.right(), deg2rad(kind == 0 ? -flexDeg : kind == 1 ? flexDeg : 0));
  Quat lq = (flex * base).normalized();
  for (int i = 0; i < (int)(seconds * 90); ++i) {
    double t = i / 90.0, k = std::min(1.0, t / 2.0);
    Quat hq = slerp(Quat{}, lq, k);
    if (jitterDeg > 0 && k >= 1) hq = (axisAngle({1, 0, 0}, deg2rad(jitterDeg * std::sin(5 * t))) * axisAngle({0, 0, 1}, deg2rad(jitterDeg * std::cos(3 * t))) * hq).normalized();
    SolverInput in; in.t = t; in.hmd = {{{0, lerpd(E, bed + 0.13, k), 0.7 * k}, hq}, true};
    Vec3 up = hq.up(), fw = hq.forward(), rt = hq.right(), neck = in.hmd.pose.p + hq.rotate({0, -0.2, 0.11});
    in.lc = {{neck - rt * 0.2 - up * handDrop + fw * 0.08, hq}, true}; in.rc = {{neck + rt * 0.2 - up * handDrop + fw * 0.08, hq}, true};
    o = s.update(in);
  }
  return o;
}
Profile prof() { return defaultProfile(); }
Posture kindOf(int k) { return k == 0 ? Posture::LyingBack : k == 1 ? Posture::LyingFront : k == 2 ? Posture::LyingLeft : Posture::LyingRight; }
// Holds a head pose still for `sec` seconds with the hands hanging `handDrop` below the head.
Posture hold(double y, double pitchDeg, double handDrop, double sec = 5.0) {
  Profile p = prof(); Solver s; s.configure(p); SolverOutput o;
  for (int i = 0; i < (int)(sec * 90); ++i) {
    SolverInput in; in.t = i / 90.0; Quat hq = axisAngle({1, 0, 0}, deg2rad(pitchDeg));
    in.hmd = {{{0, y, 0}, hq}, true}; in.lc = {{{-0.2, y - handDrop, -0.1}, Quat{}}, true}; in.rc = {{{0.2, y - handDrop, -0.1}, Quat{}}, true}; o = s.update(in);
  }
  return o.posture;
}
}  // namespace

TEST(lying_is_detected_on_beds_with_pillows_in_all_four_kinds) {
  Profile p = prof();
  for (int kind = 0; kind < 4; ++kind)
    for (double bed : {0.0, 0.35, 0.5, 0.65, 0.8})
      for (double flex : {0.0, 20.0, 40.0}) {
        Solver s; s.configure(p); SolverOutput o = lieDown(s, p, bed, flex, kind);
        CHECK_MSG(o.posture == kindOf(kind), "kind %d bed %.2f flex %.0f -> %s", kind, bed, flex, postureName(o.posture));
        double segErr = o.diag.maxSegErr; CHECK_MSG(segErr < 1e-6, "segment error %g", segErr);
        CHECK(finitePose(o.tr[kWaist].pose));
      }
}
TEST(lying_survives_head_jitter_and_ends_when_the_user_gets_up) {
  Profile p = prof(); Solver s; s.configure(p);
  SolverOutput o = lieDown(s, p, 0.5, 20, 0, 0.3, 8.0, 12.0);          // +-12 degrees of head wobble for 6 s
  CHECK(o.posture == Posture::LyingBack);
  int flips = 0; Posture last = o.posture;
  for (int i = 0; i < 90 * 5; ++i) {                                      // keep wobbling: no state change allowed
    double t = 8.0 + i / 90.0; Quat hq = axisAngle({1, 0, 0}, deg2rad(12 * std::sin(5 * t))) * frameFromUpFwd({0, 0, -1}, {0, 1, 0});
    SolverInput in; in.t = t; in.hmd = {{{0, 0.63, 0.7}, hq.normalized()}, true}; in.lc = {{{-0.2, 0.55, 0.3}, hq.normalized()}, true}; in.rc = {{{0.2, 0.55, 0.3}, hq.normalized()}, true};
    o = s.update(in); if (o.posture != last) { ++flips; last = o.posture; }
  }
  CHECK_MSG(flips == 0, "posture flipped %d times while lying", flips);
  double t0 = 13.0; for (int i = 0; i < 90 * 4; ++i) {                    // stand up
    double k = std::min(1.0, i / 180.0), t = t0 + i / 90.0; Quat hq = slerp(frameFromUpFwd({0, 0, -1}, {0, 1, 0}), Quat{}, k);
    SolverInput in; in.t = t; in.hmd = {{{0, lerpd(0.63, 1.62, k), 0.7 * (1 - k)}, hq}, true}; in.lc = {{{-0.2, lerpd(0.55, 0.95, k), 0.3 * (1 - k)}, hq}, true}; in.rc = {{{0.2, lerpd(0.55, 0.95, k), 0.3 * (1 - k)}, hq}, true}; o = s.update(in);
  }
  CHECK(o.posture == Posture::Standing);
}
TEST(lying_is_not_reported_for_look_up_bend_over_or_reclined_sitting) {
  CHECK(hold(1.62, 90, 0.6) == Posture::Standing);                       // standing, looking at the ceiling: head tilted exactly like lying on the back
  CHECK(hold(1.62, 60, 0.6) == Posture::Standing);
  CHECK(hold(1.62, -90, 0.6) == Posture::Standing);                      // looking at the floor
  CHECK(!isLying(hold(0.75, -90, 0.7)));                                  // bent over at the waist, hands hanging
  CHECK(!isLying(hold(0.9, -70, 0.6)));
  CHECK(!isLying(hold(0.9, 80, 0.4)));                                    // crouching and looking up
  CHECK(hold(1.1, 0, 0.4) == Posture::Sitting);                           // sitting, torso reclined about 45 degrees
  CHECK(hold(1.15, -60, 0.4) == Posture::Sitting);                        // sitting, looking down at the lap
}
TEST(lying_thresholds_change_the_decision) {
  Profile p = prof(); p.poses.lieHeightRatio = 0.3;                       // user says: only very low heads are lying
  Solver s; s.configure(p); CHECK(lieDown(s, p, 0.5, 0, 0).posture != Posture::LyingBack);
  Profile q = prof(); q.poses.lieEnterUp = 0.05; q.poses.lieExitUp = 0.1;  // user says: tilt must be nearly horizontal
  Solver s2; s2.configure(q); CHECK(lieDown(s2, q, 0.5, 40, 0).posture != Posture::LyingBack);
  Solver s3; s3.configure(prof()); CHECK(lieDown(s3, prof(), 0.5, 40, 0).posture == Posture::LyingBack);
}

TEST(squat_has_hip_hinge_knee_limit_and_planted_feet) {
  Profile p = prof(); Proportions P = Proportions::from(p.body); Run r = runScenario(Scenario::Squat, p);
  double maxLean = 0, minFootY = 9, maxKneeBend = 0, maxKneeSpeed = 0, maxFootSlide = 0;
  for (size_t i = 0; i < r.out.size(); ++i) {
    const auto& o = r.out[i];
    maxLean = std::max(maxLean, o.diag.leanDeg); minFootY = std::min(minFootY, std::min(o.tr[kLeftFoot].pose.p.y, o.tr[kRightFoot].pose.p.y));
    for (int side = 0; side < 2; ++side) {
      Vec3 hip = side ? o.skel.hipR : o.skel.hipL, k = o.tr[side ? kRightKnee : kLeftKnee].pose.p, f = o.tr[side ? kRightFoot : kLeftFoot].pose.p;
      double a = distance(hip, k), b = distance(k, f), d = distance(hip, f), c = (a * a + b * b - d * d) / (2 * a * b);
      maxKneeBend = std::max(maxKneeBend, 180 - rad2deg(std::acos(clampd(c, -1, 1))));
      if (i) maxKneeSpeed = std::max(maxKneeSpeed, distance(k, r.out[i - 1].tr[side ? kRightKnee : kLeftKnee].pose.p) * 90);
    }
    maxFootSlide = std::max(maxFootSlide, distance(withY(o.tr[kLeftFoot].pose.p, 0), withY(r.out[0].tr[kLeftFoot].pose.p, 0)));
    CHECK(finitePose(o.tr[kWaist].pose)); CHECK_MSG(o.diag.maxSegErr < 1e-6, "segment error %g", o.diag.maxSegErr);
  }
  CHECK_MSG(maxLean > 12, "torso never leaned in a deep squat (%.1f deg)", maxLean);                       // hips go back, torso inclines
  CHECK_MSG(minFootY > P.ankleHeight - 0.01, "foot pushed into the floor: y=%.3f", minFootY);
  CHECK_MSG(maxKneeBend < p.tracking.kneeMaxBendDeg - 5, "knee bend %.0f deg beyond the comfort limit", maxKneeBend);
  CHECK_MSG(maxKneeSpeed < 2.2, "knee speed %.2f m/s", maxKneeSpeed);
  CHECK_MSG(maxFootSlide < 0.05, "feet slid %.3f m during the squat", maxFootSlide);
  const auto& bot = r.at(2.8); CHECK(bot.posture == Posture::Standing); CHECK(bot.tr[kWaist].pose.p.y > 0.2);   // pelvis does not touch the floor
  const auto& up = r.at(5.8); CHECK(up.tr[kWaist].pose.p.y > 0.8);                                          // and is back up at the end
}
TEST(shallow_knee_bend_does_not_move_the_hips_back) {
  Profile p = prof(); Solver s; s.configure(p); SolverOutput o;
  for (int i = 0; i < 450; ++i) {                                                                             // head 12 cm lower than standing: a small bend
    SolverInput in; in.t = i / 90.0; in.hmd = {{{0, 1.50, 0}, Quat{}}, true}; in.lc = {{{-0.2, 0.9, -0.1}, Quat{}}, true}; in.rc = {{{0.2, 0.9, -0.1}, Quat{}}, true}; o = s.update(in);
  }
  CHECK(o.posture == Posture::Standing); CHECK_MSG(o.diag.leanDeg < 6, "lean %.1f deg for a shallow bend", o.diag.leanDeg);
}

TEST(sit_styles_change_the_body_and_stay_valid) {
  Profile base = prof(); Run chair = runScenario(Scenario::Sit, base);
  const auto& c = chair.at(5.0); CHECK(c.posture == Posture::Sitting);
  Vec3 chairKneeL = c.tr[kLeftKnee].pose.p, chairKneeR = c.tr[kRightKnee].pose.p;
  for (SitStyle st : {SitStyle::Upright, SitStyle::Reclined, SitStyle::CrossLegged}) {
    Profile q = base; q.poses.sitStyle = st; Run r = runScenario(Scenario::Sit, q); const auto& o = r.at(5.0);
    CHECK_MSG(o.posture == Posture::Sitting, "style %d not detected as sitting", (int)st);
    double d = 0; for (int i = 0; i < kRoleCount; ++i) d += distance(o.tr[i].pose.p, c.tr[i].pose.p);
    CHECK_MSG(d > 0.1, "style %d does not change the seated body (%.3f)", (int)st, d);
    for (auto& x : r.out) { for (auto& t : x.tr) if (t.valid) CHECK(finitePose(t.pose)); CHECK_MSG(x.diag.maxSegErr < 1e-6, "style %d segment error %g", (int)st, x.diag.maxSegErr); }
    for (auto& x : r.out) CHECK(x.tr[kLeftFoot].pose.p.y >= 0.02);
    if (st == SitStyle::Reclined) CHECK_MSG(o.diag.leanDeg > 12 && o.skel.neck.z > o.tr[kWaist].pose.p.z - 0.02 + 0.0 - 1.0, "reclined lean %.1f", o.diag.leanDeg);
    if (st == SitStyle::Reclined) { Vec3 up = o.tr[kChest].pose.q.up(); CHECK_MSG(up.z > 0.15, "reclined chest not leaning back (up.z=%.2f)", up.z); }   // +Z is behind the user
    if (st == SitStyle::Upright) CHECK(std::fabs(o.tr[kChest].pose.q.up().z) < c.tr[kChest].pose.q.up().z + 0.05 + 0.2);
    if (st == SitStyle::CrossLegged) {
      CHECK_MSG(o.tr[kWaist].pose.p.y < 0.5, "cross-legged pelvis too high (%.2f)", o.tr[kWaist].pose.p.y);
      double spread = std::fabs(o.tr[kRightKnee].pose.p.x - o.tr[kLeftKnee].pose.p.x), chairSpread = std::fabs(chairKneeR.x - chairKneeL.x);
      CHECK_MSG(spread > chairSpread + 0.1, "knees not splayed (%.2f vs %.2f)", spread, chairSpread);
      CHECK_MSG(o.tr[kLeftFoot].pose.p.x > o.tr[kRightFoot].pose.p.x, "ankles are not crossed");
    }
  }
}
TEST(sit_style_is_saved_loaded_and_sanitised) {
  Profile p = prof(); p.poses.sitStyle = SitStyle::Reclined;
  json j = p; CHECK(j["poses"]["sitStyle"] == "reclined");
  Profile q = j.get<Profile>(); CHECK(q.poses.sitStyle == SitStyle::Reclined);
  json old = json(prof()); old["poses"].erase("sitStyle"); CHECK(old.get<Profile>().poses.sitStyle == SitStyle::Chair);   // profile from an earlier build
  json bad = json(prof()); bad["poses"]["sitStyle"] = "levitating"; Profile b = bad.get<Profile>(); sanitize(b); CHECK(b.poses.sitStyle == SitStyle::Chair);
  Profile e = prof(); e.poses.sitStyle = (SitStyle)42; CHECK(sanitize(e) > 0); CHECK(e.poses.sitStyle == SitStyle::Chair);
}
TEST(old_profiles_keep_working_and_get_the_new_lying_defaults) {
  json old = json(prof()); old["poses"]["lieHeightRatio"] = 0.45; old["poses"]["lieEnterUp"] = 0.5; old["poses"]["lieExitUp"] = 0.7;
  json p = old; Profile q = p.get<Profile>(); migratePoseDefaults(q.poses);
  CHECK_NEAR(q.poses.lieHeightRatio, PoseSettings{}.lieHeightRatio, 1e-9); CHECK_NEAR(q.poses.lieEnterUp, PoseSettings{}.lieEnterUp, 1e-9);
  Profile custom = prof(); custom.poses.lieHeightRatio = 0.4; migratePoseDefaults(custom.poses); CHECK_NEAR(custom.poses.lieHeightRatio, 0.4, 1e-9);   // user values are never overwritten
}
