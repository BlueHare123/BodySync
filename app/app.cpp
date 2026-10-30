#include "app.h"
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <algorithm>
#include <chrono>
#include <cstdarg>
#include "bodysync/fsutil.h"
#include "bodysync/log.h"

namespace bs::ui {
namespace fs = std::filesystem;
static double nowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
std::string fmt(const char* f, ...) { char b[512]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a); return b; }

App::App(const std::string& pipeName, bool startEngine) {
  auto l = loadAppSettings(); as = l.value; sanitize(as); bs::ui::setLanguage(as.language);
  refreshProfiles();
  bool ok = false;
  if (std::find(profileNames.begin(), profileNames.end(), as.activeProfile) != profileNames.end()) ok = selectProfile(as.activeProfile);
  if (!ok && !profileNames.empty()) ok = selectProfile(profileNames.front());
  if (!ok) { prof = Profile{}; createProfile("Default", nullptr); }
  if (l.status == LoadStatus::Recovered) toast(tr("Settings file was damaged; defaults restored (the original was kept as .corrupt-*)."), 1);
  engine.setProfile(prof); engine.setPublishSimulation(as.publishSimulation);
  if (startEngine) engine.start(pipeName);
}
App::~App() { flushSave(); engine.stop(); }

void App::toast(const std::string& t, int level) { toasts.push_back({t, nowS() + (level ? 7.0 : 4.0), level}); if (toasts.size() > 5) toasts.pop_front(); logf(level == 2 ? LogLevel::Error : level == 1 ? LogLevel::Warn : LogLevel::Info, "%s", t.c_str()); }
void App::markDirty() { sanitize(prof); engine.setProfile(prof); dirty = true; lastChange = nowS(); }
void App::refreshProfiles() { profileNames = listPresets("profiles"); }
bool App::saveNow() {
  std::string err; prof.name = safeName(prof.name).empty() ? "Default" : prof.name;
  if (!saveProfileFile(presetPath("profiles", prof.name), prof, &err)) { if (nowS() - lastSaveFail > 10) toast(std::string(tr("Could not save the profile: ")) + err, 2); lastSaveFail = nowS(); return false; }
  as.activeProfile = prof.name; as.language = language(); saveAppSettings(as); dirty = false; refreshProfiles(); return true;
}
void App::flushSave() { if (dirty && as.autosave) saveNow(); }
bool App::selectProfile(const std::string& name) {
  auto l = loadProfileFile(presetPath("profiles", name));
  if (l.status == LoadStatus::Missing) { toast(tr("Profile not found."), 1); return false; }
  if (l.status == LoadStatus::Recovered) toast(std::string(tr("Profile was damaged; defaults loaded (the original was kept as .corrupt-*): ")) + l.message, 1);
  else if (l.status == LoadStatus::IntegrityWarning) toast(tr("Profile was edited by hand (checksum mismatch); loaded anyway."), 1);
  prof = l.value; prof.name = name; sanitize(prof); engine.setProfile(prof); as.activeProfile = name; saveAppSettings(as); dirty = l.status == LoadStatus::Migrated; return true;
}
bool App::createProfile(const std::string& raw, const Profile* from) {
  std::string n = safeName(raw); if (n.empty()) { toast(tr("Invalid profile name."), 1); return false; }
  if (fs::exists(presetPath("profiles", n))) { toast(tr("A profile with this name already exists."), 1); return false; }
  Profile p = from ? *from : Profile{}; p.name = n; std::string err;
  if (!saveProfileFile(presetPath("profiles", n), p, &err)) { toast(std::string(tr("Could not save the profile: ")) + err, 2); return false; }
  refreshProfiles(); return selectProfile(n);
}
bool App::deleteProfile(const std::string& name) {
  if (!deletePreset("profiles", name)) { toast(tr("Could not delete the profile."), 2); return false; }
  refreshProfiles(); dirty = false;
  if (!profileNames.empty()) return selectProfile(profileNames.front());
  prof = Profile{}; return createProfile("Default", nullptr);
}
void App::setLanguage(const std::string& c) { bs::ui::setLanguage(c); as.language = language(); saveAppSettings(as); }
void App::setUiScale(float s) { as.uiScale = s; sanitize(as); saveAppSettings(as); }
void App::setSource(InputSource s) { engine.setSource(s); }

