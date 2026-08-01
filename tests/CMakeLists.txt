include(FetchContent)

# Catch2 is test-only and permissively licensed (BSL-1.0), so it is fetched
# rather than vendored as a submodule. Pinned to a tag for reproducibility,
# per docs/07-tech-stack.md section 5.
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(Catch2)

add_executable(SaamVedaTests
    CoreSmokeTests.cpp)

target_compile_features(SaamVedaTests PRIVATE cxx_std_20)

# Deliberately NOT linked against juce or tracktion. docs/05-architecture.md
# requires core/ to be testable with no UI and no engine, so the suite runs in
# milliseconds. If this target ever needs juce_gui or tracktion to compile,
# something has leaked downward and the layering is broken.
target_link_libraries(SaamVedaTests PRIVATE Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(SaamVedaTests)
