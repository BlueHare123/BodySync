// Reads HMD / controllers / physical trackers through the public OpenVR client API (openvr_api.dll, loaded at run time so the
// app starts and explains itself even when the runtime is missing). Background application: never launches SteamVR.
#pragma once
#include <string>
#include <vector>
#include "bodysync/solver.h"

namespace bs {
struct PhysDevice { std::string serial, system, model; bool connected = false, poseValid = false; double quality = 0; Pose pose; int index = -1; };
struct VrFrame { DeviceSample hmd, lc, rc; std::vector<PhysDevice> trackers; };
enum class VrState { NoRuntimeDll, NotRunning, Connected, Error };

class VrSource {
 public:
  VrSource(); ~VrSource();
  bool poll(VrFrame& out);                 // true while connected; retries a connection every 2 s otherwise
  VrState state() const;
  std::string status() const;              // human readable, English (UI translates by key)
  std::string lastError() const;
 private:
  struct Impl; Impl* d_;
};
}  // namespace bs
