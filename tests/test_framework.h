// Minimal test runner (no external dependency).
#pragma once
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>
struct TestCase { const char* name; std::function<void()> fn; };
inline std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }
inline int& failCount() { static int n = 0; return n; }
inline int& checkCount() { static int n = 0; return n; }
struct Registrar { Registrar(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); } };
#define TEST(name) static void test_##name(); static Registrar reg_##name(#name, test_##name); static void test_##name()
#define CHECK(c) do { ++checkCount(); if (!(c)) { ++failCount(); std::printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_MSG(c, ...) do { ++checkCount(); if (!(c)) { ++failCount(); std::printf("    FAIL %s:%d: %s -> ", __FILE__, __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)
#define CHECK_NEAR(a, b, eps) do { ++checkCount(); double _a = (a), _b = (b); if (!(std::fabs(_a - _b) <= (eps))) { ++failCount(); std::printf("    FAIL %s:%d: %s ~ %s (%g vs %g, eps %g)\n", __FILE__, __LINE__, #a, #b, _a, _b, (double)(eps)); } } while (0)
