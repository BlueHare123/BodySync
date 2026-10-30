// BodySync math: right-handed, +Y up, -Z forward, metres, radians (same convention as OpenVR).
#pragma once
#include <algorithm>
#include <cmath>
namespace bs {
constexpr double kPi = 3.14159265358979323846;
inline double deg2rad(double d) { return d * kPi / 180.0; }
inline double rad2deg(double r) { return r * 180.0 / kPi; }
inline double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline double lerpd(double a, double b, double t) { return a + (b - a) * t; }
inline double smoothstep(double t) { t = clampd(t, 0, 1); return t * t * (3 - 2 * t); }
inline double wrapPi(double a) { a = std::fmod(a + kPi, 2 * kPi); if (a < 0) a += 2 * kPi; return a - kPi; }
// exponential approach with time constant tau (tau<=0 -> instant)
inline double expAlpha(double dt, double tau) { return tau <= 1e-9 ? 1.0 : 1.0 - std::exp(-dt / tau); }

struct Vec3 {
  double x = 0, y = 0, z = 0;
  Vec3() = default;
  Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
  Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
  Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }
  Vec3 operator-() const { return {-x, -y, -z}; }
  Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
  bool isFinite() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }
  double length() const { return std::sqrt(x * x + y * y + z * z); }
  double lengthXZ() const { return std::sqrt(x * x + z * z); }
  Vec3 normalized(const Vec3& fallback = {0, 1, 0}) const {
    double l = length();
    return (l > 1e-12 && std::isfinite(l)) ? *this / l : fallback;
  }
};
inline Vec3 operator*(double s, const Vec3& v) { return v * s; }
inline double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline Vec3 lerp(const Vec3& a, const Vec3& b, double t) { return a + (b - a) * t; }
inline double distance(const Vec3& a, const Vec3& b) { return (a - b).length(); }
inline Vec3 withY(Vec3 v, double y) { v.y = y; return v; }
// remove the component of v along unit vector n
inline Vec3 rejectFrom(const Vec3& v, const Vec3& n) { return v - n * dot(v, n); }
// p = anchor + dir(p-anchor)*len : keeps a segment at exactly `len`
inline Vec3 constrainDistance(const Vec3& anchor, const Vec3& p, double len, const Vec3& fallbackDir = {0, -1, 0}) {
  return anchor + (p - anchor).normalized(fallbackDir) * len;
}

