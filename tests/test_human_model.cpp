// Accuracy tests against a synthetic human: a kinematic body with KNOWN joint positions produces headset/controller poses,
// the solver sees only those, and its elbows / knees / feet / pelvis are compared with the truth.
// Limits: the "truth" is a model (gravity-hanging elbows, knees along the toes, alternating gait), not motion capture of a real person.
// Print the full table with BS_DIAG=1.
#include <algorithm>
#include <cstdlib>
#include "test_helpers.h"
using namespace bs;

double g_truthBack = 0.4;   // anatomical variant of the truth elbow: 0 = gravity only, >0 = elbow bulges backwards
namespace {
struct Truth { Vec3 pelvis, neck; double torsoYaw = 0; Pose hmd, wrist[2]; Vec3 elbow[2], knee[2], foot[2]; bool stepping[2] = {false, false}; };

enum class Act { Reach, WalkLook, HandsOnly, BodyTurnHeadStill, WalkStraight };
static const char* actName(Act a) { switch (a) { case Act::Reach: return "reach+look"; case Act::WalkLook: return "walk+look"; case Act::HandsOnly: return "hands only"; case Act::BodyTurnHeadStill: return "body turn, head still"; default: return "walk straight"; } }

struct Human {
  Proportions P;           // the person's real proportions (may differ from the profile)
  Act act;
  double ankle, hipH;
  Vec3 foot[2], from[2]; double st[2] = {0, 0}; bool stepping[2] = {false, false}; bool init = false; Vec3 prevP;
  Human(const Proportions& p, Act a) : P(p), act(a) { ankle = P.ankleHeight; hipH = ankle + P.legReach() * 0.985; }

