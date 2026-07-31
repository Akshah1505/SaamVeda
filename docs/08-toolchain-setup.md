# 08 — Toolchain Setup

**Document version:** 1.0
**Date:** 2026-07-25
**Status:** ⚠️ **Reference only — nothing here has been executed.** No software has been installed.

---

## 1. Current machine state (verified 2026-07-25)

| Component | Status |
|---|---|
| CPU | 16 cores ✅ — good for parallel C++ builds |
| RAM | 16 GB ✅ |
| Disk | D: 75 GB free · C: 63 GB free |
| OS | Windows 11 Home Single Language, 10.0.26200 |
| **Git** | 2.54.0 ✅ installed |
| **ffmpeg** | 8.1.1 ✅ installed (useful later for codec work) |
| **CMake** | ❌ **not installed** |
| **Visual Studio / MSVC** | ❌ **not installed** — no VS2022, no Build Tools |
| **Ninja** | ❌ not installed |
| Node.js / Python | v22.22.0 / 3.10.11 + 3.11.9 — present, not needed for this stack |
| GPU | RTX 3050, 4 GB — irrelevant; audio work is CPU-bound |

`D:\SaamVeda` currently contains only this documentation and is **not yet a git repository.**

## 2. ⚠️ Known risk before you begin

An **Application Control policy** on this machine has already been observed blocking a DLL from
loading (`php_pdo_sqlite.dll`, during unrelated environment probing). Blocked verbatim:

> "An Application Control policy has blocked this file"

This matters because it may also block:
- Compiler and linker executables
- Newly built binaries
- **Plugin scanning** — which loads arbitrary third-party DLLs and is exactly the kind of behaviour
  such policies target

**Verify this before committing to the timeline.** The cheapest test is Step 3 below: if a
hello-world C++ binary compiles and runs, the toolchain is not blocked. Do this in week 1, not
week 9 when plugin hosting is due.

## 3. Installation sequence

### Step 1 — Visual Studio 2022 Community

Download from <https://visualstudio.microsoft.com/downloads/>.

Select the **"Desktop development with C++"** workload. Ensure these components:

- MSVC v143 — VS 2022 C++ x64/x86 build tools
- Windows 11 SDK (latest)
- C++ CMake tools for Windows *(this installs CMake — Step 2 may be unnecessary)*
- C++ AddressSanitizer *(recommended — catches audio-thread memory bugs early)*

**Disk:** budget **20–40 GB** depending on components. C: has 63 GB free, so this fits, but not
comfortably alongside much else. The installer allows relocating the download cache and some shared
components to D: — worth doing.

> **Why the full IDE rather than Build Tools?** The debugger. Audio bugs are timing-dependent and
> often only reproducible under a real debugger; the standalone Build Tools do not include it.

### Step 2 — CMake (if not already installed by VS)

Verify first:

```bash
cmake --version
```

If missing, install from <https://cmake.org/download/> (Windows x64 installer) and **select "Add
CMake to the system PATH"**. Minimum version 3.22.

### Step 3 — Verify the toolchain before going further

This is the go/no-go gate for §2. Do not skip it.

```bash
cd /d/SaamVeda && mkdir -p scratch/toolchain-test && cd scratch/toolchain-test
printf '#include <cstdio>\nint main(){ std::printf("toolchain ok\\n"); }\n' > main.cpp
printf 'cmake_minimum_required(VERSION 3.22)\nproject(tc CXX)\nset(CMAKE_CXX_STANDARD 20)\nadd_executable(tc main.cpp)\n' > CMakeLists.txt
cmake -B build -G "Visual Studio 17 2022" -A x64 && cmake --build build --config Release && ./build/Release/tc.exe
```

Expected output: `toolchain ok`

If this fails with a policy or permission error rather than a compilation error, **stop and resolve
the Application Control issue before proceeding.** Everything downstream depends on it.

