#include <catch2/catch_test_macros.hpp>

// Phase 1 placeholder. Its only job is to prove the test toolchain works:
// Catch2 resolves, the target compiles with no JUCE or tracktion dependency,
// and ctest discovers and runs it.
//
// Real core/ tests arrive in Phase 2 with the session model and command bus.
// See docs/11-testing-strategy.md section 3 for what they must cover, including
// the undo invariant property test.

TEST_CASE ("test toolchain is wired up", "[smoke]")
{
    REQUIRE (1 + 1 == 2);
}
