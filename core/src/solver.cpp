#include "bodysync/solver.h"

namespace bs {
const char* postureName(Posture p) {
  switch (p) {
    case Posture::Standing: return "Standing";
    case Posture::Sitting: return "Sitting";
    case Posture::LyingBack: return "Lying (back)";
    case Posture::LyingFront: return "Lying (front)";
    case Posture::LyingLeft: return "Lying (left side)";
    case Posture::LyingRight: return "Lying (right side)";
  }
  return "?";
}
const char* sourceName(Source s) {
  switch (s) { case Source::Virtual: return "Virtual"; case Source::Physical: return "Physical"; case Source::Blend: return "Blend"; default: return "None"; }
}
static Vec3 UP() { return {0, 1, 0}; }

void Solver::configure(const Profile& p) {
  cfg_ = p; sanitize(cfg_);
  P_ = Proportions::from(cfg_.body);
  fp_ = filterFromSmoothing(cfg_.tracking.smoothing);
  floorY_ = cfg_.calibration.floorOffsetM;
  refEye_ = cfg_.calibration.eyeHeightM > 0.5 ? cfg_.calibration.eyeHeightM : P_.eyeHeight;
}
void Solver::reset() { Profile c = cfg_; *this = Solver(); configure(c); }

Pose Solver::alignFwd(const Pose& p) const {
  const auto& c = cfg_.calibration; Quat q = yawQuat(deg2rad(c.yawOffsetDeg)); Vec3 off{c.posOffset[0], c.posOffset[1], c.posOffset[2]};
  return {pivot_ + q.rotate(p.p - pivot_) + off, (q * p.q).normalized()};
}
Pose Solver::alignInv(const Pose& p) const {
  const auto& c = cfg_.calibration; Quat q = yawQuat(deg2rad(c.yawOffsetDeg)); Vec3 off{c.posOffset[0], c.posOffset[1], c.posOffset[2]};
  return {pivot_ + q.conj().rotate(p.p - off - pivot_), (q.conj() * p.q).normalized()};
}

void Solver::condition(Cond& c, const DeviceSample& s, double dt, double holdSec) {
  bool good = s.valid && s.pose.p.isFinite() && s.pose.q.isFinite();
  Pose np;
  if (good) { double n = s.pose.q.norm(); good = n > 0.5 && n < 2.0; np = {s.pose.p, s.pose.q.normalized()}; }
  if (good && c.have) {
    double gap = std::max(1e-3, c.sinceGood + dt);
    if (distance(np.p, c.last.p) / gap > cfg_.tracking.maxSpeedMps) {
      if (++c.rejects > 12) c.rejects = 0;          // persistent: treat as relocalisation, accept
      else { good = false; ++c.rejectedTotal; }      // single glitch: reject, hold last good pose
    } else c.rejects = 0;
  }
  if (good) {
    if (c.have) c.vel = lerp(c.vel, (np.p - c.last.p) / std::max(1e-3, c.sinceGood + dt), 0.3);
    c.last = np; c.have = true; c.sinceGood = 0;
  } else c.sinceGood += dt;
  c.ok = c.have && c.sinceGood <= holdSec;
}

void Solver::classify(double dt, const Quat& hq, const Vec3& hp) {
  const auto& ps = cfg_.poses; const auto& tk = cfg_.tracking;
  headRatio_ = (hp.y - floorY_) / std::max(refEye_, 0.3);
  // Orientation is low-passed (about 0.12 s): small head movements while lying must not flip the posture.
  double a = haveLp_ ? expAlpha(dt, 0.12) : 1.0;
  upLp_ = lerp(upLp_, hq.up(), a); fwdLp_ = lerp(fwdLp_, hq.forward(), a); rightLp_ = lerp(rightLp_, hq.right(), a); haveLp_ = true;
  Vec3 up = upLp_.normalized(UP()), fwd = fwdLp_.normalized({0, 0, -1}), right = rightLp_.normalized({1, 0, 0});
  bool lying = isLying(posture_);
  bool stationary = hs_ < 0.35 && std::fabs(vs_) < 0.15;
  lowStillT_ = (headRatio_ < ps.crouchRatio && stationary) ? lowStillT_ + dt : 0;
  double hold = ps.holdTimeSec * lerpd(1.6, 0.5, tk.postureSensitivity);
  // Independent features: (1) head tilted away from vertical, (2) head clearly below standing height (a bed or sofa is far above the floor,
  // so this limit is generous), (3) hands not hanging far below the head (bending forward to the floor tilts the head the same way, but the hands hang).
  bool handsHang = !lying && lc_.ok && rc_.ok && hp.y - (lc_.last.p.y + rc_.last.p.y) * 0.5 > 0.22 * refEye_;
  double hLim = lying ? std::min(ps.lieHeightRatio * 1.15, 0.9) : ps.lieHeightRatio;
  bool wantLie = headRatio_ < hLim && up.y < (lying ? ps.lieExitUp : ps.lieEnterUp) && !handsHang;
  // Kind: back/front are told apart by the gaze axis, sides by the head's right axis; the current kind is kept unless the other clearly wins.
  double ay = std::fabs(fwd.y), ry = std::fabs(right.y);
  bool curSide = posture_ == Posture::LyingLeft || posture_ == Posture::LyingRight, curFB = posture_ == Posture::LyingBack || posture_ == Posture::LyingFront;
  bool sideWins = curSide ? !(ay > ry + 0.25) : (curFB ? ry > ay + 0.25 : ry > ay);
  Posture lieKind = sideWins ? (right.y >= 0 ? Posture::LyingLeft : Posture::LyingRight) : (fwd.y >= 0 ? Posture::LyingBack : Posture::LyingFront);
  Posture raw;
  if (wantLie) raw = lieKind;
  else if (posture_ == Posture::Sitting) raw = (headRatio_ < ps.sitExitRatio && hs_ < 0.9) ? Posture::Sitting : Posture::Standing;
  else if (headRatio_ < ps.sitEnterRatio && stationary) raw = headRatio_ >= (ps.sitStyle == SitStyle::CrossLegged ? 0.4 : ps.crouchRatio) ? Posture::Sitting : (lowStillT_ > 3 * hold ? Posture::Sitting : Posture::Standing);
  else raw = Posture::Standing;
  bool forced = ps.mode != PostureMode::Auto;
  if (ps.mode == PostureMode::Standing) raw = Posture::Standing;
  else if (ps.mode == PostureMode::Sitting) raw = Posture::Sitting;
  else if (ps.mode == PostureMode::Lying && !isLying(raw)) raw = lying ? posture_ : Posture::LyingBack;
  if (raw == posture_) { cand_ = posture_; candT_ = 0; }
  else {
    if (raw == cand_) candT_ += dt; else { cand_ = raw; candT_ = 0; }
    if (forced || candT_ >= hold) {
      if (haveSkel_) { fromSkel_ = lastSkel_; transT_ = 0; }
      posture_ = raw; cand_ = raw; candT_ = 0;
    }
  }
}

void Solver::updateBob(double t, double dt, double headY, double speed) {
  if (!haveBob_) { bobLp_ = headY; haveBob_ = true; }
  bobLp_ += expAlpha(dt, 0.6) * (headY - bobLp_);
  double hp = headY - bobLp_;
  bobRms2_ += expAlpha(dt, 1.0) * (hp * hp - bobRms2_);
  double rms = std::sqrt(bobRms2_);
  double thr = lerpd(0.012, 0.003, cfg_.tracking.walkInPlaceSensitivity);
  bool rising = prevHp_ < 0 && hp >= 0; prevHp_ = hp;
  if (rms <= thr * 0.5) bobEvents_ = 0;
  if (rising && posture_ == Posture::Standing && speed < 0.45 && headRatio_ > 0.85 && rms > thr) {
    double gap = t - lastEv_;
    if (gap < 0.18) return;                       // debounce
    lastEv_ = t; bobEvents_ = gap < 0.9 ? bobEvents_ + 1 : 1;
    if (bobEvents_ >= 2) { wipTrigger_ = nextFoot_; nextFoot_ ^= 1; wipUntil_ = t + 0.7; }   // periodic bobbing = steps in place
  }
  diag_.bobRms = rms;
}

// When both hands are held out in front, the torso faces roughly the middle of them. Returns the heading and a 0..1 weight.
double Solver::handYawCue(double chestYaw, const Vec3& neck, double& weight) const {
  weight = 0;
  if (!lc_.ok || !rc_.ok || handBlend_[0] < 0.9 || handBlend_[1] < 0.9) return chestYaw;
  Vec3 a = lc_.last.p - neck, b = rc_.last.p - neck; a.y = b.y = 0;
  Vec3 mid = (a + b) * 0.5; double dist = mid.length();
  if (dist < 0.28 || a.length() < 0.15 || b.length() < 0.15) return chestYaw;
  Vec3 m = mid / dist;
  if (dot(a.normalized(), m) < 0.5 || dot(b.normalized(), m) < 0.5) return chestYaw;        // hands must be on the same side (otherwise they say nothing about the torso)
  double yaw = yawOf(m), ang = std::fabs(wrapPi(yaw - chestYaw));
  if (ang > deg2rad(100)) return chestYaw;
  weight = clampd((dist - 0.28) / 0.15, 0, 1) * 0.8;
  return yaw;
}

static double headYaw(const Quat& q) {   // robust when looking steeply up/down
  Vec3 f = q.forward();
  Vec3 h = f.y < -0.85 ? q.up() : (f.y > 0.85 ? -q.up() : f);
  return yawOf(h);
}

// Elbow = point on the circle that keeps both arm segments exact. The swivel angle is chosen by a small cost: hang low (gravity), flare
// slightly outwards, stay out of the torso, stay close to the previous frame (no flips). `end` is clamped to the reachable range.
Vec3 Solver::pickElbow(int i, const Vec3& sh, Vec3& end, const Quat& chestQ, const Vec3& neck, const Vec3& spine) {
  const double ua = P_.upperArm, fa = P_.foreArm, side = i == 0 ? -1 : 1;
  Vec3 right = chestQ.right();
  twoBoneIK(sh, end, ua, fa, -UP(), deg2rad(cfg_.tracking.elbowMaxBendDeg));          // clamps `end`
  Vec3 d = end - sh; double dist = std::max(d.length(), 1e-6); Vec3 dir = d / dist;
  double a = (ua * ua - fa * fa + dist * dist) / (2 * dist), r = std::sqrt(std::max(0.0, ua * ua - a * a));
  Vec3 c = sh + dir * a, u1 = rejectFrom(-UP(), dir);
  if (u1.length() < 1e-3) u1 = rejectFrom(-chestQ.forward(), dir);                    // arm straight up/down: any perpendicular will do, continuity decides
  u1 = u1.normalized({0, 0, 1}); Vec3 u2 = cross(dir, u1);
  auto cost = [&](double th) {
    Vec3 e = c + (u1 * std::cos(th) + u2 * std::sin(th)) * r, rel = e - sh;
    double o = dot(rel, right) * side / ua, cst = dot(rel, UP()) / ua - 0.2 * clampd(o, -0.5, 0.3) + 0.3 * dot(rel, chestQ.forward()) / ua;   // low, slightly outward, slightly behind the shoulder line (elbows flex forwards)
    Vec3 v = e - neck; v = v - spine * dot(v, spine); double dd = v.length();
    if (dd < 0.13) cst += 40 * ((0.13 - dd) / ua) * ((0.13 - dd) / ua);
    if (e.y < floorY_ + 0.03) cst += 20 * ((floorY_ + 0.03 - e.y) / ua) * ((floorY_ + 0.03 - e.y) / ua);   // lying: the elbow rests on the floor, not in it
    if (haveElbow_[i]) cst += 2.0 * dot(e - elbowPrev_[i], e - elbowPrev_[i]) / (ua * ua);
    return cst;
  };
  const int N = 72; double best = 1e30, bt = 0, cs[N];
  for (int k = 0; k < N; ++k) { cs[k] = cost(2 * kPi * k / N); if (cs[k] < best) { best = cs[k]; bt = k; } }
  int b = (int)bt; double l = cs[(b + N - 1) % N], m = cs[b], rr = cs[(b + 1) % N], den = l - 2 * m + rr;
  double th = 2 * kPi * (b + (den > 1e-9 ? clampd(0.5 * (l - rr) / den, -0.5, 0.5) : 0.0)) / N;   // parabolic refinement between samples
  Vec3 e = c + (u1 * std::cos(th) + u2 * std::sin(th)) * r;
  elbowPrev_[i] = e; haveElbow_[i] = true;
  return e;
}

void Solver::solveArms(Skeleton& s, const SolverInput& in, const Quat& chestQ) {
  Vec3 up = chestQ.up(), right = chestQ.right(), fwd = chestQ.forward(), spine = up;
  s.shL = s.neck - right * P_.shoulderHalf - up * 0.02;
  s.shR = s.neck + right * P_.shoulderHalf - up * 0.02;
  const Cond* hc[2] = {&lc_, &rc_};
  for (int i = 0; i < 2; ++i) {
    double side = i == 0 ? -1 : 1; Vec3 sh = i == 0 ? s.shL : s.shR;
    Vec3 rest = sh - up * ((P_.upperArm + P_.foreArm) * 0.9) + right * (side * 0.05) + fwd * 0.05;   // relaxed hanging arm
    Vec3 end = lerp(rest, hc[i]->last.p, handBlend_[i]);
    double total = P_.upperArm + P_.foreArm; Vec3 toHand = end - sh; double over = toHand.length() - total * 0.985;
    if (over > 0) { sh = sh + toHand.normalized() * std::min(over, 0.07) * 0.8; (i == 0 ? s.shL : s.shR) = sh; }   // reaching far: the shoulder follows a little
    int role = i == 0 ? kLeftElbow : kRightElbow;
    Vec3 elbow = pickElbow(i, sh, end, chestQ, s.neck, spine);
    if (in.phys[role].valid) elbow = in.phys[role].pose.p;
    else segErr_ = std::max({segErr_, std::fabs(distance(sh, elbow) - P_.upperArm), std::fabs(distance(elbow, end) - P_.foreArm)});
    s.j[role] = {elbow, frameFromUpFwd(sh - elbow, end - elbow)};
    (i == 0 ? s.handL : s.handR) = end;
  }
}

void Solver::solveLegs(Skeleton& s, const SolverInput& in, const Pose& pelvis, const std::array<Pose, 2>& feet, const std::array<Vec3, 2>& poles) {
  for (int i = 0; i < 2; ++i) {
    double side = i == 0 ? -1 : 1;
    Vec3 hip = pelvis.p + pelvis.q.right() * (side * P_.hipHalf);
    (i == 0 ? s.hipL : s.hipR) = hip;
    Vec3 end = feet[i].p; int rk = i == 0 ? kLeftKnee : kRightKnee, rf = i == 0 ? kLeftFoot : kRightFoot;
    Vec3 pole = poles[i];
    if (!in.phys[rf].valid) {                      // too far from the hip: slide the foot towards the body on the floor instead of lifting it
      double R = P_.legReach() * 0.9985, dy = end.y - hip.y; Vec3 hv{end.x - hip.x, 0, end.z - hip.z};
      if (dy <= -R) end = {hip.x, hip.y - R, hip.z};   // hip higher than the leg is long: the foot hangs straight below it (smallest possible lift)
      else if (std::fabs(dy) < R) { double hmax = std::sqrt(R * R - dy * dy); if (hv.length() > hmax) { Vec3 n = hv.normalized(); end.x = hip.x + n.x * hmax; end.z = hip.z + n.z * hmax; } }
    }
    if (in.phys[rk].valid) pole = in.phys[rk].pose.p - (hip + end) * 0.5;
    Vec3 knee = twoBoneIK(hip, end, P_.upperLeg, P_.lowerLeg, pole, deg2rad(cfg_.tracking.kneeMaxBendDeg));
    if (in.phys[rk].valid) knee = in.phys[rk].pose.p;
    else segErr_ = std::max({segErr_, std::fabs(distance(hip, knee) - P_.upperLeg), std::fabs(distance(knee, end) - P_.lowerLeg)});
    s.j[rk] = {knee, frameFromUpFwd(hip - knee, poles[i])};
    s.j[rf] = {end, feet[i].q};
  }
}

static Vec3 offsetLocal(const Quat& q, const Vec3& pos, const std::array<double, 3>& o) { return pos + q.rotate({o[0], o[1], -o[2]}); }   // right, up, forward
static Vec3 clampLean(const Vec3& neck, const Vec3& P, double L, double maxRad) {
  Vec3 v = (neck - P).normalized(UP());
  double ang = std::acos(clampd(v.y, -1, 1));
  if (ang <= maxRad) return P;
  Vec3 h = Vec3{v.x, 0, v.z}.normalized({0, 0, -1});
  return neck - (h * std::sin(maxRad) + UP() * std::cos(maxRad)) * L;
}

Skeleton Solver::genStanding(const SolverInput& in, double dt, const Vec3& neck, const Pose& hmd, const Quat&) {
  const auto& tk = cfg_.tracking; const auto& ps = cfg_.poses; const auto& cal = cfg_.calibration;
  Skeleton s; s.head = hmd.p; s.neck = neck;
  double L = P_.spine; Vec3 fwdC = flatFwd(chestYaw_);
  Vec3 C = feetInit_ ? (foot_[0].pos + foot_[1].pos) * 0.5 : neck;
  Vec3 P;
  bool physWaist = in.phys[kWaist].valid;
  if (physWaist) P = in.phys[kWaist].pose.p;
  else {
    Vec3 tgt = lerp(C, neck, tk.pelvisSway);
    Vec3 d{tgt.x - neck.x, 0, tgt.z - neck.z};
    d = d - fwdC * (L * std::sin(deg2rad(ps.standLeanDeg)));   // leaning forward puts the pelvis behind the neck
    double dmax = L * std::sin(deg2rad(tk.spineMaxLeanDeg));
    if (d.length() > dmax) d = d.normalized() * dmax;
    P = {neck.x + d.x, neck.y - std::sqrt(std::max(L * L - dot(d, d), 0.0025)), neck.z + d.z};   // |neck-pelvis| == L
  }
  Vec3 Pstance = P;                              // where the feet expect the pelvis (the squat below must not drag planted feet along)
  if (!physWaist && feetInit_ && !in.phys[kLeftFoot].valid && !in.phys[kRightFoot].valid) {
    // Squat / deep knee bend: with the feet planted the hips cannot come closer to the ankles than the knees allow. A person whose head drops while it
    // stays over the feet sends the hips BACK and leans the torso forward; solve that from the constraints (spine length, knee limit) instead of
    // letting the hips sink straight down (which folded the knees past their limit and pushed the feet into the floor).
    double ankleY = floorY_ + P_.ankleHeight, minChord = chordForBend(P_.upperLeg, P_.lowerLeg, deg2rad(tk.kneeMaxBendDeg * 0.92));
    double dmax = L * std::sin(deg2rad(tk.spineMaxLeanDeg)); Vec3 back = -fwdC;
    for (int it = 0; it < 5; ++it) {
      double vy = P.y - ankleY, need = minChord * smoothstep(std::sqrt(clampd((minChord - vy) / minChord, 0, 1)));   // soft onset of the exact circle sqrt(chord^2-vy^2): no snap when the limit is first reached
      double have = dot(Vec3{P.x - C.x, 0, P.z - C.z}, back);
      if (have < need) { P.x += back.x * (need - have); P.z += back.z * (need - have); }
      Vec3 d{P.x - neck.x, 0, P.z - neck.z}; if (d.length() > dmax) { d = d.normalized() * dmax; P.x = neck.x + d.x; P.z = neck.z + d.z; }
      P.y = neck.y - std::sqrt(std::max(L * L - dot(d, d), 0.0025));
    }
    P = lerp(Pstance, P, smoothstep((headRatio_ - 0.35) / 0.15));   // the head close to the floor is a person going down (lying), not a squat: no hip solution there
  }
  Vec3 sdir = (neck - P).normalized(UP());
  Quat pelvisQ = frameFromUpFwd(lerp(UP(), sdir, 0.5), flatFwd(pelvisYaw_));
  Quat chestQ = frameFromUpFwd(sdir, flatFwd(chestYaw_));

  double yawP = pelvisYaw_, lat = P_.hipHalf + cal.footLateralM;
  double stand = floorY_ + P_.ankleHeight + P_.legReach() * 0.9999, air = std::max(0.0, P.y - stand);   // tiptoe / jump lifts the feet
  if (air > 0.05 && std::fabs(vs_) > 0.6) airUntil_ = now_ + 0.5;                                         // a fast vertical move = a real jump / hop
  double hipTop = P.y + std::fabs(pelvisQ.right().y) * P_.hipHalf;                                          // the higher hip decides (pelvis tilts when leaning sideways)
  if (!physWaist && now_ > airUntil_ && hipTop > stand + 0.04) { P.y -= hipTop - (stand + 0.04); air = 0.04; }                    // otherwise the spine bends (leaning, sidestepping): the pelvis cannot be higher than the legs allow
  Vec3 velXZ{headVel_.x, 0, headVel_.z}, look = velXZ * 0.18;
  if (look.length() > 0.35) look = look.normalized() * 0.35;
  Vec3 id[2]; double iy[2], sideOf[2] = {-1, 1};
  for (int i = 0; i < 2; ++i) {
    id[i] = Vec3{Pstance.x, 0, Pstance.z} + flatRight(yawP) * (sideOf[i] * lat) + flatFwd(yawP) * cal.footForwardM + look;
    id[i].y = floorY_ + P_.ankleHeight + air;
    iy[i] = yawP - sideOf[i] * deg2rad(cal.footToeOutDeg);
  }
  if (!feetInit_) { for (int i = 0; i < 2; ++i) { foot_[i] = Foot{}; foot_[i].pos = id[i]; foot_[i].yaw = iy[i]; } feetInit_ = true; }
  bool physFoot[2] = {in.phys[kLeftFoot].valid, in.phys[kRightFoot].valid};
  Vec3 hipPos[2] = {P + pelvisQ.right() * (-P_.hipHalf), P + pelvisQ.right() * P_.hipHalf};
  double speed = hs_;
  double dur = clampd((0.42 - 0.12 * clampd(speed, 0, 2)) * tk.stepDurationScale, 0.15, 0.9);
  double lift = tk.stepHeightM * (0.6 + 0.4 * clampd(speed, 0, 1));
  for (int i = 0; i < 2; ++i) {   // advance swing phases / follow the floor
    Foot& f = foot_[i];
    if (physFoot[i]) { f.pos = in.phys[i == 0 ? kLeftFoot : kRightFoot].pose.p; f.yaw = iy[i]; f.stepping = false; f.pitch = 0; f.contact = f.pos.y < floorY_ + P_.ankleHeight + 0.04; continue; }
    if (f.stepping) {
      f.t += dt / f.dur;
      if (f.t >= 1) { f.stepping = false; f.pos = id[i]; f.yaw = iy[i]; f.pitch = 0; f.contact = true; }
      else {
        double sm = smoothstep(f.t);
        f.pos = lerp(f.from, id[i], sm); f.pos.y = id[i].y + f.lift * std::sin(kPi * f.t);
        f.yaw = wrapPi(f.yawFrom + wrapPi(iy[i] - f.yawFrom) * sm);
        f.pitch = 0.3 * std::sin(2 * kPi * (f.t + 0.5));   // toe-off, then heel strike
        f.contact = f.t > 0.96;   // airborne for the whole swing, lands in the last 4%
      }
    } else { f.pos.y = id[i].y; f.pitch = 0; f.contact = true; }
  }
  auto startStep = [&](int i, double lf, double du) { Foot& f = foot_[i]; f.stepping = true; f.t = 0; f.from = f.pos; f.yawFrom = f.yaw; f.lift = lf; f.dur = du; f.contact = false; };
  int forced = wipTrigger_; wipTrigger_ = -1;
  if (forced >= 0) { if (!foot_[forced].stepping && !physFoot[forced]) startStep(forced, std::max(0.03, tk.stepHeightM * 0.8), clampd(0.28 * tk.stepDurationScale, 0.15, 0.9)); }
  else {
    double R = lerpd(0.10, 0.35, tk.footLock); int pick = -1; double best = 1.0;
    for (int i = 0; i < 2; ++i) {
      const Foot& f = foot_[i]; if (f.stepping || physFoot[i]) continue;
      double score = Vec3{f.pos.x - id[i].x, 0, f.pos.z - id[i].z}.length() / R;
      if (std::fabs(wrapPi(f.yaw - iy[i])) > deg2rad(55)) score = std::max(score, 1.5);
      if (dot(f.pos - P, flatRight(yawP)) * sideOf[i] < 0.04) score = std::max(score, 1.5);   // planted foot drifted past the body midline (turning): step it back to its own side
      if (distance(hipPos[i], f.pos) > P_.legReach() * 1.06) score = std::max(score, 1.5);   // trailing heel may lift ~6 cm (IK clamps the reach); beyond that: must step
      if (score > best) { best = score; pick = i; }
    }
    if (pick >= 0) { const Foot& o = foot_[1 - pick]; bool busy = o.stepping && !(speed > 1.5 && o.t > 0.5); if (!busy) startStep(pick, lift, dur); }
  }
  std::array<Pose, 2> feet; std::array<Vec3, 2> poles;
  for (int i = 0; i < 2; ++i) {
    feet[i] = {foot_[i].pos, (yawQuat(foot_[i].yaw) * axisAngle({1, 0, 0}, foot_[i].pitch)).normalized()};
    poles[i] = flatFwd(foot_[i].yaw - sideOf[i] * deg2rad(ps.kneeSplayDeg));
    contact_[i] = foot_[i].contact;
  }
  if (!physFoot[0] && !physFoot[1]) {   // feet never cross: keep a minimum gap measured across the pelvis
    Vec3 rF = flatRight(yawP); double sep = dot(feet[1].p - feet[0].p, rF), minSep = 0.10;
    if (sep < minSep) { Vec3 push = rF * ((minSep - sep) * 0.5); feet[0].p = feet[0].p - push; feet[1].p = feet[1].p + push; }
  }
  s.j[kWaist] = {offsetLocal(pelvisQ, P, cal.pelvisOffset), pelvisQ};
  s.j[kChest] = {P + sdir * (0.82 * L), chestQ};
  if (in.phys[kChest].valid) s.j[kChest] = in.phys[kChest].pose;
  solveLegs(s, in, {P, pelvisQ}, feet, poles);
  solveArms(s, in, chestQ);
  diag_.hipHeight = P.y - floorY_; diag_.leanDeg = rad2deg(std::acos(clampd(sdir.y, -1, 1)));
  return s;
}

Skeleton Solver::genSitting(const SolverInput& in, const Vec3& neck, const Pose& hmd, const Quat&) {
  const auto& tk = cfg_.tracking; const auto& ps = cfg_.poses; const auto& cal = cfg_.calibration;
  Skeleton s; s.head = hmd.p; s.neck = neck;
  double L = P_.spine; Vec3 fwdC = flatFwd(chestYaw_);
  // Seat style: shifts the torso lean, pelvis position, leg extension and knee splay of the seated body (the style is the user's choice; whether the user IS seated is detected).
  struct Shape { double leanAddDeg, backAddM, legExtScale, heightAddM, splayAddDeg; };
  static const Shape kShape[4] = {{0, 0, 1.0, 0, 0},            // chair: the original seated shape
                                  {-3, -0.04, 0.65, 0, -6},      // upright: vertical torso, lower legs close to vertical, knees nearly forward
                                  {-24, 0.0, 1.3, -0.02, 4},     // reclined: torso leans back, legs stretched out
                                  {2, 0.0, 0.0, 0, 55}};         // cross-legged on the floor (handled below)
  const Shape& sh = kShape[(int)ps.sitStyle];
  bool floorSit = ps.sitStyle == SitStyle::CrossLegged;
  double lean = deg2rad(ps.sitLeanDeg + sh.leanAddDeg);
  Vec3 P;
  if (in.phys[kWaist].valid) P = in.phys[kWaist].pose.p;
  else {
    P = neck - (UP() * std::cos(lean) + fwdC * std::sin(lean)).normalized() * L - fwdC * (ps.sitPelvisBackM + sh.backAddM);
    P.y = std::max(P.y + ps.sitPelvisHeightOffsetM + sh.heightAddM, floorY_ + (floorSit ? 0.05 : P_.ankleHeight + 0.05));
    P = constrainDistance(neck, P, L);                       // keep spine length exact
    P = clampLean(neck, P, L, deg2rad(tk.spineMaxLeanDeg));
  }
  Vec3 sdir = (neck - P).normalized(UP());
  Quat pelvisQ = frameFromUpFwd(lerp(UP(), sdir, 0.5), flatFwd(pelvisYaw_));
  Quat chestQ = frameFromUpFwd(sdir, flatFwd(chestYaw_));
  std::array<Pose, 2> feet; std::array<Vec3, 2> poles; double reach = P_.legReach() * 0.9999;
  for (int i = 0; i < 2; ++i) {
    double side = i == 0 ? -1 : 1;
    int rf = i == 0 ? kLeftFoot : kRightFoot;
    Vec3 hip = P + pelvisQ.right() * (side * P_.hipHalf);
    double footY = floorY_ + P_.ankleHeight, dY = hip.y - footY;
    double horiz = dY < reach ? std::sqrt(reach * reach - dY * dY) : 0;
    Vec3 f;
    if (floorSit) {   // ankles cross in front of the pelvis: each foot tucked under the opposite knee
      f = Vec3{hip.x, footY, hip.z} + flatFwd(pelvisYaw_) * (0.30 * P_.legReach() + cal.footForwardM) + flatRight(pelvisYaw_) * (-side * (P_.hipHalf + 0.05 - ps.sitFeetSpreadM));
    } else {
      f = Vec3{hip.x, footY, hip.z} + flatFwd(pelvisYaw_) * (horiz * clampd(ps.sitLegExtend * sh.legExtScale, 0.1, 1.0) + cal.footForwardM) + flatRight(pelvisYaw_) * (side * (ps.sitFeetSpreadM + cal.footLateralM));
    }
    double yaw = pelvisYaw_ - side * deg2rad(floorSit ? 25 : cal.footToeOutDeg);
    if (in.phys[rf].valid) f = in.phys[rf].pose.p;
    feet[i] = {f, yawQuat(yaw)};
    poles[i] = flatFwd(yaw - side * deg2rad(ps.kneeSplayDeg + sh.splayAddDeg));
    contact_[i] = (f.y - footY) < 0.03;
  }
  s.j[kWaist] = {offsetLocal(pelvisQ, P, cal.pelvisOffset), pelvisQ};
  s.j[kChest] = {P + sdir * (0.82 * L), chestQ};
  if (in.phys[kChest].valid) s.j[kChest] = in.phys[kChest].pose;
  solveLegs(s, in, {P, pelvisQ}, feet, poles);
  solveArms(s, in, chestQ);
  diag_.hipHeight = P.y - floorY_; diag_.leanDeg = rad2deg(std::acos(clampd(sdir.y, -1, 1)));
  return s;
}

Skeleton Solver::genLying(const SolverInput& in, const Vec3& neck, const Pose& hmd, const Quat& headQ) {
  const auto& ps = cfg_.poses; const auto& cal = cfg_.calibration;
  Skeleton s; s.head = hmd.p; s.neck = neck;
  Vec3 aRaw = rejectFrom(headQ.up(), UP());                    // head "up" along the floor = feet->head axis
  if (aRaw.length() > 0.2) lieAxis_ = lerp(lieAxis_, aRaw.normalized(), expAlpha(dt_, 0.25)).normalized(lieAxis_);
  Vec3 a = lieAxis_, f;
  switch (posture_) {
    case Posture::LyingFront: f = -UP(); break;
    case Posture::LyingLeft: f = cross(a, UP()); break;
    case Posture::LyingRight: f = cross(UP(), a); break;
    default: f = UP(); break;
  }
  Quat bodyQ = frameFromUpFwd(a, f); Vec3 rB = bodyQ.right();
  double L = P_.spine;
  Vec3 P = in.phys[kWaist].valid ? in.phys[kWaist].pose.p : neck - a * L;
  if (!in.phys[kWaist].valid) P.y = std::max(P.y, floorY_ + ps.lieFloorOffsetM);
  bool side = posture_ == Posture::LyingLeft || posture_ == Posture::LyingRight;
  double chord = chordForBend(P_.upperLeg, P_.lowerLeg, deg2rad(side ? ps.lieSideKneeBendDeg : ps.lieKneeBendDeg));
  std::array<Pose, 2> feet; std::array<Vec3, 2> poles;
  for (int i = 0; i < 2; ++i) {
    double sd = i == 0 ? -1 : 1; int rf = i == 0 ? kLeftFoot : kRightFoot;
    Vec3 hip = P + rB * (sd * P_.hipHalf);
    Vec3 fp = hip - a * chord; fp.y = std::max(fp.y, floorY_ + 0.03);
    if (in.phys[rf].valid) fp = in.phys[rf].pose.p;
    feet[i] = {fp, bodyQ};
    poles[i] = posture_ == Posture::LyingFront ? -f : f;
    contact_[i] = true;
  }
  s.j[kWaist] = {offsetLocal(bodyQ, P, cal.pelvisOffset), bodyQ};
  s.j[kChest] = {P + a * (0.82 * L), bodyQ};
  if (in.phys[kChest].valid) s.j[kChest] = in.phys[kChest].pose;
  solveLegs(s, in, {P, bodyQ}, feet, poles);
  solveArms(s, in, bodyQ);
  diag_.hipHeight = P.y - floorY_; diag_.leanDeg = 90;
  return s;
}

static Skeleton blendSkel(const Skeleton& a, const Skeleton& b, double t) {
  Skeleton r;
  for (int i = 0; i < kRoleCount; ++i) r.j[i] = lerpPose(a.j[i], b.j[i], t);
  r.head = lerp(a.head, b.head, t); r.neck = lerp(a.neck, b.neck, t); r.shL = lerp(a.shL, b.shL, t); r.shR = lerp(a.shR, b.shR, t);
  r.hipL = lerp(a.hipL, b.hipL, t); r.hipR = lerp(a.hipR, b.hipR, t); r.handL = lerp(a.handL, b.handL, t); r.handR = lerp(a.handR, b.handR, t);
  return r;
}

SolverOutput Solver::update(const SolverInput& in) {
  const auto& tk = cfg_.tracking; const auto& cal = cfg_.calibration;
  double dt = haveT_ ? clampd(in.t - lastT_, 1e-3, 0.1) : 1.0 / 90;
  haveT_ = true; lastT_ = in.t; now_ = in.t; dt_ = dt; segErr_ = 0;
  condition(hmd_, in.hmd, dt, 0.25); condition(lc_, in.lc, dt, 0.25); condition(rc_, in.rc, dt, 0.25);
  SolverOutput out;
  out.hmdOk = hmd_.ok; out.lcOk = lc_.ok; out.rcOk = rc_.ok;
  out.health = !hmd_.ok ? Health::HmdLost : (!lc_.ok || !rc_.ok ? Health::ControllerLost : Health::Ok);
  const Cond* cs[3] = {&hmd_, &lc_, &rc_};
  for (int i = 0; i < 3; ++i) { diag_.sinceGood[i] = cs[i]->sinceGood; diag_.rejected[i] = cs[i]->rejectedTotal; }
  diag_.nonFinite = nonFinite_;

  if (!hmd_.ok) {                         // head tracking lost: hold the body briefly, then withdraw the trackers
    hmdLostFor_ += dt; resumeT_ = 0;
    for (int r = 0; r < kRoleCount; ++r) {
      TrackerOut& o = out.tr[r]; o.active = tk.enabled[r];
      if (haveHeld_ && hmdLostFor_ < 0.5) { o.pose = heldOut_[r]; o.valid = true; o.confidence = 0.3; o.src = Source::Virtual; }
    }
    out.posture = posture_; out.transition = transT_; out.skel = lastSkel_; out.diag = diag_;
    return out;
  }
  if (hmdLostFor_ > 0) { hmdLostFor_ = 0; for (auto& f : filt_) f.reset(); haveBob_ = false; }

  Pose hmd = hmd_.last;
  pivot_ = {hmd.p.x, 0, hmd.p.z};
  Quat headQ = (hmd.q * axisAngle({1, 0, 0}, -deg2rad(cal.headPitchOffsetDeg))).normalized();
  Vec3 neck = hmd.p + headQ.rotate({0, -P_.neckToEyeUp, P_.neckToEyeBack});
  SolverInput inp = in;                    // physical samples into solver space (virtual body is shifted by the alignment, physical points must not be)
  for (auto& s : inp.phys) if (s.valid) { if (s.pose.isFinite()) s.pose = alignInv(s.pose); else s.valid = false; }

  { Vec3 v = haveHead_ ? (hmd.p - prevHead_) / dt : Vec3{}; headVel_ = lerp(headVel_, v, haveHead_ ? expAlpha(dt, 0.15) : 1.0); prevHead_ = hmd.p; haveHead_ = true;
    hs_ = headVel_.lengthXZ(); vs_ = headVel_.y; }
  for (int i = 0; i < 2; ++i) {            // controller loss: arms relax towards a resting pose, return smoothly
    const Cond& c = i == 0 ? lc_ : rc_; double tgt = c.ok ? 1 : 0;
    if (!c.have) handBlend_[i] = 0; else handBlend_[i] += (tgt - handBlend_[i]) * expAlpha(dt, tgt > handBlend_[i] ? 0.2 : 0.5);
  }
  classify(dt, headQ, hmd.p);
  updateBob(in.t, dt, hmd.p.y, hs_);
  if (isLying(posture_)) yawInit_ = false;
  else {                                   // torso yaw: head-driven with a dead zone, pelvis follows within the spine twist limit
    double target = headYaw(headQ) + deg2rad(cal.torsoYawBiasDeg);
    if (!yawInit_) { double w0 = 0, c0 = handYawCue(target, neck, w0); chestYaw_ = pelvisYaw_ = w0 > 0.3 ? c0 : target; yawInit_ = true; }   // hands held out in front say where the body faces
    double hw = 0, cue = handYawCue(chestYaw_, neck, hw), cueTau = 0.6;
    if (hw <= 0 && hs_ > 0.5) {                              // walking forwards: the torso faces the direction of travel (sidesteps / backwards with the head forward are left to the head)
      double vy = yawOf(Vec3{headVel_.x, 0, headVel_.z});
      if (std::fabs(wrapPi(vy - target)) < deg2rad(50)) { cue = vy; hw = clampd((hs_ - 0.5) / 0.5, 0, 1) * 0.7; cueTau = 0.2; }
    }
    double dz0 = deg2rad(std::min(tk.torsoYawDeadzoneDeg, tk.neckTwistDeg)), dz = dz0 + (deg2rad(tk.neckTwistDeg) - dz0) * hw, e = wrapPi(target - chestYaw_);
    if (std::fabs(e) > dz) chestYaw_ = wrapPi(target - (e > 0 ? dz : -dz));
    if (hw > 0) chestYaw_ = wrapPi(chestYaw_ + wrapPi(cue - chestYaw_) * hw * expAlpha(dt, cueTau));
    double w = clampd(hs_ / 1.2, 0, 1);
    double wc = hw > 0 ? 0 : w;                              // a hand / travel cue decides: do not drag the torso back to the head direction at the same time
    chestYaw_ = wrapPi(chestYaw_ + wrapPi(target - chestYaw_) * expAlpha(dt, lerpd(tk.torsoYawFollowSec, 0.25, wc)));
    pelvisYaw_ = wrapPi(pelvisYaw_ + wrapPi(chestYaw_ - pelvisYaw_) * expAlpha(dt, lerpd(0.6, 0.15, w)));
    double lim = deg2rad(tk.spineTwistDeg), ep = wrapPi(chestYaw_ - pelvisYaw_);
    if (std::fabs(ep) > lim) pelvisYaw_ = wrapPi(chestYaw_ - (ep > 0 ? lim : -lim));
  }
  Skeleton cur;
  if (posture_ == Posture::Standing) cur = genStanding(inp, dt, neck, hmd, headQ);
  else { feetInit_ = false; cur = posture_ == Posture::Sitting ? genSitting(inp, neck, hmd, headQ) : genLying(inp, neck, hmd, headQ); }
  if (transT_ < 1) { transT_ = std::min(1.0, transT_ + dt / std::max(cfg_.poses.transitionSec, 0.05)); cur = blendSkel(fromSkel_, cur, smoothstep(transT_)); }
  lastSkel_ = cur; haveSkel_ = true;

  bool resuming = resumeT_ < 1;
  if (resuming) resumeT_ = std::min(1.0, resumeT_ + dt / 0.5);
  for (int r = 0; r < kRoleCount; ++r) {
    TrackerOut& o = out.tr[r]; o.active = tk.enabled[r];
    bool phys = in.phys[r].valid && in.phys[r].pose.isFinite();
    Pose p;
    if (phys) { p = in.phys[r].pose; filt_[r].reset(); o.src = Source::Physical; }
    else {
      p = alignFwd(cur.j[r]);
      p.p.y = std::max(p.p.y, floorY_ + 0.02);
      FilterParams fpr = fp_; if (r == kLeftFoot || r == kRightFoot) fpr.minCutoff *= 3;   // feet follow a planned, already smooth path: less lag when they land
      if (p.isFinite()) p = filt_[r].apply(p, dt, fpr);
      o.src = Source::Virtual;
    }
    if (!p.isFinite()) {                    // never publish NaN/inf
      ++nonFinite_;
      if (!havePrev_[r]) { o.valid = false; continue; }
      p = prevOut_[r];
    }
    p.q = p.q.normalized();
    if (resuming && !phys && haveHeld_) p = lerpPose(heldOut_[r], p, smoothstep(resumeT_));
    else if (havePrev_[r] && !phys) {      // output speed limiter
      Vec3 d = p.p - prevOut_[r].p; double maxd = tk.maxSpeedMps * dt;
      if (d.length() > maxd) p.p = prevOut_[r].p + d.normalized() * maxd;
    }
    if (havePrev_[r]) {
      Vec3 v = (p.p - prevOut_[r].p) / dt; velLp_[r] = lerp(velLp_[r], v, 0.5);
      Quat dq = (p.q * prevOut_[r].q.conj()).normalized(); if (dq.w < 0) dq = {-dq.w, -dq.x, -dq.y, -dq.z};
      double ang = 2 * std::acos(clampd(dq.w, -1, 1)); Vec3 ax = Vec3{dq.x, dq.y, dq.z}.normalized({0, 0, 0});
      angLp_[r] = lerp(angLp_[r], ax * (ang / dt), 0.5);
    }
    prevOut_[r] = p; havePrev_[r] = true; heldOut_[r] = p;
    o.pose = p; o.vel = velLp_[r]; o.angVel = angLp_[r]; o.valid = true;
    o.confidence = phys ? 1.0 : (r == kLeftElbow ? 0.75 * handBlend_[0] : r == kRightElbow ? 0.75 * handBlend_[1] : 0.75);
  }
  haveHeld_ = true;
  out.skel = cur; out.posture = posture_; out.transition = transT_; out.walkInPlace = now_ < wipUntil_;
  out.footContact[0] = contact_[0]; out.footContact[1] = contact_[1];
  diag_.headRatio = headRatio_; diag_.speedXZ = hs_; diag_.torsoYawDeg = rad2deg(chestYaw_); diag_.pelvisYawDeg = rad2deg(pelvisYaw_);
  diag_.maxSegErr = segErr_; diag_.candidate = cand_; diag_.candidateT = candT_; diag_.nonFinite = nonFinite_;
  out.diag = diag_;
  return out;
}
}  // namespace bs
