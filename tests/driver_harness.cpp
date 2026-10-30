// Mock SteamVR host: loads the real driver_bodysync.dll, drives it through the documented driver interfaces
// and checks registration, properties, pose publishing over the real named pipe, loss handling and shutdown.
// shortcut: the mock does not model vrserver's threading or device index reuse; real SteamVR is NOT exercised here.
#include <windows.h>
#include <openvr_driver.h>
#include <chrono>
#include <cstring>
#include <map>
#include <thread>
#include "bodysync/config.h"
#include "bodysync/ipc_pipe.h"
#include "test_framework.h"
using namespace vr;
using namespace std::chrono_literals;

namespace {
struct Host : IVRDriverContext, IVRSettings, IVRProperties, IVRServerDriverHost, IVRDriverLog, IVRDriverManager, IVRResources {
  std::map<std::string, bool> bools{{"driver_bodysync/enable", true}, {"driver_bodysync/assignTrackerRoles", true}};
  std::map<std::string, std::string> strings;
  std::vector<std::string> serials; std::vector<ETrackedDeviceClass> classes; std::vector<ITrackedDeviceServerDriver*> drivers;
  std::map<uint64_t, std::map<int, std::string>> strProps; std::map<uint64_t, std::map<int, bool>> boolProps;
  std::map<uint32_t, DriverPose_t> lastPose; std::map<uint32_t, int> poseCount; std::string log;
  // IVRDriverContext
  void* GetGenericInterface(const char* v, EVRInitError* e) override {
    if (e) *e = VRInitError_None;
    if (!strcmp(v, IVRSettings_Version)) return static_cast<IVRSettings*>(this);
    if (!strcmp(v, IVRProperties_Version)) return static_cast<IVRProperties*>(this);
    if (!strcmp(v, IVRServerDriverHost_Version)) return static_cast<IVRServerDriverHost*>(this);
    if (!strcmp(v, IVRDriverLog_Version)) return static_cast<IVRDriverLog*>(this);
    if (!strcmp(v, IVRDriverManager_Version)) return static_cast<IVRDriverManager*>(this);
    if (!strcmp(v, IVRResources_Version)) return static_cast<IVRResources*>(this);
    if (e) *e = VRInitError_Init_InterfaceNotFound;
    return nullptr;
  }
  DriverHandle_t GetDriverHandle() override { return 1; }
  // IVRSettings
  const char* GetSettingsErrorNameFromEnum(EVRSettingsError) override { return "err"; }
  void SetBool(const char* s, const char* k, bool v, EVRSettingsError* e) override { bools[std::string(s) + "/" + k] = v; if (e) *e = VRSettingsError_None; }
  void SetInt32(const char*, const char*, int32_t, EVRSettingsError* e) override { if (e) *e = VRSettingsError_None; }
  void SetFloat(const char*, const char*, float, EVRSettingsError* e) override { if (e) *e = VRSettingsError_None; }
  void SetString(const char* s, const char* k, const char* v, EVRSettingsError* e) override { strings[std::string(s) + "/" + k] = v; if (e) *e = VRSettingsError_None; }
  bool GetBool(const char* s, const char* k, EVRSettingsError* e) override { auto i = bools.find(std::string(s) + "/" + k); if (e) *e = i == bools.end() ? VRSettingsError_ReadFailed : VRSettingsError_None; return i != bools.end() && i->second; }
  int32_t GetInt32(const char*, const char*, EVRSettingsError* e) override { if (e) *e = VRSettingsError_ReadFailed; return 0; }
  float GetFloat(const char*, const char*, EVRSettingsError* e) override { if (e) *e = VRSettingsError_ReadFailed; return 0; }
  void GetString(const char* s, const char* k, char* out, uint32_t n, EVRSettingsError* e) override {
    auto i = strings.find(std::string(s) + "/" + k); if (n) out[0] = 0;
    if (i == strings.end()) { if (e) *e = VRSettingsError_ReadFailed; return; }
    std::snprintf(out, n, "%s", i->second.c_str()); if (e) *e = VRSettingsError_None;
  }
  void RemoveSection(const char*, EVRSettingsError* e) override { if (e) *e = VRSettingsError_None; }
  void RemoveKeyInSection(const char*, const char*, EVRSettingsError* e) override { if (e) *e = VRSettingsError_None; }
  // IVRProperties
  ETrackedPropertyError ReadPropertyBatch(PropertyContainerHandle_t, PropertyRead_t*, uint32_t) override { return TrackedProp_UnknownProperty; }
  ETrackedPropertyError WritePropertyBatch(PropertyContainerHandle_t c, PropertyWrite_t* b, uint32_t n) override {
    for (uint32_t i = 0; i < n; ++i) {
      if (b[i].writeType != PropertyWrite_Set) continue;
      if (b[i].unTag == k_unStringPropertyTag) strProps[c][b[i].prop] = std::string((const char*)b[i].pvBuffer);
      else if (b[i].unTag == k_unBoolPropertyTag) boolProps[c][b[i].prop] = *(const bool*)b[i].pvBuffer;
      b[i].eError = TrackedProp_Success;
    }
    return TrackedProp_Success;
  }
  const char* GetPropErrorNameFromEnum(ETrackedPropertyError) override { return "err"; }
  PropertyContainerHandle_t TrackedDeviceToPropertyContainer(TrackedDeviceIndex_t d) override { return 1000 + d; }
  // IVRServerDriverHost
  bool TrackedDeviceAdded(const char* serial, ETrackedDeviceClass c, ITrackedDeviceServerDriver* d) override {
    uint32_t idx = (uint32_t)serials.size() + 1;   // index 0 belongs to the HMD in real SteamVR
    serials.push_back(serial); classes.push_back(c); drivers.push_back(d); serialToIndex[serial] = idx; indexToDriver[idx] = d;
    return d->Activate(idx) == VRInitError_None;
  }
  void TrackedDevicePoseUpdated(uint32_t id, const DriverPose_t& p, uint32_t sz) override { if (sz == sizeof(DriverPose_t)) { lastPose[id] = p; ++poseCount[id]; } }
  void VsyncEvent(double) override {}
  void VendorSpecificEvent(uint32_t, EVREventType, const VREvent_Data_t&, double) override {}
  bool IsExiting() override { return false; }
  bool PollNextEvent(VREvent_t*, uint32_t) override { return false; }
  void GetRawTrackedDevicePoses(float, TrackedDevicePose_t*, uint32_t) override {}
  void RequestRestart(const char*, const char*, const char*, const char*) override {}
  uint32_t GetFrameTimings(Compositor_FrameTiming*, uint32_t) override { return 0; }
  void SetDisplayEyeToHead(uint32_t, const HmdMatrix34_t&, const HmdMatrix34_t&) override {}
  void SetDisplayProjectionRaw(uint32_t, const HmdRect2_t&, const HmdRect2_t&) override {}
  void SetRecommendedRenderTargetSize(uint32_t, uint32_t, uint32_t) override {}
  // IVRDriverLog
  void Log(const char* m) override { log += m; }
  // IVRDriverManager
  uint32_t GetDriverCount() const override { return 1; }
  uint32_t GetDriverName(DriverId_t, char* v, uint32_t n) override { std::snprintf(v, n, "bodysync"); return 9; }
  DriverHandle_t GetDriverHandle(const char*) override { return 1; }
  bool IsEnabled(DriverId_t) const override { return true; }
  // IVRResources
  uint32_t LoadSharedResource(const char*, char*, uint32_t) override { return 0; }
  uint32_t GetResourceFullPath(const char*, const char*, char*, uint32_t) override { return 0; }