  static Vec3 handLocal(int k) {   // right hand, relative to the right shoulder, torso frame (right, up, fwd)
    static const Vec3 L[] = {{0.05, -0.60, 0.05}, {0.05, -0.05, 0.58}, {0.10, 0.52, 0.15}, {0.60, -0.05, 0.05}, {-0.25, -0.15, 0.45}, {0.10, -0.40, 0.40}, {0.15, -0.35, -0.18}, {0.30, 0.10, 0.50}};
    return L[k % 8];
  }
  Vec3 hand(double t, int side) const {  // side 0 left, 1 right
    double per = 1.6, u = t / per + (side ? 0.37 : 0); int k = (int)std::floor(u); double f = smoothstep((u - k - 0.35) / 0.65);
    int seq[8] = {0, 1, 4, 2, 3, 5, 6, 7};
    if (act == Act::WalkLook || act == Act::WalkStraight) {   // arms swing at the sides while walking
      double sw = 0.14 * std::sin(2 * kPi * 0.9 * t + (side ? kPi : 0));
      return {0.05, -0.60, 0.05 + sw};
    }
    return lerp(handLocal(seq[k % 8]), handLocal(seq[(k + 1) % 8]), f);
  }
  Truth at(double t, double dt) {
    Truth T; double ψ = 0, φ = 0, pitch = 0; Vec3 pxz; double lean = deg2rad(4);
    switch (act) {
      case Act::Reach: ψ = deg2rad(35) * std::sin(0.30 * t); φ = deg2rad(55) * std::sin(0.55 * t + 1); pitch = deg2rad(12) * std::sin(0.8 * t); pxz = {0.04 * std::sin(0.7 * t), 0, 0.03 * std::cos(0.5 * t)}; break;
      case Act::HandsOnly: pitch = deg2rad(5) * std::sin(0.9 * t); break;
      case Act::BodyTurnHeadStill: ψ = deg2rad(50) * std::sin(0.35 * t); φ = -ψ; break;   // body turns, the head keeps looking the same way
      case Act::WalkLook: case Act::WalkStraight: {
        double w = 1.1 / 1.5, a = w * t;
        if (act == Act::WalkLook) { pxz = {1.5 * (std::cos(a) - 1), 0, -1.5 * std::sin(a)}; ψ = a; φ = deg2rad(40) * std::sin(0.9 * t); }
        else { pxz = {0, 0, -1.1 * t}; φ = deg2rad(15) * std::sin(0.7 * t); }
        break;
      }
    }
    Quat tq = yawQuat(ψ); Vec3 tr = tq.right(), tf = tq.forward(), up{0, 1, 0};
    T.torsoYaw = ψ;
    Vec3 P0{pxz.x, hipH, pxz.z}; T.pelvis = P0;
    Vec3 sdir = (up * std::cos(lean) + tf * std::sin(lean)).normalized(); T.neck = P0 + sdir * P.spine;
    Quat torsoQ = frameFromUpFwd(sdir, tf); Vec3 sr = torsoQ.right(), su = torsoQ.up(), sf = torsoQ.forward();
    Quat hq = (yawQuat(ψ + φ) * axisAngle({1, 0, 0}, pitch)).normalized();
    T.hmd = {T.neck + hq.rotate({0, P.neckToEyeUp, -P.neckToEyeBack}), hq};
    for (int s = 0; s < 2; ++s) {
      double sd = s ? 1 : -1; Vec3 sh = T.neck + sr * (sd * P.shoulderHalf) - su * 0.02;
      Vec3 loc = hand(t, s); loc.x *= sd; double reach = (P.upperArm + P.foreArm) * 0.97; if (loc.length() > reach) loc = loc.normalized() * reach;
      Vec3 w = sh + sr * loc.x + su * loc.y + sf * loc.z;
      Vec3 pole = (-up * 1.0 + sr * (sd * 0.2) - sf * g_truthBack).normalized(); Vec3 e = w;
      T.elbow[s] = twoBoneIK(sh, e, P.upperArm, P.foreArm, pole, deg2rad(150));
      T.wrist[s] = {e, frameFromUpFwd(su, e - T.elbow[s])};
    }
    // feet: planted until the body moves away, then alternate steps (independent of the solver's step logic)
    Vec3 vel = init ? (P0 - prevP) / std::max(dt, 1e-4) : Vec3{}; prevP = P0;
    Vec3 ideal[2];
    for (int s = 0; s < 2; ++s) { double sd = s ? 1 : -1; ideal[s] = Vec3{P0.x, ankle, P0.z} + tq.right() * (sd * 0.10) + Vec3{vel.x, 0, vel.z} * 0.30; }
    if (!init) { for (int s = 0; s < 2; ++s) foot[s] = ideal[s]; init = true; }
    for (int s = 0; s < 2; ++s) if (stepping[s]) {
      st[s] += dt / 0.42; double k = smoothstep(st[s]);
      if (st[s] >= 1) { stepping[s] = false; foot[s] = ideal[s]; } else { foot[s] = lerp(from[s], ideal[s], k); foot[s].y = ankle + 0.10 * std::sin(kPi * st[s]); }
    }
    for (int s = 0; s < 2; ++s) if (!stepping[s] && !stepping[1 - s]) {
      double d = Vec3{foot[s].x - ideal[s].x, 0, foot[s].z - ideal[s].z}.length();
      double d1 = Vec3{foot[1 - s].x - ideal[1 - s].x, 0, foot[1 - s].z - ideal[1 - s].z}.length();
      if (d > 0.30 && d >= d1) { stepping[s] = true; st[s] = 0; from[s] = foot[s]; }
    }
    for (int s = 0; s < 2; ++s) {
      double sd = s ? 1 : -1; Vec3 hip = P0 + tq.right() * (sd * P.hipHalf); Vec3 f = foot[s];
      T.knee[s] = twoBoneIK(hip, f, P.upperLeg, P.lowerLeg, tq.forward() + tq.right() * (sd * 0.17), deg2rad(150));
      T.foot[s] = f; T.stepping[s] = stepping[s];
    }
    return T;
  }
};

struct Stat { std::vector<double> v; void add(double x) { v.push_back(x); }
  double mean() const { double s = 0; for (double x : v) s += x; return v.empty() ? 0 : s / v.size(); }
  double pct(double p) const { if (v.empty()) return 0; auto c = v; std::sort(c.begin(), c.end()); return c[(size_t)(p * (c.size() - 1))]; } };

struct Result { Stat yaw, elbow, knee, foot, pelvis, hand; int elbowFlips = 0, kneeFlips = 0, feetCrossed = 0, kneeBackwards = 0, frames = 0; double footSlide = 0; };

static Result run(Act act, const Profile& prof, const Proportions& real, double dur, double hz = 90) {
  Solver solver; solver.configure(prof); Human h(real, act); Result R; double dt = 1.0 / hz;
  Vec3 prevKneeN[2], prevElbN[2]; bool haveN = false; SolverOutput prev; bool havePrev = false;
  for (double t = 0; t < dur; t += dt) {
    Truth T = h.at(t, dt);
    SolverInput in; in.t = t; in.hmd = {T.hmd, true}; in.lc = {T.wrist[0], true}; in.rc = {T.wrist[1], true};
    SolverOutput o = solver.update(in);
    if (t < 2.0) { prev = o; havePrev = true; continue; }
    ++R.frames;
    for (int s = 0; s < 2; ++s) {
      int rk = s ? kRightKnee : kLeftKnee, rf = s ? kRightFoot : kLeftFoot, re = s ? kRightElbow : kLeftElbow;
      R.elbow.add(distance(o.tr[re].pose.p, T.elbow[s])); R.knee.add(distance(o.tr[rk].pose.p, T.knee[s]));
      R.foot.add(distance(o.tr[rf].pose.p, T.foot[s])); R.hand.add(distance((s ? o.skel.handR : o.skel.handL), T.wrist[s].p));
      // plausibility independent of truth
      Vec3 hip = o.skel.j[kWaist].p, knee = o.tr[rk].pose.p, foot = o.tr[rf].pose.p;
      Vec3 mid = (o.skel.hipL + o.skel.hipR) * 0.5; (void)mid; Vec3 hp = s ? o.skel.hipR : o.skel.hipL;
      Vec3 n = cross(knee - hp, foot - knee).normalized({0, 0, 0});
      if (haveN && prevKneeN[s].length() > 0.5 && n.length() > 0.5 && dot(n, prevKneeN[s]) < 0.3) ++R.kneeFlips;
      prevKneeN[s] = n;
      Vec3 fwdFoot = o.tr[rf].pose.q.forward(); Vec3 bulge = knee - (hp + foot) * 0.5; Vec3 bh{bulge.x, 0, bulge.z};
      if (bh.length() > 0.02 && dot(bh.normalized(), Vec3{fwdFoot.x, 0, fwdFoot.z}.normalized({0, 0, -1})) < -0.2) ++R.kneeBackwards;
      Vec3 sh = s ? o.skel.shR : o.skel.shL; Vec3 en = cross(o.tr[re].pose.p - sh, (s ? o.skel.handR : o.skel.handL) - o.tr[re].pose.p).normalized({0, 0, 0});
      if (haveN && prevElbN[s].length() > 0.5 && en.length() > 0.5 && dot(en, prevElbN[s]) < 0.3) ++R.elbowFlips;
      prevElbN[s] = en;
      if (o.footContact[s] && havePrev && prev.footContact[s]) R.footSlide += distance(o.tr[rf].pose.p, prev.tr[rf].pose.p);
    }
    haveN = true;
    R.pelvis.add(distance(o.tr[kWaist].pose.p, T.pelvis)); R.yaw.add(std::fabs(rad2deg(wrapPi(deg2rad(o.diag.torsoYawDeg) - T.torsoYaw))));
    Vec3 pl = o.tr[kLeftFoot].pose.p, pr = o.tr[kRightFoot].pose.p; Vec3 right = o.tr[kWaist].pose.q.right();
    if (dot(pr - pl, right) < 0.0) ++R.feetCrossed;
    prev = o; havePrev = true;
  }
  return R;
}
}  // namespace

