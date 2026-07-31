# 03 — Software Requirements Specification

**Document version:** 1.0
**Date:** 2026-07-25
**Prepared for:** Academic project submission
**Standard:** Structured after IEEE 830

---

## 1. Introduction

### 1.1 Purpose

This document specifies the software requirements for a Digital Audio Workstation (DAW) application
for Microsoft Windows. It is intended for the developer, academic supervisors, and evaluators.

It specifies the **first release** (P0/P1 tier, 93 requirements). The complete product target of 196
features is enumerated in [`04-feature-backlog.md`](04-feature-backlog.md).

### 1.2 Scope

The software records, edits, sequences, mixes, and exports audio and MIDI. It hosts third-party VST3
plugins. It targets feature parity with Audacity (audio recording and editing) and FL Studio (MIDI
sequencing and production) combined.

**Benefits:** a single application covering a workflow that currently requires several, with no
licence cost to the end user.

### 1.3 Definitions and Abbreviations

| Term | Definition |
|---|---|
| **DAW** | Digital Audio Workstation |
| **Clip** | A bounded region of audio or MIDI positioned on the timeline |
| **Track** | A timeline lane holding clips of one type |
| **Buffer size** | Samples processed per audio callback; determines latency |
| **Dropout** | Audible glitch caused by missing an audio callback deadline |
| **VST3** | Steinberg's Virtual Studio Technology plugin format, version 3 |
| **PDC** | Plugin Delay Compensation |
| **Null test** | Verifying two signals are identical by inverting one and summing |
| **Non-destructive** | Editing that alters playback instructions, not source files |
| **ValueTree** | JUCE's hierarchical, observable, undoable data structure |
| **LUFS** | Loudness Units Full Scale |
| **Realtime-safe** | Code guaranteed to complete without unbounded delay |

### 1.4 References

- [`01-project-charter.md`](01-project-charter.md) — vision and constraints
- [`02-prd.md`](02-prd.md) — product requirements
- [`04-feature-backlog.md`](04-feature-backlog.md) — full 196-feature parity list
- [`05-architecture.md`](05-architecture.md) — technical design
- JUCE framework: <https://juce.com>
- tracktion_engine: <https://github.com/Tracktion/tracktion_engine>

### 1.5 Overview

§2 describes overall context and constraints. §3 specifies detailed requirements. §4 covers
verification.

---

## 2. Overall Description

### 2.1 Product Perspective

A **standalone desktop application** — no server, no network dependency, no user accounts. It
interfaces with:

| External entity | Interface |
|---|---|
| Audio hardware | WASAPI / ASIO drivers |
| MIDI hardware | Windows MIDI API |
| VST3 plugins | Steinberg VST3 SDK, via JUCE |
| File system | Project files, audio files, presets |

### 2.2 Product Functions

1. Record audio from hardware inputs onto multiple tracks
2. Record MIDI from external controllers
3. Edit clips non-destructively, with destructive render available
4. Sequence MIDI via a piano roll and step sequencer
5. Host VST3 effects and instruments
6. Mix with faders, sends, automation, and metering
7. Apply built-in effects (EQ, dynamics, reverb, delay)
8. Save and restore complete project state
9. Export to WAV, MP3, FLAC, OGG

### 2.3 User Characteristics

| Class | Expertise | Expectation |
|---|---|---|
| Producer / musician | Familiar with DAW concepts | Conventions matching existing DAWs |
| Student / hobbyist | Learning | Discoverable UI, forgiving undo |
| Evaluator | Software engineering | Architectural quality, documentation |

Users are assumed familiar with tracks, clips, and mixing. The software does not teach music
production.

### 2.4 Constraints

| ID | Constraint |
|---|---|
| C1 | Windows 11 x64 only |
| C2 | C++20; JUCE 8 and tracktion_engine |
| C3 | Single part-time developer |
| C4 | ~16 weeks to first milestone |
| C5 | Audio callbacks must be realtime-safe — no allocation, locks, or I/O |
| C6 | Licensing constrains distribution — see [`10-licensing-compliance.md`](10-licensing-compliance.md) |
| C7 | Reference codebases (LMMS, Ardour) are GPL; their code must not be copied |

### 2.5 Assumptions and Dependencies

- A working audio interface with WASAPI or ASIO drivers is available
- The C++ toolchain will be installed before development (**not currently installed**)
- JUCE and tracktion_engine remain available under their present licences
- Third-party VST3 plugins are available for testing

---

## 3. Specific Requirements

Priority: **P0** = first release, mandatory · **P1** = first release, desirable.

### 3.1 Functional Requirements

#### 3.1.1 Audio Engine