  std::map<std::string, uint32_t> serialToIndex; std::map<uint32_t, ITrackedDeviceServerDriver*> indexToDriver;
};

struct Loaded {
  HMODULE dll = nullptr; IServerTrackedDeviceProvider* prov = nullptr; Host host;
  bool load() {
    char path[MAX_PATH]; GetModuleFileNameA(nullptr, path, MAX_PATH); std::string p = path; p = p.substr(0, p.find_last_of("\\/") + 1);
    const char* env = std::getenv("BODYSYNC_DRIVER_DLL");
    std::string dllPath = env ? env : p + "stage\\driver\\bodysync\\bin\\win64\\driver_bodysync.dll";
    dll = LoadLibraryA(dllPath.c_str()); if (!dll) { std::printf("    cannot load %s (err %lu)\n", dllPath.c_str(), GetLastError()); return false; }
    auto f = (void* (*)(const char*, int*))(void*)GetProcAddress(dll, "HmdDriverFactory"); if (!f) return false;
    int rc = 0; prov = (IServerTrackedDeviceProvider*)f(IServerTrackedDeviceProvider_Version, &rc); return prov && rc == 0;
  }
  ~Loaded() { if (prov) prov->Cleanup(); if (dll) FreeLibrary(dll); }
  template <class F> bool pump(F f, int ms = 4000) { auto t0 = std::chrono::steady_clock::now(); while (std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds(ms)) { prov->RunFrame(); if (f()) return true; std::this_thread::sleep_for(5ms); } prov->RunFrame(); return f(); }
};
bs::ipc::PoseFrame frame(double x, bool active = true) {
  bs::ipc::PoseFrame f; f.timestampUs = 1; f.timeOffsetSec = -0.004f;
  for (int i = 0; i < 8; ++i) { bs::ipc::Pose8 p; p.id = (uint8_t)i; p.active = active; p.valid = true; p.pos[0] = x + i; p.pos[1] = 0.5; p.pos[2] = -1; p.rot[0] = 1; p.vel[0] = 0.25f; f.poses.push_back(p); }
  return f;
}
}  // namespace