static bool fileDialog(bool save, std::wstring& path) {
  wchar_t buf[MAX_PATH] = {0}; if (save) wcsncpy(buf, path.c_str(), MAX_PATH - 1);
  OPENFILENAMEW o{}; o.lStructSize = sizeof o; o.hwndOwner = GetActiveWindow(); o.lpstrFile = buf; o.nMaxFile = MAX_PATH;
  o.lpstrFilter = L"BodySync profile (*.json)\0*.json\0All files\0*.*\0"; o.lpstrDefExt = L"json";
  o.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
  if (!(save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o))) return false;
  path = buf; return true;
}
void App::importProfileDialog() {
  std::wstring p; if (!fileDialog(false, p)) return;
  auto l = importProfile(fs::path(p));
  if (l.status == LoadStatus::Recovered || l.status == LoadStatus::Missing) { toast(std::string(tr("Import failed: ")) + l.message, 2); return; }
  std::string n = safeName(l.value.name).empty() ? "Imported" : safeName(l.value.name), base = n; for (int i = 2; fs::exists(presetPath("profiles", n)); ++i) n = base + " " + std::to_string(i);   // never overwrite an existing profile
  if (createProfile(n, &l.value)) toast(std::string(tr("Imported as: ")) + n);
}
void App::exportProfileDialog() {
  std::wstring p = fsutil::fromUtf8(prof.name + ".bodysync.json").wstring(); if (!fileDialog(true, p)) return;
  std::string err; if (exportProfile(prof, fs::path(p), &err)) toast(tr("Profile exported.")); else toast(std::string(tr("Export failed: ")) + err, 2);
}
void App::openFolder(const fs::path& p) { std::error_code ec; fs::create_directories(p, ec); ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL); }

// ---------------- calibration wizard ----------------
const char* App::wizStepName(CaptureKind k) {
  switch (k) { case CaptureKind::Floor: return N_("Floor"); case CaptureKind::Stand: return N_("Stand"); case CaptureKind::TPose: return N_("T-pose"); case CaptureKind::Verify: return N_("Verify"); default: return ""; }
}
void App::wizStart(Wizard::Mode m) {
  wiz = {}; wiz.mode = m; wiz.staged = prof;
  wiz.steps = m == Wizard::Quick ? std::vector<CaptureKind>{CaptureKind::Floor, CaptureKind::Stand, CaptureKind::Verify}
                                 : std::vector<CaptureKind>{CaptureKind::Floor, CaptureKind::Stand, CaptureKind::TPose, CaptureKind::Verify};
}
void App::wizBegin() { wiz.counting = true; wiz.countdownEnd = nowS() + 3.0; wiz.msg.clear(); wiz.stepOk = false; wiz.verifyDone = false; }
void App::wizCancel() { engine.cancelCapture(); engine.setProfile(prof); wiz = {}; }
void App::wizCommit() {
  Profile keep = prof; prof = wiz.staged; prof.name = keep.name; prof.calibration.valid = true; markDirty(); wiz = {}; toast(tr("Calibration saved to the profile."));
}
void App::wizPoll() {
  if (wiz.mode == Wizard::Off || wiz.finished) return;
  CaptureKind k = wiz.steps[(size_t)wiz.step];
  if (wiz.counting && nowS() >= wiz.countdownEnd) {
    wiz.counting = false; wiz.capturing = true;
    if (k == CaptureKind::Verify) engine.setProfile(wiz.staged);            // verify what would be saved, without committing it
    engine.startCapture(k, k == CaptureKind::Floor ? 2.0 : k == CaptureKind::Verify ? 4.0 : 3.0);
  }
  Capture c; if (!wiz.capturing || !engine.takeCapture(c)) return;
  wiz.capturing = false; Profile& st = wiz.staged;
  if (k == CaptureKind::Floor) {
    double best = 1e9; int n = 0;
    for (int side = 0; side < 2; ++side) {
      std::vector<double> y; for (auto& s : c.samples) if (side ? s.rcOk : s.lcOk) y.push_back((side ? s.rc : s.lc).p.y);
      if (y.size() < 20) continue;
      double lo = *std::min_element(y.begin(), y.end()), hi = *std::max_element(y.begin(), y.end());
      if (hi - lo > 0.05) continue;                                        // that controller was moving
      best = std::min(best, median(y)); ++n;
    }
    constexpr double kControllerAboveFloor = 0.03;                         // shortcut: fixed 3 cm between controller origin and floor; ceiling: very thick controllers/cases, upgrade: per-model offsets or manual floor field
    if (!n) { wiz.msg = N_("No steady controller found. Lay one controller flat on the floor and keep it still."); wiz.stepOk = false; return; }
    st.calibration.floorOffsetM = best - kControllerAboveFloor; wiz.stepOk = true; wiz.msg = N_("Floor level captured.");
  } else if (k == CaptureKind::Stand) {
    HeightResult h = calibrateHeight(c.samples, st.calibration.floorOffsetM); NeutralResult nr = calibrateNeutral(c.samples);
    if (!h.ok) { wiz.msg = h.msg; wiz.stepOk = false; return; }
    apply(st, h); apply(st, nr); wiz.stepOk = true; wiz.msg = N_("Height captured.");
  } else if (k == CaptureKind::TPose) {
    TPoseResult t = calibrateTPose(c.samples, st.body);
    if (!t.ok) { wiz.msg = t.msg; wiz.stepOk = false; return; }
    apply(st, t); wiz.stepOk = true; wiz.msg = t.msg.empty() ? std::string(N_("Arm length captured.")) : t.msg;
  } else {
    wiz.rep = verifySkeleton(c.outs); wiz.verifyDone = true; wiz.stepOk = wiz.rep.pass; wiz.msg = wiz.rep.pass ? N_("Verification passed.") : N_("Verification found problems.");
    engine.setProfile(prof);                                               // back to the saved profile until the user commits
  }
}

