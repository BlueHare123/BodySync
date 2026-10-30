#include "bodysync/config_store.h"
#include "bodysync/fsutil.h"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>

namespace fs = std::filesystem;
namespace bs {
using fsutil::fromUtf8; using fsutil::toUtf8;

fs::path dataDir() {
  if (auto e = fsutil::envPath("BODYSYNC_DATA_DIR")) return *e;
  if (auto l = fsutil::envPath("LOCALAPPDATA")) return *l / "BodySync";
  if (auto h = fsutil::envPath("HOME")) return *h / ".local" / "share" / "BodySync";
  return fs::path("BodySync-data");
}
std::string checksumHex(const json& payload) {   // FNV-1a 64 over the canonical (key-sorted) dump
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : payload.dump()) { h ^= c; h *= 1099511628211ull; }
  char b[20]; std::snprintf(b, sizeof b, "%016llx", (unsigned long long)h);
  return b;
}
bool writeFileAtomic(const fs::path& p, const std::string& text, std::string* err) {
  std::error_code ec;
  if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
  fs::path tmp = p; tmp += ".tmp";
  if (!fsutil::writeFile(tmp, text)) { if (err) *err = "cannot write " + toUtf8(tmp); return false; }
  fs::rename(tmp, p, ec);   // replaces an existing file
  if (ec) { if (err) *err = "rename failed: " + ec.message(); fs::remove(tmp, ec); return false; }
  return true;
}
bool saveEnvelope(const fs::path& p, const std::string& kind, const json& data, std::string* err) {
  json env = {{"format", "bodysync"}, {"kind", kind}, {"version", kConfigVersion}, {"checksum", checksumHex(data)}, {"data", data}};
  return writeFileAtomic(p, env.dump(2), err);
}
static std::string stamp() { std::time_t t = std::time(nullptr); char b[32]; std::strftime(b, sizeof b, "%Y%m%d-%H%M%S", std::localtime(&t)); return b; }

Loaded<json> loadEnvelope(const fs::path& p, const std::string& kind) {
  Loaded<json> r; r.value = nullptr;
  std::error_code ec;
  if (!fs::exists(p, ec)) { r.status = LoadStatus::Missing; return r; }
  auto text = fsutil::readFile(p);   // closed before any rename below (Windows refuses to rename open files)
  json root = text ? json::parse(*text, nullptr, false) : json(json::value_t::discarded);
  if (root.is_discarded() || !root.is_object()) {
    fs::path bad = p; bad += ".corrupt-" + stamp(); fs::rename(p, bad, ec);   // keep evidence, never delete user data
    r.status = LoadStatus::Recovered; r.message = "corrupt file moved to " + toUtf8(bad.filename());
    return r;
  }
  if (root.value("format", "") == "bodysync" && root.contains("data")) {
    if (root.value("kind", kind) != kind) { r.status = LoadStatus::Recovered; r.message = "file has wrong kind '" + root.value("kind", "") + "'"; return r; }
    r.value = root["data"];
    int ver = root.value("version", 0);
    if (root.value("checksum", "") != checksumHex(r.value)) { r.status = LoadStatus::IntegrityWarning; r.message = "checksum mismatch (file edited outside BodySync?)"; }
    else if (ver < kConfigVersion) { r.status = LoadStatus::Migrated; r.message = "migrated from format v" + std::to_string(ver); }
    else if (ver > kConfigVersion) { r.status = LoadStatus::Migrated; r.message = "file is from a newer BodySync (v" + std::to_string(ver) + "); unknown fields ignored"; }
  } else {   // legacy v0: the file itself is the payload
    r.value = root; r.status = LoadStatus::Migrated; r.message = "migrated from legacy (unversioned) format";
  }
  return r;
}

Loaded<Profile> loadProfileFile(const fs::path& p) {
  Loaded<json> e = loadEnvelope(p, "profile");
  Loaded<Profile> r; r.status = e.status; r.message = e.message;
  if (e.value.is_null()) { r.value.name = safeName(toUtf8(p.stem())); if (r.value.name.empty()) r.value.name = "Default"; return r; }
  try { r.value = e.value.get<Profile>(); }
  catch (const std::exception& ex) { r.value = Profile{}; r.status = LoadStatus::Recovered; r.message = std::string("invalid profile: ") + ex.what(); }
  { PoseSettings before = r.value.poses; migratePoseDefaults(r.value.poses); if (r.status == LoadStatus::Ok && (before.lieHeightRatio != r.value.poses.lieHeightRatio)) { r.status = LoadStatus::Migrated; r.message = "lying-detection defaults updated to v1.0.0"; } }
  if (sanitize(r.value) && r.status == LoadStatus::Ok) { r.status = LoadStatus::Migrated; r.message = "out-of-range values were corrected"; }
  return r;
}
bool saveProfileFile(const fs::path& p, const Profile& prof, std::string* err) { Profile c = prof; sanitize(c); return saveEnvelope(p, "profile", json(c), err); }

Loaded<AppSettings> loadAppSettings() {
  Loaded<json> e = loadEnvelope(dataDir() / "settings.json", "settings");
  Loaded<AppSettings> r; r.status = e.status; r.message = e.message;
  if (!e.value.is_null()) { try { r.value = e.value.get<AppSettings>(); } catch (...) { r.value = AppSettings{}; r.status = LoadStatus::Recovered; } }
  sanitize(r.value);
  return r;
}
bool saveAppSettings(const AppSettings& s, std::string* err) { AppSettings c = s; sanitize(c); return saveEnvelope(dataDir() / "settings.json", "settings", json(c), err); }

fs::path presetPath(const std::string& kind, const std::string& name) { return dataDir() / kind / fromUtf8(name + ".json"); }
std::vector<std::string> listPresets(const std::string& kind) {
  std::vector<std::string> v; std::error_code ec;
  for (auto& d : fs::directory_iterator(dataDir() / kind, ec))
    if (d.is_regular_file() && d.path().extension() == ".json") v.push_back(toUtf8(d.path().stem()));
  std::sort(v.begin(), v.end());
  return v;
}
bool deletePreset(const std::string& kind, const std::string& name) { std::error_code ec; return fs::remove(presetPath(kind, safeName(name)), ec); }
bool exportProfile(const Profile& p, const fs::path& to, std::string* err) { return saveProfileFile(to, p, err); }
Loaded<Profile> importProfile(const fs::path& from) {
  Loaded<Profile> r = loadProfileFile(from);
  if (r.status == LoadStatus::Recovered || r.status == LoadStatus::Missing) {   // do not rename a user's file we merely tried to import
    std::error_code ec; fs::path bad = from; bad += ".corrupt-" + stamp();
    if (fs::exists(bad, ec)) fs::rename(bad, from, ec);
  }
  return r;
}
}  // namespace bs