| ID | Requirement | Priority |
|---|---|:---:|
| SRS-1.1 | Enumerate available audio devices and inputs/outputs | P0 |
| SRS-1.2 | Allow selection of driver type, sample rate, and buffer size | P0 |
| SRS-1.3 | Support sample rates 44.1–192 kHz | P0 |
| SRS-1.4 | Support buffer sizes 64–2048 samples | P0 |
| SRS-1.5 | Provide play, stop, and loop transport controls | P0 |
| SRS-1.6 | Display and allow editing of tempo and time signature | P0 |
| SRS-1.7 | Display a playhead synchronised to audio output | P0 |
| SRS-1.8 | Provide a metronome with count-in | P0 |

#### 3.1.2 Track Management

| ID | Requirement | Priority |
|---|---|:---:|
| SRS-2.1 | Create, delete, rename, and reorder audio and MIDI tracks | P0 |
| SRS-2.2 | Provide per-track mute, solo, gain, and pan | P0 |
| SRS-2.3 | Support at least 64 simultaneous tracks | P0 |
| SRS-2.4 | Allow adjustable track height and colour | P0 |
| SRS-2.5 | Persist all track state across save/load | P0 |

#### 3.1.3 Recording

| ID | Requirement | Priority |
|---|---|:---:|
| SRS-3.1 | Arm tracks for recording and select an input source | P0 |
| SRS-3.2 | Record audio to disk while playing back existing tracks | P0 |
| SRS-3.3 | Provide input monitoring with a level meter | P0 |
| SRS-3.4 | Compensate automatically for input/output latency | P0 |
| SRS-3.5 | Record MIDI input onto MIDI tracks | P0 |
| SRS-3.6 | Preserve recorded audio if the application terminates unexpectedly | P0 |
| SRS-3.7 | Support multi-input simultaneous recording | P1 |

#### 3.1.4 Editing

| ID | Requirement | Priority |
|---|---|:---:|
| SRS-4.1 | Move, trim, split, duplicate, and delete clips | P0 |
| SRS-4.2 | Apply fade-in and fade-out with selectable curves | P0 |
| SRS-4.3 | Snap edits to a configurable grid | P0 |
| SRS-4.4 | Provide unlimited undo/redo for every state-changing operation | P0 |
| SRS-4.5 | Display an undo history list | P0 |
| SRS-4.6 | Render a clip or selection destructively to a new audio file | P0 |
| SRS-4.7 | Provide clip gain and per-clip volume envelopes | P1 |
| SRS-4.8 | Support crossfades between overlapping clips | P1 |

#### 3.1.5 MIDI Editing

| ID | Requirement | Priority |
|---|---|:---:|
| SRS-5.1 | Display MIDI clips in a piano roll editor | P0 |
| SRS-5.2 | Draw, move, resize, and delete notes | P0 |
| SRS-5.3 | Edit per-note velocity | P0 |
| SRS-5.4 | Quantize with adjustable strength | P0 |
| SRS-5.5 | Highlight and optionally snap to a selected musical scale | P1 |
| SRS-5.6 | Provide a step sequencer grid for pattern entry | P1 |

#### 3.1.6 Plugin Hosting

| ID | Requirement | Priority |
|---|---|:---:|
| SRS-6.1 | Scan configured folders for VST3 plugins | P0 |
| SRS-6.2 | Perform scanning out-of-process so a faulty plugin cannot crash the host | P0 |
| SRS-6.3 | Cache scan results between sessions | P0 |
| SRS-6.4 | Load VST3 effects into per-track chains | P0 |
| SRS-6.5 | Load VST3 instruments onto MIDI tracks | P0 |
| SRS-6.6 | Display the plugin's native editor window | P0 |
| SRS-6.7 | Apply plugin delay compensation | P0 |
| SRS-6.8 | Persist plugin state across save/load | P0 |
| SRS-6.9 | Save and recall plugin presets | P1 |

#### 3.1.7 Mixing and Automation

| ID | Requirement | Priority |
|---|---|:---:|
| SRS-7.1 | Provide a mixer view with faders, pan, mute, and solo | P0 |
| SRS-7.2 | Display peak and RMS level meters | P0 |
| SRS-7.3 | Provide send/return buses | P0 |
| SRS-7.4 | Provide a master track with its own effect chain | P0 |
| SRS-7.5 | Automate volume, pan, and any plugin parameter | P0 |
| SRS-7.6 | Edit automation as editable curves on lanes | P0 |
| SRS-7.7 | Support sidechain routing | P1 |
| SRS-7.8 | Map hardware MIDI controls to parameters (MIDI learn) | P1 |

#### 3.1.8 Built-in Effects

| ID | Requirement | Priority |
|---|---|:---:|
| SRS-8.1 | Parametric EQ with spectrum display | P0 |
| SRS-8.2 | Compressor, limiter, and noise gate | P0 |
| SRS-8.3 | Algorithmic reverb | P0 |
| SRS-8.4 | Tempo-syncable delay | P0 |
| SRS-8.5 | Modulation effects — chorus, flanger, phaser | P1 |
| SRS-8.6 | Distortion and waveshaping | P1 |
| SRS-8.7 | Click-free bypass on all effects | P0 |