TEST(driver_factory_rejects_unknown_interface) {
  Loaded L; CHECK(L.load()); if (!L.dll) return;
  auto f = (void* (*)(const char*, int*))(void*)GetProcAddress(L.dll, "HmdDriverFactory"); int rc = 0;
  CHECK(f("IWhatever_999", &rc) == nullptr && rc == VRInitError_Init_InterfaceNotFound);
}
TEST(driver_registers_eight_generic_trackers_with_properties) {
  Loaded L; if (!L.load()) { CHECK(false); return; }
  CHECK(L.prov->Init(&L.host) == VRInitError_None);
  CHECK(L.host.serials.size() == 8);
  for (int r = 0; r < bs::kRoleCount && r < (int)L.host.serials.size(); ++r) {
    CHECK(L.host.serials[r] == bs::roleSerial(r)); CHECK(L.host.classes[r] == TrackedDeviceClass_GenericTracker);
    auto& sp = L.host.strProps[1000 + L.host.serialToIndex[bs::roleSerial(r)]];
    CHECK(sp[Prop_SerialNumber_String] == bs::roleSerial(r)); CHECK(sp[Prop_TrackingSystemName_String] == "bodysync"); CHECK(sp[Prop_ManufacturerName_String] == "BodySync");
    CHECK(sp[Prop_InputProfilePath_String].find("{bodysync}/") == 0);
    CHECK(L.host.strings["trackers//devices/bodysync/" + std::string(bs::roleSerial(r))] == bs::roleSteamVRName(r));   // role auto-assignment (best effort)
  }
  CHECK(L.host.log.find("registered 8 trackers") != std::string::npos);
  const char* const* iv = L.prov->GetInterfaceVersions(); bool hasDev = false; for (; iv && *iv; ++iv) hasDev |= !strcmp(*iv, ITrackedDeviceServerDriver_Version); CHECK(hasDev);
}
TEST(driver_keeps_user_chosen_roles_and_honours_settings) {
  { Loaded L; L.host.strings["trackers//devices/bodysync/BodySync_Waist"] = "TrackerRole_Chest"; if (!L.load()) { CHECK(false); return; }
    CHECK(L.prov->Init(&L.host) == VRInitError_None); CHECK(L.host.strings["trackers//devices/bodysync/BodySync_Waist"] == "TrackerRole_Chest"); }   // never overrides the user
  { Loaded L; L.host.bools["driver_bodysync/assignTrackerRoles"] = false; if (!L.load()) { CHECK(false); return; }
    CHECK(L.prov->Init(&L.host) == VRInitError_None); CHECK(L.host.serials.size() == 8); CHECK(L.host.strings.empty()); }
  { Loaded L; L.host.bools["driver_bodysync/enable"] = false; if (!L.load()) { CHECK(false); return; }
    CHECK(L.prov->Init(&L.host) == VRInitError_None); CHECK(L.host.serials.empty()); }   // disabled: loads, does nothing
}
TEST(driver_publishes_poses_received_over_the_pipe) {
  Loaded L; if (!L.load()) { CHECK(false); return; }
  CHECK(L.prov->Init(&L.host) == VRInitError_None);
  bs::ipc::PipeClient cli; cli.start();
  CHECK(L.pump([&] { return cli.info().connected; }));
  CHECK(cli.info().ack.trackerCount == 8);
  cli.submitPoses(frame(3.0));
  uint32_t waist = L.host.serialToIndex[bs::roleSerial(0)], foot = L.host.serialToIndex[bs::roleSerial(7)];
  CHECK(L.pump([&] { return L.host.poseCount[foot] > 0; }));
  auto& p = L.host.lastPose[waist]; auto& q = L.host.lastPose[foot];
  CHECK(p.poseIsValid && p.deviceIsConnected && p.result == TrackingResult_Running_OK);
  CHECK_NEAR(p.vecPosition[0], 3.0, 1e-9); CHECK_NEAR(q.vecPosition[0], 10.0, 1e-9); CHECK_NEAR(p.vecPosition[2], -1, 1e-9);
  CHECK_NEAR(p.vecVelocity[0], 0.25, 1e-6); CHECK_NEAR(p.qRotation.w, 1, 1e-9);
  CHECK_NEAR(p.qWorldFromDriverRotation.w, 1, 0); CHECK_NEAR(p.qDriverFromHeadRotation.w, 1, 0);   // identity, never zeroed
  CHECK_NEAR(p.poseTimeOffset, -0.004, 1e-6);
  CHECK(L.pump([&] { auto s = cli.info().status; return s.framesReceived > 0 && (s.flags & bs::ipc::kDevicesRegistered) && s.activeMask == 0xFF; }));   // status reaches the app
  cli.stop();
}
TEST(driver_inactive_masked_and_lost_trackers) {
  Loaded L; if (!L.load()) { CHECK(false); return; }
  CHECK(L.prov->Init(&L.host) == VRInitError_None);
  bs::ipc::PipeClient cli; cli.start(); CHECK(L.pump([&] { return cli.info().connected; }));
  uint32_t knee = L.host.serialToIndex[bs::roleSerial(bs::kLeftKnee)], waist = L.host.serialToIndex[bs::roleSerial(0)];
  cli.submitConfig(0xFF & ~(1u << bs::kLeftKnee)); cli.submitPoses(frame(0));
  CHECK(L.pump([&] { return L.host.poseCount[waist] > 0 && !L.host.lastPose[knee].deviceIsConnected && L.host.poseCount[knee] > 0; }));
  CHECK(!L.host.lastPose[knee].poseIsValid);                                    // disabled in the app -> not valid
  CHECK(L.host.lastPose[waist].poseIsValid);
  cli.submitPoses(frame(0, false));                                              // app marks everything inactive
  CHECK(L.pump([&] { return !L.host.lastPose[waist].deviceIsConnected; }));
  CHECK(!L.host.lastPose[waist].poseIsValid);
  cli.stop();
}
TEST(driver_trackers_disappear_when_app_exits_or_stalls) {
  Loaded L; if (!L.load()) { CHECK(false); return; }
  CHECK(L.prov->Init(&L.host) == VRInitError_None);
  uint32_t waist = L.host.serialToIndex[bs::roleSerial(0)];
  { bs::ipc::PipeClient cli; cli.start(); CHECK(L.pump([&] { return cli.info().connected; })); cli.submitPoses(frame(0));
    CHECK(L.pump([&] { return L.host.lastPose[waist].poseIsValid; })); cli.stop(); }   // app exits
  CHECK(L.pump([&] { return !L.host.lastPose[waist].deviceIsConnected; }));          // trackers vanish instead of freezing
  CHECK(!L.host.lastPose[waist].poseIsValid);
  bs::ipc::PipeClient c2; c2.start(); CHECK(L.pump([&] { return c2.info().connected; })); c2.submitPoses(frame(1));   // reconnect works
  CHECK(L.pump([&] { return L.host.lastPose[waist].poseIsValid; }));
  // stall: connected but silent for > 500 ms
  auto t0 = std::chrono::steady_clock::now(); while (std::chrono::steady_clock::now() - t0 < 900ms) { L.prov->RunFrame(); std::this_thread::sleep_for(10ms); }
  CHECK(!L.host.lastPose[waist].deviceIsConnected);
  c2.stop();
}
TEST(driver_cleanup_is_idempotent_and_releases_pipe) {
  { Loaded L; if (!L.load()) { CHECK(false); return; } CHECK(L.prov->Init(&L.host) == VRInitError_None); L.prov->Cleanup(); L.prov = nullptr; }
  Loaded L2; if (!L2.load()) { CHECK(false); return; }                                                         // second load/init after full shutdown
  CHECK(L2.prov->Init(&L2.host) == VRInitError_None);
  bs::ipc::PipeClient cli; cli.start(); CHECK(L2.pump([&] { return cli.info().connected; })); cli.stop();
}

