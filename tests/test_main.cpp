#include <cstring>
#include "test_framework.h"
int main(int argc, char** argv) {
  const char* filter = argc > 1 ? argv[1] : nullptr;
  int ran = 0, failedTests = 0;
  for (auto& t : registry()) {
    if (filter && !std::strstr(t.name, filter)) continue;
    int before = failCount();
    try { t.fn(); } catch (const std::exception& e) { ++failCount(); std::printf("    EXCEPTION: %s\n", e.what()); }
    bool ok = failCount() == before; ++ran; if (!ok) ++failedTests;
    std::printf("[%s] %s\n", ok ? " OK " : "FAIL", t.name);
  }
  std::printf("\n%d tests, %d checks, %d failed tests, %d failed checks\n", ran, checkCount(), failedTests, failCount());
  return failCount() ? 1 : 0;
}
