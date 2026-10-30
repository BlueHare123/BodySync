// Tolerant JSON <-> config conversion: a missing or wrongly-typed key keeps its default (forward/backward compatible).
#pragma once
#include <array>
#include "config.h"

namespace bs {
NLOHMANN_JSON_SERIALIZE_ENUM(PostureMode, {{PostureMode::Auto, "auto"}, {PostureMode::Standing, "standing"}, {PostureMode::Sitting, "sitting"}, {PostureMode::Lying, "lying"}})
NLOHMANN_JSON_SERIALIZE_ENUM(SitStyle, {{SitStyle::Chair, "chair"}, {SitStyle::Upright, "upright"}, {SitStyle::Reclined, "reclined"}, {SitStyle::CrossLegged, "cross_legged"}})
NLOHMANN_JSON_SERIALIZE_ENUM(RoleAssign::Priority, {{RoleAssign::Priority::PhysicalPreferred, "physical_preferred"}, {RoleAssign::Priority::VirtualPreferred, "virtual_preferred"},
                                                    {RoleAssign::Priority::PhysicalOnly, "physical_only"}, {RoleAssign::Priority::VirtualOnly, "virtual_only"}})

template <class T> void jget(const json& j, const char* k, T& v) {
  auto it = j.find(k);
  if (it == j.end()) return;
  try { v = it->template get<T>(); } catch (...) {}
}
template <class T, size_t N> void jget(const json& j, const char* k, std::array<T, N>& v) {
  auto it = j.find(k);
  if (it == j.end() || !it->is_array()) return;
  for (size_t i = 0; i < N && i < it->size(); ++i) { try { v[i] = (*it)[i].template get<T>(); } catch (...) {} }
}
template <size_t N> void jget(const json& j, const char* k, std::array<RoleAssign, N>& v);

#define BS_TO(f) j[#f] = v.f;
#define BS_FROM(f) jget(j, #f, v.f);
#define BS_JSON(Type, FIELDS) \
  inline void to_json(json& j, const Type& v) { j = json::object(); FIELDS(BS_TO) } \
  inline void from_json(const json& j, Type& v) { if (!j.is_object()) throw std::runtime_error(#Type " is not an object"); FIELDS(BS_FROM) }

#define BODY_F(X) X(heightM) X(legScale) X(spineScale) X(armScale) X(widthScale)
#define CAL_F(X) X(valid) X(eyeHeightM) X(headPitchOffsetDeg) X(torsoYawBiasDeg) X(yawOffsetDeg) X(posOffset) X(floorOffsetM) X(pelvisOffset) X(footLateralM) X(footForwardM) X(footToeOutDeg) X(timestamp)
#define TRK_F(X) X(enabled) X(smoothing) X(pelvisSway) X(torsoYawDeadzoneDeg) X(torsoYawFollowSec) X(footLock) X(stepHeightM) X(stepDurationScale) X(walkInPlaceSensitivity) X(postureSensitivity) \
  X(kneeMaxBendDeg) X(elbowMaxBendDeg) X(spineMaxLeanDeg) X(neckTwistDeg) X(spineTwistDeg) X(maxSpeedMps) X(updateRateHz) X(poseTimeOffsetMs)
#define POSE_F(X) X(mode) X(sitStyle) X(sitEnterRatio) X(sitExitRatio) X(crouchRatio) X(lieHeightRatio) X(lieEnterUp) X(lieExitUp) X(holdTimeSec) X(transitionSec) X(standLeanDeg) X(sitLeanDeg) \
  X(sitPelvisHeightOffsetM) X(sitPelvisBackM) X(sitLegExtend) X(sitFeetSpreadM) X(kneeSplayDeg) X(lieKneeBendDeg) X(lieSideKneeBendDeg) X(lieFloorOffsetM)
#define ROLE_F(X) X(serial) X(priority)
#define MIX_F(X) X(enabled) X(blendSeconds) X(lossTimeoutSec) X(jumpSpeedMps) X(staleSeconds) X(roles)
#define PROF_F(X) X(version) X(name) X(body) X(calibration) X(tracking) X(poses) X(mixed)
#define APP_F(X) X(version) X(language) X(uiScale) X(activeProfile) X(autosave) X(publishSimulation)

BS_JSON(BodyProfile, BODY_F)
BS_JSON(Calibration, CAL_F)
BS_JSON(TrackingSettings, TRK_F)
BS_JSON(PoseSettings, POSE_F)
BS_JSON(RoleAssign, ROLE_F)
template <size_t N> void jget(const json& j, const char* k, std::array<RoleAssign, N>& v) {
  auto it = j.find(k);
  if (it == j.end() || !it->is_array()) return;
  for (size_t i = 0; i < N && i < it->size(); ++i) { try { v[i] = (*it)[i].template get<RoleAssign>(); } catch (...) {} }
}
BS_JSON(MixedSettings, MIX_F)
BS_JSON(Profile, PROF_F)
BS_JSON(AppSettings, APP_F)
}  // namespace bs
