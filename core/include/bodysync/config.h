// BodySync configuration model. Every field here is read by the solver / mixer / engine (no decorative settings).
#pragma once
#include <array>
#include <string>
#include <vector>
#include "json_fwd.h"
#include "math.h"

namespace bs {
constexpr int kRoleCount = 8;
enum Role : int { kWaist, kChest, kLeftElbow, kRightElbow, kLeftKnee, kRightKnee, kLeftFoot, kRightFoot };
const char* roleName(int r);          // "Waist" ...
const char* roleNameRu(int r);        // "Таз" ...
const char* roleSerial(int r);        // "BodySync_Waist" (stable device identity)
const char* roleSteamVRName(int r);   // "TrackerRole_Waist" (SteamVR tracker role string)
constexpr int kConfigVersion = 1;

struct BodyProfile {            // body proportions (all scale factors multiply anthropometric defaults)
  double heightM = 1.75;
  double legScale = 1, spineScale = 1, armScale = 1, widthScale = 1;
};
struct Calibration {
  bool valid = false;
  double eyeHeightM = 0;          // measured standing eye height (0 = derive from heightM)
  double headPitchOffsetDeg = 0;  // headset tilt when looking straight ahead
  double torsoYawBiasDeg = 0;     // torso yaw relative to head yaw measured in T-pose
  double yawOffsetDeg = 0;        // alignment: rotates virtual body about the head
  std::array<double, 3> posOffset{0, 0, 0};   // alignment: world-space offset of virtual body (m)
  double floorOffsetM = 0;        // floor height in tracking space
  std::array<double, 3> pelvisOffset{0, 0, 0};  // pelvis tracker offset in pelvis frame: right, up, forward (m)
  double footLateralM = 0.02;     // feet stance: extra outward offset per foot
  double footForwardM = 0.0;
  double footToeOutDeg = 8;
  std::string timestamp;
};
struct TrackingSettings {
  std::array<bool, kRoleCount> enabled{true, true, true, true, true, true, true, true};
  double smoothing = 0.45;              // 0..1
  double pelvisSway = 0.5;              // 0 pelvis stays over feet .. 1 pelvis follows the neck
  double torsoYawDeadzoneDeg = 35;      // head can turn this far before the torso follows
  double torsoYawFollowSec = 3.0;       // slow relax of torso yaw to head yaw
  double footLock = 0.6;                // 0..1 how far a planted foot is kept before stepping
  double stepHeightM = 0.12;
  double stepDurationScale = 1.0;
  double walkInPlaceSensitivity = 0.5;  // 0..1
  double postureSensitivity = 0.5;      // 0..1 (higher = switches posture faster)
  double kneeMaxBendDeg = 145, elbowMaxBendDeg = 145, spineMaxLeanDeg = 60, neckTwistDeg = 75, spineTwistDeg = 40;
  double maxSpeedMps = 12;              // jump limiter for inputs and outputs
  int updateRateHz = 90;
  double poseTimeOffsetMs = 0;          // sent to the driver as DriverPose_t::poseTimeOffset (negative = older pose)
};
enum class PostureMode { Auto, Standing, Sitting, Lying };
enum class SitStyle { Chair, Upright, Reclined, CrossLegged };   // how the seated body is shaped (the posture itself is still detected from the data)
struct PoseSettings {
  PostureMode mode = PostureMode::Auto;
  SitStyle sitStyle = SitStyle::Chair;                  // v1.0.0: chair (default) / upright / reclined / cross-legged on the floor
  double sitEnterRatio = 0.80, sitExitRatio = 0.88;   // head height / standing eye height
  double crouchRatio = 0.62;                            // below this a stationary user is crouching (or sitting on the floor)
  double lieHeightRatio = 0.70, lieEnterUp = 0.70, lieExitUp = 0.85;   // v1.0.0: raised (a bed puts the head at 0.45-0.55 of eye height; neck flexion on a pillow tilts the head up-vector to ~0.65)
  double holdTimeSec = 0.6, transitionSec = 0.7;
  double standLeanDeg = 0, sitLeanDeg = 0;           // torso lean corrections (+ = leans forward)
  double sitPelvisHeightOffsetM = 0, sitPelvisBackM = 0.08, sitLegExtend = 0.75, sitFeetSpreadM = 0.0;
  double kneeSplayDeg = 10;                            // knees point this far outward of the foot direction
  double lieKneeBendDeg = 15, lieSideKneeBendDeg = 40, lieFloorOffsetM = 0.10;
};
struct RoleAssign {
  std::string serial;     // physical tracker serial ("" = virtual only)
  enum class Priority { PhysicalPreferred, VirtualPreferred, PhysicalOnly, VirtualOnly } priority = Priority::PhysicalPreferred;
};
struct MixedSettings {
  bool enabled = true;
  double blendSeconds = 0.4, lossTimeoutSec = 0.25, jumpSpeedMps = 8.0, staleSeconds = 1.0;
  std::array<RoleAssign, kRoleCount> roles{};
};
struct Profile {
  int version = kConfigVersion;
  std::string name = "Default";
  BodyProfile body;
  Calibration calibration;
  TrackingSettings tracking;
  PoseSettings poses;
  MixedSettings mixed;
};
struct AppSettings {
  int version = kConfigVersion;
  std::string language = "en";      // "en" | "ru" | "de" | "fr" | "es"
  double uiScale = 1.0;             // interface scale, 0.75 .. 1.75
  std::string activeProfile = "Default";
  bool autosave = true;
  bool publishSimulation = false;   // testing only: let simulated poses reach SteamVR
};

// Clamp every value into its valid range; NaN/inf -> defaults. Returns number of fields corrected.
int sanitize(Profile& p);
// Profiles written before v1.0.0 that still carry the OLD default lying thresholds (0.45 / 0.5 / 0.7) get the new defaults; any other value is the user's and is kept.
void migratePoseDefaults(PoseSettings& p);
int sanitize(BodyProfile& p);
int sanitize(Calibration& p);
int sanitize(PoseSettings& p);
int sanitize(AppSettings& p);
std::string safeName(const std::string& s);   // file-system safe preset name ("" if unusable)

// anthropometric model derived from BodyProfile
struct Proportions {
  double height, eyeHeight, upperLeg, lowerLeg, ankleHeight, spine, hipHalf, shoulderHalf, upperArm, foreArm;
  double neckToEyeUp, neckToEyeBack;   // C7 -> HMD offset in head frame (m)
  static Proportions from(const BodyProfile& b);
  double legReach() const { return upperLeg + lowerLeg; }
};
}  // namespace bs
