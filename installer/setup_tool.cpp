// bodysync_setup.exe: registers / unregisters the BodySync SteamVR driver folder using the documented SteamVR tool (vrpathreg)
// or, if that tool is unavailable, by editing the user-level openvrpaths.vrpath file (backup first). Never touches SteamVR processes.
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include "bodysync/config_store.h"
#include "bodysync/fsutil.h"

using namespace bs; namespace fs = std::filesystem;

static fs::path vrPathFile() {
  auto la = fsutil::envPath("LOCALAPPDATA"); return la ? *la / "openvr" / "openvrpaths.vrpath" : fs::path();
}
static std::string norm(std::string s) {
  std::replace(s.begin(), s.end(), '/', '\\'); while (s.size() > 3 && s.back() == '\\') s.pop_back();
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); }); return s;
}
static json readPaths(std::string* err) {
  fs::path f = vrPathFile(); auto txt = fsutil::readFile(f);
  if (!txt) { *err = "SteamVR path file not found (" + fsutil::toUtf8(f) + "). Install and start SteamVR once, then retry."; return nullptr; }
  try { json j = json::parse(*txt); if (!j.is_object()) throw std::runtime_error("not an object"); return j; } catch (const std::exception& e) { *err = std::string("cannot parse openvrpaths.vrpath: ") + e.what(); return nullptr; }
}
static bool isRegistered(const std::string& dir) {
  std::string e; json j = readPaths(&e); if (j.is_null() || !j.contains("external_drivers") || !j["external_drivers"].is_array()) return false;
  for (auto& d : j["external_drivers"]) if (d.is_string() && norm(d.get<std::string>()) == norm(dir)) return true;
  return false;
}
static fs::path findVrPathReg() {
  std::string e; json j = readPaths(&e); if (j.is_null() || !j.contains("runtime") || !j["runtime"].is_array()) return {};
  for (auto& r : j["runtime"]) if (r.is_string()) { fs::path p = fsutil::fromUtf8(r.get<std::string>()) / "bin" / "win64" / "vrpathreg.exe"; std::error_code ec; if (fs::exists(p, ec)) return p; }
  return {};
}
static int runTool(const fs::path& exe, const std::wstring& args) {
  std::wstring cmd = L"\"" + exe.wstring() + L"\" " + args; STARTUPINFOW si{sizeof si}; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; PROCESS_INFORMATION pi{};
  if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return -1;
  DWORD w = WaitForSingleObject(pi.hProcess, 15000); DWORD code = 1; if (w == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code); else TerminateProcess(pi.hProcess, 1);
  CloseHandle(pi.hProcess); CloseHandle(pi.hThread); return (int)code;
}
static bool editJson(const std::string& dir, bool add, std::string* err) {
  json j = readPaths(err); if (j.is_null()) return false;
  if (!j.contains("external_drivers") || !j["external_drivers"].is_array()) j["external_drivers"] = json::array();
  json out = json::array(); bool had = false;
  for (auto& d : j["external_drivers"]) { if (d.is_string() && norm(d.get<std::string>()) == norm(dir)) { had = true; if (add) out.push_back(d); } else out.push_back(d); }
  if (add && !had) out.push_back(dir);
  j["external_drivers"] = out;
  fs::path f = vrPathFile(); std::error_code ec; fs::copy_file(f, fs::path(f) += ".bodysync-backup", fs::copy_options::skip_existing, ec);   // first change keeps the original
  return writeFileAtomic(f, j.dump(2), err);
}
static bool validDriverDir(const std::string& dir, std::string* err) {
  fs::path d = fsutil::fromUtf8(dir);
  if (!fs::exists(d / "driver.vrdrivermanifest") || !fs::exists(d / "bin" / "win64" / "driver_bodysync.dll")) { *err = "not a BodySync driver folder (driver.vrdrivermanifest / bin\\win64\\driver_bodysync.dll missing): " + dir; return false; }
  return true;
}
static bool steamVrRunning() {   // informational only; we never close it
  HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); if (s == INVALID_HANDLE_VALUE) return false; PROCESSENTRY32W e{sizeof e}; bool r = false;
  for (BOOL ok = Process32FirstW(s, &e); ok; ok = Process32NextW(s, &e)) if (!_wcsicmp(e.szExeFile, L"vrserver.exe")) r = true;
  CloseHandle(s); return r;
}

int wmain(int argc, wchar_t** argv) {
  std::string cmd = argc > 1 ? fsutil::toUtf8(argv[1]) : "", dir = argc > 2 ? fsutil::toUtf8(argv[2]) : "", err;
  if (cmd == "register" || cmd == "unregister") {
    bool add = cmd == "register";
    if (dir.empty()) { std::fputs("usage: bodysync_setup register|unregister <driver folder>\n", stderr); return 64; }
    { std::error_code ec; dir = fsutil::toUtf8(fs::absolute(fsutil::fromUtf8(dir), ec)); while (dir.size() > 3 && (dir.back() == '\\' || dir.back() == '/')) dir.pop_back(); }
    if (add && !validDriverDir(dir, &err)) { std::fprintf(stderr, "ERROR: %s\n", err.c_str()); return 3; }
    { std::string e; if (readPaths(&e).is_null()) { if (add) { std::fprintf(stderr, "ERROR: %s\n", e.c_str()); return 2; } std::puts("SteamVR path file not found; nothing to unregister."); return 0; } }
    if (isRegistered(dir) == add) { std::puts(add ? "Driver is already registered." : "Driver is not registered."); return 0; }
    fs::path reg = findVrPathReg(); bool done = false;
    if (!reg.empty()) { std::wstring a = std::wstring(add ? L"adddriver" : L"removedriver") + L" \"" + fsutil::fromUtf8(dir).wstring() + L"\""; int rc = runTool(reg, a); done = isRegistered(dir) == add; if (!done) std::printf("vrpathreg returned %d without effect; using the path file directly.\n", rc); }
    if (!done) { if (!editJson(dir, add, &err)) { std::fprintf(stderr, "ERROR: %s\n", err.c_str()); return 4; } done = isRegistered(dir) == add; }
    if (!done) { std::fputs("ERROR: change could not be verified.\n", stderr); return 5; }
    std::printf("%s: %s\n", add ? "Driver registered" : "Driver unregistered", dir.c_str());
    if (steamVrRunning()) std::puts("SteamVR is running: restart SteamVR (BodySync did not close it) for the change to take effect.");
    return 0;
  }
  if (cmd == "check") {
    std::string e; json j = readPaths(&e);
    std::printf("openvrpaths.vrpath: %s\n", j.is_null() ? e.c_str() : "found");
    fs::path reg = findVrPathReg(); std::printf("vrpathreg: %s\n", reg.empty() ? "not found (path file will be edited directly)" : fsutil::toUtf8(reg).c_str());
    if (!dir.empty()) std::printf("driver registered: %s\n", isRegistered(dir) ? "yes" : "no");
    std::printf("SteamVR running: %s\n", steamVrRunning() ? "yes" : "no");
    return j.is_null() ? 2 : (dir.empty() || isRegistered(dir) ? 0 : 1);
  }
  std::fputs("BodySync setup helper\nusage: bodysync_setup register <driver folder> | unregister <driver folder> | check [driver folder]\n", stderr);
  return 64;
}
