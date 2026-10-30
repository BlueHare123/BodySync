// All pages. Every control edits the Profile/AppSettings that the solver, mixer and engine really read (see test_settings_effect).
#include "ui.h"
#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include "bodysync/log.h"

namespace bs::ui {
namespace {
App* A; float em = 16;
const ImVec4 kAccent{0.30f, 0.78f, 0.69f, 1}, kWarn{0.96f, 0.72f, 0.26f, 1}, kBad{0.93f, 0.36f, 0.36f, 1}, kGood{0.40f, 0.82f, 0.50f, 1}, kDim{0.60f, 0.64f, 0.70f, 1}, kPhys{0.98f, 0.62f, 0.25f, 1};

void Tip(const char* t) { if (t && *t && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetItemTooltip("%s", tr(t)); }
// Row layout used by every labelled control: label column on the left, control in the middle, value on the right (never on top of the grab).
float LabelW() { return std::max(15 * em, ImGui::GetContentRegionAvail().x * 0.42f); }
bool RowLabel(const char* label, const char* tip, float& x0, float& rest) {
  x0 = ImGui::GetCursorPosX(); float lw = LabelW(); rest = ImGui::GetContentRegionAvail().x - lw;
  ImGui::AlignTextToFramePadding(); ImGui::PushTextWrapPos(ImGui::GetCursorScreenPos().x + lw - em * 0.6f); ImGui::TextUnformatted(tr(label)); ImGui::PopTextWrapPos();
  if (tip && *tip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", tr(tip));
  ImGui::SameLine(x0 + lw); return rest > 6 * em;
}
void ValueText(const char* txt) { ImGui::SameLine(); ImGui::AlignTextToFramePadding(); ImGui::TextColored({0.86f, 0.90f, 0.93f, 1}, "%s", txt); }
bool Slider(const char* label, double& v, double lo, double hi, const char* f, const char* tip) {
  float x0, rest; RowLabel(label, tip, x0, rest); float vw = 5.2f * em;
  float x = (float)v; ImGui::SetNextItemWidth(std::min(rest - vw - ImGui::GetStyle().ItemSpacing.x, 26 * em));
  bool ch = ImGui::SliderFloat((std::string("##") + label).c_str(), &x, (float)lo, (float)hi, "", ImGuiSliderFlags_AlwaysClamp);
  Tip(tip); if (ch) { v = x; A->markDirty(); }
  char buf[48]; std::snprintf(buf, sizeof buf, f, (double)x); ValueText(buf); return ch;
}
bool SliderI(const char* label, int& v, int lo, int hi, const char* tip) {
  float x0, rest; RowLabel(label, tip, x0, rest); float vw = 5.2f * em;
  ImGui::SetNextItemWidth(std::min(rest - vw - ImGui::GetStyle().ItemSpacing.x, 26 * em));
  bool ch = ImGui::SliderInt((std::string("##") + label).c_str(), &v, lo, hi, ""); Tip(tip); if (ch) A->markDirty();
  char buf[32]; std::snprintf(buf, sizeof buf, "%d", v); ValueText(buf); return ch;
}
bool Check(const char* label, bool& v, const char* tip) { bool ch = ImGui::Checkbox((std::string(tr(label)) + "##" + label).c_str(), &v); Tip(tip); if (ch) A->markDirty(); return ch; }
bool Combo(const char* label, int& idx, std::initializer_list<const char*> items, const char* tip, float w = 0) {
  std::vector<const char*> v(items); bool ch = false; float x0, rest; RowLabel(label, tip, x0, rest);
  ImGui::SetNextItemWidth(w > 0 ? w : std::min(rest, 22 * em));
  if (ImGui::BeginCombo((std::string("##") + label).c_str(), tr(v[(size_t)std::clamp(idx, 0, (int)v.size() - 1)]))) {
    for (int i = 0; i < (int)v.size(); ++i) if (ImGui::Selectable(tr(v[(size_t)i]), i == idx)) { idx = i; ch = true; }
    ImGui::EndCombo();
  }
  Tip(tip); if (ch) A->markDirty(); return ch;
}
void Chip(const char* text, ImVec4 col, const char* detail = nullptr) {
  ImVec2 p = ImGui::GetCursorScreenPos(); float r = em * 0.32f;
  ImGui::GetWindowDrawList()->AddCircleFilled({p.x + r + 2, p.y + em * 0.62f}, r, ImGui::ColorConvertFloat4ToU32(col));
  ImGui::Dummy({r * 2 + 8, em}); ImGui::SameLine(); ImGui::TextWrapped("%s", text);
  if (detail && *detail) { ImGui::SameLine(); ImGui::TextColored(kDim, "%s", detail); }
}
bool Card(const char* id, float w, float h) { ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {14, 12}); bool o = ImGui::BeginChild(id, {w, h}, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding | (h == 0 ? ImGuiChildFlags_AutoResizeY : 0)); ImGui::PopStyleVar(); return o; }
void Title(const char* t) { if (headFont) ImGui::PushFont(headFont); ImGui::TextColored(kAccent, "%s", tr(t)); if (headFont) ImGui::PopFont(); ImGui::Spacing(); }
bool Button(const char* t, bool primary = false, float w = 0) {
  if (primary) { ImGui::PushStyleColor(ImGuiCol_Button, {0.20f, 0.52f, 0.47f, 1}); ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.26f, 0.62f, 0.56f, 1}); }
  bool b = ImGui::Button((std::string(tr(t)) + "##" + t).c_str(), {w, 0}); if (primary) ImGui::PopStyleColor(2); return b;
}
void Para(const char* t, ImVec4 col = {0.78f, 0.80f, 0.84f, 1}) { ImGui::PushStyleColor(ImGuiCol_Text, col); ImGui::TextWrapped("%s", tr(t)); ImGui::PopStyleColor(); }

// ---------- skeleton preview ----------
void DrawBody(const SolverOutput& o, int view, ImVec2 size) {
  ImVec2 p0 = ImGui::GetCursorScreenPos(); ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(p0, {p0.x + size.x, p0.y + size.y}, IM_COL32(18, 21, 26, 255), 8);
  const Skeleton& s = o.skel; const auto& t = o.tr;
  Vec3 P[16] = {s.head, s.neck, s.shL, s.shR, t[kWaist].pose.p, s.hipL, s.hipR, t[kLeftKnee].pose.p, t[kRightKnee].pose.p, t[kLeftFoot].pose.p, t[kRightFoot].pose.p, t[kLeftElbow].pose.p, t[kRightElbow].pose.p, s.handL, s.handR, t[kChest].pose.p};
  auto proj = [&](const Vec3& v) { return view == 0 ? ImVec2((float)v.x, (float)-v.y) : view == 1 ? ImVec2((float)-v.z, (float)-v.y) : ImVec2((float)v.x, (float)v.z); };
  float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f; bool ok = true;
  for (auto& v : P) { if (!v.isFinite()) { ok = false; break; } ImVec2 q = proj(v); x0 = std::min(x0, q.x); x1 = std::max(x1, q.x); y0 = std::min(y0, q.y); y1 = std::max(y1, q.y); }
  const char* names[3] = {N_("Front"), N_("Side"), N_("Top")}; dl->AddText({p0.x + 8, p0.y + 6}, IM_COL32(150, 160, 172, 255), tr(names[view]));
  if (!ok) { ImGui::Dummy(size); return; }
  float cw = std::max(0.9f, x1 - x0), ch = std::max(0.9f, y1 - y0), sc = std::min((size.x - 40) / cw, (size.y - 44) / ch);
  float cx = p0.x + size.x / 2 - (x0 + x1) / 2 * sc, cy = p0.y + size.y / 2 + 8 - (y0 + y1) / 2 * sc;
  auto S = [&](const Vec3& v) { ImVec2 q = proj(v); return ImVec2(cx + q.x * sc, cy + q.y * sc); };
  if (view < 2) { float fy = cy + (float)(-A->prof.calibration.floorOffsetM) * sc; dl->AddLine({p0.x + 10, fy}, {p0.x + size.x - 10, fy}, IM_COL32(70, 78, 90, 255), 1.5f); }
  static const int bones[][2] = {{0, 1}, {1, 15}, {15, 4}, {1, 2}, {1, 3}, {2, 11}, {11, 13}, {3, 12}, {12, 14}, {4, 5}, {4, 6}, {5, 7}, {7, 9}, {6, 8}, {8, 10}};
  for (auto& b : bones) dl->AddLine(S(P[b[0]]), S(P[b[1]]), IM_COL32(110, 124, 140, 255), 2.f * em / 16);
  dl->AddCircle(S(P[0]), 0.09f * sc, IM_COL32(150, 165, 180, 255), 20, 2.f);
  static const int rolePt[kRoleCount] = {4, 15, 11, 12, 7, 8, 9, 10};
  for (int r = 0; r < kRoleCount; ++r) {
    const TrackerOut& to = o.tr[r]; if (!to.active) continue;
    ImU32 col = !to.valid ? IM_COL32(230, 90, 90, 255) : to.src == Source::Physical ? IM_COL32(250, 158, 64, 255) : to.src == Source::Blend ? IM_COL32(245, 215, 90, 255) : IM_COL32(76, 199, 176, 255);
    dl->AddCircleFilled(S(P[rolePt[r]]), em * 0.42f, col); dl->AddCircle(S(P[rolePt[r]]), em * 0.42f, IM_COL32(10, 12, 15, 255), 16, 1.5f);
  }
  ImGui::Dummy(size);
}
std::string VrText(const std::string& s) { const std::string pre = "OpenVR error: "; return s.rfind(pre, 0) == 0 ? std::string(tr(pre.c_str())) + s.substr(pre.size()) : std::string(tr(s.c_str())); }
const char* srcName(Source s) { switch (s) { case Source::Virtual: return N_("Virtual"); case Source::Physical: return N_("Physical"); case Source::Blend: return N_("Blend"); default: return N_("None"); } }

// ---------- presets ----------
std::map<std::string, std::pair<double, std::vector<std::string>>> g_presetCache;
template <class T> void PresetBar(const char* kind, T& value, const char* title) {
  PresetUi& u = A->presetUi[kind]; auto& cache = g_presetCache[kind];
  if (A->now - cache.first > 1.0) { cache.second = listPresets(kind); cache.first = A->now; }
  auto& names = cache.second; if (u.sel >= (int)names.size()) u.sel = -1;
  ImGui::PushID(kind); ImGui::SeparatorText(tr(title));
  ImGui::SetNextItemWidth(12 * em);
  if (ImGui::BeginCombo("##sel", u.sel >= 0 ? names[(size_t)u.sel].c_str() : tr("(choose a preset)"))) { for (int i = 0; i < (int)names.size(); ++i) if (ImGui::Selectable(names[(size_t)i].c_str(), i == u.sel)) { u.sel = i; std::snprintf(u.name, sizeof u.name, "%s", names[(size_t)i].c_str()); } ImGui::EndCombo(); }
  ImGui::SameLine(); ImGui::BeginDisabled(u.sel < 0);
  if (Button(N_("Load"))) { auto l = loadPreset<T>(kind, names[(size_t)u.sel]); if (l.status == LoadStatus::Missing || l.status == LoadStatus::Recovered) A->toast(std::string(tr("Could not load the preset: ")) + l.message, 2); else { value = l.value; A->markDirty(); A->toast(tr("Preset loaded.")); } }
  ImGui::SameLine(); if (Button(N_("Overwrite"))) { std::string e; if (savePreset(kind, names[(size_t)u.sel], value, &e)) { A->toast(tr("Preset updated.")); cache.first = 0; } else A->toast(std::string(tr("Could not save the preset: ")) + e, 2); }
  ImGui::SameLine(); if (Button(N_("Delete"))) { if (deletePreset(kind, names[(size_t)u.sel])) { cache.first = 0; u.sel = -1; A->toast(tr("Preset deleted.")); } }
  ImGui::EndDisabled();
  ImGui::SetNextItemWidth(12 * em); ImGui::InputTextWithHint("##name", tr("new preset name"), u.name, sizeof u.name); ImGui::SameLine();
  if (Button(N_("Save as new"))) {
    std::string n = safeName(u.name);
    if (n.empty()) A->toast(tr("Invalid preset name."), 1);
    else if (std::find(names.begin(), names.end(), n) != names.end()) A->toast(tr("A preset with this name exists. Select it and use Overwrite."), 1);
    else { std::string e; if (savePreset(kind, n, value, &e)) { A->toast(tr("Preset saved.")); cache.first = 0; } else A->toast(std::string(tr("Could not save the preset: ")) + e, 2); }
  }
  ImGui::PopID();
}

// ---------- Dashboard ----------
void Dashboard() {
  const Snapshot& s = A->snap; float W = ImGui::GetContentRegionAvail().x, H = ImGui::GetContentRegionAvail().y, lw = std::min(W * 0.46f, 31 * em);
  bool sim = s.source == InputSource::Simulation; Profile& p = A->prof;
  ImGui::BeginChild("left", {lw, H}, false);
  if (Card("status", lw - 4, 0)) {
    Title(N_("Status"));
    if (sim) Chip(tr("Input: SIMULATION (not real tracking)"), kWarn);
    else Chip(VrText(s.vrStatus).c_str(), s.vr == VrState::Connected ? kGood : s.vr == VrState::Error || s.vr == VrState::NoRuntimeDll ? kBad : kWarn);
    Chip(s.ipc.connected ? tr("BodySync driver: connected") : tr("BodySync driver: not detected"), s.ipc.connected ? kGood : kWarn, s.ipc.connected ? fmt("v%u, %u %s", s.ipc.ack.driverVersion, s.ipc.ack.trackerCount, tr("trackers")).c_str() : nullptr);
    Chip(s.publishing ? tr("Sending trackers to SteamVR") : tr("Not sending trackers"), s.publishing ? kGood : kDim);
    Chip(postureLabel((int)s.out.posture).c_str(), kAccent, s.out.transition < 0.999 ? tr("changing posture...") : nullptr);
    Chip(fmt("%s %.0f %s", tr("Update rate"), s.loopHz, tr("Hz")).c_str(), s.loopHz > p.tracking.updateRateHz * 0.8 ? kGood : kWarn);
  }
  ImGui::EndChild();
  if (Card("controls", lw - 4, 0)) {
    Title(N_("Controls"));
    bool pub = A->publishUser; if (ImGui::Checkbox(tr("Send trackers to SteamVR"), &pub)) { A->publishUser = pub; A->engine.setPublish(pub); }
    Tip(N_("Turns the eight body trackers on or off inside SteamVR. Off makes them disappear from SteamVR."));
    int src = sim ? 1 : 0; ImGui::TextColored(kDim, "%s", tr("Input source"));
    if (ImGui::RadioButton(tr("Live (SteamVR)"), src == 0)) A->setSource(InputSource::Live); Tip(N_("Use the real headset and controllers."));
    if (ImGui::RadioButton(tr("Simulation"), src == 1)) A->setSource(InputSource::Simulation); Tip(N_("Offline test motion. Not real tracking; never sent to SteamVR unless enabled on the Diagnostics page."));
  }
  ImGui::EndChild();
  if (Card("profile", lw - 4, 0)) {
    Title(N_("Profile"));
    ImGui::SetNextItemWidth(lw - 4 * em);
    if (ImGui::BeginCombo("##prof", p.name.c_str())) {
      for (auto& n : A->profileNames) if (ImGui::Selectable(n.c_str(), n == p.name) && n != p.name) { if (A->dirty && !A->as.autosave) { A->pendingProfile = n; A->modal = "unsaved"; } else { A->flushSave(); A->selectProfile(n); } }
      ImGui::EndCombo();
    }
    if (Button(N_("New"))) { A->nameBuf[0] = 0; A->modal = "new"; } ImGui::SameLine();
    if (Button(N_("Duplicate"))) { std::snprintf(A->nameBuf, sizeof A->nameBuf, "%s copy", p.name.c_str()); A->modal = "dup"; } ImGui::SameLine();
    if (Button(N_("Delete"))) A->modal = "delete";
    if (Button(N_("Save now"))) { if (A->saveNow()) A->toast(tr("Profile saved.")); } ImGui::SameLine();
    if (Button(N_("Import..."))) A->importProfileDialog(); ImGui::SameLine(); if (Button(N_("Export..."))) A->exportProfileDialog();
    if (ImGui::Checkbox(tr("Autosave changes"), &A->as.autosave)) saveAppSettings(A->as);
    Tip(N_("When on, changes are written to the profile file about a second after you stop editing. When off, use Save now."));
    if (A->dirty) { ImGui::SameLine(); ImGui::TextColored(kWarn, "%s", tr("unsaved")); }
  }
  ImGui::EndChild();
  // warnings
  std::vector<std::pair<std::string, ImVec4>> w;
  if (!p.calibration.valid) w.push_back({tr("This profile is not calibrated yet. Open the Calibration page."), kWarn});
  if (!sim && s.vr == VrState::NoRuntimeDll) w.push_back({tr("openvr_api.dll was not found next to BodySync.exe. Reinstall BodySync."), kBad});
  if (!sim && s.vr == VrState::NotRunning) w.push_back({tr("SteamVR is not running. Start SteamVR; BodySync connects automatically."), kWarn});
  if (!sim && s.vr == VrState::Connected && !s.ipc.connected) w.push_back({tr("SteamVR is running but the BodySync driver did not answer. Install the driver (see INSTALL) and restart SteamVR."), kWarn});
  if (s.ipc.connected && s.ipc.ack.trackerCount != kRoleCount) w.push_back({tr("The driver registered fewer trackers than expected. Check the SteamVR log."), kWarn});
  if (!sim && s.vr == VrState::Connected && !s.out.hmdOk) w.push_back({tr("Headset is not tracked right now."), kBad});
  if (!sim && s.vr == VrState::Connected && s.out.hmdOk && (!s.out.lcOk || !s.out.rcOk)) w.push_back({tr("A controller is not tracked; arms are held or estimated."), kWarn});
  if (sim) w.push_back({tr(A->as.publishSimulation ? "SIMULATION: simulated poses ARE being sent to SteamVR (testing option)." : "SIMULATION: nothing is sent to SteamVR."), kWarn});
  if (!w.empty()) { if (Card("warn", lw - 4, 0)) { Title(N_("Attention")); for (auto& x : w) { ImGui::PushStyleColor(ImGuiCol_Text, x.second); ImGui::TextWrapped("%s", x.first.c_str()); ImGui::PopStyleColor(); ImGui::Spacing(); } } ImGui::EndChild(); }
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("right", {0, H}, false);
  float vw = (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / 3; float vh = std::min(ImGui::GetContentRegionAvail().y * 0.48f, 24 * em);
  for (int v = 0; v < 3; ++v) { if (v) ImGui::SameLine(); ImGui::BeginGroup(); DrawBody(s.out, v, {vw, vh}); ImGui::EndGroup(); }
  ImGui::Spacing();
  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {6, 2});
  if (ImGui::BeginTable("trk", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn(tr("Tracker"), 0, 1.6f); ImGui::TableSetupColumn(tr("State"), 0, 1.1f); ImGui::TableSetupColumn(tr("Source"), 0, 1.3f); ImGui::TableSetupColumn(tr("Confidence"), 0, 1.5f); ImGui::TableSetupColumn(tr("Height (m)"), 0, 1.1f); ImGui::TableHeadersRow();
    for (int r = 0; r < kRoleCount; ++r) {
      const TrackerOut& t = s.out.tr[r]; ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(roleLabel(r).c_str());
      ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); if (!t.active) ImGui::TextColored(kDim, "%s", tr("Off")); else if (t.valid) ImGui::TextColored(kGood, "%s", tr("Active")); else ImGui::TextColored(kBad, "%s", tr("Lost"));
      ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(tr(srcName(t.src)));
      ImGui::TableNextColumn(); ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::GetFrameHeight() - em * 0.8f) * 0.5f); ImGui::ProgressBar((float)t.confidence, {-1, em * 0.8f}, "");
      ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::Text("%.2f", t.pose.p.y - p.calibration.floorOffsetM);
    }
    ImGui::EndTable();
  }
  ImGui::PopStyleVar();
  Para(N_("Colors: teal = estimated, orange = physical tracker, yellow = blending, red = lost."), kDim);
  ImGui::EndChild();
}