#### 3.1.9 Persistence and Export

| ID | Requirement | Priority |
|---|---|:---:|
| SRS-9.1 | Save complete project state to a single file | P0 |
| SRS-9.2 | Load a saved project restoring all state exactly | P0 |
| SRS-9.3 | Autosave at intervals not exceeding 60 seconds | P0 |
| SRS-9.4 | Recover from autosave after abnormal termination | P0 |
| SRS-9.5 | Export to WAV, FLAC, MP3, and OGG | P0 |
| SRS-9.6 | Export a selected region only | P1 |
| SRS-9.7 | Export individual tracks as stems | P1 |

### 3.2 Non-Functional Requirements

#### 3.2.1 Performance

| ID | Requirement |
|---|---|
| SRS-NF1.1 | Zero dropouts with 32 tracks and 10 plugins at a 128-sample buffer |
| SRS-NF1.2 | Round-trip latency under 20 ms at a 128-sample buffer |
| SRS-NF1.3 | Startup under 5 seconds, excluding plugin scan |
| SRS-NF1.4 | 50-track project loads in under 10 seconds |
| SRS-NF1.5 | Waveform for a 10-minute file displays within 2 seconds |
| SRS-NF1.6 | UI maintains 60 fps during scroll and zoom |
| SRS-NF1.7 | Export completes faster than real time |

#### 3.2.2 Reliability

| ID | Requirement |
|---|---|
| SRS-NF2.1 | No data loss on abnormal termination beyond the autosave interval |
| SRS-NF2.2 | A faulty plugin must not corrupt project data |
| SRS-NF2.3 | Recording continues uninterrupted for at least 2 hours |

#### 3.2.3 Correctness

| ID | Requirement |
|---|---|
| SRS-NF3.1 | Exported audio is bit-identical to real-time playback (null test) |
| SRS-NF3.2 | A saved and reloaded project produces identical output |
| SRS-NF3.3 | Automation is sample-accurate |

#### 3.2.4 Safety and Maintainability

| ID | Requirement |
|---|---|
| SRS-NF4.1 | No memory allocation, locking, or I/O on the audio thread |
| SRS-NF4.2 | `core/` compiles and tests without UI or engine dependencies |
| SRS-NF4.3 | All state mutations pass through the command bus |
| SRS-NF4.4 | No GPL-licensed source is incorporated (see C7) |

#### 3.2.5 Usability

| ID | Requirement |
|---|---|
| SRS-NF5.1 | Keyboard shortcuts follow prevailing DAW conventions |
| SRS-NF5.2 | Every destructive action is undoable |
| SRS-NF5.3 | Long operations show progress and remain cancellable |

### 3.3 External Interface Requirements

| Interface | Requirement |
|---|---|
| **User** | Windowed desktop GUI; resizable; high-DPI aware |
| **Hardware** | WASAPI (shared and exclusive) and ASIO; Windows MIDI |
| **Software** | VST3 SDK via JUCE; audio codecs for MP3/FLAC/OGG |
| **File** | Project file; WAV/AIFF/FLAC/MP3/OGG import and export |

---

## 4. Verification

Each requirement is verified by the method below. Detail in
[`11-testing-strategy.md`](11-testing-strategy.md).

| Requirement class | Verification method |
|---|---|
| Audio engine, performance | Instrumented measurement under defined load |
| Editing, MIDI, persistence | Automated unit and integration tests |
| Correctness (null tests) | Automated bit-comparison of render vs playback |
| Plugin hosting | Smoke test against a 20-plugin reference set |
| Usability | Review by at least 3 peer/faculty reviewers |
| Realtime safety | Debug-build allocation detector on the audio thread |

---

## 5. Traceability

| SRS section | PRD requirement | Backlog IDs | Roadmap phase |
|---|---|---|---|
| 3.1.1 Audio engine | FR1 | A8.5, B11.8 | 1–2 |
| 3.1.2 Tracks | FR2 | A4.1–A4.4, A4.12 | 2–4 |
| 3.1.3 Recording | FR4, FR5 | A2.1–A2.6, B10.1 | 5–6 |
| 3.1.4 Editing | FR6, FR7 | A3.1–A3.13, A5.19 | 7–8 |
| 3.1.5 MIDI editing | FR9, FR14 | B2.1–B2.11, B1.1–B1.5 | 11–12 |
| 3.1.6 Plugins | FR8 | B7.1–B7.5 | 9–10 |
| 3.1.7 Mixing | FR10 | B4.1–B4.10, B5.1–B5.4 | 13–14 |
| 3.1.8 Effects | FR11 | B9.1–B9.8, A5.1–A5.11 | 9–10 |
| 3.1.9 Persistence | FR12, FR13 | A1.6–A1.13 | 15–16 |
