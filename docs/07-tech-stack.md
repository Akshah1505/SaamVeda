# 07 — Technology Stack & Selection Rationale

**Document version:** 1.0
**Date:** 2026-07-25

---

## 1. Selected stack

| Layer | Technology | Role |
|---|---|---|
| Language | **C++20** | Application and DSP |
| Application framework | **JUCE 8** | UI, audio device I/O, plugin hosting, DSP primitives |
| DAW engine | **tracktion_engine** | Transport, recording, mixing, automation, rendering |
| Build | **CMake** | Cross-platform build configuration |
| Compiler | **MSVC** (Visual Studio 2022) | Native Windows toolchain |
| Testing | **Catch2** | Unit and integration tests |
| Version control | **Git** | With JUCE and tracktion as submodules |

**None of this is installed yet.** See [`08-toolchain-setup.md`](08-toolchain-setup.md).

## 2. How this decision was reached

The starting proposal was to merge the LMMS and Ardour codebases. That was investigated and
rejected — see [`01-project-charter.md`](01-project-charter.md) §6. The evaluation below covers what
was considered afterwards.

### 2.1 Options evaluated

| Option | Verdict |
|---|---|
| Merge LMMS + Ardour | **Rejected** — no shared foundation; hundreds of person-years; Ardour unbuildable on Windows |
| Fork Ardour | **Rejected** — not buildable on the development platform |
| Fork LMMS | **Viable, not chosen** — see §2.3 |
| Contribute to Zrythm | **Viable, not chosen** — see §2.4 |
| Write everything from scratch | **Rejected** — see §2.5 |
| **JUCE + tracktion_engine** | **Selected** — see §2.6 |

### 2.2 Ardour — eliminated by platform

Verified from Ardour's own community: no current Ardour developer builds on Windows. Windows
releases are cross-compiled from Linux using MinGW. One developer reported the process taking three
weeks; the team advises against attempting it and provides no build support, describing it as a
drain on limited resources.

The development machine runs Windows 11. This is disqualifying, independent of any technical merit.
Ardour remains valuable as an **architectural reference**.

### 2.3 Fork LMMS — viable, not chosen

**For:** GPL-2.0 and free; 10.2k stars; CMake with `vcpkg.json`, so it genuinely builds on Windows;
already ships 15+ synthesizers, VST2 and SoundFont2 support, a step sequencer, and a piano roll.
Its long-missing audio recording feature has been under active development through 2025, with a PR
close to completion as of February 2026.

**Against:** inheriting a codebase of roughly two decades means inheriting its architecture, not
choosing one — which conflicts directly with the flat-marginal-cost goal in
[`05-architecture.md`](05-architecture.md) §1. It also permanently locks the project to GPL-2.0,
foreclosing the licensing decision before it is made. And it is a *fork*, not the new project the
author asked for.

### 2.4 Zrythm — viable, not chosen

AGPL-3.0, C++23, Qt/QML + JUCE, 3.1k stars, 8,377 commits. Already has piano roll, audio editor,
audio and MIDI recording with takes, and hosts VST3/CLAP/LV2/LADSPA/AU.

**Against:** as with LMMS, contributing means adopting someone else's architecture, and AGPL-3.0
forecloses the licensing decision more aggressively than GPL does. Zrythm is nonetheless the single
best **architectural reference** available — a modern C++ DAW solving the same problems on a
comparable framework.

### 2.5 From scratch — rejected

Writing an audio engine means building an audio graph, disk streaming, plugin hosting for VST3,
sample-accurate automation, and plugin delay compensation. That is a multi-year effort *before the
first visible feature exists*, and none of it is something a user or an examiner ever sees.

This is the single most common way single-developer DAW projects fail: all effort consumed by
invisible plumbing, nothing demonstrable, momentum lost.

### 2.6 JUCE + tracktion_engine — selected

**tracktion_engine** is a C++20 JUCE module providing a high-level model for sequence-based audio
applications, from simple players through complete DAWs. It supplies audio recording, MIDI, plugin
hosting, mixing, automation, and rendering. It builds on Windows, macOS, Linux, Raspberry Pi, iOS,
and Android. 1.4k stars, ~2,300 commits, actively maintained.

