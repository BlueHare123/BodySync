// Full-body pose estimator: HMD + two controllers (+ optional physical trackers) -> 8 body points.
#pragma once
#include <array>
#include "config.h"
#include "filters.h"
#include "math.h"

namespace bs {
struct DeviceSample { Pose pose; bool valid = false; double quality = 1.0; };
struct SolverInput {
  double t = 0;                       // monotonic seconds
  DeviceSample hmd, lc, rc;
  std::array<DeviceSample, kRoleCount> phys{};   // physical trackers already assigned to roles (and gated by the mixer)
};
enum class Posture { Standing, Sitting, LyingBack, LyingFront, LyingLeft, LyingRight };
const char* postureName(Posture p);
inline bool isLying(Posture p) { return p >= Posture::LyingBack; }
enum class Source { None, Virtual, Physical, Blend };
const char* sourceName(Source s);
enum class Health { Ok, ControllerLost, HmdLost };

struct TrackerOut {
  Pose pose; Vec3 vel, angVel;
  bool valid = false;        // pose may be published
  bool active = false;       // role enabled and device should appear connected
  double confidence = 0;     // 0..1 (estimates are never measurements)
  Source src = Source::None;
  double physWeight = 0;     // mixer blend 0 virtual .. 1 physical
};
struct Skeleton {            // world-space joints before per-tracker output conditioning
  std::array<Pose, kRoleCount> j{};
  Vec3 head, neck, shL, shR, hipL, hipR, handL, handR;
};
struct SolverDiag {
  double headRatio = 0, hipHeight = 0, leanDeg = 0, speedXZ = 0, torsoYawDeg = 0, pelvisYawDeg = 0, maxSegErr = 0, bobRms = 0, candidateT = 0;
  Posture candidate = Posture::Standing;
  std::array<double, 3> sinceGood{};     // hmd, left, right
  std::array<int, 3> rejected{};         // total rejected samples
  int nonFinite = 0;
};
struct SolverOutput {
  std::array<TrackerOut, kRoleCount> tr{};
  Skeleton skel;
  Posture posture = Posture::Standing;
  double transition = 1;                 // 1 = settled
  Health health = Health::HmdLost;
  bool hmdOk = false, lcOk = false, rcOk = false;
  bool footContact[2] = {true, true};
  bool walkInPlace = false;
  SolverDiag diag;
};

class Solver {
 public:
  Solver() { configure(Profile{}); }
  void configure(const Profile& p);       // may be called at any time (tracking thread)
  void reset();
  SolverOutput update(const SolverInput& in);
  const Profile& profile() const { return cfg_; }
 private:
  struct Cond { Pose last; bool have = false; double sinceGood = 1e9; int rejects = 0; int rejectedTotal = 0; bool ok = false; Vec3 vel; };
  struct Foot { Vec3 pos; double yaw = 0, t = 0, dur = 0.3, lift = 0.1, yawFrom = 0, pitch = 0; Vec3 from; bool stepping = false, contact = true; };
  void condition(Cond& c, const DeviceSample& s, double dt, double holdSec);
  void classify(double dt, const Quat& headQ, const Vec3& headPos);
  Skeleton genStanding(const SolverInput& in, double dt, const Vec3& neck, const Pose& hmd, const Quat& headQ);
  Skeleton genSitting(const SolverInput& in, const Vec3& neck, const Pose& hmd, const Quat& headQ);
  Skeleton genLying(const SolverInput& in, const Vec3& neck, const Pose& hmd, const Quat& headQ);
  void solveArms(Skeleton& s, const SolverInput& in, const Quat& chestQ);
  Vec3 pickElbow(int i, const Vec3& sh, Vec3& end, const Quat& chestQ, const Vec3& neck, const Vec3& spineDir);
  void solveLegs(Skeleton& s, const SolverInput& in, const Pose& pelvis, const std::array<Pose, 2>& feet, const std::array<Vec3, 2>& poleDirs);
  double handYawCue(double chestYaw, const Vec3& neck, double& weight) const;
  void updateBob(double t, double dt, double headY, double speed);
  Vec3 flatFwd(double yaw) const { return {-std::sin(yaw), 0, -std::cos(yaw)}; }
  Vec3 flatRight(double yaw) const { return {std::cos(yaw), 0, -std::sin(yaw)}; }
  Pose alignFwd(const Pose& p) const;
  Pose alignInv(const Pose& p) const;

  Profile cfg_; Proportions P_; FilterParams fp_; double dt_ = 1.0 / 90, segErr_ = 0;
  Cond hmd_, lc_, rc_;
  bool haveT_ = false; double lastT_ = 0, now_ = 0;
  Vec3 pivot_;
  // posture machine
  Posture posture_ = Posture::Standing, cand_ = Posture::Standing; double candT_ = 0, lowStillT_ = 0, transT_ = 1;
  Skeleton fromSkel_, lastSkel_; bool haveSkel_ = false;
  double headRatio_ = 1, floorY_ = 0, refEye_ = 1.6;
  Vec3 upLp_{0, 1, 0}, fwdLp_{0, 0, -1}, rightLp_{1, 0, 0}; bool haveLp_ = false;   // low-passed head axes for posture detection
  Vec3 headVel_, prevHead_; double hs_ = 0, vs_ = 0; bool haveHead_ = false;
  // torso
  bool yawInit_ = false; double chestYaw_ = 0, pelvisYaw_ = 0;
  double handBlend_[2] = {1, 1};
  Vec3 elbowPrev_[2]; bool haveElbow_[2] = {false, false};   // elbow continuity between frames
  Vec3 lieAxis_{0, 0, -1};
  // feet
  Foot foot_[2]; bool feetInit_ = false; double airUntil_ = -1;
  // walk in place
  double bobLp_ = 0, bobRms2_ = 0, prevHp_ = 0, lastEv_ = -10, wipUntil_ = -1; int bobEvents_ = 0, nextFoot_ = 0; bool haveBob_ = false; int wipTrigger_ = -1;
  // outputs
  std::array<PoseFilter, kRoleCount> filt_; std::array<Pose, kRoleCount> prevOut_{}; std::array<bool, kRoleCount> havePrev_{};
  std::array<Vec3, kRoleCount> velLp_; std::array<Vec3, kRoleCount> angLp_;
  double hmdLostFor_ = 0, resumeT_ = 1; std::array<Pose, kRoleCount> heldOut_{}; bool haveHeld_ = false;
  int nonFinite_ = 0; SolverDiag diag_; bool contact_[2] = {true, true};
};
}  // namespace bs