// ---------- Calibration ----------
void PageCalibration() {
  Wizard& w = A->wiz; const Snapshot& s = A->snap; Profile& p = A->prof; Calibration& c = p.calibration; BodyProfile& b = p.body;
  bool sim = s.source == InputSource::Simulation;
  if (Card("wiz", 0, 0)) {
    Title(N_("Calibration wizard"));
    if (w.mode == Wizard::Off) {
      Para(N_("Quick: floor, height, check (about 30 seconds). Advanced: adds a T-pose to measure arm length. You need to wear the headset and hold both controllers."));
      if (sim) Para(N_("Switch the input source to Live on the Dashboard to calibrate. Simulated data is never used for calibration."), kWarn);
      ImGui::BeginDisabled(sim || s.vr != VrState::Connected);
      if (Button(N_("Quick calibration"), true)) A->wizStart(Wizard::Quick); ImGui::SameLine(); if (Button(N_("Advanced calibration"))) A->wizStart(Wizard::Advanced);
      ImGui::EndDisabled();
      if (!sim && s.vr != VrState::Connected) ImGui::TextColored(kDim, "%s", tr("SteamVR must be running."));
      if (c.valid) ImGui::TextColored(kGood, "%s %s", tr("Calibrated:"), c.timestamp.empty() ? "-" : c.timestamp.c_str()); else ImGui::TextColored(kWarn, "%s", tr("Not calibrated"));
    } else {
      A->wizPoll(); CaptureKind k = w.steps[(size_t)w.step];
      for (size_t i = 0; i < w.steps.size(); ++i) { if (i) ImGui::SameLine(); ImGui::TextColored((int)i == w.step ? kAccent : (int)i < w.step ? kGood : kDim, "%zu. %s", i + 1, tr(App::wizStepName(w.steps[i]))); }
      ImGui::Spacing();
      const char* inst = k == CaptureKind::Floor ? N_("Lay one controller flat on the floor and do not touch it. Then press Start.") :
                         k == CaptureKind::Stand ? N_("Stand upright, feet together, look straight ahead, arms down. Then press Start.") :
                         k == CaptureKind::TPose ? N_("Hold both arms straight out to the sides at shoulder height (T-pose), looking straight ahead. Then press Start.") :
                                                   N_("Stand naturally and move a little: step, turn, bend your knees. The result is checked for stability.");
      Para(inst);
      if (w.counting) ImGui::TextColored(kWarn, "%s %.0f...", tr("Get ready:"), std::ceil(std::max(0.0, w.countdownEnd - A->now)));
      else if (w.capturing) { ImGui::ProgressBar((float)s.capProgress, {20 * em, 0}); ImGui::SameLine(); ImGui::TextUnformatted(tr("Capturing...")); }
      if (!w.msg.empty()) ImGui::TextColored(w.stepOk ? kGood : kBad, "%s", tr(w.msg.c_str()));
      if (w.verifyDone) { ImGui::Text("%s: %.1f %s   %s: %.1f %s   %s: %.1f %s", tr("Segment error"), w.rep.maxSegErrM * 1000, tr("mm"), tr("Asymmetry"), w.rep.asymmetryM * 1000, tr("mm"), tr("Pelvis jitter"), w.rep.jitterRmsM * 1000, tr("mm")); for (auto& m : w.rep.messages) ImGui::BulletText("%s", tr(m.c_str())); }
      bool busy = w.counting || w.capturing;
      ImGui::BeginDisabled(busy);
      bool last = w.step + 1 == (int)w.steps.size();
      if (!(w.stepOk && !last) && !(w.verifyDone)) { if (Button(w.msg.empty() ? N_("Start") : N_("Retry"), true)) A->wizBegin(); }
      if (w.stepOk && !last) { ImGui::SameLine(); if (Button(N_("Next step"), true)) { ++w.step; w.msg.clear(); w.stepOk = false; } }
      if (w.verifyDone) {
        if (Button(w.rep.pass ? N_("Save calibration") : N_("Save anyway"), w.rep.pass)) A->wizCommit();
        ImGui::SameLine(); if (Button(N_("Repeat check"))) A->wizBegin();
      }
      ImGui::SameLine(); if (Button(N_("Cancel"))) A->wizCancel();
      ImGui::EndDisabled();
    }
  }
  ImGui::EndChild();
  if (Card("manual", 0, 0)) {
    Title(N_("Body proportions"));
    Slider(N_("Height (m)"), b.heightM, 0.9, 2.4, "%.3f", N_("Total body height. All limb lengths derive from it."));
    Slider(N_("Leg length scale"), b.legScale, 0.6, 1.5, "%.2f", N_("Multiplies the default leg length. Raise if your knees look too low, lower if the feet float."));
    Slider(N_("Torso length scale"), b.spineScale, 0.6, 1.5, "%.2f", N_("Multiplies the default distance from hips to neck."));
    Slider(N_("Arm length scale"), b.armScale, 0.6, 1.5, "%.2f", N_("Multiplies the default arm length. The T-pose step measures this."));
    Slider(N_("Body width scale"), b.widthScale, 0.6, 1.5, "%.2f", N_("Widens or narrows shoulders, hips and foot spacing."));
    PresetBar<BodyProfile>("body", b, N_("Body presets"));
  }
  ImGui::EndChild();
  if (Card("align", 0, 0)) {
    Title(N_("Alignment and offsets"));
    Slider(N_("Measured eye height (m)"), c.eyeHeightM, 0.0, 2.4, "%.3f", N_("Standing eye height above the floor. 0 means derive it from the body height."));
    Slider(N_("Floor level (m)"), c.floorOffsetM, -1.0, 1.0, "%.3f", N_("Height of the floor in the tracking space. The Floor step measures this."));
    Slider(N_("Headset tilt (deg)"), c.headPitchOffsetDeg, -45, 45, "%.1f", N_("How far the headset points up or down when you look straight ahead."));
    Slider(N_("Torso yaw bias (deg)"), c.torsoYawBiasDeg, -45, 45, "%.1f", N_("Rotation of the body relative to the head direction measured in T-pose."));
    Slider(N_("Body yaw offset (deg)"), c.yawOffsetDeg, -180, 180, "%.1f", N_("Rotates the whole virtual body around the head. Use if the trackers appear turned."));
    Slider(N_("Offset right (m)"), c.posOffset[0], -2, 2, "%.3f", N_("Moves the whole virtual body to the right (+) or left (-)."));
    Slider(N_("Offset up (m)"), c.posOffset[1], -2, 2, "%.3f", N_("Moves the whole virtual body up (+) or down (-)."));
    Slider(N_("Offset forward (m)"), c.posOffset[2], -2, 2, "%.3f", N_("Moves the whole virtual body along the Z axis."));
    Slider(N_("Pelvis offset right (m)"), c.pelvisOffset[0], -0.3, 0.3, "%.3f", N_("Moves the waist tracker sideways within the pelvis."));
    Slider(N_("Pelvis offset up (m)"), c.pelvisOffset[1], -0.3, 0.3, "%.3f", N_("Moves the waist tracker up or down within the pelvis."));
    Slider(N_("Pelvis offset forward (m)"), c.pelvisOffset[2], -0.3, 0.3, "%.3f", N_("Moves the waist tracker forward or backward within the pelvis."));
    Slider(N_("Foot spacing (m)"), c.footLateralM, -0.2, 0.3, "%.3f", N_("Extra outward distance of each foot from the body centre line."));
    Slider(N_("Foot forward offset (m)"), c.footForwardM, -0.3, 0.3, "%.3f", N_("Shifts both feet forward or backward."));
    Slider(N_("Toe-out angle (deg)"), c.footToeOutDeg, -30, 45, "%.0f", N_("How far each foot points outward."));
    if (Button(N_("Reset calibration"))) A->modal = "resetcal";
    PresetBar<Calibration>("calibration", c, N_("Calibration presets"));
  }
  ImGui::EndChild();
}