static void report(const char* tag, Act a, const Result& r) {
  if (!std::getenv("BS_DIAG")) return;
  std::printf("  %-26s %-22s elbow %.3f/%.3f  knee %.3f/%.3f  foot %.3f/%.3f  pelvis %.3f  hand %.3f  flips e%d k%d  kneeBack %d  crossed %d  slide %.2f  yaw err %.1f deg\n", tag, actName(a),
              r.elbow.mean(), r.elbow.pct(0.95), r.knee.mean(), r.knee.pct(0.95), r.foot.mean(), r.foot.pct(0.95), r.pelvis.mean(), r.hand.mean(), r.elbowFlips, r.kneeFlips, r.kneeBackwards, r.feetCrossed, r.footSlide, r.yaw.mean());
}

TEST(human_model_accuracy) {
  Profile prof = defaultProfile(); Proportions same = Proportions::from(prof.body);
  Proportions diff = same; diff.upperArm *= 1.05; diff.foreArm *= 1.05; diff.upperLeg *= 0.97; diff.lowerLeg *= 0.97;   // the real person differs a little from the profile
  for (Act a : {Act::HandsOnly, Act::Reach, Act::BodyTurnHeadStill, Act::WalkStraight, Act::WalkLook}) {
    Result r = run(a, prof, same, 24); report("exact proportions", a, r);
    Result d = run(a, prof, diff, 24); report("proportions +-5%", a, d);
    for (const Result* x : {&r, &d}) {
      CHECK_MSG(x->elbow.mean() < 0.13, "%s: elbow mean %.3f", actName(a), x->elbow.mean());
      CHECK_MSG(x->elbow.pct(0.95) < 0.30, "%s: elbow p95 %.3f", actName(a), x->elbow.pct(0.95));
      CHECK_MSG(x->hand.mean() < 0.02, "%s: the solver must reach the hand (%.3f)", actName(a), x->hand.mean());
      CHECK_MSG(x->pelvis.mean() < 0.06, "%s: pelvis %.3f", actName(a), x->pelvis.mean());
      CHECK_MSG(x->feetCrossed == 0, "%s: feet crossed in %d frames", actName(a), x->feetCrossed);
      CHECK_MSG(x->kneeBackwards == 0 && x->kneeFlips == 0 && x->elbowFlips == 0, "%s: flips k%d e%d back %d", actName(a), x->kneeFlips, x->elbowFlips, x->kneeBackwards);
    }
  }
  Result w = run(Act::WalkStraight, prof, same, 24); CHECK_MSG(w.yaw.mean() < 5, "walking straight: torso yaw error %.1f deg", w.yaw.mean());
  Result l = run(Act::WalkLook, prof, same, 24); CHECK_MSG(l.yaw.mean() < 18, "walking in a circle while looking around: torso yaw error %.1f deg", l.yaw.mean());
}

