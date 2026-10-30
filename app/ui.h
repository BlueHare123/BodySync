#pragma once
#include "app.h"
struct ImFont;
namespace bs::ui {
extern ImFont* headFont;         // bold heading font (set by the host after loading fonts; null = body font)
void applyTheme(float dpiScale);
void drawApp(App& app);          // whole window contents (between NewFrame and Render)
}  // namespace bs::ui
