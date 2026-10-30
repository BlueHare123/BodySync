// Noise filters. One-Euro style adaptive low-pass for positions and orientations (slerp based, no Euler angles).
#pragma once
#include "math.h"
namespace bs {
struct FilterParams { double minCutoff = 2.0; double beta = 4.0; };
// smoothing 0..1 -> cutoff 30 Hz (almost raw) .. 0.7 Hz (very smooth), exponential interpolation
inline FilterParams filterFromSmoothing(double s) {
  s = clampd(s, 0, 1);
  return {30.0 * std::pow(0.7 / 30.0, s), 4.0};
}
inline double lpAlpha(double cutoff, double dt) {
  double tau = 1.0 / (2 * kPi * std::max(cutoff, 1e-3));
  return 1.0 / (1.0 + tau / std::max(dt, 1e-6));
}
class OneEuroVec3 {
 public:
  Vec3 apply(const Vec3& x, double dt, const FilterParams& fp) {
    if (!init_ || dt <= 0 || dt > 0.25) { y_ = x; prev_ = x; speed_ = 0; init_ = true; return y_; }
    double raw = (x - prev_).length() / dt;
    speed_ += lpAlpha(1.0, dt) * (raw - speed_);
    prev_ = x;
    y_ = y_ + (x - y_) * lpAlpha(fp.minCutoff + fp.beta * speed_, dt);
    return y_;
  }
  void reset() { init_ = false; }
 private:
  Vec3 y_, prev_; double speed_ = 0; bool init_ = false;
};
class OneEuroQuat {
 public:
  Quat apply(const Quat& x, double dt, const FilterParams& fp) {
    Quat xn = x.normalized();
    if (!init_ || dt <= 0 || dt > 0.25) { y_ = xn; prev_ = xn; speed_ = 0; init_ = true; return y_; }
    double raw = angleBetween(xn, prev_) / dt;
    speed_ += lpAlpha(1.0, dt) * (raw - speed_);
    prev_ = xn;
    y_ = slerp(y_, xn, lpAlpha(fp.minCutoff + fp.beta * 0.25 * speed_, dt));
    return y_;
  }
  void reset() { init_ = false; }
 private:
  Quat y_, prev_; double speed_ = 0; bool init_ = false;
};
struct PoseFilter {
  OneEuroVec3 pos; OneEuroQuat rot;
  Pose apply(const Pose& p, double dt, const FilterParams& fp) { return {pos.apply(p.p, dt, fp), rot.apply(p.q, dt, fp)}; }
  void reset() { pos.reset(); rot.reset(); }
};
}  // namespace bs
