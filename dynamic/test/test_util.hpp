#pragma once

#include <cstdio>
#include <string>

// Minimal test harness (same style as test/test_main.cpp).
namespace dyntest {

inline int tests_run = 0;
inline int tests_passed = 0;
inline int tests_failed = 0;
inline int prev_failed = 0;

} // namespace dyntest

#define ASSERT(cond, msg)                                                  \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::fprintf(stderr, "  FAIL: %s (line %d): %s\n", msg,        \
                         __LINE__, #cond);                                 \
            dyntest::tests_failed++;                                       \
            return;                                                        \
        }                                                                  \
    } while (0)

#define RUN_TEST(fn)                                                       \
    do {                                                                   \
        dyntest::tests_run++;                                              \
        dyntest::prev_failed = dyntest::tests_failed;                      \
        std::fprintf(stderr, "  Running: %s ... ", #fn);                   \
        fn();                                                              \
        if (dyntest::tests_failed == dyntest::prev_failed) {              \
            dyntest::tests_passed++;                                       \
            std::fprintf(stderr, "OK\n");                                  \
        }                                                                  \
    } while (0)

#define TEST_SUMMARY()                                                     \
    do {                                                                   \
        std::fprintf(stderr, "\n=== Results: %d/%d passed, %d failed ===\n",\
                     dyntest::tests_passed, dyntest::tests_run,            \
                     dyntest::tests_failed);                               \
    } while (0)
