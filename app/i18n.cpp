#include "i18n.h"
#include <unordered_map>
#include "bodysync/config.h"
#include "bodysync/sim.h"
#include "bodysync/solver.h"

namespace bs::ui {
namespace {
using Map = std::unordered_map<std::string, const char*>;
std::string g_lang = "en"; std::set<std::string> g_missing;
const Map& table(const std::string& code) {
  static const Map ru = {
#include "lang_ru.inc"
  };
  static const Map de = {
#include "lang_de.inc"
  };
  static const Map fr = {
#include "lang_fr.inc"
  };
  static const Map es = {
#include "lang_es.inc"
  };
  static const Map none;
  return code == "ru" ? ru : code == "de" ? de : code == "fr" ? fr : code == "es" ? es : none;
}
}  // namespace
const std::vector<LanguageInfo>& languages() {
  static const std::vector<LanguageInfo> l = {{"en", "English"}, {"ru", "Русский"}, {"de", "Deutsch"}, {"fr", "Français"}, {"es", "Español"}};
  return l;
}
void setLanguage(const std::string& c) { g_lang = "en"; for (auto& l : languages()) if (c == l.code) g_lang = c; }
const std::string& language() { return g_lang; }
const std::set<std::string>& missingTranslations() { return g_missing; }
void clearMissingTranslations() { g_missing.clear(); }
size_t translationCount(const std::string& code) { return table(code).size(); }
bool translationFor(const std::string& code, const std::string& en, std::string& out) { auto& t = table(code); auto it = t.find(en); if (it == t.end()) return false; out = it->second; return true; }
const char* tr(const char* en) {
  if (g_lang == "en" || !en || !*en) return en;
  const Map& t = table(g_lang); auto it = t.find(en); if (it != t.end()) return it->second;
  g_missing.insert(g_lang + ": " + en); return en;
}
std::string roleLabel(int r) { return tr(roleName(r)); }
std::string scenarioLabel(int s) { return tr(scenarioName((Scenario)s)); }
std::string postureLabel(int p) {
  switch ((Posture)p) {
    case Posture::Standing: return tr("Standing"); case Posture::Sitting: return tr("Sitting");
    case Posture::LyingBack: return tr("Lying on back"); case Posture::LyingFront: return tr("Lying on front");
    case Posture::LyingLeft: return tr("Lying on left side"); case Posture::LyingRight: return tr("Lying on right side");
  }
  return "?";
}
}  // namespace bs::ui
