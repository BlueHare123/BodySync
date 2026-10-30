#include <random>
#include "bodysync/filters.h"
#include "test_helpers.h"
using namespace bs;

static double maxJump(const Run& r, int role) {   // largest per-frame displacement of a tracker
  double m = 0; for (size_t i = 1; i < r.out.size(); ++i) if (r.out[i].tr[role].valid && r.out[i - 1].tr[role].valid) m = std::max(m, distance(r.out[i].tr[role].pose.p, r.out[i - 1].tr[role].pose.p));
  return m;
}
static bool allFinite(const Run& r) {
  for (auto& o : r.out) for (auto& t : o.tr) { if (t.valid && !finitePose(t.pose)) return false; if (!t.vel.isFinite() || !t.angVel.isFinite()) return false; }
  return true;
}

TEST(solver_neutral_standing) {
  Profile p = defaultProfile(); Run r = runScenario(Scenario::Neutral, p);
  Proportions P = Proportions::from(p.body);
  CHECK(allFinite(r));
  for (auto& o : r.out) CHECK(o.posture == Posture::Standing);
  const auto& o = r.at(3.5);
  for (int i = 0; i < kRoleCount; ++i) { CHECK(o.tr[i].valid); CHECK(o.tr[i].active); CHECK(o.tr[i].src == Source::Virtual); CHECK(o.tr[i].confidence > 0); }
  double hip = o.tr[kWaist].pose.p.y; CHECK_MSG(hip > 0.8 && hip < 1.0, "hip %.3f", hip);              // ~0.53*1.74
  CHECK_NEAR(o.tr[kLeftFoot].pose.p.y, P.ankleHeight, 0.02); CHECK_NEAR(o.tr[kRightFoot].pose.p.y, P.ankleHeight, 0.02);
  CHECK(o.tr[kLeftFoot].pose.p.x < o.tr[kRightFoot].pose.p.x);                                            // left is -X (user faces -Z)
  CHECK(o.tr[kLeftKnee].pose.p.y > P.ankleHeight && o.tr[kLeftKnee].pose.p.y < hip);
  CHECK(o.tr[kChest].pose.p.y > hip);
  CHECK(o.tr[kLeftElbow].pose.p.x < o.tr[kRightElbow].pose.p.x);
  CHECK_MSG(o.diag.maxSegErr < 1e-6, "segment error %g", o.diag.maxSegErr);                            // IK keeps segment lengths exact
  CHECK(o.footContact[0] && o.footContact[1]);
  for (int i = 0; i < kRoleCount; ++i) CHECK_MSG(maxJump(r, i) < 0.02, "role %d jump %.3f", i, maxJump(r, i));
}
TEST(solver_segment_lengths_and_symmetry_during_motion) {
  Profile p = defaultProfile(); Proportions P = Proportions::from(p.body);
  for (Scenario sc : {Scenario::StepLeft, Scenario::Squat, Scenario::Walk, Scenario::TurnInPlace}) {
    Run r = runScenario(sc, p); CHECK(allFinite(r));
    double worst = 0; for (auto& o : r.out) worst = std::max(worst, o.diag.maxSegErr);
    CHECK_MSG(worst < 1e-6, "%s: segment error %g", scenarioName(sc), worst);
    for (auto& o : r.out) {
      // pelvis -> neck distance is the (scaled) spine length at all times; feet can never be further than leg reach from the pelvis
      CHECK(distance(o.tr[kLeftFoot].pose.p, o.tr[kWaist].pose.p) < P.legReach() + P.hipHalf + 0.3);
      CHECK(o.tr[kWaist].pose.p.y > 0.2);
    }
  }
}
TEST(solver_foot_stepping_and_locking) {
  Profile p = defaultProfile();
  Run r = runScenario(Scenario::StepLeft, p);
  double minX = 0; bool lifted = false; int stepsL = 0; bool prev = true;
  for (auto& o : r.out) { minX = std::min(minX, o.tr[kLeftFoot].pose.p.x); if (!o.footContact[0]) lifted = true; if (prev && !o.footContact[0]) ++stepsL; prev = o.footContact[0]; }
  CHECK_MSG(minX < -0.3, "left foot followed to x=%.2f", minX); CHECK(lifted); CHECK(stepsL >= 1);
  // locked foot: while the head sways by < the step radius a planted foot must not slide
  Run n = runScenario(Scenario::Neutral, p); double slide = 0;
  for (size_t i = 1; i < n.out.size(); ++i) slide = std::max(slide, distance(n.out[i].tr[kLeftFoot].pose.p, n.out[i - 1].tr[kLeftFoot].pose.p));
  CHECK(slide < 0.002);
  // footLock: higher lock => fewer steps
  auto steps = [&](double lock) { Profile q = p; q.tracking.footLock = lock; Run s = runScenario(Scenario::Walk, q); int c = 0; bool pr = true; for (auto& o : s.out) { bool f = !o.footContact[0] || !o.footContact[1]; if (f && !pr) ++c; pr = f; } return c; };
  CHECK(steps(0.0) > steps(1.0));
}
TEST(solver_squat_keeps_feet_planted) {
  Profile p = defaultProfile(); Run r = runScenario(Scenario::Squat, p);
  const auto& a = r.at(0.3); const auto& b = r.at(2.6);
  CHECK(b.posture == Posture::Standing);                                                                  // a squat is not a sit
  CHECK(b.tr[kWaist].pose.p.y < a.tr[kWaist].pose.p.y - 0.25);
  CHECK(distance(withY(b.tr[kLeftFoot].pose.p, 0), withY(a.tr[kLeftFoot].pose.p, 0)) < 0.12);
  CHECK(b.tr[kLeftKnee].pose.p.z < b.tr[kWaist].pose.p.z + 0.05 || b.tr[kLeftKnee].pose.p.z < a.tr[kLeftKnee].pose.p.z - 0.05);   // knee pushed forward (-Z)
  CHECK(b.footContact[0] && b.footContact[1]);
  for (auto& o : r.out) CHECK(o.tr[kLeftFoot].pose.p.y >= 0.02);
}
TEST(solver_sit_and_stand_transitions) {
  Profile p = defaultProfile(); Run r = runScenario(Scenario::Sit, p);
  CHECK(r.at(0.5).posture == Posture::Standing);
  CHECK(r.at(5.0).posture == Posture::Sitting);
  CHECK(r.at(8.8).posture == Posture::Standing);
  const auto& s = r.at(5.0);
  CHECK_MSG(s.tr[kWaist].pose.p.y > 0.3 && s.tr[kWaist].pose.p.y < 0.75, "seated hip %.2f", s.tr[kWaist].pose.p.y);
  CHECK(s.tr[kLeftFoot].pose.p.z < s.tr[kWaist].pose.p.z - 0.15);                                         // feet in front of the pelvis
  CHECK(s.tr[kLeftFoot].pose.p.y < 0.15);                                                                 // resting on the floor
  CHECK(s.tr[kLeftKnee].pose.p.y > s.tr[kLeftFoot].pose.p.y);
  CHECK(allFinite(r)); CHECK_MSG(s.diag.maxSegErr < 1e-6, "seg %g", s.diag.maxSegErr);
  for (int i = 0; i < kRoleCount; ++i) CHECK_MSG(maxJump(r, i) < 0.08, "role %d jump %.3f in sit/stand", i, maxJump(r, i));   // blends are smooth
  // transition really blends over transitionSec
  bool sawBlend = false; for (auto& o : r.out) if (o.transition > 0 && o.transition < 1) sawBlend = true; CHECK(sawBlend);
}
TEST(solver_lying_postures) {
  Profile p = defaultProfile();
  Run b = runScenario(Scenario::LieBack, p); const auto& e = b.at(6.0);
  CHECK(e.posture == Posture::LyingBack);
  CHECK(e.tr[kWaist].pose.p.y < 0.4);
  CHECK(e.tr[kLeftFoot].pose.p.z > e.tr[kWaist].pose.p.z + 0.4);    // feet further along +Z than the pelvis (head points to -Z)
  Quat wq = e.tr[kWaist].pose.q; CHECK_NEAR((wq.up() - Vec3{0, 0, -1}).length(), 0, 0.15); CHECK(wq.forward().y > 0.9);   // belly up
  CHECK(allFinite(b)); CHECK(b.at(8.9).posture == Posture::Standing);
  Run s = runScenario(Scenario::LieSide, p); CHECK(s.at(6.0).posture == Posture::LyingLeft);
  CHECK(s.at(6.0).tr[kWaist].pose.p.y < 0.5); CHECK(allFinite(s));
  double w = 0; for (auto& o : s.out) w = std::max(w, o.diag.maxSegErr); CHECK_MSG(w < 1e-6, "seg %g", w);
  for (int i = 0; i < kRoleCount; ++i) CHECK_MSG(maxJump(s, i) < 0.09, "role %d jump %.3f lying", i, maxJump(s, i));
  // all four lying kinds are reachable from head orientation
  auto kind = [&](Vec3 up, Vec3 fwd) {
    Solver sv; Profile q = p; q.poses.holdTimeSec = 0.05; sv.configure(q); SolverOutput o;
    for (int i = 0; i < 200; ++i) { SolverInput in; in.t = i / 90.0; Quat hq = frameFromUpFwd(up, fwd); in.hmd = {{{0, 0.15, 0}, hq}, true}; in.lc = {{{-0.3, 0.1, 0}, hq}, true}; in.rc = {{{0.3, 0.1, 0}, hq}, true}; o = sv.update(in); }
    return o.posture; };
  CHECK(kind({0, 0, -1}, {0, 1, 0}) == Posture::LyingBack); CHECK(kind({0, 0, -1}, {0, -1, 0}) == Posture::LyingFront);
  CHECK(kind({0, 0, -1}, cross({0, 0, -1}, {0, 1, 0})) == Posture::LyingLeft); CHECK(kind({0, 0, -1}, cross({0, 1, 0}, {0, 0, -1})) == Posture::LyingRight);
}
TEST(posture_hysteresis_and_debounce) {
  Profile p = defaultProfile(); Solver sv; sv.configure(p); int changes = 0; Posture last = Posture::Standing;
  auto feed = [&](double t, double y) { SolverInput in; in.t = t; Quat q; in.hmd = {{{0, y, 0}, q}, true}; in.lc = {{{-0.2, y - 0.6, 0}, q}, true}; in.rc = {{{0.2, y - 0.6, 0}, q}, true}; SolverOutput o = sv.update(in); if (o.posture != last) { ++changes; last = o.posture; } return o; };
  double t = 0; for (; t < 2; t += 1 / 90.0) feed(t, 1.62);
  for (double u = 0; u < 0.25; u += 1 / 90.0, t += 1 / 90.0) feed(t, 1.62 * 0.7);                         // brief dip < holdTime: ignored
  for (double u = 0; u < 1.0; u += 1 / 90.0, t += 1 / 90.0) feed(t, 1.62);
  CHECK(changes == 0);
  std::mt19937 g(3); std::uniform_real_distribution<double> noise(-0.03, 0.03);
  for (double u = 0; u < 3.0; u += 1 / 90.0, t += 1 / 90.0) feed(t, 1.62 * 0.72);                          // sit down
  CHECK(last == Posture::Sitting); int c0 = changes;
  double mid = 1.62 * 0.84;                                                                              // between enter (0.80) and exit (0.88): hysteresis band
  for (double u = 0; u < 6.0; u += 1 / 90.0, t += 1 / 90.0) feed(t, mid + noise(g));
  CHECK_MSG(changes == c0, "flipped %d times inside the hysteresis band", changes - c0); CHECK(last == Posture::Sitting);
  for (double u = 0; u < 2.0; u += 1 / 90.0, t += 1 / 90.0) feed(t, 1.62);
  CHECK(last == Posture::Standing);
  // forced mode
  Profile f = p; f.poses.mode = PostureMode::Sitting; sv.configure(f); sv.reset(); last = Posture::Standing; changes = 0; t = 0;
  for (; t < 1; t += 1 / 90.0) feed(t, 1.62);
  CHECK(last == Posture::Sitting);
  // walking while low must not become sitting
  Profile q = p; sv.configure(q); sv.reset(); last = Posture::Standing;
  for (t = 0; t < 4; t += 1 / 90.0) { SolverInput in; in.t = t; Quat qq; in.hmd = {{{0, 1.62 * 0.7, -1.2 * t}, qq}, true}; in.lc = {{{-0.2, 0.5, -1.2 * t}, qq}, true}; in.rc = {{{0.2, 0.5, -1.2 * t}, qq}, true}; if (sv.update(in).posture != Posture::Standing) { CHECK(false); break; } }
}
TEST(solver_walk_in_place_detects_bobbing) {
  Profile p = defaultProfile(); Run r = runScenario(Scenario::WalkInPlace, p);
  bool wip = false; int stepsL = 0, stepsR = 0; bool pl = true, pr = true;
  for (auto& o : r.out) { if (o.walkInPlace) wip = true; if (pl && !o.footContact[0]) ++stepsL; if (pr && !o.footContact[1]) ++stepsR; pl = o.footContact[0]; pr = o.footContact[1]; }
  CHECK(wip); CHECK_MSG(stepsL >= 3 && stepsR >= 3, "steps L%d R%d", stepsL, stepsR);
  CHECK(!r.at(0.5).walkInPlace); CHECK(!r.at(5.8).walkInPlace);                                          // not before / after the bobbing
  Run n = runScenario(Scenario::Neutral, p); for (auto& o : n.out) CHECK(!o.walkInPlace);
  double maxX = 0; for (auto& o : r.out) maxX = std::max(maxX, std::fabs(o.tr[kLeftFoot].pose.p.x + 0.1));
  CHECK_MSG(maxX < 0.12, "feet stay roughly in place (%.2f)", maxX);
}
TEST(solver_walk_and_turn) {
  Profile p = defaultProfile(); Run r = runScenario(Scenario::Walk, p); CHECK(allFinite(r));
  const auto& e = r.out.back();
  CHECK(distance(withY(e.tr[kWaist].pose.p, 0), withY(r.out[0].tr[kWaist].pose.p, 0)) > 4);
  CHECK(distance(withY(e.tr[kLeftFoot].pose.p, 0), withY(e.tr[kWaist].pose.p, 0)) < 0.9);                 // feet keep up with the body
  for (int i = 0; i < kRoleCount; ++i) CHECK_MSG(maxJump(r, i) < 0.16, "role %d jump %.3f", i, maxJump(r, i));
  double yaw0 = yawOf(r.out[10].tr[kChest].pose.q), yaw1 = yawOf(e.tr[kChest].pose.q);
  CHECK_MSG(std::fabs(rad2deg(wrapPi(yaw1 - yaw0))) > 60, "chest yaw changed %.0f deg", rad2deg(wrapPi(yaw1 - yaw0)));
  Run t = runScenario(Scenario::TurnInPlace, p); double ty = rad2deg(yawOf(t.out.back().tr[kWaist].pose.q)); CHECK_MSG(std::fabs(std::fabs(ty) - 180) < 40, "pelvis yaw after 180 turn %.0f", ty);
  bool stepped = false; for (auto& o : t.out) if (!o.footContact[0] || !o.footContact[1]) stepped = true; CHECK(stepped);   // turning in place pivots the feet
}
TEST(solver_controller_loss_and_recovery) {
  Profile p = defaultProfile(); Run r = runScenario(Scenario::LoseController, p);
  CHECK(r.at(1.0).health == Health::Ok); CHECK(r.at(3.0).health == Health::ControllerLost); CHECK(r.at(5.0).health == Health::Ok);
  CHECK(allFinite(r)); for (auto& o : r.out) { CHECK(o.tr[kRightElbow].valid); CHECK(o.tr[kWaist].valid); }
  CHECK(r.at(3.3).tr[kRightElbow].confidence < 0.3); CHECK(r.at(5.5).tr[kRightElbow].confidence > 0.7);   // estimate marked uncertain, then trusted again
  for (int i = 0; i < kRoleCount; ++i) CHECK_MSG(maxJump(r, i) < 0.03, "role %d jump %.3f", i, maxJump(r, i));   // arm fades, no snap
  CHECK(distance(r.at(3.3).tr[kRightElbow].pose.p, r.at(1.0).tr[kRightElbow].pose.p) < 0.4);
}
TEST(solver_hmd_loss_hold_then_withdraw_then_smooth_resume) {
  Profile p = defaultProfile(); Run r = runScenario(Scenario::Recovery, p);
  CHECK(r.at(1.5).health == Health::ControllerLost);
  CHECK(r.at(3.9).health == Health::HmdLost && r.at(3.9).tr[kWaist].valid);       // short loss: last pose held
  CHECK(r.at(4.6).health == Health::HmdLost && !r.at(4.6).tr[kWaist].valid);       // long loss: trackers withdrawn (poseIsValid=false)
  CHECK(r.at(4.6).tr[kWaist].active);                                              // device itself remains present
  CHECK(r.at(6.5).health == Health::Ok && r.at(6.5).tr[kWaist].valid);
  CHECK(allFinite(r));
  double j = 0; for (size_t i = 1; i < r.out.size(); ++i) if (r.out[i].tr[kWaist].valid && r.out[i - 1].tr[kWaist].valid) j = std::max(j, distance(r.out[i].tr[kWaist].pose.p, r.out[i - 1].tr[kWaist].pose.p));
  CHECK_MSG(j < 0.05, "waist jump after resume %.3f", j);
}
TEST(solver_rejects_glitch_accepts_relocalisation) {
  Profile p = defaultProfile(); Run r = runScenario(Scenario::SuddenMove, p); CHECK(allFinite(r));
  CHECK(r.at(2.2).diag.rejected[0] > 0);
  double j = 0; for (size_t i = 1; i < r.out.size() && i < (size_t)(3.9 * 90); ++i) j = std::max(j, distance(r.out[i].tr[kWaist].pose.p, r.out[i - 1].tr[kWaist].pose.p));
  CHECK_MSG(j < 0.05, "glitch leaked into output (%.2f m)", j);
  CHECK_MSG(r.at(6.5).tr[kWaist].pose.p.x > 1.0, "relocalised waist x=%.2f", r.at(6.5).tr[kWaist].pose.p.x);   // genuine jump is followed
  double speed = 0; for (size_t i = 1; i < r.out.size(); ++i) speed = std::max(speed, distance(r.out[i].tr[kWaist].pose.p, r.out[i - 1].tr[kWaist].pose.p) * 90);
  CHECK_MSG(speed <= p.tracking.maxSpeedMps + 0.5, "output speed %.1f m/s", speed);
}
TEST(solver_survives_invalid_input) {
  Profile p = defaultProfile(); Run r = runScenario(Scenario::InvalidInput, p);
  CHECK(allFinite(r)); int valid = 0; for (auto& o : r.out) if (o.tr[kWaist].valid) ++valid; CHECK(valid > (int)r.out.size() * 0.9);
  for (auto& o : r.out) { CHECK(o.tr[kWaist].valid ? finitePose(o.tr[kWaist].pose) : true); }
}
TEST(solver_fuzz_never_emits_nan) {
  Profile p = defaultProfile(); p.mixed.roles[kLeftFoot].serial = "x"; Solver s; s.configure(p); Mixer m; m.configure(p.mixed);
  std::mt19937 g(99); std::uniform_real_distribution<double> u(-3, 3); std::uniform_int_distribution<int> k(0, 12);
  const double bad[] = {NAN, INFINITY, -INFINITY, 1e300, -1e300, 0.0};
  auto wild = [&]() { return k(g) == 0 ? bad[g() % 6] : u(g); };
  auto sample = [&]() { DeviceSample d; d.valid = k(g) != 0; d.pose = {{wild(), wild(), wild()}, {wild(), wild(), wild(), wild()}}; d.quality = k(g) == 0 ? NAN : 1; return d; };
  bool hmdOk = true; int nonFiniteOut = 0;
  for (int i = 0; i < 20000; ++i) {
    SolverInput in; in.t = i / 90.0; in.hmd = sample(); in.lc = sample(); in.rc = sample(); in.phys[kLeftFoot] = sample(); in.phys[kWaist] = sample();
    if (k(g) < 8) { in.hmd = {{{0, 1.6, 0}, Quat{}}, true}; }
    m.gate(1 / 90.0, in, hmdOk); SolverOutput o = s.update(in); m.blend(1 / 90.0, o); hmdOk = o.hmdOk;
    for (auto& t : o.tr) if (t.valid && !finitePose(t.pose)) ++nonFiniteOut;
  }
  CHECK(nonFiniteOut == 0);
}
TEST(solver_alignment_offsets) {
  Profile p = defaultProfile(); Run a = runScenario(Scenario::Neutral, p);
  Profile q = p; q.calibration.posOffset = {0.1, 0.0, -0.2}; Run b = runScenario(Scenario::Neutral, q);
  Vec3 d = b.at(3).tr[kWaist].pose.p - a.at(3).tr[kWaist].pose.p; CHECK_NEAR(d.x, 0.1, 0.01); CHECK_NEAR(d.z, -0.2, 0.01);
  Profile y = p; y.calibration.yawOffsetDeg = 30; Run c = runScenario(Scenario::Neutral, y);
  CHECK_NEAR(rad2deg(wrapPi(yawOf(c.at(3).tr[kWaist].pose.q) - yawOf(a.at(3).tr[kWaist].pose.q))), 30, 1.0);
  Profile f = p; f.calibration.floorOffsetM = 0.1; f.calibration.eyeHeightM = 1.62; Run e = runScenario(Scenario::Neutral, f);   // floor 10 cm up, head not: user is "shorter"
  CHECK(e.at(3).tr[kLeftFoot].pose.p.y > a.at(3).tr[kLeftFoot].pose.p.y + 0.05);
}
