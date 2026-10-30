// Localisation: the English text is the key. tr() returns the text for the selected language (en, ru, de, fr, es); missing entries fall back to English
// and are recorded so the self-test can fail on them.
#pragma once
#include <set>
#include <string>
#include <vector>
namespace bs::ui {
struct LanguageInfo { const char* code; const char* nativeName; };
const std::vector<LanguageInfo>& languages();          // en, ru, de, fr, es (the names are never translated: a user must be able to find their language)
void setLanguage(const std::string& code);             // unknown codes select English
const std::string& language();
const char* tr(const char* english);
const std::set<std::string>& missingTranslations();    // keys requested in a non-English language that have no translation (used by the self-test)
void clearMissingTranslations();
size_t translationCount(const std::string& code);      // number of entries of a language (self-test)
bool translationFor(const std::string& code, const std::string& english, std::string& out);
std::string roleLabel(int role);                       // localised tracker role name
std::string postureLabel(int posture);
std::string scenarioLabel(int scenario);
#define N_(s) s                                        // marks a literal for extraction; translated later by tr()
}  // namespace bs::ui
