#include "selftest.h"
#include <windows.h>
#include <imgui.h>
#include <chrono>
#include <cstdio>
#include <thread>
#include "app.h"
#include "bodysync/fsutil.h"
#include "bodysync/ipc_pipe.h"
#include "bodysync/log.h"
#include "ui.h"

using namespace bs;
namespace fs = std::filesystem;
namespace {
std::string g_out; int g_fail = 0, g_checks = 0;
void say(const std::string& s) { g_out += s + "\n"; std::fputs((s + "\n").c_str(), stdout); std::fflush(stdout); }
void check(bool ok, const std::string& what) { ++g_checks; if (!ok) { ++g_fail; say("  FAIL: " + what); } }
using namespace std::chrono_literals;
template <class F> bool waitFor(F f, int ms = 4000) { auto t0 = std::chrono::steady_clock::now(); while (std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds(ms)) { if (f()) return true; std::this_thread::sleep_for(5ms); } return f(); }

void testSolver() {
  say("[1] solver scenarios (all must stay finite and keep segment lengths)");
  Profile p; p.body.heightM = 1.62 / 0.93; p.calibration.valid = true; p.calibration.eyeHeightM = 1.62;
  for (int s = 0; s < kScenarioCount; ++s) {
    Solver solver; solver.configure(p); Mixer mixer; mixer.configure(p.mixed); SimSource sim(1.62); bool finite = true, hmdOk = true; double maxErr = 0; double dt = 1.0 / 90;
    for (double t = 0; t < scenarioDuration((Scenario)s); t += dt) {
      SolverInput in = sim.sample((Scenario)s, t); in.t = t; mixer.gate(dt, in, hmdOk); SolverOutput o = solver.update(in); mixer.blend(dt, o); hmdOk = o.hmdOk;
      for (auto& tr : o.tr) if (tr.valid) finite &= tr.pose.isFinite() && std::fabs(tr.pose.q.norm() - 1) < 1e-6;
      maxErr = std::max(maxErr, o.diag.maxSegErr);
    }
    check(finite, std::string("finite output in ") + scenarioName((Scenario)s)); check(maxErr < 0.02, std::string("segment error in ") + scenarioName((Scenario)s));
  }
}
void testConfig() {
  say("[2] profile storage (temporary folder)");
  Profile p; p.name = "SelfTest"; p.body.heightM = 1.83; fs::path f = dataDir() / "profiles" / "SelfTest.json";
  check(saveProfileFile(f, p), "save profile"); auto l = loadProfileFile(f); check(l.status == LoadStatus::Ok && l.value.body.heightM == 1.83, "load profile");
  fsutil::writeFile(f, "{ broken"); auto r = loadProfileFile(f); check(r.status == LoadStatus::Recovered, "corrupt profile is recovered, not fatal");
}
void testEngineToPipe() {
  say("[3] engine (simulation) -> named pipe -> receiver");
  std::string pipe = R"(\\.\pipe\BodySync.SelfTest.)" + std::to_string(GetCurrentProcessId());
  std::atomic<int> frames{0}; std::atomic<int> valid{0}; ipc::ServerCallbacks cb;
  cb.onPoses = [&](const ipc::PoseFrame& f) { ++frames; int v = 0; for (auto& p : f.poses) v += p.valid; valid = v; };
  cb.getStatus = [] { return ipc::StatusMsg{}; };
  ipc::PipeServer srv(pipe, cb); srv.start();
  Engine eng; Profile p; p.calibration.valid = true; eng.setProfile(p); eng.setSource(InputSource::Simulation); eng.setPublishSimulation(true); eng.setScenario(Scenario::Walk, true); eng.start(pipe);
  check(waitFor([&] { return frames > 30; }), "poses arrive over the pipe"); check(valid == kRoleCount, "all eight trackers valid in simulation");
  auto s = eng.snapshot(); check(s.ipc.connected, "client connected"); check(s.loopHz > 30, "tracking loop runs at a sensible rate");
  eng.stop(); srv.stop();
}
void testUi() {
  say("[4] user interface, headless: every page in all five languages");
  IMGUI_CHECKVERSION(); ImGuiContext* ctx = ImGui::CreateContext(); ImGuiIO& io = ImGui::GetIO(); io.IniFilename = nullptr; io.DisplaySize = {1240, 800};
  unsigned char* px; int w, h; io.Fonts->AddFontDefault(); io.Fonts->GetTexDataAsRGBA32(&px, &w, &h); ui::applyTheme(1.0f);
  {
    std::string pipe = R"(\\.\pipe\BodySync.SelfTestUi.)" + std::to_string(GetCurrentProcessId());
    ui::App app(pipe, true); auto frame = [&](int n) { for (int i = 0; i < n; ++i) { io.DeltaTime = 1.0f / 60; ImGui::NewFrame(); ui::drawApp(app); ImGui::Render(); std::this_thread::sleep_for(8ms); } };
    for (const char* lang : {"en", "ru", "de", "fr", "es"}) {
      app.setLanguage(lang);
      for (int src = 0; src < 2; ++src) {
        app.setSource(src ? InputSource::Simulation : InputSource::Live);
        for (int pg = 0; pg < ui::App::kPages; ++pg) { app.page = pg; frame(4); }
      }
      app.page = 1; app.wizStart(ui::Wizard::Advanced); for (size_t st = 0; st < app.wiz.steps.size(); ++st) { app.wiz.step = (int)st; app.wiz.msg = "Floor level captured."; app.wiz.stepOk = st % 2; frame(2); }
      app.wiz.step = 3; app.wiz.verifyDone = true; app.wiz.rep.messages = {"pelvis is unstable - stand still, or raise smoothing"}; frame(2); app.wizCancel();
      for (const char* m : {"new", "dup", "delete", "unsaved", "resetcal"}) { app.modal = m; frame(3); app.modal.clear(); frame(2); }
      app.toast("test", 0); app.toast("warn", 1); app.toast("err", 2); frame(2);
    }
    check(ui::missingTranslations().empty(), "all visible texts have a translation in ru, de, fr and es");
    for (auto& m : ui::missingTranslations()) say("  missing translation: " + m);
    for (const char* lang : {"ru", "de", "fr", "es"}) { app.setLanguage(lang); check(app.as.language == lang, std::string("language ") + lang + " is stored in the settings"); }
    app.setLanguage("xx"); check(ui::language() == "en", "unknown language falls back to English");
    std::string err; std::string f = app.saveReport(&err); check(!f.empty() && fs::exists(fsutil::fromUtf8(f)), "diagnostic report is written " + err);
    ui::setLanguage("en");
  }
  ImGui::DestroyContext(ctx);
}
}  // namespace