// ---------------- diagnostics report ----------------
std::string App::buildReport() {
  Snapshot s = engine.snapshot(); std::string r;
  auto line = [&](const std::string& t) { r += t + "\n"; };
  line(std::string("BodySync ") + BODYSYNC_VERSION + " diagnostic report"); line("Generated: " + timestampNow());
  line("NOTE: contains tracker serial numbers and profile settings; review before sharing. No other personal data is collected.");
  line(""); line("== Connection =="); line("Input source: " + std::string(s.source == InputSource::Live ? "Live" : "SIMULATION (not real tracking)"));
  line("SteamVR: " + s.vrStatus); line(fmt("Driver pipe: %s, driver v%u, trackers %u, runtime %s", s.ipc.connected ? "connected" : "not connected", s.ipc.ack.driverVersion, s.ipc.ack.trackerCount, s.ipc.ack.runtime));
  line(fmt("Driver status: flags=0x%x frames=%llu published=%llu activeMask=0x%x", s.ipc.status.flags, (unsigned long long)s.ipc.status.framesReceived, (unsigned long long)s.ipc.status.posesPublished, s.ipc.status.activeMask));
  line(fmt("Client: sent=%llu dropped=%llu reconnects=%llu lastError=%s", (unsigned long long)s.ipc.framesSent, (unsigned long long)s.ipc.framesDropped, (unsigned long long)s.ipc.reconnects, s.ipc.lastError.c_str()));
  line(fmt("Publishing: %s; loop %.1f Hz (max step %.1f ms); solve %.0f us", s.publishing ? "yes" : "no", s.loopHz, s.loopMaxMs, s.solveUsAvg));
  line(""); line("== Solver ==");
  line(fmt("Posture: %s (candidate %s, %.2fs); transition %.2f; health %d", postureName(s.out.posture), postureName(s.out.diag.candidate), s.out.diag.candidateT, s.out.transition, (int)s.out.health));
  line(fmt("HMD ok=%d  left=%d  right=%d; sinceGood %.2f/%.2f/%.2f; rejected %d/%d/%d; nonFinite %d", s.out.hmdOk, s.out.lcOk, s.out.rcOk, s.out.diag.sinceGood[0], s.out.diag.sinceGood[1], s.out.diag.sinceGood[2], s.out.diag.rejected[0], s.out.diag.rejected[1], s.out.diag.rejected[2], s.out.diag.nonFinite));
  line(fmt("headRatio %.3f hipHeight %.3f lean %.1f deg speed %.2f m/s maxSegErr %.4f m", s.out.diag.headRatio, s.out.diag.hipHeight, s.out.diag.leanDeg, s.out.diag.speedXZ, s.out.diag.maxSegErr));
  for (int i = 0; i < kRoleCount; ++i) line(fmt("  %-10s active=%d valid=%d src=%s conf=%.2f physW=%.2f | mixer: assigned=%d healthy=%d inCharge=%d (%s)", roleName(i), s.out.tr[i].active, s.out.tr[i].valid, sourceName(s.out.tr[i].src), s.out.tr[i].confidence, s.out.tr[i].physWeight, s.mixer[i].assigned, s.mixer[i].healthy, s.mixer[i].physInCharge, s.mixer[i].reason.c_str()));
  line(""); line("== Physical trackers seen =="); for (auto& t : s.trackers) line(fmt("  %s (%s / %s) valid=%d quality=%.2f", t.serial.c_str(), t.system.c_str(), t.model.c_str(), t.poseValid, t.quality));
  line(""); line("== Profile =="); line(json(prof).dump(2)); line("");
  line("== Log (newest last) =="); for (auto& e : Logger::instance().tail(300)) line(fmt("[%s] %s", levelName(e.level), e.text.c_str()));
  return r;
}
std::string App::saveReport(std::string* err) {
  fs::path dir = dataDir() / "reports"; std::error_code ec; fs::create_directories(dir, ec);
  std::string ts = timestampNow(); for (char& ch : ts) if (ch == ' ' || ch == ':') ch = ch == ' ' ? '_' : '-';
  fs::path f = dir / ("BodySync-report-" + ts + ".txt");
  if (!fsutil::writeFile(f, buildReport())) { if (err) *err = "cannot write " + fsutil::toUtf8(f); return {}; }
  return fsutil::toUtf8(f);
}
}  // namespace bs::ui