TEST(arms_stay_out_of_the_torso_and_keep_segment_lengths) {
  Profile prof = defaultProfile(); Proportions P = Proportions::from(prof.body);
  for (Act a : {Act::HandsOnly, Act::Reach, Act::BodyTurnHeadStill}) {
    Solver solver; solver.configure(prof); Human h(P, a); double dt = 1.0 / 90, minD = 9, seg = 0;
    for (double t = 0; t < 20; t += dt) {
      Truth T = h.at(t, dt); SolverInput in; in.t = t; in.hmd = {T.hmd, true}; in.lc = {T.wrist[0], true}; in.rc = {T.wrist[1], true};
      SolverOutput o = solver.update(in); if (t < 2) continue;
      seg = std::max(seg, o.diag.maxSegErr);
      Vec3 spine = (o.skel.neck - o.tr[kWaist].pose.p).normalized({0, 1, 0});
      for (int s = 0; s < 2; ++s) { Vec3 v = o.tr[s ? kRightElbow : kLeftElbow].pose.p - o.skel.neck; v = v - spine * dot(v, spine); minD = std::min(minD, v.length()); }
    }
    CHECK_MSG(minD > 0.10, "%s: elbow came within %.3f m of the spine axis", actName(a), minD);
    CHECK_MSG(seg < 1e-6, "%s: segment length error %g", actName(a), seg);
  }
}

TEST(feet_stay_on_the_floor_when_the_leg_is_too_short) {
  // brisk sidestepping: a planted foot that gets out of reach must slide on the floor, not float
  Profile prof = defaultProfile(); Proportions P = Proportions::from(prof.body); Solver solver; solver.configure(prof);
  double dt = 1.0 / 90, worstLift = 0;
  for (double t = 0; t < 6; t += dt) {
    SolverInput in; in.t = t; double x = 0.8 * std::sin(1.6 * t);   // brisk sidestepping, peak 1.3 m/s
    in.hmd = {{{x, 1.62, 0}, Quat{}}, true}; in.lc = {{{x - 0.2, 1.0, -0.1}, Quat{}}, true}; in.rc = {{{x + 0.2, 1.0, -0.1}, Quat{}}, true};
    SolverOutput o = solver.update(in); if (t < 1) continue;
    for (int s = 0; s < 2; ++s) if (o.footContact[s]) worstLift = std::max(worstLift, o.tr[s ? kRightFoot : kLeftFoot].pose.p.y - P.ankleHeight);
  }
  CHECK_MSG(worstLift < 0.08, "a planted foot floated %.3f m above the floor", worstLift);
}