// ---------- Body tracking ----------
void Tracking() {
  TrackingSettings& t = A->prof.tracking;
  if (Card("trk1", 0, 0)) {
    Title(N_("Trackers"));
    Para(N_("Choose which of the eight trackers appear in SteamVR."), kDim);
    for (int r = 0; r < kRoleCount; ++r) { if (r % 4) ImGui::SameLine(); Check(roleName(r), t.enabled[(size_t)r], N_("Disabled trackers disappear from SteamVR.")); }
  }
  ImGui::EndChild();
  if (Card("trk2", 0, 0)) {
    Title(N_("Smoothing and motion"));
    Slider(N_("Smoothing"), t.smoothing, 0, 1, "%.2f", N_("Higher is steadier but adds delay. Lower follows quickly but can jitter."));
    Slider(N_("Pelvis sway"), t.pelvisSway, 0, 1, "%.2f", N_("0 keeps the pelvis centred over the feet; 1 lets it follow the neck fully."));
    Slider(N_("Torso turn dead zone (deg)"), t.torsoYawDeadzoneDeg, 5, 90, "%.0f", N_("How far you can turn your head before the torso starts to follow."));
    Slider(N_("Torso follow time (s)"), t.torsoYawFollowSec, 0.2, 20, "%.1f", N_("How slowly the torso relaxes to face where the head looks."));
    Slider(N_("Foot lock"), t.footLock, 0, 1, "%.2f", N_("How far a planted foot is kept in place before a step is taken. Higher means fewer, larger steps."));
    Slider(N_("Step height (m)"), t.stepHeightM, 0, 0.4, "%.2f", N_("How high a foot lifts during a step."));
    Slider(N_("Step duration scale"), t.stepDurationScale, 0.4, 2.5, "%.2f", N_("Scales how long a step takes. Higher is slower."));
    Slider(N_("Walk-in-place sensitivity"), t.walkInPlaceSensitivity, 0, 1, "%.2f", N_("How easily head bobbing is recognised as walking in place."));
    Slider(N_("Posture sensitivity"), t.postureSensitivity, 0, 1, "%.2f", N_("Higher switches between standing, sitting and lying sooner."));
  }
  ImGui::EndChild();
  if (Card("trk3", 0, 0)) {
    Title(N_("Limits and output"));
    Slider(N_("Knee max bend (deg)"), t.kneeMaxBendDeg, 60, 170, "%.0f", N_("Largest knee bend the solver allows."));
    Slider(N_("Elbow max bend (deg)"), t.elbowMaxBendDeg, 60, 170, "%.0f", N_("Largest elbow bend the solver allows."));
    Slider(N_("Spine max lean (deg)"), t.spineMaxLeanDeg, 10, 85, "%.0f", N_("Largest forward or sideways lean of the torso."));
    Slider(N_("Neck twist limit (deg)"), t.neckTwistDeg, 20, 120, "%.0f", N_("Largest turn of the head against the chest."));
    Slider(N_("Spine twist limit (deg)"), t.spineTwistDeg, 5, 90, "%.0f", N_("Largest turn of the chest against the pelvis."));
    Slider(N_("Max speed (m/s)"), t.maxSpeedMps, 2, 50, "%.0f", N_("Inputs and outputs faster than this are treated as glitches and limited."));
    SliderI(N_("Update rate (Hz)"), t.updateRateHz, 30, 240, N_("How often poses are calculated and sent to SteamVR."));
    Slider(N_("Pose time offset (ms)"), t.poseTimeOffsetMs, -100, 100, "%.0f", N_("Tells SteamVR the pose is this many milliseconds old (negative) or newer. Use it to hide latency."));
  }
  ImGui::EndChild();
}