### Step 4 — Repository and submodules

```bash
cd /d/SaamVeda && git init && git branch -M main
```

Add the frameworks as pinned submodules:

```bash
git submodule add https://github.com/juce-framework/JUCE.git modules/JUCE
git submodule add https://github.com/Tracktion/tracktion_engine.git modules/tracktion_engine
git submodule update --init --recursive
```

> **Pin to commits, not branches.** After adding, check out a specific tag/commit in each submodule
> and commit that state. A submodule tracking a moving branch means your build changes underneath
> you without warning — record the exact versions in [`07-tech-stack.md`](07-tech-stack.md) §5.

Note that `tracktion_engine` pulls JUCE as its own dependency; confirm whether a separate JUCE
submodule is needed or whether tracktion's copy should be used, to avoid two JUCE versions in one
build.

### Step 5 — `.gitignore`

```gitignore
build/
scratch/
out/
.vs/
*.user
CMakeUserPresets.json
media/
*.wav
*.mp3
*.flac
*.rsproj
```

Audio files are excluded deliberately — they are large and belong in releases or test fixtures, not
in git history, where they cannot be removed later without a rewrite.

### Step 6 — First CMake project

Skeleton for `CMakeLists.txt` (exact API details to be confirmed against the pinned JUCE/tracktion
versions at build time):

```cmake
cmake_minimum_required(VERSION 3.22)
project(SaamVedaStudio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(modules/JUCE)
add_subdirectory(modules/tracktion_engine)

juce_add_gui_app(SaamVedaStudio PRODUCT_NAME "SaamVeda Studio")

target_sources(SaamVedaStudio PRIVATE src/Main.cpp)

target_compile_definitions(SaamVedaStudio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SaamVedaStudio PRIVATE
    juce::juce_audio_utils
    tracktion::tracktion_engine
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
```

### Step 7 — Build

```bash
cd /d/SaamVeda && cmake -B build -G "Visual Studio 17 2022" -A x64 && cmake --build build --config Debug --parallel 16
```

First build compiles JUCE and tracktion_engine in full — expect **15–40 minutes**. Subsequent
incremental builds are far quicker.

## 4. Recommended additions

| Tool | Purpose | Priority |
|---|---|---|
| **Ninja** | Much faster incremental builds than MSBuild | Recommended |
| **ccache / sccache** | Caches compilation across clean builds | Optional |
| **A VST3 plugin set** | Needed to test Phase 7 (e.g. Surge XT, Vital — both free) | Required by week 9 |
| **An ASIO driver** | Low-latency testing; ASIO4ALL if no audio interface | Recommended |
| **Audio interface** | Real latency testing | Optional |

## 5. Verification checklist

Before declaring Phase 1 complete:

- [ ] `cmake --version` reports ≥ 3.22
- [ ] Step 3 toolchain test prints `toolchain ok`
- [ ] **No Application Control blocks encountered**
- [ ] `git status` works in `D:\SaamVeda`
- [ ] Both submodules present with pinned commits recorded
- [ ] Full build succeeds in Debug and Release
- [ ] Application window opens
- [ ] Audio device panel lists real devices
- [ ] A WAV file plays audibly
- [ ] Catch2 test suite runs with one passing test

## 6. Troubleshooting

| Symptom | Likely cause |
|---|---|
| "Application Control policy has blocked this file" | §2 risk has materialised — resolve before continuing |
| CMake cannot find a compiler | VS C++ workload not installed, or run outside a dev shell |
| Submodule directories empty | `git submodule update --init --recursive` not run |
| Two JUCE versions conflict | See the note in Step 4 |
| No audio devices listed | Driver issue; try WASAPI before ASIO |
| Build runs out of disk | First build is large; check C: free space |

## 7. What this document deliberately does not do

It does **not** install anything. Every command above is documented for execution at the start of
Phase 1, by a person who has read §2 and accepted the Application Control risk.
