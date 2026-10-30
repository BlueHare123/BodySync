#include "test_helpers.h"
using namespace bs;
using Pri = RoleAssign::Priority;

static Profile mixedProfile(Pri pri = Pri::PhysicalPreferred) { Profile p = defaultProfile(); p.mixed.roles[kLeftFoot].serial = "SIM-LeftFoot"; p.mixed.roles[kLeftFoot].priority = pri; return p; }

TEST(mixed_physical_virtual_switching_with_blend) {
  Run r = runScenario(Scenario::LoseTracker, mixedProfile());
  const auto& a = r.at(2.0);                                                    // physical tracker healthy and in charge
  CHECK(a.tr[kLeftFoot].src == Source::Physical); CHECK(!a.tr[kLeftFoot].valid && !a.tr[kLeftFoot].active);   // virtual twin hidden: no duplicate for the role
  CHECK_NEAR(a.tr[kLeftFoot].physWeight, 1, 1e-9);
  // the solver really uses the physical foot: the left knee is exactly one shin length from it
  double shin = Proportions::from(mixedProfile().body).lowerLeg;
  CHECK_NEAR(distance(a.tr[kLeftKnee].pose.p, Vec3{-0.10, 0.068, 0}), shin, 0.03);
  bool sawBlend = false, sawVirtualActive = false; Pose prev; bool havePrev = false; double jump = 0;
  for (size_t i = 0; i < r.out.size(); ++i) {
    const auto& t = r.out[i].tr[kLeftFoot]; double time = i / 90.0;
    if (time > 2.4 && time < 3.4 && t.src == Source::Blend) sawBlend = true;
    if (time > 3.6 && time < 4.9) { if (t.src == Source::Virtual && t.valid && t.active) sawVirtualActive = true; CHECK(t.src != Source::Physical); }
    if (t.valid && havePrev) jump = std::max(jump, distance(t.pose.p, prev.p));
    if (t.valid) { prev = t.pose; havePrev = true; }
  }
  CHECK(sawBlend); CHECK(sawVirtualActive);                                    // physical lost -> virtual estimate takes over
  CHECK_MSG(jump < 0.06, "published foot jumped %.3f m during hand-over", jump);   // smooth: no teleport between sources
  const auto& b = r.at(8.5);
  CHECK(b.tr[kLeftFoot].src == Source::Physical && !b.tr[kLeftFoot].valid);    // recovered and hidden again
  CHECK(r.at(3.0).tr[kRightFoot].src == Source::Virtual && r.at(3.0).tr[kRightFoot].valid);   // other roles untouched
}
TEST(mixed_priorities_and_disable) {
  { Run r = runScenario(Scenario::LoseTracker, mixedProfile(Pri::PhysicalOnly)); for (auto& o : r.out) CHECK(!o.tr[kLeftFoot].valid && !o.tr[kLeftFoot].active); }   // never a virtual twin
  { Run r = runScenario(Scenario::LoseTracker, mixedProfile(Pri::VirtualOnly)); for (auto& o : r.out) { CHECK(o.tr[kLeftFoot].valid); CHECK(o.tr[kLeftFoot].src == Source::Virtual); } }   // physical ignored
  { Run r = runScenario(Scenario::LoseTracker, mixedProfile(Pri::VirtualPreferred)); CHECK(r.at(2.0).tr[kLeftFoot].valid); CHECK(r.at(2.0).tr[kLeftFoot].src == Source::Virtual); }   // virtual wins while HMD is fine
  { Profile p = mixedProfile(); p.mixed.enabled = false; Run r = runScenario(Scenario::LoseTracker, p); for (auto& o : r.out) { CHECK(o.tr[kLeftFoot].valid); CHECK(o.tr[kLeftFoot].src == Source::Virtual); } }   // mixed mode off
  { Profile p = defaultProfile(); Run r = runScenario(Scenario::LoseTracker, p); for (auto& o : r.out) CHECK(o.tr[kLeftFoot].src == Source::Virtual); }   // no serial assigned
  { Profile p = mixedProfile(); p.mixed.blendSeconds = 3.0; Run r = runScenario(Scenario::LoseTracker, p); CHECK(r.at(0.8).tr[kLeftFoot].src == Source::Blend || r.at(0.8).tr[kLeftFoot].src == Source::Physical); }
}
TEST(mixed_quality_gate_detects_bad_physical_data) {
  Profile p = mixedProfile(); Mixer m; m.configure(p.mixed); Solver s; s.configure(p); SimSource sim;
  double t = 0; bool ok = true;
  auto step = [&](double secs, auto mutate) { for (double e = 0; e < secs; e += 1 / 90.0, t += 1 / 90.0) { SolverInput in = sim.sample(Scenario::Neutral, t); in.t = t; in.phys[kLeftFoot] = {{{-0.1 + 0.0004 * std::sin(t * 31), 0.07, 0}, Quat{}}, true, 1.0}; mutate(in, t); m.gate(1 / 90.0, in, ok); SolverOutput o = s.update(in); m.blend(1 / 90.0, o); } };
  step(1.5, [](SolverInput&, double) {}); CHECK(m.role(kLeftFoot).healthy && m.role(kLeftFoot).usable);
  step(2.0, [](SolverInput& in, double) { in.phys[kLeftFoot].pose.p.x = -0.1; });             // frozen: byte-identical pose forever = a dead tracker
  CHECK(!m.role(kLeftFoot).healthy); CHECK(m.role(kLeftFoot).reason.find("stale") != std::string::npos);
  step(1.0, [&](SolverInput& in, double tt) { in.phys[kLeftFoot].pose.p.x += 0.001 * std::sin(tt * 20); }); CHECK(m.role(kLeftFoot).healthy);   // moving again -> recovers
  step(1.0, [&](SolverInput& in, double tt) { in.phys[kLeftFoot].pose.p.x += 0.001 * std::sin(tt * 20); in.phys[kLeftFoot].quality = 0.2; }); CHECK(!m.role(kLeftFoot).healthy);   // low quality
  step(1.0, [&](SolverInput& in, double tt) { in.phys[kLeftFoot].pose.p.x += 0.001 * std::sin(tt * 20); }); CHECK(m.role(kLeftFoot).healthy);
  bool lost = false; step(0.8, [&](SolverInput& in, double tt) { in.phys[kLeftFoot].pose.p.x += (int)(tt * 90) % 2 ? 5.0 : 0.0; if (!m.role(kLeftFoot).healthy) lost = true; }); CHECK(lost);   // teleporting tracker rejected
  step(1.0, [&](SolverInput& in, double tt) { in.phys[kLeftFoot].pose.p.x = NAN; (void)tt; }); CHECK(!m.role(kLeftFoot).healthy);                                       // NaN pose
}
TEST(mixed_dropout_shorter_than_timeout_is_bridged) {
  Profile p = mixedProfile(); Mixer m; m.configure(p.mixed); Solver s; s.configure(p); SimSource sim; double t = 0; bool anyVirtual = false;
  for (int i = 0; i < 600; ++i, t += 1 / 90.0) {
    SolverInput in = sim.sample(Scenario::Neutral, t); in.t = t; in.phys[kLeftFoot] = {{{-0.1 + 0.001 * std::sin(t * 9), 0.07, 0}, Quat{}}, !(i > 300 && i < 310), 1.0};   // 0.1 s dropout (< 0.25 s timeout)
    m.gate(1 / 90.0, in, true); SolverOutput o = s.update(in); m.blend(1 / 90.0, o);
    if (i > 100 && o.tr[kLeftFoot].src != Source::Physical) anyVirtual = true;
  }
  CHECK(!anyVirtual);
}
TEST(mixed_physical_pelvis_and_chest_drive_the_solver) {
  Profile p = defaultProfile(); p.mixed.roles[kWaist].serial = "W"; p.mixed.roles[kChest].serial = "C"; Solver s; s.configure(p); Mixer m; m.configure(p.mixed); SimSource sim;
  SolverOutput o; for (int i = 0; i < 200; ++i) { double t = i / 90.0; SolverInput in = sim.sample(Scenario::Neutral, t); in.t = t;
    double nz = 0.0003 * std::sin(t * 17); in.phys[kWaist] = {{{0.05 + nz, 0.93, 0.02}, Quat{}}, true, 1}; in.phys[kChest] = {{{0.05, 1.2 + nz, 0.0}, Quat{}}, true, 1}; m.gate(1 / 90.0, in, true); o = s.update(in); m.blend(1 / 90.0, o); }
  CHECK(o.tr[kWaist].src == Source::Physical); CHECK_NEAR(o.tr[kWaist].pose.p.x, 0.05, 1e-3); CHECK_NEAR(o.tr[kChest].pose.p.y, 1.2, 1e-3);
  double hip = distance(o.skel.hipL, o.tr[kLeftKnee].pose.p); CHECK_NEAR(hip, Proportions::from(p.body).upperLeg, 1e-3);   // legs hang from the *physical* pelvis
  CHECK_NEAR((o.skel.hipL + o.skel.hipR).x / 2, 0.05, 0.01);
}

