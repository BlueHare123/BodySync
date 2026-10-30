// BodySync SteamVR driver: registers 8 GenericTracker devices and publishes the poses received from BodySync.exe
// over a local named pipe. The driver never reads other processes' memory and never injects anything.
#include <openvr_driver.h>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#include "bodysync/config.h"
#include "bodysync/ipc_pipe.h"

#ifndef BODYSYNC_VERSION
#define BODYSYNC_VERSION "0.0.0"
#endif
using namespace vr;
namespace {
constexpr uint32_t kDriverVersion = 100;
uint64_t nowMs() { return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

class Provider;
class TrackerDevice : public ITrackedDeviceServerDriver {
 public:
  explicit TrackerDevice(int role) : role_(role) {}
  EVRInitError Activate(uint32_t id) override {
    id_ = id;
    PropertyContainerHandle_t c = VRProperties()->TrackedDeviceToPropertyContainer(id);
    VRProperties()->SetStringProperty(c, Prop_TrackingSystemName_String, "bodysync");
    VRProperties()->SetStringProperty(c, Prop_ModelNumber_String, "BodySync Virtual Tracker");
    VRProperties()->SetStringProperty(c, Prop_SerialNumber_String, bs::roleSerial(role_));
    VRProperties()->SetStringProperty(c, Prop_ManufacturerName_String, "BodySync");
    VRProperties()->SetStringProperty(c, Prop_InputProfilePath_String, "{bodysync}/input/bodysync_tracker_profile.json");
    VRProperties()->SetBoolProperty(c, Prop_DeviceProvidesBatteryStatus_Bool, false);
    VRProperties()->SetBoolProperty(c, Prop_DeviceCanPowerOff_Bool, false);
    VRProperties()->SetBoolProperty(c, Prop_NeverTracked_Bool, false);
    return VRInitError_None;
  }
  void Deactivate() override { id_ = k_unTrackedDeviceIndexInvalid; }
  void EnterStandby() override {}
  void* GetComponent(const char*) override { return nullptr; }
  void DebugRequest(const char* req, char* resp, uint32_t n) override { if (n) std::snprintf(resp, n, "BodySync tracker %s (request '%s')", bs::roleSerial(role_), req ? req : ""); }
  DriverPose_t GetPose() override { return last_; }
  uint32_t id() const { return id_; }
  bool activated() const { return id_ != k_unTrackedDeviceIndexInvalid; }
  void publish(const bs::ipc::Pose8* p, float timeOffset, bool connected) {
    if (!activated()) return;
    DriverPose_t d{};
    d.qWorldFromDriverRotation = {1, 0, 0, 0}; d.qDriverFromHeadRotation = {1, 0, 0, 0};   // identity (a zeroed quaternion is invalid)
    d.willDriftInYaw = false; d.shouldApplyHeadModel = false;
    d.deviceIsConnected = connected;
    bool valid = connected && p && p->valid && p->active;
    d.poseIsValid = valid; d.result = valid ? TrackingResult_Running_OK : TrackingResult_Uninitialized;
    d.poseTimeOffset = timeOffset;
    if (p) {
      for (int i = 0; i < 3; ++i) { d.vecPosition[i] = p->pos[i]; d.vecVelocity[i] = p->vel[i]; d.vecAngularVelocity[i] = p->angVel[i]; }
      d.qRotation = {p->rot[0], p->rot[1], p->rot[2], p->rot[3]};
    } else d.qRotation = {1, 0, 0, 0};
    last_ = d;
    VRServerDriverHost()->TrackedDevicePoseUpdated(id_, d, sizeof(DriverPose_t));
    published_ = valid;
  }
  bool published() const { return published_; }
 private:
  int role_; uint32_t id_ = k_unTrackedDeviceIndexInvalid; DriverPose_t last_{}; bool published_ = false;
};

class Provider : public IServerTrackedDeviceProvider {
 public:
  EVRInitError Init(IVRDriverContext* ctx) override {
    try {
      VR_INIT_SERVER_DRIVER_CONTEXT(ctx);
      inited_ = true;
      log(std::string("BodySync driver ") + BODYSYNC_VERSION + " starting");
      if (!VRSettings()->GetBool("driver_bodysync", "enable")) { log("disabled in settings (driver_bodysync/enable=false)"); flush(); return VRInitError_None; }
      bool roles = VRSettings()->GetBool("driver_bodysync", "assignTrackerRoles");
      for (int r = 0; r < bs::kRoleCount; ++r) {
        devices_[r] = std::make_unique<TrackerDevice>(r);
        if (!VRServerDriverHost()->TrackedDeviceAdded(bs::roleSerial(r), TrackedDeviceClass_GenericTracker, devices_[r].get())) { log(std::string("TrackedDeviceAdded failed for ") + bs::roleSerial(r)); devices_[r].reset(); continue; }
        ++registered_;
        if (roles) assignRole(r);
      }
      bs::ipc::ServerCallbacks cb;
      cb.driverVersion = kDriverVersion; cb.trackerCount = (uint32_t)registered_; cb.enabledMask = mask_; cb.runtime = "SteamVR";
      cb.log = [this](const std::string& s) { log(s); };
      cb.onConnected = [this] { std::lock_guard<std::mutex> lk(m_); connected_ = true; };
      cb.onDisconnected = [this] { std::lock_guard<std::mutex> lk(m_); connected_ = false; dropPending_ = true; havePending_ = false; };
      cb.onPoses = [this](const bs::ipc::PoseFrame& f) { std::lock_guard<std::mutex> lk(m_); pending_ = f; havePending_ = true; lastRx_ = nowMs(); ++framesRx_; };
      cb.onConfig = [this](uint32_t m) { std::lock_guard<std::mutex> lk(m_); mask_ = m; };
      cb.getStatus = [this] { return status(); };
      server_ = std::make_unique<bs::ipc::PipeServer>(bs::ipc::kPipeName, cb);
      server_->start();
      log("registered " + std::to_string(registered_) + " trackers, IPC server started");
      flush();
      return VRInitError_None;
    } catch (const std::exception& e) { try { log(std::string("Init failed: ") + e.what()); flush(); } catch (...) {} return VRInitError_Driver_Failed; }
    catch (...) { return VRInitError_Driver_Failed; }
  }
  void Cleanup() override {
    if (!inited_) return;   // Cleanup without a successful Init (or twice) is harmless
    inited_ = false;
    if (server_) { server_->stop(); server_.reset(); }
    log("BodySync driver stopping"); flush();
    for (auto& d : devices_) d.reset();
    VR_CLEANUP_SERVER_DRIVER_CONTEXT();
  }
  const char* const* GetInterfaceVersions() override { return k_InterfaceVersions; }
  // Main-thread pump: IPC thread only stores the latest frame; all SteamVR calls happen here.
  void RunFrame() override {
    if (!inited_) return;
    bs::ipc::PoseFrame f; bool have = false, drop = false, conn; uint32_t mask; uint64_t last;
    { std::lock_guard<std::mutex> lk(m_); if (havePending_) { f = std::move(pending_); havePending_ = false; have = true; } drop = dropPending_; dropPending_ = false; conn = connected_; mask = mask_; last = lastRx_; }
    if (have) {
      std::array<const bs::ipc::Pose8*, bs::kRoleCount> by{}; for (auto& p : f.poses) if (p.id < bs::kRoleCount) by[p.id] = &p;
      for (int r = 0; r < bs::kRoleCount; ++r) {
        if (!devices_[r]) continue;
        bool en = (mask >> r) & 1; const bs::ipc::Pose8* p = by[r];
        devices_[r]->publish(p, f.timeOffsetSec, en && p && p->active);   // a device is "connected" only while the app says it is active
        if (devices_[r]->published()) ++posesPublished_;
      }
      lastPublish_ = nowMs();
    } else if (conn && lastPublish_ && nowMs() - last > 500 && nowMs() - lastPublish_ > 500) { drop = true; lastPublish_ = 0; log("no poses for 500 ms - trackers set to lost"); }   // app alive but stalled
    if (drop) for (auto& d : devices_) if (d) d->publish(nullptr, 0, false);   // app gone / stalled: trackers disappear instead of freezing
    flush();
  }
  bool ShouldBlockStandbyMode() override { return false; }
  void EnterStandby() override {}
  void LeaveStandby() override {}

  bs::ipc::StatusMsg status() {
    std::lock_guard<std::mutex> lk(m_); bs::ipc::StatusMsg s{};
    s.flags = (connected_ ? (uint32_t)bs::ipc::kClientConnected : 0u) | (registered_ == bs::kRoleCount ? (uint32_t)bs::ipc::kDevicesRegistered : 0u) | (lastRx_ && nowMs() - lastRx_ < 500 ? (uint32_t)bs::ipc::kReceivingPoses : 0u);
    s.framesReceived = framesRx_; s.posesPublished = posesPublished_;
    for (int r = 0; r < bs::kRoleCount; ++r) if (devices_[r] && devices_[r]->published()) s.activeMask |= 1u << r;
    return s;
  }
 private:
  void assignRole(int r) {   // best effort, never overrides a role the user already chose
    char buf[64] = {0}; std::string name = bs::roleSerial(r);
    for (const char* prefix : {"/devices/bodysync/", "/devices/BodySync/"}) {
      std::string key = prefix + name; EVRSettingsError e = VRSettingsError_None;
      VRSettings()->GetString(k_pch_Trackers_Section, key.c_str(), buf, sizeof buf, &e);
      if (e != VRSettingsError_None || !buf[0]) VRSettings()->SetString(k_pch_Trackers_Section, key.c_str(), bs::roleSteamVRName(r));
    }
  }
  void log(const std::string& s) { std::lock_guard<std::mutex> lk(logM_); logs_.push_back(s); if (logs_.size() > 200) logs_.erase(logs_.begin()); }   // thread-safe: delivered from RunFrame
  void flush() { std::vector<std::string> l; { std::lock_guard<std::mutex> lk(logM_); l.swap(logs_); } if (VRDriverLog()) for (auto& s : l) VRDriverLog()->Log(("[BodySync] " + s + "\n").c_str()); }

  bool inited_ = false;
  std::array<std::unique_ptr<TrackerDevice>, bs::kRoleCount> devices_; int registered_ = 0;
  std::unique_ptr<bs::ipc::PipeServer> server_;
  std::mutex m_, logM_; std::vector<std::string> logs_;
  bs::ipc::PoseFrame pending_; bool havePending_ = false, dropPending_ = false, connected_ = false; uint32_t mask_ = 0xFF;
  uint64_t lastRx_ = 0, lastPublish_ = 0, framesRx_ = 0, posesPublished_ = 0;
};
Provider g_provider;
}  // namespace

extern "C" __declspec(dllexport) void* HmdDriverFactory(const char* name, int* returnCode) {
  if (name && std::strcmp(name, IServerTrackedDeviceProvider_Version) == 0) return &g_provider;
  if (returnCode) *returnCode = VRInitError_Init_InterfaceNotFound;
  return nullptr;
}
