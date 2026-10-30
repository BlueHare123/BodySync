#include <cstdlib>
#include "bodysync/fsutil.h"
#include "bodysync/config_store.h"
#include "test_helpers.h"
using namespace bs;
namespace fs = std::filesystem;

static fs::path tempDir(const char* tag) {
  // non-ASCII directory on purpose: users with Cyrillic account names must work
  fs::path d = fs::temp_directory_path() / bs::fsutil::fromUtf8(std::string("bodysync-тест-") + tag + "-" + std::to_string((long long)std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(d);
#ifdef _WIN32
  _wputenv_s(L"BODYSYNC_DATA_DIR", d.c_str());
#else
  setenv("BODYSYNC_DATA_DIR", d.c_str(), 1);
#endif
  return d;
}
static void writeText(const fs::path& p, const std::string& t) { fs::create_directories(p.parent_path()); bs::fsutil::writeFile(p, t); }
static std::string readText(const fs::path& p) { return bs::fsutil::readFile(p).value_or(""); }

TEST(config_json_roundtrip_is_lossless) {
  Profile p; p.name = "Alice"; p.body.heightM = 1.83; p.body.legScale = 1.07; p.calibration.valid = true; p.calibration.posOffset = {0.1, -0.2, 0.3};
  p.tracking.enabled[3] = false; p.tracking.updateRateHz = 120; p.poses.mode = PostureMode::Sitting; p.poses.sitLegExtend = 0.55;
  p.mixed.roles[kLeftFoot].serial = "LHR-1234"; p.mixed.roles[kLeftFoot].priority = RoleAssign::Priority::PhysicalOnly;
  json j = p; Profile q = j.get<Profile>();
  CHECK(json(q) == j); CHECK(q.name == "Alice"); CHECK(!q.tracking.enabled[3] && q.tracking.enabled[2]); CHECK(q.poses.mode == PostureMode::Sitting);
  CHECK(q.mixed.roles[kLeftFoot].priority == RoleAssign::Priority::PhysicalOnly && q.mixed.roles[kLeftFoot].serial == "LHR-1234");
  CHECK_NEAR(q.calibration.posOffset[2], 0.3, 0);
}
TEST(config_tolerates_missing_and_wrong_types) {
  json j = json::parse(R"({"name":"X","body":{"heightM":"tall","legScale":1.2},"tracking":{"enabled":[false,false],"smoothing":0.9,"updateRateHz":"fast"},"unknown_future_field":42})");
  Profile p = j.get<Profile>();
  CHECK_NEAR(p.body.heightM, 1.75, 0); CHECK_NEAR(p.body.legScale, 1.2, 0); CHECK(!p.tracking.enabled[0] && !p.tracking.enabled[1] && p.tracking.enabled[5]);
  CHECK_NEAR(p.tracking.smoothing, 0.9, 0); CHECK(p.tracking.updateRateHz == 90);
  bool threw = false; try { json("[1]").get<Profile>(); json::parse("[1]").get<Profile>(); } catch (...) { threw = true; } CHECK(threw);   // non-object rejected
}
TEST(config_sanitize_clamps_everything) {
  Profile p; double nan = std::nan("");
  p.body.heightM = nan; p.body.legScale = 9; p.tracking.smoothing = -4; p.tracking.updateRateHz = 100000; p.tracking.maxSpeedMps = INFINITY;
  p.poses.sitEnterRatio = 0.9; p.poses.sitExitRatio = 0.5; p.poses.holdTimeSec = nan; p.calibration.posOffset = {99, nan, 0}; p.name = "../../evil";
  p.mixed.roles[0].serial = "A"; p.mixed.roles[2].serial = "A"; p.mixed.roles[4].serial = "B";
  int n = sanitize(p);
  CHECK(n > 8); CHECK_NEAR(p.body.heightM, 1.75, 0); CHECK_NEAR(p.body.legScale, 1.5, 0); CHECK_NEAR(p.tracking.smoothing, 0, 0); CHECK(p.tracking.updateRateHz == 90);
  CHECK_NEAR(p.tracking.maxSpeedMps, 12, 0); CHECK(p.poses.sitExitRatio > p.poses.sitEnterRatio); CHECK(std::isfinite(p.poses.holdTimeSec));   // hysteresis enforced
  CHECK_NEAR(p.calibration.posOffset[0], 2, 0); CHECK_NEAR(p.calibration.posOffset[1], 0, 0);
  CHECK(p.name.find('/') == std::string::npos && !p.name.empty());
  CHECK(p.mixed.roles[0].serial == "A" && p.mixed.roles[2].serial.empty() && p.mixed.roles[4].serial == "B");     // one physical tracker, one role
  CHECK(sanitize(p) == 0);                                                                                       // idempotent
  CHECK(safeName("a/b\\c:d*e") == "abcde"); CHECK(safeName("  ..  ").empty()); CHECK(safeName("Профиль 1") == "Профиль 1");
}
TEST(config_save_load_and_atomic_replace) {
  fs::path d = tempDir("save"); Profile p; p.name = "Bob"; p.body.heightM = 1.9;
  fs::path f = d / "profiles" / "Bob.json"; std::string err;
  CHECK(saveProfileFile(f, p, &err)); CHECK(!fs::exists(fs::path(f) += ".tmp"));
  auto l = loadProfileFile(f); CHECK(l.status == LoadStatus::Ok); CHECK_NEAR(l.value.body.heightM, 1.9, 0);
  p.body.heightM = 1.6; CHECK(saveProfileFile(f, p)); CHECK_NEAR(loadProfileFile(f).value.body.heightM, 1.6, 0);   // overwrite works
  CHECK(loadProfileFile(d / "nothing.json").status == LoadStatus::Missing);
  AppSettings a; a.language = "ru"; a.activeProfile = "Bob"; CHECK(saveAppSettings(a)); auto la = loadAppSettings(); CHECK(la.value.language == "ru" && la.value.activeProfile == "Bob");
  fs::remove_all(d);
}
TEST(config_corrupt_file_recovers_and_keeps_evidence) {
  fs::path d = tempDir("corrupt"); fs::path f = d / "profiles" / "Broken.json";
  writeText(f, "{ this is not json ");
  auto l = loadProfileFile(f); CHECK(l.status == LoadStatus::Recovered); CHECK(!l.message.empty());
  CHECK(l.value.body.heightM == 1.75); CHECK(!fs::exists(f));                                                   // moved aside, not deleted
  bool kept = false; for (auto& e : fs::directory_iterator(d / "profiles")) if (e.path().string().find(".corrupt-") != std::string::npos) kept = true; CHECK(kept);
  writeText(d / "settings.json", "\x01\x02garbage"); auto s = loadAppSettings(); CHECK(s.status == LoadStatus::Recovered); CHECK(s.value.language == "en");
  writeText(f, "[1,2,3]"); CHECK(loadProfileFile(f).status == LoadStatus::Recovered);
  writeText(f, ""); CHECK(loadProfileFile(f).status == LoadStatus::Recovered);
  fs::remove_all(d);
}
TEST(config_integrity_and_migration) {
  fs::path d = tempDir("integrity"); fs::path f = d / "p.json"; Profile p; p.body.heightM = 1.8; saveProfileFile(f, p);
  std::string t = readText(f); size_t i = t.find("1.8"); CHECK(i != std::string::npos); t.replace(i, 3, "1.9"); writeText(f, t);   // hand edit without fixing the checksum
  auto l = loadProfileFile(f); CHECK(l.status == LoadStatus::IntegrityWarning); CHECK_NEAR(l.value.body.heightM, 1.9, 0);   // still usable
  writeText(f, R"({"name":"Old","body":{"heightM":1.7},"tracking":{"smoothing":0.3}})");   // legacy v0: no envelope, no version
  l = loadProfileFile(f); CHECK(l.status == LoadStatus::Migrated); CHECK_NEAR(l.value.body.heightM, 1.7, 0); CHECK_NEAR(l.value.tracking.smoothing, 0.3, 0); CHECK(l.value.version == kConfigVersion);
  CHECK_NEAR(l.value.poses.sitEnterRatio, 0.80, 0);                                                              // new fields get defaults
  writeText(f, R"({"format":"bodysync","kind":"profile","version":99,"checksum":"x","data":{"name":"Future","body":{"heightM":1.6},"newThing":{}}})");
  l = loadProfileFile(f); CHECK(l.value.name == "Future"); CHECK_NEAR(l.value.body.heightM, 1.6, 0);             // newer file: unknown parts ignored
  writeText(f, R"({"format":"bodysync","kind":"settings","version":1,"checksum":"x","data":{}})"); CHECK(loadProfileFile(f).status == LoadStatus::Recovered);   // wrong kind
  fs::remove_all(d);
}
TEST(config_presets_profiles_import_export) {
  fs::path d = tempDir("presets"); PoseSettings ps; ps.sitLegExtend = 0.4; BodyProfile bp; bp.heightM = 2.0;
  CHECK(savePreset("poses", "Couch", ps)); CHECK(savePreset("body", "Tall", bp)); CHECK(!savePreset("poses", "///", ps));
  CHECK(listPresets("poses") == std::vector<std::string>{"Couch"});
  CHECK_NEAR(loadPreset<PoseSettings>("poses", "Couch").value.sitLegExtend, 0.4, 0); CHECK_NEAR(loadPreset<BodyProfile>("body", "Tall").value.heightM, 2.0, 0);
  CHECK(loadPreset<PoseSettings>("poses", "Missing").status == LoadStatus::Missing);
  CHECK(loadPreset<PoseSettings>("body", "Tall").status == LoadStatus::Recovered);                               // wrong kind refused
  CHECK(deletePreset("poses", "Couch")); CHECK(listPresets("poses").empty());
  Profile p; p.name = "Carol"; p.body.heightM = 1.55; fs::path ex = d / "export" / "carol.bodysync.json";
  CHECK(exportProfile(p, ex)); auto im = importProfile(ex); CHECK(im.status == LoadStatus::Ok && im.value.name == "Carol" && im.value.body.heightM == 1.55);
  writeText(d / "bad.json", "nope"); auto bad = importProfile(d / "bad.json"); CHECK(bad.status == LoadStatus::Recovered); CHECK(fs::exists(d / "bad.json"));   // a file we only tried to import is left alone
  fs::remove_all(d);
}
TEST(config_unicode_profile_names) {
  fs::path d = tempDir("utf8"); Profile p; p.name = "Профиль Анна";
  CHECK(saveProfileFile(presetPath("profiles", p.name), p)); auto names = listPresets("profiles"); CHECK(names.size() == 1 && names[0] == "Профиль Анна");
  if (!names.empty()) { CHECK(loadProfileFile(presetPath("profiles", names[0])).value.name == "Профиль Анна"); }
  fs::remove_all(d);
}