// ---- calibration ----
static std::vector<CalibSample> standStill(double eye, int n = 120, double wobble = 0.004) {
  std::vector<CalibSample> v; for (int i = 0; i < n; ++i) { CalibSample c; c.hmd = {{0, eye + wobble * std::sin(i * 0.4), 0}, Quat{}}; c.hmdOk = true; v.push_back(c); } return v;
}
TEST(calibration_height_tpose_neutral_verify) {
  auto h = calibrateHeight(standStill(1.62), 0); CHECK(h.ok); CHECK_NEAR(h.eyeHeight, 1.62, 0.01); CHECK_NEAR(h.height, 1.62 / 0.93, 0.01);
  CHECK(!calibrateHeight(standStill(1.62, 10), 0).ok);                                                           // too few samples
  CHECK(!calibrateHeight(standStill(1.62, 100, 0.2), 0).ok);                                                     // moving
  CHECK(!calibrateHeight(standStill(0.3), 0).ok);                                                                // nonsense height
  CHECK_NEAR(calibrateHeight(standStill(1.72), 0.1).eyeHeight, 1.62, 0.01);                                      // floor offset honoured
  Profile p; apply(p, h); CHECK(p.calibration.valid && std::fabs(p.body.heightM - 1.742) < 0.01 && !p.calibration.timestamp.empty());
  BodyProfile body; body.heightM = 1.75; double arm = (0.186 + 0.2) * 1.75 * 1.1, half = 0.129 * 1.75;           // user with 10% longer arms
  std::vector<CalibSample> tp; for (int i = 0; i < 60; ++i) { CalibSample c; c.hmd = {{0, 1.63, 0}, Quat{}}; c.lc = {{-(half + arm), 1.4, 0}, Quat{}}; c.rc = {{half + arm, 1.4, 0}, Quat{}}; c.hmdOk = c.lcOk = c.rcOk = true; tp.push_back(c); }
  auto t = calibrateTPose(tp, body); CHECK(t.ok); CHECK_NEAR(t.armScale, 1.1, 0.02); CHECK_NEAR(t.torsoYawBiasDeg, 0, 0.5);
  for (auto& c : tp) { c.hmd.q = yawQuat(deg2rad(20)); } t = calibrateTPose(tp, body); CHECK_NEAR(t.torsoYawBiasDeg, -20, 1.0);   // head turned 20deg left while the hands say "facing forward"
  for (auto& c : tp) c.rcOk = false;
  CHECK(!calibrateTPose(tp, body).ok);
  for (auto& c : tp) { c.rcOk = true; c.rc.p.y = 2.5; } CHECK(!calibrateTPose(tp, body).ok);                      // hands not level
  std::vector<CalibSample> nu = standStill(1.6); for (auto& c : nu) c.hmd.q = axisAngle({1, 0, 0}, deg2rad(8)); auto n = calibrateNeutral(nu); CHECK(n.ok); CHECK_NEAR(n.headPitchOffsetDeg, 8, 0.01);
  Profile q = defaultProfile(); apply(q, n); Solver s; s.configure(q); SolverInput in; in.hmd = {{{0, 1.62, 0}, axisAngle({1, 0, 0}, deg2rad(8))}, true}; in.lc = {{{-0.2, 1, 0}, Quat{}}, true}; in.rc = {{{0.2, 1, 0}, Quat{}}, true};
  std::vector<SolverOutput> outs; Run r = runScenario(Scenario::Neutral, defaultProfile()); auto v = verifySkeleton(r.out); CHECK_MSG(v.pass, "verify: %s (seg %g asym %g jit %g)", v.messages.empty() ? "" : v.messages[0].c_str(), v.maxSegErrM, v.asymmetryM, v.jitterRmsM);
  CHECK(v.maxSegErrM < 1e-6 && v.asymmetryM < 0.06 && v.jitterRmsM < 0.03);
  Run bad = runScenario(Scenario::StepLeft, defaultProfile()); CHECK(!verifySkeleton(bad.out).pass);              // moving user is flagged as unstable
  CHECK(!verifySkeleton({}).pass);
}
TEST(calibration_height_changes_skeleton) {
  Profile a = defaultProfile(), b = a; b.body.heightM = 2.0; b.body.legScale = 1.2;
  Run ra = runScenario(Scenario::Neutral, a), rb = runScenario(Scenario::Neutral, b);
  CHECK(distance(ra.at(3).tr[kWaist].pose.p, rb.at(3).tr[kWaist].pose.p) > 0.05);
}
