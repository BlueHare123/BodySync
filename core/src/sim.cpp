#include "bodysync/sim.h"
#include <limits>

namespace bs {
static const char* kN[kScenarioCount] = {"Neutral standing", "Step left", "Step right", "Turn in place", "Squat", "Sit down / stand up", "Lie on back", "Lie on side",
                                          "Walk in place", "Walk and turn", "Lose one controller", "Lose a physical tracker", "Tracking recovery", "Sudden pose change", "Invalid input data"};
static const char* kR[kScenarioCount] = {"Нейтральная стойка", "Шаг левой ногой", "Шаг правой ногой", "Поворот на месте", "Приседание", "Сесть / встать", "Лёжа на спине", "Лёжа на боку",
                                          "Ходьба на месте", "Ходьба с поворотом", "Потеря одного контроллера", "Потеря физического трекера", "Возвращение отслеживания", "Резкое изменение позы", "Некорректные входные данные"};
static const double kD[kScenarioCount] = {4, 5, 5, 5, 6, 9, 9, 9, 6, 9, 6, 9, 8, 7, 6};
const char* scenarioName(Scenario s) { return kN[(int)s]; }
const char* scenarioNameRu(Scenario s) { return kR[(int)s]; }
double scenarioDuration(Scenario s) { return kD[(int)s]; }

static double ramp(double t, double a, double b) { return smoothstep((t - a) / (b - a)); }

SolverInput SimSource::sample(Scenario sc, double t) const {
  SolverInput in; in.t = t;
  const double E = eye_;
  Vec3 hp{0, E, 0}; double yaw = 0; Quat hq; bool lying = false; Quat lieQ;
  switch (sc) {
    case Scenario::Neutral: break;
    case Scenario::StepLeft: hp.x = -0.45 * (ramp(t, 0.5, 1.5) - ramp(t, 3.0, 4.0)); break;
    case Scenario::StepRight: hp.x = 0.45 * (ramp(t, 0.5, 1.5) - ramp(t, 3.0, 4.0)); break;
    case Scenario::TurnInPlace: yaw = kPi * ramp(t, 1.0, 3.0); break;
    case Scenario::Squat: hp.y = E * (1 - 0.42 * (ramp(t, 0.5, 2.0) - ramp(t, 3.2, 4.7))); hp.z = 0.08 * (ramp(t, 0.5, 2.0) - ramp(t, 3.2, 4.7)); break;
    case Scenario::Sit: { double k = ramp(t, 1.0, 2.8) - ramp(t, 6.0, 7.5); hp.y = E * (1 - 0.27 * k); hp.z = 0.25 * k; break; }
    case Scenario::LieBack: case Scenario::LieSide: {
      double k = ramp(t, 1.0, 4.0) - ramp(t, 6.5, 8.5);
      Vec3 a{0, 0, -1}, f = sc == Scenario::LieBack ? Vec3{0, 1, 0} : cross(a, Vec3{0, 1, 0});
      lieQ = frameFromUpFwd(a, f); lying = true;
      hq = slerp(Quat{}, lieQ, k);
      hp = {0, lerpd(E, 0.16, k * k * (3 - 2 * k)), 0.7 * k};
      break;
    }
    case Scenario::WalkInPlace: hp.y = E + (t > 1.0 && t < 5.0 ? 0.02 * std::sin(2 * kPi * 1.8 * (t - 1.0)) : 0); break;
    case Scenario::Walk: {
      double s = std::min(t, 7.0), turn = ramp(t, 3.5, 5.5);
      yaw = kPi / 2 * turn;
      double d = 1.1 * s;   // path length; heading follows the turn
      double ang = 0, x = 0, z = 0, dt = 0.02;
      for (double u = 0; u < s; u += dt) { ang = kPi / 2 * ramp(u, 3.5, 5.5); x += -std::sin(ang) * 1.1 * dt; z += -std::cos(ang) * 1.1 * dt; }
      (void)d; hp = {x, E + 0.015 * std::sin(2 * kPi * 1.9 * s), z};
      break;
    }
    default: break;
  }
  if (!lying) hq = yawQuat(yaw);
  Vec3 up = hq.up(), fwd = hq.forward(), right = hq.right();
  Vec3 neck = hp + hq.rotate({0, -0.118 * 1.75, 0.065 * 1.75});
  Quat hand = frameFromUpFwd(up, fwd);
  in.hmd = {{hp, hq}, true};
  in.lc = {{neck - right * 0.225 - up * 0.57 + fwd * 0.08, hand}, true};
  in.rc = {{neck + right * 0.225 - up * 0.57 + fwd * 0.08, hand}, true};
  // small deterministic sway keeps filters honest
  in.hmd.pose.p.x += 0.002 * std::sin(1.3 * t); in.hmd.pose.p.z += 0.002 * std::cos(0.9 * t);
  switch (sc) {
    case Scenario::LoseController: if (t > 2.0 && t < 3.5) in.rc.valid = false; break;
    case Scenario::LoseTracker: {
      in.phys[kLeftFoot] = {{{-0.10 + 0.0004 * std::sin(7 * t), 0.068, hp.z + 0.0004 * std::cos(5 * t)}, yawQuat(yaw)}, !(t > 2.5 && t < 5.0), 1.0};   // real trackers always jitter a little
      break;
    }
    case Scenario::Recovery:
      if (t > 1.0 && t < 2.5) in.lc.valid = false;
      if (t > 3.5 && t < 5.0) in.hmd.valid = false;
      break;
    case Scenario::SuddenMove:
      if (t > 2.0 && t < 2.02) in.hmd.pose.p.x += 2.0;      // single-frame glitch: must be rejected
      if (t >= 4.0) { in.hmd.pose.p.x += 1.5; in.lc.pose.p.x += 1.5; in.rc.pose.p.x += 1.5; }   // real relocalisation: accepted
      break;
    case Scenario::InvalidInput: {
      int k = (int)(t * 90);
      const double nan = std::numeric_limits<double>::quiet_NaN(), inf = std::numeric_limits<double>::infinity();
      if (k % 37 == 5) in.hmd.pose.p.x = nan;
      if (k % 41 == 7) in.lc.pose.q = {0, 0, 0, 0};
      if (k % 43 == 9) in.rc.pose.p.y = inf;
      if (k % 47 == 11) in.hmd.pose.q = {nan, 0, 0, 1};
      if (k % 53 == 13) in.lc.pose.q = {1e30, 0, 0, 0};
      break;
    }
    default: break;
  }
  return in;
}
}  // namespace bs