// ---------- Poses ----------
void Poses() {
  PoseSettings& ps = A->prof.poses; const Snapshot& s = A->snap;
  if (Card("pose0", 0, 0)) {
    Title(N_("Current posture"));
    ImGui::Text("%s: %s", tr("Detected"), postureLabel((int)s.out.posture).c_str());
    ImGui::Text("%s: %s (%.2f %s)", tr("Candidate"), postureLabel((int)s.out.diag.candidate).c_str(), s.out.diag.candidateT, tr("s"));
    ImGui::Text("%s: %.2f   %s: %.2f %s", tr("Head height ratio"), s.out.diag.headRatio, tr("Hip height"), s.out.diag.hipHeight, tr("m"));
    int m = (int)ps.mode; if (Combo(N_("Posture mode"), m, {N_("Automatic"), N_("Always standing"), N_("Always sitting"), N_("Always lying")}, N_("Automatic detects the posture from head height. The others force one posture."))) ps.mode = (PostureMode)m;
  }
  ImGui::EndChild();
  if (Card("pose1", 0, 0)) {
    Title(N_("Detection thresholds"));
    Slider(N_("Sit enter ratio"), ps.sitEnterRatio, 0.4, 0.95, "%.2f", N_("Head height divided by standing eye height below which sitting is detected."));
    Slider(N_("Sit exit ratio"), ps.sitExitRatio, 0.5, 1.0, "%.2f", N_("Ratio above which standing is detected again. Always kept above the enter ratio."));
    Slider(N_("Crouch ratio"), ps.crouchRatio, 0.2, 0.9, "%.2f", N_("Below this a still user counts as crouching or sitting on the floor."));
    Slider(N_("Lying height ratio"), ps.lieHeightRatio, 0.1, 0.8, "%.2f", N_("Head height ratio below which lying down is considered."));
    Slider(N_("Lying enter (head up-vector)"), ps.lieEnterUp, 0.0, 0.95, "%.2f", N_("The head must tilt past this before lying is detected."));
    Slider(N_("Lying exit (head up-vector)"), ps.lieExitUp, 0.05, 0.99, "%.2f", N_("The head must come back past this before lying ends."));
    Slider(N_("Hold time (s)"), ps.holdTimeSec, 0.05, 5, "%.2f", N_("A posture must persist this long before the switch happens."));
    Slider(N_("Transition time (s)"), ps.transitionSec, 0.05, 5, "%.2f", N_("Length of the smooth blend between postures."));
  }
  ImGui::EndChild();
  if (Card("pose2a", 0, 0)) {
    Title(N_("Seated pose"));
    int st = (int)ps.sitStyle;
    if (Combo(N_("Sitting style"), st, {N_("Chair (normal)"), N_("Upright torso"), N_("Relaxed, leaning back"), N_("Cross-legged on the floor")}, N_("Shapes the seated body: pelvis, torso, knees and feet. Whether you are seated is still detected from your head height and movement."))) ps.sitStyle = (SitStyle)st;
    static const char* desc[4] = {N_("Sitting on a chair: thighs forward, lower legs down, feet in front of the knees."), N_("Sitting straight on the edge of a seat: vertical torso, lower legs close to vertical."),
                                  N_("Leaning back in a chair or sofa: the torso is inclined backwards and the legs are stretched out."), N_("Sitting on the floor with crossed legs: low pelvis, wide knees, ankles crossed in front.")};
    Para(desc[std::clamp(st, 0, 3)], kDim);
    if (Button(N_("Restore default sitting settings"))) { PoseSettings d; ps.sitStyle = d.sitStyle; ps.sitLeanDeg = d.sitLeanDeg; ps.sitPelvisHeightOffsetM = d.sitPelvisHeightOffsetM; ps.sitPelvisBackM = d.sitPelvisBackM; ps.sitLegExtend = d.sitLegExtend; ps.sitFeetSpreadM = d.sitFeetSpreadM; ps.sitEnterRatio = d.sitEnterRatio; ps.sitExitRatio = d.sitExitRatio; A->markDirty(); A->toast(tr("Sitting settings restored.")); }
  }
  ImGui::EndChild();
  if (Card("pose2", 0, 0)) {
    Title(N_("Standing and sitting shape"));
    Slider(N_("Standing lean (deg)"), ps.standLeanDeg, -30, 30, "%.0f", N_("Adds forward (+) or backward (-) lean to the standing torso."));
    Slider(N_("Sitting lean (deg)"), ps.sitLeanDeg, -30, 45, "%.0f", N_("Adds forward (+) or backward (-) lean to the seated torso."));
    Slider(N_("Seat height offset (m)"), ps.sitPelvisHeightOffsetM, -0.3, 0.3, "%.2f", N_("Raises or lowers the seated pelvis."));
    Slider(N_("Seat back offset (m)"), ps.sitPelvisBackM, -0.2, 0.4, "%.2f", N_("Moves the seated pelvis behind the neck."));
    Slider(N_("Seated leg extension"), ps.sitLegExtend, 0.2, 1.0, "%.2f", N_("How far the feet reach out in front when sitting (1 = legs straight)."));
    Slider(N_("Seated feet spread (m)"), ps.sitFeetSpreadM, -0.1, 0.4, "%.2f", N_("Extra distance between the feet when sitting."));
    Slider(N_("Knee splay (deg)"), ps.kneeSplayDeg, -20, 50, "%.0f", N_("How far the knees point outward from the foot direction."));
  }
  ImGui::EndChild();
  if (Card("pose3", 0, 0)) {
    Title(N_("Lying shape"));
    Slider(N_("Knee bend on back (deg)"), ps.lieKneeBendDeg, 0, 90, "%.0f", N_("Knee bend when lying on the back or front."));
    Slider(N_("Knee bend on side (deg)"), ps.lieSideKneeBendDeg, 0, 120, "%.0f", N_("Knee bend when lying on the side."));
    Slider(N_("Pelvis height when lying (m)"), ps.lieFloorOffsetM, 0, 0.4, "%.2f", N_("Minimum pelvis height above the floor when lying."));
    PresetBar<PoseSettings>("poses", ps, N_("Pose presets"));
  }
  ImGui::EndChild();
}

