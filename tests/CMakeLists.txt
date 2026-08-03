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
    CoreSmokeTests.cpp
    SessionTests.cpp
    ../src/core/Session.cpp)

target_compile_features(SaamVedaTests PRIVATE cxx_std_20)

# Deliberately not linked against JUCE UI or tracktion_engine. core/ uses
# juce::ValueTree, but docs/05-architecture.md requires it to compile without
# UI, audio devices, or engine dependencies.
target_include_directories(SaamVedaTests PRIVATE ../src)
target_link_libraries(SaamVedaTests PRIVATE
    Catch2::Catch2WithMain
    juce::juce_data_structures
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# PRE_TEST defers test enumeration to `ctest` time. The default (POST_BUILD)
# runs the freshly-linked executable during the build, so anything that stops
# the binary running - on this machine, Smart App Control blocking unsigned
# builds - turns a successful compile into a failed build. Keeping discovery out
# of the build makes that failure land where it belongs: on the test step.
catch_discover_tests(SaamVedaTests DISCOVERY_MODE PRE_TEST)
