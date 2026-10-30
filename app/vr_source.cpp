#include "vr_source.h"
#include <windows.h>
#include <openvr.h>
#include <chrono>
#include <map>
#include "bodysync/fsutil.h"
#include "bodysync/log.h"

namespace bs {
namespace {
using InitFn = uint32_t (*)(vr::EVRInitError*, vr::EVRApplicationType);
using ShutFn = void (*)();
using IfaceFn = void* (*)(const char*, vr::EVRInitError*);
using DescFn = const char* (*)(vr::EVRInitError);
double nowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
}  // namespace

struct VrSource::Impl {
  HMODULE dll = nullptr; InitFn init = nullptr; ShutFn shut = nullptr; IfaceFn iface = nullptr; DescFn desc = nullptr;
  vr::IVRSystem* sys = nullptr; VrState st = VrState::NotRunning; std::string status = "SteamVR is not running", err;
  double nextTry = 0; bool dllTried = false;
  struct Info { std::string serial, system, model; double read = -1e9; };
  std::map<int, Info> info;

  void loadDll() {
    dllTried = true;
    wchar_t p[MAX_PATH]; GetModuleFileNameW(nullptr, p, MAX_PATH);
    std::filesystem::path dir = std::filesystem::path(p).parent_path();
    dll = LoadLibraryW((dir / L"openvr_api.dll").c_str());
    if (!dll) dll = LoadLibraryW(L"openvr_api.dll");
    if (dll) {
      init = (InitFn)(void*)GetProcAddress(dll, "VR_InitInternal"); shut = (ShutFn)(void*)GetProcAddress(dll, "VR_ShutdownInternal");
      iface = (IfaceFn)(void*)GetProcAddress(dll, "VR_GetGenericInterface"); desc = (DescFn)(void*)GetProcAddress(dll, "VR_GetVRInitErrorAsEnglishDescription");
    }
    if (!init || !shut || !iface) { st = VrState::NoRuntimeDll; status = "openvr_api.dll is missing or invalid"; logf(LogLevel::Error, "openvr_api.dll not usable"); }
  }
  void disconnect(const char* why) {
    if (sys) { shut(); sys = nullptr; logf(LogLevel::Info, "OpenVR disconnected: %s", why); }
    st = VrState::NotRunning; status = "SteamVR is not running"; info.clear();
  }
  void tryConnect() {
    vr::EVRInitError e = vr::VRInitError_None;
    init(&e, vr::VRApplication_Background);
    if (e != vr::VRInitError_None) {
      if (e == vr::VRInitError_Init_NoServerForBackgroundApp) { st = VrState::NotRunning; status = "SteamVR is not running"; }
      else { st = VrState::Error; status = std::string("OpenVR error: ") + (desc ? desc(e) : "?"); if (err != status) logf(LogLevel::Warn, "%s", status.c_str()); err = status; }
      return;
    }
    sys = (vr::IVRSystem*)iface(vr::IVRSystem_Version, &e);
    if (!sys || e != vr::VRInitError_None) { shut(); sys = nullptr; st = VrState::Error; status = "OpenVR: IVRSystem unavailable"; return; }
    st = VrState::Connected; status = "Connected to SteamVR"; err.clear(); logf(LogLevel::Info, "Connected to SteamVR (background application)");
  }
  std::string str(int idx, vr::ETrackedDeviceProperty prop) {
    char b[256]; vr::ETrackedPropertyError pe = vr::TrackedProp_Success; uint32_t n = sys->GetStringTrackedDeviceProperty((uint32_t)idx, prop, b, sizeof b, &pe);
    return pe == vr::TrackedProp_Success && n > 1 ? std::string(b) : std::string();
  }
  static DeviceSample sample(const vr::TrackedDevicePose_t& p) {
    DeviceSample s; s.pose = poseFromMat34(p.mDeviceToAbsoluteTracking.m);
    bool ok = p.bPoseIsValid && p.bDeviceIsConnected && s.pose.isFinite();
    switch (p.eTrackingResult) { case vr::TrackingResult_Running_OK: s.quality = 1; break; case vr::TrackingResult_Running_OutOfRange: case vr::TrackingResult_Calibrating_OutOfRange: s.quality = 0.3; break; default: s.quality = 0; }
    s.valid = ok && s.quality > 0; return s;
  }
};

VrSource::VrSource() : d_(new Impl) {}
VrSource::~VrSource() { if (d_->sys) d_->shut(); if (d_->dll) FreeLibrary(d_->dll); delete d_; }
VrState VrSource::state() const { return d_->st; }
std::string VrSource::status() const { return d_->status; }
std::string VrSource::lastError() const { return d_->err; }

bool VrSource::poll(VrFrame& out) {
  Impl& d = *d_; out = {};
  if (!d.dllTried) d.loadDll();
  if (!d.init) return false;
  if (!d.sys) { double t = nowS(); if (t < d.nextTry) return false; d.nextTry = t + 2.0; d.tryConnect(); if (!d.sys) return false; }   // shortcut: connect attempt runs on the tracking thread (returns immediately when SteamVR is off)
  vr::VREvent_t ev;
  while (d.sys->PollNextEvent(&ev, sizeof ev)) {
    if (ev.eventType == vr::VREvent_Quit || ev.eventType == vr::VREvent_DriverRequestedQuit) { d.sys->AcknowledgeQuit_Exiting(); d.disconnect("SteamVR is shutting down"); d.nextTry = nowS() + 2.0; return false; }
  }
  vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
  d.sys->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseRawAndUncalibrated, 0.f, poses, vr::k_unMaxTrackedDeviceCount);
  out.hmd = Impl::sample(poses[vr::k_unTrackedDeviceIndex_Hmd]);
  vr::TrackedDeviceIndex_t li = d.sys->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand), ri = d.sys->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
  if (li < vr::k_unMaxTrackedDeviceCount) out.lc = Impl::sample(poses[li]);
  if (ri < vr::k_unMaxTrackedDeviceCount && ri != li) out.rc = Impl::sample(poses[ri]);
  double t = nowS();
  for (int i = 1; i < (int)vr::k_unMaxTrackedDeviceCount; ++i) {
    if (!poses[i].bDeviceIsConnected || d.sys->GetTrackedDeviceClass((uint32_t)i) != vr::TrackedDeviceClass_GenericTracker) { d.info.erase(i); continue; }
    Impl::Info& inf = d.info[i];
    if (inf.serial.empty() || t - inf.read > 5) { inf.serial = d.str(i, vr::Prop_SerialNumber_String); inf.system = d.str(i, vr::Prop_TrackingSystemName_String); inf.model = d.str(i, vr::Prop_ModelNumber_String); inf.read = t; }
    if (inf.system == "bodysync" || inf.serial.empty()) continue;   // never feed our own virtual trackers back in
    DeviceSample s = Impl::sample(poses[i]); PhysDevice pd;
    pd.serial = inf.serial; pd.system = inf.system; pd.model = inf.model; pd.connected = true; pd.poseValid = s.valid; pd.quality = s.quality; pd.pose = s.pose; pd.index = i;
    out.trackers.push_back(std::move(pd));
  }
  return true;
}
}  // namespace bs
