#pragma once
#include <string>
// Runs the built-in checks (solver scenarios, config storage, engine -> pipe transport, headless UI in both languages).
// Writes the report to stdout and to outFile (default: <data dir>/selftest.txt). Returns 0 when everything passed.
int runSelfTest(const std::wstring& outFile);
