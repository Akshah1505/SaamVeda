# 13 — Project Synopsis

**For academic submission**
**Date:** 2026-07-25
**Project title:** Design and Implementation of a Digital Audio Workstation for Windows
**Working name:** SaamVeda Studio *(provisional — see [`01-project-charter.md`](01-project-charter.md) D1)*

---

## 1. Abstract

This project designs and implements a Digital Audio Workstation (DAW) — a desktop application for
recording, editing, sequencing, mixing, and exporting audio and MIDI — for Microsoft Windows.

The work addresses a genuine gap in freely available music production software. Among the leading
open-source tools, no single application covers the complete production workflow: Audacity records
and edits audio but offers no MIDI sequencing or virtual instruments; LMMS sequences MIDI and
synthesises sound but cannot record audio; Ardour records and mixes but is not practically buildable
or supported on Microsoft Windows. Producers consequently move files between applications.

The system is implemented in C++20 using the JUCE framework and the tracktion_engine audio engine.
It follows a layered architecture with a strict separation between a user-interface-independent core
state model, a command-based mutation layer providing universal undo, and a real-time audio engine
governed by strict thread-safety constraints. Application state is held in a single observable tree
structure, from which serialisation, change notification, and undo are derived uniformly rather than
implemented per feature.

The first release delivers multitrack audio and MIDI recording, non-destructive clip editing with a
destructive render facility, VST3 plugin hosting, a piano roll MIDI editor, a mixer with parameter
automation, and export to standard audio formats. Correctness is verified objectively using null
testing, in which rendered output is compared bit-for-bit against real-time playback.

## 2. Problem Statement

Music production requires two distinct capabilities: **audio recording and editing**, and **MIDI
sequencing with virtual instruments**. Commercial tools such as FL Studio and Pro Tools combine
both, but are proprietary and costly. Free and open-source alternatives are each strong in one area
and weak or absent in the other.

A comparison of the leading free tools:

| Capability | Audacity | LMMS | Ardour |
|---|:---:|:---:|:---:|
| Step-sequencer grid | No | Yes | No |
| Piano roll MIDI editing | No | Yes | Yes |
| Direct microphone recording | Yes | No | Yes |
| Destructive waveform editing | Yes | No | Non-destructive |
| Built-in virtual instruments | No | Yes | Effects only |
| Practical Windows support | Yes | Yes | **No** |

No row is satisfied by all three. The user must therefore install, learn, and transfer files between
multiple applications for a single piece of work.

## 3. Objectives

1. Design a layered software architecture for a DAW that supports sustained feature growth without
   structural rewriting.
2. Implement real-time multitrack audio recording and playback meeting professional latency and
   stability requirements.
3. Implement non-destructive clip editing with a universal undo mechanism.
4. Implement MIDI sequencing through a piano roll editor.
5. Integrate third-party VST3 plugin hosting, including plugin delay compensation.
6. Implement a mixing environment with parameter automation.
7. Implement reliable project persistence and audio export.
8. Verify correctness objectively through automated null testing.

## 4. Scope

**Included in the first release:** audio and MIDI recording; clip editing with undo; VST3 effect and
instrument hosting; piano roll; mixer with automation; built-in effects (EQ, dynamics, reverb,
delay); project save and load; export to WAV, FLAC, MP3, and OGG.

**Excluded from the first release, planned subsequently:** a native virtual instrument suite (third-
party VST3 hosting provides this capability in the interim), time-stretching and audio warping,
spectral editing, pitch correction, and additional plugin formats. A complete 196-item feature
specification is maintained, of which 93 items constitute the first release.

**Permanently excluded:** cross-platform releases, mobile versions, cloud collaboration, and video
editing.

## 5. Methodology

**Development model:** incremental and phased. Thirteen phases across sixteen weeks, each concluding
in independently demonstrable working software. Higher-risk components — toolchain validation, audio
stability, plugin hosting — are deliberately scheduled early, when schedule remains to recover.

**Technology selection.** Five approaches were evaluated: merging the LMMS and Ardour codebases;
forking Ardour; forking LMMS; contributing to Zrythm; and implementing an audio engine from first
principles. Each was rejected with recorded justification. Merging was rejected because the two
codebases share no common foundation — differing user-interface toolkits, audio engines, and data
models — and together represent several hundred person-years of work. Ardour was rejected because no
current Ardour developer builds on Windows and the project provides no Windows build support.
Implementing an engine from first principles was rejected because it would consume the entire
schedule on infrastructure invisible to the user.

The selected approach — JUCE with tracktion_engine — provides a production-quality audio engine
while leaving the whole of the application layer to be authored as original work.

**Architecture.** Five layers with a strict downward dependency rule: user interface, command layer,
core state model, engine wrapper, and services. The core state model has no dependency on the user
interface or audio engine, rendering it fully unit-testable without audio hardware.

**Verification.** Automated unit tests over the core model; null testing to confirm bit-exact
rendering; a debug-build detector enforcing real-time safety on the audio thread; and per-phase
manual quality assurance.

## 6. Expected Outcomes

1. A functioning DAW capable of producing a complete piece of music.
2. A documented, layered architecture demonstrating separation of concerns and real-time
   constraint handling.
3. An automated test suite including objective bit-exactness verification.
4. A complete engineering document set: requirements specification, architecture, data model,
   test strategy, and risk register.
5. A demonstrable end-to-end workflow from recording through to export.

## 7. Technologies

| Component | Technology |
|---|---|
| Language | C++20 |
| Application framework | JUCE 8 |
| Audio engine | tracktion_engine |
| Build system | CMake |
| Compiler | MSVC (Visual Studio 2022) |
| Testing | Catch2 |
| Version control | Git |
| Plugin standard | VST3 |
| Audio drivers | WASAPI, ASIO |

## 8. Timeline

| Weeks | Phase |
|:---:|---|
| 1 | Toolchain foundation and walking skeleton |
| 2–3 | Session model, command bus, transport |
| 4 | Track management and waveform display |
| 5–6 | Audio and MIDI recording |
| 7–8 | Clip editing and undo |
| 9–10 | VST3 plugin hosting and built-in effects |
| 11–12 | Piano roll and pattern sequencing |
| 13–14 | Mixer and automation |
| 15 | Persistence and export |
| 16 | Stabilisation, testing, and demonstration |

## 9. Significance

The project addresses a real deficiency in freely available production software and produces a tool
of practical use to its author and to other musicians. Academically, it demonstrates competence in
areas rarely exercised in undergraduate work: hard real-time constraint satisfaction, lock-free
inter-thread communication, digital signal processing, plugin architecture and binary interface
integration, and the design of a state model supporting universal undo.

The engineering discipline required — particularly the prohibition on memory allocation and locking
within the audio callback, and the use of null testing for objective correctness — represents a
category of software engineering rigour distinct from conventional application development.
