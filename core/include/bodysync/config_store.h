// Persistence: versioned envelope {format,kind,version,checksum,data}; atomic writes; corrupt-file recovery.
#pragma once
#include <filesystem>
#include <string>
#include <type_traits>
#include <vector>
#include "config.h"
#include "config_json.h"

namespace bs {
std::filesystem::path dataDir();            // BODYSYNC_DATA_DIR, else %LOCALAPPDATA%\BodySync (~/.local/share/BodySync elsewhere)
enum class LoadStatus { Ok, Missing, Migrated, IntegrityWarning, Recovered };
template <class T> struct Loaded { T value{}; LoadStatus status = LoadStatus::Ok; std::string message; };

std::string checksumHex(const json& payload);
bool writeFileAtomic(const std::filesystem::path& p, const std::string& text, std::string* err = nullptr);
bool saveEnvelope(const std::filesystem::path& p, const std::string& kind, const json& data, std::string* err = nullptr);
// Loads+validates. On corrupt JSON the file is renamed to *.corrupt-<time> and status=Recovered (data = null).
Loaded<json> loadEnvelope(const std::filesystem::path& p, const std::string& kind);

Loaded<Profile> loadProfileFile(const std::filesystem::path& p);
bool saveProfileFile(const std::filesystem::path& p, const Profile& prof, std::string* err = nullptr);
Loaded<AppSettings> loadAppSettings();
bool saveAppSettings(const AppSettings& s, std::string* err = nullptr);

// Named presets under dataDir()/<kind>/<name>.json. kind: "profiles","body","poses","calibration"
std::filesystem::path presetPath(const std::string& kind, const std::string& name);
std::vector<std::string> listPresets(const std::string& kind);
bool deletePreset(const std::string& kind, const std::string& name);
template <class T> bool savePreset(const std::string& kind, const std::string& name, const T& v, std::string* err = nullptr);
template <class T> Loaded<T> loadPreset(const std::string& kind, const std::string& name);
// import/export of whole profiles to/from arbitrary paths
bool exportProfile(const Profile& p, const std::filesystem::path& to, std::string* err = nullptr);
Loaded<Profile> importProfile(const std::filesystem::path& from);


template <class T> const char* presetKindTag();
template <> inline const char* presetKindTag<BodyProfile>() { return "body"; }
template <> inline const char* presetKindTag<PoseSettings>() { return "poses"; }
template <> inline const char* presetKindTag<Calibration>() { return "calibration"; }
template <class T> bool savePreset(const std::string& kind, const std::string& name, const T& v, std::string* err) {
  std::string n = safeName(name);
  if (n.empty()) { if (err) *err = "invalid name"; return false; }
  return saveEnvelope(presetPath(kind, n), presetKindTag<T>(), json(v), err);
}
template <class T> Loaded<T> loadPreset(const std::string& kind, const std::string& name) {
  Loaded<json> e = loadEnvelope(presetPath(kind, safeName(name)), presetKindTag<T>());
  Loaded<T> r; r.status = e.status; r.message = e.message;
  if (!e.value.is_null()) { try { r.value = e.value.get<T>(); if constexpr (std::is_same_v<T, PoseSettings>) migratePoseDefaults(r.value); sanitize(r.value); } catch (const std::exception& ex) { r.status = LoadStatus::Recovered; r.message = ex.what(); r.value = T{}; } }
  return r;
}
}  // namespace bs
