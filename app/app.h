// Application state + logic (profiles, autosave, calibration wizard, reports). Drawing lives in ui.cpp.
#pragma once
#include <deque>
#include <map>
#include "bodysync/config_store.h"
#include "engine.h"
#include "i18n.h"

namespace bs::ui {
struct Toast { std::string text; double until; int level; };   // level 0 info, 1 warn, 2 error
struct PresetUi { int sel = -1; char name[64] = ""; };

struct Wizard {
  enum Mode { Off, Quick, Advanced } mode = Off;
  std::vector<CaptureKind> steps; int step = 0; Profile staged;
  double countdownEnd = 0; bool counting = false, capturing = false, stepOk = false, finished = false;
  std::string msg; VerifyReport rep; bool verifyDone = false;
};

std::string fmt(const char* f, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;
class App {
 public:
  App(const std::string& pipeName, bool startEngine);
  ~App();
  void draw();                               // one frame of UI (call between ImGui::NewFrame and Render)
  void flushSave();

  // state
  Engine engine; Profile prof; AppSettings as; std::vector<std::string> profileNames; bool dirty = false; double lastChange = 0, lastSaveFail = -100;
  static constexpr int kPages = 6; int page = 0; Snapshot snap; double now = 0; std::deque<Toast> toasts; Wizard wiz; std::map<std::string, PresetUi> presetUi;
  bool quit = false, publishUser = true; std::string modal; char nameBuf[64] = ""; std::string pendingProfile;
  // profile management
  void toast(const std::string& t, int level = 0);
  void markDirty();                          // settings changed: push to engine, schedule autosave
  void refreshProfiles();
  bool selectProfile(const std::string& name);
  bool createProfile(const std::string& name, const Profile* from);
  bool deleteProfile(const std::string& name);
  bool saveNow();
  void importProfileDialog(); void exportProfileDialog();
  void setLanguage(const std::string& code);
  void setUiScale(float s);
  void setSource(InputSource s);
  // calibration wizard
  void wizStart(Wizard::Mode m); void wizBegin(); void wizPoll(); void wizCancel(); void wizCommit();
  static const char* wizStepName(CaptureKind k);
  // diagnostics
  std::string buildReport(); std::string saveReport(std::string* err = nullptr);
  void openFolder(const std::filesystem::path& p);
};
}  // namespace bs::ui
