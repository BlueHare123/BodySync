// BodySync desktop app: Win32 window + Direct3D 11 + Dear ImGui.
#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <dxgi.h>
#include <shellapi.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include "app.h"
#include "bodysync/fsutil.h"
#include "bodysync/log.h"
#include "selftest.h"
#include "ui.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
using namespace bs;

namespace {
ID3D11Device* g_dev = nullptr; ID3D11DeviceContext* g_ctx = nullptr; IDXGISwapChain* g_sc = nullptr; ID3D11RenderTargetView* g_rtv = nullptr;
bool g_occluded = false;

void createRtv() {
  ID3D11Texture2D* bb = nullptr; g_sc->GetBuffer(0, IID_PPV_ARGS(&bb)); g_dev->CreateRenderTargetView(bb, nullptr, &g_rtv); bb->Release();
}
void releaseRtv() { if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; } }
bool createDevice(HWND hwnd) {
  DXGI_SWAP_CHAIN_DESC sd{}; sd.BufferCount = 2; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  D3D_FEATURE_LEVEL fl; const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
  for (D3D_DRIVER_TYPE t : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP})   // WARP = software fallback, keeps the app usable on machines without a usable GPU driver
    if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr, t, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd, &g_sc, &g_dev, &fl, &g_ctx))) { createRtv(); return true; }
  return false;
}
void cleanupDevice() { releaseRtv(); if (g_sc) g_sc->Release(); if (g_ctx) g_ctx->Release(); if (g_dev) g_dev->Release(); g_sc = nullptr; g_ctx = nullptr; g_dev = nullptr; }

LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return true;
  switch (m) {
    case WM_SIZE: if (g_dev && w != SIZE_MINIMIZED) { releaseRtv(); g_sc->ResizeBuffers(0, LOWORD(l), HIWORD(l), DXGI_FORMAT_UNKNOWN, 0); createRtv(); } return 0;
    case WM_GETMINMAXINFO: { auto* mm = (MINMAXINFO*)l; UINT d = GetDpiForWindow(h); mm->ptMinTrackSize = {(LONG)(980 * d / 96), (LONG)(640 * d / 96)}; return 0; }
    case WM_DPICHANGED: { auto* r = (RECT*)l; SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE); return 0; }   // shortcut: fonts keep the DPI scale from start-up; upgrade: rebuild the atlas on WM_DPICHANGED
    case WM_SYSCOMMAND: if ((w & 0xfff0) == SC_KEYMENU) return 0; break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
  }
  return DefWindowProcW(h, m, w, l);
}
void attachConsole() {
  if (AttachConsole(ATTACH_PARENT_PROCESS)) { FILE* f; freopen_s(&f, "CONOUT$", "w", stdout); freopen_s(&f, "CONOUT$", "w", stderr); }
}
void loadFonts(float scale) {
  ImGuiIO& io = ImGui::GetIO(); wchar_t win[MAX_PATH], exe[MAX_PATH]; GetWindowsDirectoryW(win, MAX_PATH); GetModuleFileNameW(nullptr, exe, MAX_PATH);
  namespace fs = std::filesystem; fs::path dir = fs::path(exe).parent_path() / L"fonts";
  // Glyphs: Basic Latin + Latin-1 (de, fr, es accents, ß, ¿ ¡ « »), Latin Extended-A (œ Œ ÿ), Cyrillic (ru), general punctuation and a few symbols.
  static ImVector<ImWchar> ranges; ImFontGlyphRangesBuilder b; b.AddRanges(io.Fonts->GetGlyphRangesDefault()); b.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
  static const ImWchar extra[] = {0x0100, 0x017F, 0x2010, 0x2027, 0x2030, 0x203A, 0x20AC, 0x20AC, 0x2116, 0x2116, 0x2190, 0x2193, 0x2212, 0x2212, 0};
  b.AddRanges(extra); b.BuildRanges(&ranges);
  auto load = [&](const std::vector<fs::path>& cand, float px) -> ImFont* {
    for (auto& p : cand) { std::error_code ec; if (!fs::exists(p, ec)) continue; std::string path = fsutil::toUtf8(p); if (ImFont* f = io.Fonts->AddFontFromFileTTF(path.c_str(), px * scale, nullptr, ranges.Data)) return f; }
    return nullptr;
  };
  // The bundled Noto Sans (SIL OFL, covers all five languages) is used first, so the look does not depend on what is installed. System fonts are only a fallback.
  std::vector<fs::path> body = {dir / L"NotoSans-Regular.ttf", fs::path(win) / L"Fonts" / L"segoeui.ttf", fs::path(win) / L"Fonts" / L"tahoma.ttf", fs::path(win) / L"Fonts" / L"arial.ttf"};
  std::vector<fs::path> bold = {dir / L"NotoSans-Bold.ttf", fs::path(win) / L"Fonts" / L"segoeuib.ttf", fs::path(win) / L"Fonts" / L"tahomabd.ttf", fs::path(win) / L"Fonts" / L"arialbd.ttf"};
  ImFont* f = load(body, 16.0f);
  if (!f) { ImFontConfig c; c.SizePixels = 16.0f * scale; io.Fonts->AddFontDefault(&c); return; }   // Latin only; other languages would show '?'
  ui::headFont = load(bold, 17.5f);
}
}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
  int argc = 0; LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  std::string lang; int startPage = 0; bool selftest = false, simulate = false; std::wstring dataDirArg, outFile;
  for (int i = 1; i < argc; ++i) {
    std::wstring a = argv[i];
    if (a == L"--selftest") selftest = true;
    else if (a == L"--simulate") simulate = true;
    else if (a == L"--version") { attachConsole(); std::printf("BodySync %s\n", BODYSYNC_VERSION); return 0; }
    else if (a == L"--help" || a == L"-h" || a == L"/?") { attachConsole(); std::printf("BodySync %s\nUsage: BodySync.exe [--selftest [--out file]] [--simulate] [--page 0-5] [--lang en|ru|de|fr|es] [--data-dir path] [--version]\n", BODYSYNC_VERSION); return 0; }
    else if (a == L"--page" && i + 1 < argc) startPage = _wtoi(argv[++i]);
    else if (a == L"--lang" && i + 1 < argc) lang = fsutil::toUtf8(argv[++i]);
    else if (a == L"--data-dir" && i + 1 < argc) dataDirArg = argv[++i];
    else if (a == L"--out" && i + 1 < argc) outFile = argv[++i];
  }
  if (!dataDirArg.empty()) _wputenv_s(L"BODYSYNC_DATA_DIR", dataDirArg.c_str());
  if (selftest) { attachConsole(); int rc = runSelfTest(outFile); LocalFree(argv); return rc; }

  HANDLE mtx = CreateMutexW(nullptr, TRUE, L"Local\\BodySync.SingleInstance");
  if (GetLastError() == ERROR_ALREADY_EXISTS) { if (HWND w = FindWindowW(L"BodySyncWnd", nullptr)) { ShowWindow(w, SW_RESTORE); SetForegroundWindow(w); } return 0; }
  Logger::instance().start(bs::dataDir() / "logs" / "bodysync.log");
  logf(LogLevel::Info, "BodySync %s starting", BODYSYNC_VERSION);

  WNDCLASSEXW wc{sizeof wc, CS_CLASSDC, wndProc, 0, 0, inst, LoadIconW(inst, MAKEINTRESOURCEW(1)), LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr, L"BodySyncWnd", LoadIconW(inst, MAKEINTRESOURCEW(1))};
  RegisterClassExW(&wc);
  float dpi = 1.0f; { HDC dc = GetDC(nullptr); dpi = (float)GetDeviceCaps(dc, LOGPIXELSX) / 96.0f; ReleaseDC(nullptr, dc); }
  int W = (int)(1240 * dpi), H = (int)(800 * dpi);
  HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"BodySync", WS_OVERLAPPEDWINDOW, (GetSystemMetrics(SM_CXSCREEN) - W) / 2, (GetSystemMetrics(SM_CYSCREEN) - H) / 2, W, H, nullptr, nullptr, inst, nullptr);
  dpi = (float)GetDpiForWindow(hwnd) / 96.0f;
  BOOL dark = TRUE; DwmSetWindowAttribute(hwnd, 20, &dark, sizeof dark);
  if (!createDevice(hwnd)) { MessageBoxW(nullptr, L"Direct3D 11 is not available on this computer.", L"BodySync", MB_ICONERROR); return 1; }
  ShowWindow(hwnd, SW_SHOWDEFAULT); UpdateWindow(hwnd);

  IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io = ImGui::GetIO(); io.IniFilename = nullptr; io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  loadFonts(dpi); ui::applyTheme(dpi);
  ImGui_ImplWin32_Init(hwnd); ImGui_ImplDX11_Init(g_dev, g_ctx);
  {
    ui::App app(kPipeName, true);
    if (!lang.empty()) app.setLanguage(lang);
    if (simulate) app.setSource(InputSource::Simulation);
    app.page = startPage < 0 || startPage >= ui::App::kPages ? 0 : startPage;
    MSG msg{}; bool done = false;
    while (!done) {
      while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); if (msg.message == WM_QUIT) done = true; }
      if (done) break;
      if (IsIconic(hwnd) || g_occluded) { Sleep(60); if (g_occluded && g_sc->Present(0, DXGI_PRESENT_TEST) != DXGI_STATUS_OCCLUDED) g_occluded = false; if (IsIconic(hwnd)) continue; }
      io.FontGlobalScale = (float)app.as.uiScale;
      ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
      ui::drawApp(app);
      ImGui::Render();
      const float clear[4] = {0.075f, 0.086f, 0.105f, 1};
      g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr); g_ctx->ClearRenderTargetView(g_rtv, clear); ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
      HRESULT hr = g_sc->Present(1, 0); g_occluded = hr == DXGI_STATUS_OCCLUDED;
    }
  }   // App destructor: saves pending changes, stops the engine (driver then hides the trackers)
  ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
  cleanupDevice(); DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName, inst);
  logf(LogLevel::Info, "BodySync exited"); Logger::instance().stop(); CloseHandle(mtx); LocalFree(argv);
  return 0;
}