int runSelfTest(const std::wstring& outFile) {
  fs::path tmp = fs::temp_directory_path() / fsutil::fromUtf8("bodysync-selftest-" + std::to_string(GetCurrentProcessId()));
  std::error_code ec; fs::create_directories(tmp, ec); _wputenv_s(L"BODYSYNC_DATA_DIR", tmp.c_str());   // never touches the user's real profiles
  Logger::instance().start(tmp / "logs" / "selftest.log");
  say(std::string("BodySync ") + BODYSYNC_VERSION + " self-test"); auto t0 = std::chrono::steady_clock::now();
  testSolver(); testConfig(); testEngineToPipe(); testUi();
  say(std::string(g_fail ? "RESULT: FAILED" : "RESULT: PASSED") + " (" + std::to_string(g_checks) + " checks, " + std::to_string(g_fail) + " failed, " + std::to_string((int)std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count()) + " s)");
  say("Note: this does not test SteamVR, physical trackers or any VR application.");
  Logger::instance().stop();
  fs::path out = outFile.empty() ? tmp / "selftest.txt" : fs::path(outFile); fsutil::writeFile(out, g_out);
  if (outFile.empty()) { std::string s = "Report: " + fsutil::toUtf8(out); std::puts(s.c_str()); }
  return g_fail ? 1 : 0;
}
