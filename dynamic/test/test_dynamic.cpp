#include "test_util.hpp"

void test_skeleton() {
    ASSERT(true, "skeleton builds and runs");
}

int main() {
    std::fprintf(stderr, "\n=== Feline-PK Dynamic Tests ===\n\n");
    RUN_TEST(test_skeleton);
    TEST_SUMMARY();
    return dyntest::tests_failed > 0 ? 1 : 0;
}