// ---------- Mixed tracking ----------
void Mixed() {
  MixedSettings& m = A->prof.mixed; const Snapshot& s = A->snap;
  if (Card("mx0", 0, 0)) {
    Title(N_("Physical trackers"));
    Check(N_("Use physical trackers"), m.enabled, N_("When off, every role is estimated from the headset and controllers."));
    Para(N_("Assign a physical tracker to a role. It replaces the estimate while it is healthy; if it is lost, BodySync blends back to the estimate and returns when the tracker is stable again."), kDim);
    Slider(N_("Blend time (s)"), m.blendSeconds, 0.05, 5, "%.2f", N_("Length of the hand-over between physical and estimated pose."));
    Slider(N_("Loss timeout (s)"), m.lossTimeoutSec, 0.05, 5, "%.2f", N_("A tracker must be bad this long before it is considered lost."));
    Slider(N_("Jump limit (m/s)"), m.jumpSpeedMps, 1, 50, "%.1f", N_("A physical pose that moves faster than this is rejected as a glitch."));
    Slider(N_("Frozen after (s)"), m.staleSeconds, 0.2, 30, "%.1f", N_("A tracker whose pose never changes for this long is considered frozen."));
  }
  ImGui::EndChild();
  if (Card("mx1", 0, 0)) {
    Title(N_("Role assignment"));
    if (s.source == InputSource::Simulation) ImGui::TextColored(kWarn, "%s", tr("Simulation is active: real trackers are ignored."));
    if (ImGui::BeginTable("roles", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn(tr("Role"), 0, 1.5f); ImGui::TableSetupColumn(tr("Physical tracker"), 0, 3.0f); ImGui::TableSetupColumn(tr("Priority"), 0, 2.6f); ImGui::TableSetupColumn(tr("State"), 0, 1.6f); ImGui::TableHeadersRow();
      for (int r = 0; r < kRoleCount; ++r) {
        RoleAssign& ra = m.roles[(size_t)r]; ImGui::PushID(r); ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(roleLabel(r).c_str());
        ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
        bool present = false; for (auto& t : s.trackers) present |= t.serial == ra.serial;
        std::string cur = ra.serial.empty() ? std::string(tr("(none - estimate)")) : ra.serial + (present ? "" : std::string(" ") + tr("(not connected)"));
        if (ImGui::BeginCombo("##ser", cur.c_str())) {
          if (ImGui::Selectable(tr("(none - estimate)"), ra.serial.empty())) { ra.serial.clear(); A->markDirty(); }
          for (auto& t : s.trackers) if (ImGui::Selectable((t.serial + "  [" + t.system + "]").c_str(), t.serial == ra.serial)) { for (auto& o : m.roles) if (o.serial == t.serial) o.serial.clear(); ra.serial = t.serial; A->markDirty(); }
          ImGui::EndCombo();
        }
        Tip(N_("Pick a tracker that SteamVR currently sees. One physical tracker can serve one role."));
        ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1); int pr = (int)ra.priority;
        const char* pn[4] = {N_("Physical preferred"), N_("Virtual preferred"), N_("Physical only"), N_("Virtual only")};
        if (ImGui::BeginCombo("##pri", tr(pn[pr]))) { for (int i = 0; i < 4; ++i) if (ImGui::Selectable(tr(pn[i]), i == pr)) { ra.priority = (RoleAssign::Priority)i; A->markDirty(); } ImGui::EndCombo(); }
        Tip(N_("Physical preferred: use the tracker while healthy. Virtual preferred: use the estimate, tracker only if the estimate is unavailable. Only: never switch."));
        ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); const auto& st = s.mixer[(size_t)r];
        if (ra.serial.empty()) ImGui::TextColored(kDim, "%s", tr("Estimated"));
        else if (st.physInCharge) ImGui::TextColored(kPhys, "%s", tr("Physical in charge"));
        else if (!st.healthy) ImGui::TextColored(kBad, "%s", st.reason.empty() ? tr("Tracker unavailable") : tr(st.reason.c_str()));
        else ImGui::TextColored(kWarn, "%s", tr("Tracker OK, waiting"));
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
  }
  ImGui::EndChild();
  if (Card("mx2", 0, 0)) {
    Title(N_("Trackers seen by SteamVR"));
    if (s.trackers.empty()) Para(N_("No physical trackers detected. BodySync's own virtual trackers are not listed."), kDim);
    for (auto& t : s.trackers) ImGui::BulletText("%s  (%s)  %s", t.serial.c_str(), t.system.c_str(), t.poseValid ? tr("tracking") : tr("not tracking"));
  }
  ImGui::EndChild();
}

// ---------- Diagnostics ----------
int g_logFilter = 0; uint64_t g_logFloor = 0; bool g_logFollow = true;
void Diagnostics() {
  const Snapshot& s = A->snap; const SolverDiag& d = s.out.diag; bool sim = s.source == InputSource::Simulation;
  if (Card("dg0", 0, 0)) {
    Title(N_("Simulation (diagnostic mode)"));
    Para(N_("SIMULATION generates artificial motion. It is not real tracking and is only for checking the solver and the pipeline."), kWarn);
    int src = sim ? 1 : 0; ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(tr("Input source")); ImGui::SameLine();
    if (ImGui::RadioButton(tr("Live (SteamVR)"), src == 0)) A->setSource(InputSource::Live); ImGui::SameLine(); if (ImGui::RadioButton(tr("Simulation"), src == 1)) A->setSource(InputSource::Simulation);
    static int sc = 0; static bool loop = true; sc = (int)s.scenario;
    ImGui::SetNextItemWidth(18 * em);
    if (ImGui::BeginCombo(tr("Scenario"), scenarioLabel(sc).c_str())) { for (int i = 0; i < kScenarioCount; ++i) if (ImGui::Selectable(scenarioLabel(i).c_str(), i == sc)) { A->engine.setScenario((Scenario)i, loop); } ImGui::EndCombo(); }
    ImGui::SameLine(); if (ImGui::Checkbox(tr("Loop"), &loop)) A->engine.setScenario(s.scenario, loop);
    if (Button(N_("Restart"))) A->engine.setScenario(s.scenario, loop); ImGui::SameLine(); if (Button(s.simRunning ? N_("Pause") : N_("Resume"))) A->engine.setSimRunning(!s.simRunning);
    ImGui::SameLine(); ImGui::ProgressBar(s.simDuration > 0 ? (float)(s.simT / s.simDuration) : 0, {12 * em, 0}, fmt("%.1f / %.1f %s", s.simT, s.simDuration, tr("s")).c_str());
    bool ps = A->as.publishSimulation; if (ImGui::Checkbox(tr("Allow simulated poses to reach SteamVR (testing only)"), &ps)) { A->as.publishSimulation = ps; A->engine.setPublishSimulation(ps); saveAppSettings(A->as); }
    Tip(N_("Off by default so simulated motion can never leak into a real session. Turn on only to test the driver."));
  }
  ImGui::EndChild();
  if (Card("dg1", 0, 0)) {
    Title(N_("Connections"));
    ImGui::Text("%s: %s", tr("SteamVR"), VrText(s.vrStatus).c_str());
    if (s.ipc.connected) ImGui::Text("%s: %s  (v%u, %u %s, %s)", tr("Driver pipe"), tr("connected"), s.ipc.ack.driverVersion, s.ipc.ack.trackerCount, tr("trackers"), s.ipc.ack.runtime); else ImGui::Text("%s: %s", tr("Driver pipe"), tr("not connected"));
    ImGui::Text("%s: %s%s%s", tr("Driver reports"), (s.ipc.status.flags & ipc::kDevicesRegistered) ? tr("devices registered; ") : tr("devices not registered; "), (s.ipc.status.flags & ipc::kReceivingPoses) ? tr("receiving poses; ") : tr("not receiving poses; "), (s.ipc.status.flags & ipc::kClientConnected) ? tr("client connected") : tr("no client"));
    ImGui::Text("%s: %llu   %s: %llu   %s: %llu", tr("Frames sent"), (unsigned long long)s.ipc.framesSent, tr("dropped"), (unsigned long long)s.ipc.framesDropped, tr("reconnects"), (unsigned long long)s.ipc.reconnects);
    ImGui::Text("%s: %llu   %s: %llu", tr("Frames received by driver"), (unsigned long long)s.ipc.status.framesReceived, tr("poses published"), (unsigned long long)s.ipc.status.posesPublished);
    if (!s.ipc.lastError.empty()) ImGui::TextColored(kWarn, "%s: %s", tr("Last pipe error"), tr(s.ipc.lastError.c_str()));
    ImGui::Text("%s: %.1f %s   %s: %.1f %s   %s: %.0f %s", tr("Tracking loop"), s.loopHz, tr("Hz"), tr("worst step"), s.loopMaxMs, tr("ms"), tr("solve time"), s.solveUsAvg, tr("us"));
  }
  ImGui::EndChild();
  if (Card("dg2", 0, 0)) {
    Title(N_("Solver"));
    ImGui::Text("%s: %s  %s: %.2f", tr("Posture"), postureLabel((int)s.out.posture).c_str(), tr("transition"), s.out.transition);
    ImGui::Text("%s: %s   %s: %s   %s: %s", tr("Headset"), s.out.hmdOk ? tr("ok") : tr("lost"), tr("Left controller"), s.out.lcOk ? tr("ok") : tr("lost"), tr("Right controller"), s.out.rcOk ? tr("ok") : tr("lost"));
    ImGui::Text("%s: %.3f   %s: %.2f %s   %s: %.1f%s   %s: %.2f %s", tr("Head ratio"), d.headRatio, tr("hip height"), d.hipHeight, tr("m"), tr("lean"), d.leanDeg, tr("deg"), tr("speed"), d.speedXZ, tr("m/s"));
    ImGui::Text("%s: %.1f%s   %s: %.1f%s   %s: %.2f %s", tr("Torso yaw"), d.torsoYawDeg, tr("deg"), tr("pelvis yaw"), d.pelvisYawDeg, tr("deg"), tr("segment error"), d.maxSegErr * 1000, tr("mm"));
    ImGui::Text("%s: %d / %d / %d   %s: %d", tr("Rejected samples (HMD/L/R)"), d.rejected[0], d.rejected[1], d.rejected[2], tr("non-finite values replaced"), d.nonFinite);
    ImGui::Text("%s: %s %s", tr("Foot contact"), s.out.footContact[0] ? tr("L planted") : tr("L swinging"), s.out.footContact[1] ? tr("R planted") : tr("R swinging"));
    if (s.out.walkInPlace) ImGui::TextColored(kAccent, "%s", tr("Walking in place detected"));
  }
  ImGui::EndChild();
  if (Card("dg3", 0, 0)) {
    Title(N_("Report and logs"));
    if (Button(N_("Save diagnostic report"), true)) { std::string e; std::string f = A->saveReport(&e); if (f.empty()) A->toast(std::string(tr("Could not save the report: ")) + e, 2); else A->toast(std::string(tr("Report saved: ")) + f); }
    ImGui::SameLine(); if (Button(N_("Open data folder"))) A->openFolder(dataDir());
    ImGui::SameLine(); if (Button(N_("Open reports folder"))) A->openFolder(dataDir() / "reports");
    Para(N_("Reports contain tracker serial numbers and your profile settings. Check them before sharing."), kDim);
    ImGui::SetNextItemWidth(12 * em); ImGui::Combo("##lf", &g_logFilter, (std::string(tr("All messages")) + '\0' + tr("Warnings and errors") + '\0' + tr("Errors only") + '\0').c_str());
    ImGui::SameLine(); if (Button(N_("Clear view"))) { auto t = Logger::instance().tail(1); g_logFloor = t.empty() ? 0 : t.back().timeMs + 1; }
    ImGui::SameLine(); ImGui::Checkbox(tr("Follow"), &g_logFollow);
    ImGui::SameLine(); if (Button(N_("Copy"))) { std::string all; for (auto& e : Logger::instance().tail(500)) if (e.timeMs >= g_logFloor) all += std::string("[") + levelName(e.level) + "] " + e.text + "\n"; ImGui::SetClipboardText(all.c_str()); }
    ImGui::BeginChild("log", {0, 16 * em}, ImGuiChildFlags_Borders);
    int minLevel = g_logFilter == 0 ? 0 : g_logFilter == 1 ? (int)LogLevel::Warn : (int)LogLevel::Error;
    for (auto& e : Logger::instance().tail(500)) {
      if (e.timeMs < g_logFloor || (int)e.level < minLevel) continue;
      ImVec4 col = e.level == LogLevel::Error ? kBad : e.level == LogLevel::Warn ? kWarn : kDim; ImGui::TextColored(col, "[%s]", levelName(e.level)); ImGui::SameLine(); ImGui::TextWrapped("%s", e.text.c_str());
    }
    if (g_logFollow && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
  }
  ImGui::EndChild();
}

// ---------- shell ----------
void Modals() {
  static std::string last;
  if (A->modal != last) { if (!A->modal.empty()) ImGui::OpenPopup("##modal"); last = A->modal; }
  ImVec2 c = ImGui::GetMainViewport()->GetCenter(); ImGui::SetNextWindowPos(c, ImGuiCond_Appearing, {0.5f, 0.5f});
  if (!ImGui::BeginPopupModal("##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) return;
  std::string m = A->modal; bool close = false;
  if (m == "new" || m == "dup") {
    ImGui::TextUnformatted(tr(m == "new" ? "New profile name" : "Name of the copy")); ImGui::SetNextItemWidth(18 * em); ImGui::InputText("##n", A->nameBuf, sizeof A->nameBuf);
    if (Button(N_("Create"), true)) { A->flushSave(); if (A->createProfile(A->nameBuf, m == "dup" ? &A->prof : nullptr)) close = true; }
    ImGui::SameLine(); if (Button(N_("Cancel"))) close = true;
  } else if (m == "delete") {
    ImGui::Text("%s \"%s\"?", tr("Delete profile"), A->prof.name.c_str()); Para(N_("The file is removed. This cannot be undone."), kDim);
    if (Button(N_("Delete"), true)) { A->deleteProfile(A->prof.name); close = true; } ImGui::SameLine(); if (Button(N_("Cancel"))) close = true;
  } else if (m == "unsaved") {
    Para(N_("This profile has unsaved changes."));
    if (Button(N_("Save and switch"), true)) { A->saveNow(); A->selectProfile(A->pendingProfile); close = true; }
    ImGui::SameLine(); if (Button(N_("Discard and switch"))) { A->selectProfile(A->pendingProfile); close = true; }
    ImGui::SameLine(); if (Button(N_("Cancel"))) close = true;
  } else if (m == "resetcal") {
    Para(N_("Reset all calibration values of this profile to defaults? Body proportions are kept."));
    if (Button(N_("Reset"), true)) { A->prof.calibration = ::bs::Calibration{}; A->markDirty(); close = true; } ImGui::SameLine(); if (Button(N_("Cancel"))) close = true;
  } else close = true;
  if (close) { A->modal.clear(); ImGui::CloseCurrentPopup(); }
  ImGui::EndPopup();
}
void Toasts() {
  ImVec2 vp = ImGui::GetMainViewport()->WorkPos, vs = ImGui::GetMainViewport()->WorkSize; float y = vp.y + vs.y - 12;
  while (!A->toasts.empty() && A->toasts.front().until < A->now) A->toasts.pop_front();
  ImDrawList* dl = ImGui::GetForegroundDrawList();
  for (auto it = A->toasts.rbegin(); it != A->toasts.rend(); ++it) {
    float wrap = std::min(vs.x * 0.5f, 34 * em); ImVec2 ts = ImGui::CalcTextSize(it->text.c_str(), nullptr, false, wrap); ImVec2 sz{ts.x + 24, ts.y + 18}; ImVec2 p{vp.x + vs.x - sz.x - 14, y - sz.y};
    ImU32 col = it->level == 2 ? IM_COL32(150, 50, 50, 240) : it->level == 1 ? IM_COL32(140, 100, 30, 240) : IM_COL32(30, 90, 84, 240);
    dl->AddRectFilled(p, {p.x + sz.x, p.y + sz.y}, col, 8); dl->AddText(nullptr, 0, {p.x + 12, p.y + 9}, IM_COL32(240, 244, 246, 255), it->text.c_str(), nullptr, wrap); y = p.y - 8;
  }
}
}  // namespace

void drawApp(App& app) {
  A = &app; em = ImGui::GetFontSize(); app.snap = app.engine.snapshot();
  app.now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  if (app.dirty && app.as.autosave && app.now - app.lastChange > 1.0 && app.now - app.lastSaveFail > 10) app.saveNow();
  ImGuiViewport* vp = ImGui::GetMainViewport(); ImGui::SetNextWindowPos(vp->WorkPos); ImGui::SetNextWindowSize(vp->WorkSize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
  ImGui::Begin("##root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleVar();
  float sw = 16 * em;
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.065f, 0.082f, 1));
  ImGui::BeginChild("side", {sw, 0}, false, ImGuiWindowFlags_NoScrollbar);
  ImGui::SetCursorPos({16, 16}); ImGui::TextColored(kAccent, "BodySync"); ImGui::SameLine(); ImGui::TextColored(kDim, "%s", BODYSYNC_VERSION);
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12);
  const char* pages[] = {N_("Dashboard"), N_("Calibration"), N_("Body Tracking"), N_("Poses"), N_("Mixed Tracking"), N_("Diagnostics")};
  for (int i = 0; i < App::kPages; ++i) {
    ImGui::SetCursorPosX(10); bool sel = app.page == i; ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, {0.0f, 0.5f}); ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {em * 1.0f, 6});
    ImGui::PushStyleColor(ImGuiCol_Button, sel ? ImVec4(0.18f, 0.40f, 0.38f, 1) : ImVec4(0, 0, 0, 0)); ImGui::PushStyleColor(ImGuiCol_Text, sel ? ImVec4(1, 1, 1, 1) : ImVec4(0.72f, 0.76f, 0.80f, 1));
    if (ImGui::Button((std::string(tr(pages[i])) + "##nav" + std::to_string(i)).c_str(), {sw - 20, em * 2.1f})) app.page = i;
    ImGui::PopStyleColor(2); ImGui::PopStyleVar(2);
  }
  ImGui::SetCursorPos({16, ImGui::GetWindowHeight() - em * 9.4f});
  ImGui::TextColored(kDim, "%s", tr("Language")); ImGui::SetCursorPosX(16); ImGui::SetNextItemWidth(sw - 32);
  { const char* cur = "English"; for (auto& l : languages()) if (language() == l.code) cur = l.nativeName;
    if (ImGui::BeginCombo("##lang", cur)) { for (auto& l : languages()) if (ImGui::Selectable(l.nativeName, language() == l.code)) app.setLanguage(l.code); ImGui::EndCombo(); } }
  ImGui::SetCursorPosX(16); ImGui::TextColored(kDim, "%s", tr("Interface scale")); ImGui::SetCursorPosX(16); ImGui::SetNextItemWidth(sw - 32);
  { static const float steps[] = {0.8f, 0.9f, 1.0f, 1.15f, 1.3f, 1.5f, 1.75f}; int cur = 2; float bd = 9; for (int i = 0; i < 7; ++i) if (std::fabs(steps[i] - (float)app.as.uiScale) < bd) { bd = std::fabs(steps[i] - (float)app.as.uiScale); cur = i; }
    char lab[16]; std::snprintf(lab, sizeof lab, "%.0f %%", steps[cur] * 100);
    if (ImGui::BeginCombo("##scale", lab)) { for (int i = 0; i < 7; ++i) { std::snprintf(lab, sizeof lab, "%.0f %%", steps[i] * 100); if (ImGui::Selectable(lab, i == cur)) app.setUiScale(steps[i]); } ImGui::EndCombo(); } }
  ImGui::EndChild(); ImGui::PopStyleColor();
  ImGui::SameLine(0, 0);
  ImGui::BeginChild("main", {0, 0}, false, ImGuiWindowFlags_AlwaysUseWindowPadding);
  ImGui::SetCursorPos({20, 16});
  if (headFont) ImGui::PushFont(headFont); ImGui::TextUnformatted(tr(pages[app.page])); if (headFont) ImGui::PopFont();
  if (app.snap.source == InputSource::Simulation) { ImGui::SameLine(); ImGui::TextColored(kWarn, "   %s", tr("SIMULATION - not real tracking")); }
  ImGui::SetCursorPosX(20); ImGui::BeginChild("page", {ImGui::GetContentRegionAvail().x - 20, ImGui::GetContentRegionAvail().y - 12}, false);
  switch (app.page) { case 0: Dashboard(); break; case 1: PageCalibration(); break; case 2: Tracking(); break; case 3: Poses(); break; case 4: Mixed(); break; default: Diagnostics(); }
  ImGui::EndChild(); ImGui::EndChild();
  Modals(); Toasts();
  ImGui::End();
}
}  // namespace bs::ui
