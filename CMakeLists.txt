cmake_minimum_required(VERSION 3.22)

# C is required as well as CXX: JUCE compiles C sources, and enabling it only in
# JUCE's own nested project() scope leaves CMAKE_C_* unset for parent-scope
# targets, which fails at generate time.
project(SaamVedaStudio VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static CRT on MSVC. tracktion_engine's own examples do this; the runtime must
# match across every target in the build or linking fails.
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# tracktion_engine builds four example apps by default. We only want the library.
set(TE_ADD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Brings in JUCE 8.0.13 as its own nested submodule, then the tracktion modules.
# See docs/08-toolchain-setup.md Step 4 for why JUCE is not a separate submodule.
add_subdirectory(modules/tracktion_engine)

juce_add_gui_app(SaamVedaStudio
    PRODUCT_NAME "SaamVeda Studio"
    COMPANY_NAME "SaamVeda"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Audio recording"
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE)

target_sources(SaamVedaStudio PRIVATE
    src/Main.cpp
    src/core/Session.cpp
    src/engine/EngineController.cpp
    src/engine/RealtimeSanityCheck.cpp
    src/services/TempoDetector.cpp
    src/ui/ClipBrowser.cpp
    src/ui/MainComponent.cpp
    src/ui/PlaylistPanel.cpp
    src/ui/TapTempoComponent.cpp
    src/ui/TimelineComponent.cpp
    src/ui/Toolbars.cpp
    src/ui/ZoomScrollBar.cpp)

target_include_directories(SaamVedaStudio PRIVATE src)

target_compile_definitions(SaamVedaStudio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1
    # Audio-thread allocation detector, required from Phase 2 by
    # docs/05-architecture.md section 12. Debug configurations only: it hooks
    # the debug CRT allocator and costs a branch per allocation.
    SAAMVEDA_REALTIME_CHECKS=$<IF:$<CONFIG:Debug>,1,0>)

target_link_libraries(SaamVedaStudio PRIVATE
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

if (MSVC)
    target_compile_options(SaamVedaStudio PRIVATE /W4)
endif()

enable_testing()
add_subdirectory(tests)