struct Quat {
  double w = 1, x = 0, y = 0, z = 0;
  Quat() = default;
  Quat(double w_, double x_, double y_, double z_) : w(w_), x(x_), y(y_), z(z_) {}
  bool isFinite() const { return std::isfinite(w) && std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }
  double norm() const { return std::sqrt(w * w + x * x + y * y + z * z); }
  // identity if degenerate / non-finite
  Quat normalized() const {
    double n = norm();
    if (!(n > 1e-9) || !std::isfinite(n)) return {};
    return {w / n, x / n, y / n, z / n};
  }
  Quat conj() const { return {w, -x, -y, -z}; }
  Quat operator*(const Quat& o) const {
    return {w * o.w - x * o.x - y * o.y - z * o.z, w * o.x + x * o.w + y * o.z - z * o.y,
            w * o.y - x * o.z + y * o.w + z * o.x, w * o.z + x * o.y - y * o.x + z * o.w};
  }
  Vec3 rotate(const Vec3& v) const {  // v' = q v q^-1
    Vec3 u{x, y, z};
    Vec3 t = cross(u, v) * 2.0;
    return v + t * w + cross(u, t);
  }
  Vec3 right() const { return rotate({1, 0, 0}); }
  Vec3 up() const { return rotate({0, 1, 0}); }
  Vec3 forward() const { return rotate({0, 0, -1}); }
};
inline double dot(const Quat& a, const Quat& b) { return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z; }
inline Quat axisAngle(const Vec3& axis, double ang) {
  Vec3 a = axis.normalized({0, 1, 0});
  double s = std::sin(ang / 2);
  return {std::cos(ang / 2), a.x * s, a.y * s, a.z * s};
}
inline Quat yawQuat(double yaw) { return axisAngle({0, 1, 0}, yaw); }
// heading of the forward vector about +Y (0 = facing -Z, positive = turning left)
inline double yawOf(const Vec3& fwd) { return std::atan2(-fwd.x, -fwd.z); }
inline double yawOf(const Quat& q) { return yawOf(q.forward()); }
inline Quat slerp(Quat a, Quat b, double t) {  // shortest path
  a = a.normalized(); b = b.normalized();
  double d = dot(a, b);
  if (d < 0) { b = {-b.w, -b.x, -b.y, -b.z}; d = -d; }
  if (d > 0.9995) {
    Quat r{lerpd(a.w, b.w, t), lerpd(a.x, b.x, t), lerpd(a.y, b.y, t), lerpd(a.z, b.z, t)};
    return r.normalized();
  }
  double th = std::acos(clampd(d, -1, 1)), s = std::sin(th);
  double wa = std::sin((1 - t) * th) / s, wb = std::sin(t * th) / s;
  return Quat{wa * a.w + wb * b.w, wa * a.x + wb * b.x, wa * a.y + wb * b.y, wa * a.z + wb * b.z}.normalized();
}
inline double angleBetween(Quat a, Quat b) {
  a = a.normalized(); b = b.normalized();
  return 2.0 * std::acos(clampd(std::fabs(dot(a, b)), 0, 1));
}
// quaternion from orthonormal basis columns (X right, Y up, Z back)
inline Quat fromBasis(const Vec3& X, const Vec3& Y, const Vec3& Z) {
  double m00 = X.x, m01 = Y.x, m02 = Z.x, m10 = X.y, m11 = Y.y, m12 = Z.y, m20 = X.z, m21 = Y.z, m22 = Z.z;
  double tr = m00 + m11 + m22;
  Quat q;
  if (tr > 0) { double s = std::sqrt(tr + 1.0) * 2; q = {0.25 * s, (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s}; }
  else if (m00 > m11 && m00 > m22) { double s = std::sqrt(1.0 + m00 - m11 - m22) * 2; q = {(m21 - m12) / s, 0.25 * s, (m01 + m10) / s, (m02 + m20) / s}; }
  else if (m11 > m22) { double s = std::sqrt(1.0 + m11 - m00 - m22) * 2; q = {(m02 - m20) / s, (m01 + m10) / s, 0.25 * s, (m12 + m21) / s}; }
  else { double s = std::sqrt(1.0 + m22 - m00 - m11) * 2; q = {(m10 - m01) / s, (m02 + m20) / s, (m12 + m21) / s, 0.25 * s}; }
  return q.normalized();
}
// body-segment frame: +Y = `up`, -Z = `fwd` (re-orthogonalised; falls back if parallel)
inline Quat frameFromUpFwd(const Vec3& up, const Vec3& fwd) {
  Vec3 Y = up.normalized({0, 1, 0});
  Vec3 F = rejectFrom(fwd, Y);
  if (F.length() < 1e-6) {
    F = rejectFrom({0, 0, -1}, Y);
    if (F.length() < 1e-6) F = rejectFrom({1, 0, 0}, Y);
  }
  Vec3 Z = (-F).normalized({0, 0, 1});
  Vec3 X = cross(Y, Z);
  return fromBasis(X, Y, Z);
}

struct Pose {
  Vec3 p;
  Quat q;
  bool isFinite() const { return p.isFinite() && q.isFinite(); }
  Vec3 transformPoint(const Vec3& v) const { return p + q.rotate(v); }
  Pose inverse() const { Quat qi = q.conj(); return {qi.rotate(-p), qi}; }
  Pose operator*(const Pose& o) const { return {p + q.rotate(o.p), (q * o.q).normalized()}; }
};
inline Pose lerpPose(const Pose& a, const Pose& b, double t) { return {lerp(a.p, b.p, t), slerp(a.q, b.q, t)}; }

// OpenVR HmdMatrix34_t (row-major 3x4, translation in column 3) <-> Pose
inline Pose poseFromMat34(const float m[3][4]) {
  Vec3 X{m[0][0], m[1][0], m[2][0]}, Y{m[0][1], m[1][1], m[2][1]}, Z{m[0][2], m[1][2], m[2][2]};
  return {{m[0][3], m[1][3], m[2][3]}, fromBasis(X.normalized({1, 0, 0}), Y.normalized({0, 1, 0}), Z.normalized({0, 0, 1}))};
}
inline void poseToMat34(const Pose& p, float m[3][4]) {
  Vec3 X = p.q.right(), Y = p.q.up(), Z = p.q.rotate({0, 0, 1});
  m[0][0] = (float)X.x; m[1][0] = (float)X.y; m[2][0] = (float)X.z;
  m[0][1] = (float)Y.x; m[1][1] = (float)Y.y; m[2][1] = (float)Y.z;
  m[0][2] = (float)Z.x; m[1][2] = (float)Z.y; m[2][2] = (float)Z.z;
  m[0][3] = (float)p.p.x; m[1][3] = (float)p.p.y; m[2][3] = (float)p.p.z;
}

// Two-bone IK. `end` is clamped to the reachable range (min range from max bend angle).
// pole = direction the middle joint (knee/elbow) bulges towards. Segment lengths are preserved exactly.
inline Vec3 twoBoneIK(const Vec3& root, Vec3& end, double l1, double l2, const Vec3& pole, double maxBendRad) {
  Vec3 d = end - root;
  double dist = d.length();
  double maxR = (l1 + l2) * 0.9999;
  double interior = kPi - clampd(maxBendRad, 0.05, kPi - 0.05);  // interior angle at the joint
  double minR = std::sqrt(std::max(1e-12, l1 * l1 + l2 * l2 - 2 * l1 * l2 * std::cos(interior)));
  minR = std::max(minR, std::fabs(l1 - l2) + 1e-4);
  Vec3 dir = dist > 1e-9 ? d / dist : pole.normalized({0, 0, -1});
  dist = clampd(dist, minR, maxR);
  end = root + dir * dist;
  double a = (l1 * l1 - l2 * l2 + dist * dist) / (2 * dist);
  double h = std::sqrt(std::max(0.0, l1 * l1 - a * a));
  Vec3 pp = rejectFrom(pole, dir);
  if (pp.length() < 1e-6) { pp = rejectFrom({0, 0, -1}, dir); if (pp.length() < 1e-6) pp = rejectFrom({1, 0, 0}, dir); }
  return root + dir * a + pp.normalized() * h;
}
// chord (root->end) of a two-bone chain bent by `bendRad` at the middle joint
inline double chordForBend(double l1, double l2, double bendRad) {
  return std::sqrt(std::max(1e-12, l1 * l1 + l2 * l2 - 2 * l1 * l2 * std::cos(kPi - bendRad)));
}
}  // namespace bs