// ---- end to end: real Engine (simulation) -> real named pipe -> real driver DLL (mock SteamVR host) ----
#include "engine.h"
TEST(e2e_engine_simulation_reaches_driver_and_stops_cleanly) {
  Loaded L; if (!L.load()) { CHECK(false); return; }
  CHECK(L.prov->Init(&L.host) == VRInitError_None);
  bs::Engine eng; bs::Profile p; p.body.heightM = 1.75; p.calibration.valid = true; eng.setProfile(p);
  eng.setSource(bs::InputSource::Simulation); eng.setScenario(bs::Scenario::Neutral, true); eng.setPublishSimulation(false); eng.start();
  for (int i = 0; i < 60; ++i) { L.prov->RunFrame(); std::this_thread::sleep_for(5ms); }
  uint32_t waist = L.host.serialToIndex[bs::roleSerial(bs::kWaist)], foot = L.host.serialToIndex[bs::roleSerial(bs::kLeftFoot)];
  CHECK(L.host.poseCount[waist] == 0);                        // simulated poses never reach SteamVR unless explicitly allowed
  eng.setPublishSimulation(true);
  CHECK(L.pump([&] { return L.host.lastPose[waist].poseIsValid && L.host.lastPose[foot].poseIsValid; }));
  auto& w = L.host.lastPose[waist]; auto& f = L.host.lastPose[foot];
  CHECK(w.vecPosition[1] > 0.7 && w.vecPosition[1] < 1.2); CHECK(f.vecPosition[1] > 0.0 && f.vecPosition[1] < 0.2);
  CHECK(w.deviceIsConnected && std::fabs(w.qRotation.w * w.qRotation.w + w.qRotation.x * w.qRotation.x + w.qRotation.y * w.qRotation.y + w.qRotation.z * w.qRotation.z - 1) < 1e-6);
  CHECK(L.pump([&] { return eng.snapshot().publishing; }));   // snapshot refreshes at 30 Hz
  auto s = eng.snapshot(); CHECK(s.ipc.connected); CHECK(s.publishing); CHECK_MSG(s.loopHz > 30, "loopHz=%g", s.loopHz); CHECK(s.out.hmdOk);
  CHECK(L.pump([&] { return eng.snapshot().ipc.status.framesReceived > 10; }));
  eng.setScenario(bs::Scenario::Sit, true);                   // posture change flows through
  CHECK(L.pump([&] { return eng.snapshot().out.posture == bs::Posture::Sitting; }, 8000));
  eng.setPublish(false);                                      // user switch: trackers vanish once
  CHECK(L.pump([&] { return !L.host.lastPose[waist].deviceIsConnected; }));
  eng.setPublish(true); CHECK(L.pump([&] { return L.host.lastPose[waist].poseIsValid; }));
  eng.stop();                                                 // app closes: trackers vanish, nothing freezes
  CHECK(L.pump([&] { return !L.host.lastPose[waist].deviceIsConnected; }));
}
TEST(engine_capture_collects_samples_and_reports_progress) {
  bs::Engine eng; bs::Profile p; eng.setProfile(p); eng.setSource(bs::InputSource::Simulation); eng.start("\\\\.\\pipe\\BodySync.Test.unused");
  eng.startCapture(bs::CaptureKind::Stand, 0.5); bs::Capture c; bool done = false;
  for (int i = 0; i < 300 && !done; ++i) { std::this_thread::sleep_for(10ms); done = eng.takeCapture(c); }
  CHECK(done); CHECK(c.kind == bs::CaptureKind::Stand); CHECK(c.samples.size() > 20 && c.samples.size() < 80);
  auto h = bs::calibrateHeight(c.samples, 0.0); CHECK(h.ok); CHECK_NEAR(h.eyeHeight, 1.62, 0.05);   // simulated standing eye height
  CHECK(!eng.takeCapture(c)); eng.stop();
}