**Why it wins on this project's specific constraints:**

| Constraint | How this stack satisfies it |
|---|---|
| 16 weeks to demo | The hard engine layer already exists and is production-quality |
| Must be *usable*, not just demoable | Recording and playback work from week one, so the author can use it while building |
| Windows-native | First-class MSVC support, unlike Ardour |
| "Our own new project" | 100% of authored code is the application; nothing is forked |
| Scales to 196 features | ValueTree state model gives save/load and undo uniformly |
| No GPL contamination | No GPL source is incorporated |

**Trade-offs accepted:**
- Documentation is thinner than JUCE's; the examples are the real reference material.
- The project discourages direct pull requests for copyright reasons — we consume, not contribute.
- Distributing closed-source commercially requires paid licences for **both** JUCE and Tracktion.

## 3. Licensing consequences of this choice

This is the one part of the stack decision that is hard to reverse. Summary here; full detail in
[`10-licensing-compliance.md`](10-licensing-compliance.md).

| Path | JUCE | tracktion_engine | Consequence |
|---|---|---|---|
| Open source | AGPLv3 | GPL | Source must be published; free |
| Closed commercial | Paid licence | Paid licence | Both required; ongoing subscription |
| Academic use | Educational licence available | Check terms | Excludes for-profit activity |

JUCE modules are dual-licensed under AGPLv3 and a commercial licence. The AGPLv3 path is free but
requires publishing source. The educational licence exists for universities and colleges but
explicitly excludes commercial, professional, promotional, and other for-profit activity.

**Decision D2 in the charter must be made before first distribution, not after.**

## 4. Rejected alternatives within the chosen stack

| Alternative | Why not |
|---|---|
| Qt instead of JUCE | JUCE is purpose-built for audio; Qt would need audio and plugin layers built on top |
| Rust | Excellent for DSP, but no audio framework at JUCE's maturity, and VST3 hosting is weak |
| Electron / web audio | Cannot meet the latency and realtime-safety requirements (NFR2, NFR6) |
| JUCE without tracktion_engine | Would mean writing the engine — see §2.5 |

## 5. Version pinning

Versions are pinned at first build and recorded here. Submodules are pinned to specific commits, not
branches, so builds stay reproducible.

| Component | Version | Pinned |
|---|---|---|
| JUCE | 8.x — exact version TBD at setup | Submodule commit |
| tracktion_engine | develop branch — exact commit TBD | Submodule commit |
| CMake | ≥ 3.22 | Documented minimum |
| MSVC | Visual Studio 2022, v143 toolset | Documented |
| C++ standard | C++20 | `CMAKE_CXX_STANDARD 20` |
| Catch2 | v3.x | Submodule or FetchContent |

## 6. Reference codebases (read-only)

Studied for architecture and algorithms. **No code is copied** — see
[`10-licensing-compliance.md`](10-licensing-compliance.md).

| Project | Licence | What to learn from it |
|---|---|---|
| **Zrythm** | AGPL-3.0 | Modern C++ DAW structure, plugin-format abstraction |
| **LMMS** | GPL-2.0 | Pattern/step-sequencer model, synth design |
| **Ardour** | GPL-2.0+ | Session architecture, disk streaming, engine concepts |
| **Audacity** | GPL-2.0+ | Destructive editing UX, effect dialog design |
| **tracktion_engine examples** | GPL/commercial | Direct API usage patterns — the primary practical reference |

## 7. Open technical questions

| # | Question | Resolve by |
|---|---|---|
| Q1 | Does tracktion_engine support CLAP natively, or is an extension needed? | Before B7.7 is promised |
| Q2 | Exact JUCE 8 and tracktion versions to pin | Phase 1 setup |
| Q3 | ASIO SDK licensing for ASIO support | Before ASIO ships |
| Q4 | MP3 encoding — licensing and library choice | Phase 15 |

**Q1 and Q3 are flagged deliberately.** VST3 and AU support in tracktion_engine is confirmed; CLAP
is not, and should not be promised in any external document until verified. ASIO requires agreeing
to Steinberg's SDK terms, which have redistribution conditions.
