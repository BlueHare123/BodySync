#include <imgui.h>
#include "ui.h"
namespace bs::ui {
ImFont* headFont = nullptr;
void applyTheme(float s) {
  ImGuiStyle& st = ImGui::GetStyle(); st = ImGuiStyle();
  st.WindowRounding = 0; st.ChildRounding = 8; st.FrameRounding = 6; st.PopupRounding = 8; st.GrabRounding = 6; st.TabRounding = 6; st.ScrollbarRounding = 8;
  st.WindowPadding = {14, 12}; st.FramePadding = {10, 6}; st.ItemSpacing = {10, 8}; st.ItemInnerSpacing = {8, 6}; st.ScrollbarSize = 12; st.GrabMinSize = 14;
  st.FrameBorderSize = 0; st.ChildBorderSize = 1; st.PopupBorderSize = 1; st.SeparatorTextBorderSize = 1;
  ImVec4* c = st.Colors; auto C = [](float r, float g, float b, float a = 1) { return ImVec4(r, g, b, a); };
  const ImVec4 accent = C(0.30f, 0.78f, 0.69f), accentHi = C(0.38f, 0.86f, 0.77f), accentLo = C(0.20f, 0.52f, 0.47f);
  c[ImGuiCol_Text] = C(0.90f, 0.92f, 0.94f); c[ImGuiCol_TextDisabled] = C(0.52f, 0.56f, 0.62f);
  c[ImGuiCol_WindowBg] = C(0.075f, 0.086f, 0.105f); c[ImGuiCol_ChildBg] = C(0.105f, 0.122f, 0.145f); c[ImGuiCol_PopupBg] = C(0.12f, 0.14f, 0.17f);
  c[ImGuiCol_Border] = C(0.19f, 0.22f, 0.26f); c[ImGuiCol_Separator] = C(0.19f, 0.22f, 0.26f);
  c[ImGuiCol_FrameBg] = C(0.16f, 0.185f, 0.22f); c[ImGuiCol_FrameBgHovered] = C(0.20f, 0.23f, 0.27f); c[ImGuiCol_FrameBgActive] = C(0.23f, 0.27f, 0.31f);
  c[ImGuiCol_Button] = C(0.17f, 0.20f, 0.24f); c[ImGuiCol_ButtonHovered] = C(0.22f, 0.26f, 0.31f); c[ImGuiCol_ButtonActive] = accentLo;
  c[ImGuiCol_Header] = C(0.17f, 0.20f, 0.24f); c[ImGuiCol_HeaderHovered] = C(0.22f, 0.26f, 0.31f); c[ImGuiCol_HeaderActive] = accentLo;
  c[ImGuiCol_SliderGrab] = accent; c[ImGuiCol_SliderGrabActive] = accentHi; c[ImGuiCol_CheckMark] = accent;
  c[ImGuiCol_PlotHistogram] = accent; c[ImGuiCol_TextSelectedBg] = C(0.30f, 0.78f, 0.69f, 0.35f);
  c[ImGuiCol_TableHeaderBg] = C(0.13f, 0.15f, 0.18f); c[ImGuiCol_TableRowBgAlt] = C(1, 1, 1, 0.02f); c[ImGuiCol_TableBorderLight] = C(0.17f, 0.20f, 0.24f); c[ImGuiCol_TableBorderStrong] = C(0.19f, 0.22f, 0.26f);
  c[ImGuiCol_ScrollbarBg] = C(0, 0, 0, 0); c[ImGuiCol_ScrollbarGrab] = C(0.25f, 0.29f, 0.34f);
  c[ImGuiCol_ModalWindowDimBg] = C(0, 0, 0, 0.55f);
  st.ScaleAllSizes(s);
}
}  // namespace bs::ui
