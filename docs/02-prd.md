# 02 — Product Requirements Document

**Document version:** 1.0
**Date:** 2026-07-25
**Supersedes:** `Raga_Sur_PRD.pdf` v1.0 (flute practice web application)
**Companion:** [`04-feature-backlog.md`](04-feature-backlog.md) — the complete 196-feature parity list

---

## 1. Overview

**Problem.** No single free DAW covers both halves of the music-production workflow. Audacity
records and destructively edits audio but has no MIDI sequencing, piano roll, or instruments. LMMS
sequences and synthesises but cannot record audio. Ardour records and mixes but is not practically
buildable or supported on Windows. Producers end up moving files between applications.

**Product.** A Windows-native Digital Audio Workstation combining multitrack audio recording and
editing with MIDI sequencing, instrument hosting, mixing, and automation — one application covering
the whole workflow.

**Long-term target.** Full feature parity with Audacity and FL Studio combined. All 196 identified
features are in scope; [`04-feature-backlog.md`](04-feature-backlog.md) sequences them.

**This document specifies the P0/P1 tier** — the 93 features constituting the first genuinely
usable release, targeted at the 16-week milestone. It is not the limit of the product.

## 2. Users

| User | Need |
|---|---|
| **Primary — the author** | A DAW for their own production work; will use it in preference to alternatives once capable |
| **Home producers** | One tool for recording instruments and sequencing, without switching applications |
| **Students & hobbyists** | A free, capable DAW without subscription or licence cost |
| **Academic reviewers** | Evidence of a complete, well-architected software system |

## 3. Goals & Non-Goals

**Goals**
- Record and edit multitrack audio without dropouts or artefacts
- Sequence MIDI and drive instrument plugins
- Host industry-standard VST3 plugins reliably
- Mix with automation and export to standard formats
- Remain usable for real work throughout development, not only at the end

**Non-Goals** (see [`01-project-charter.md`](01-project-charter.md) §4)
- Merging or forking existing DAW codebases
- Linux/macOS/mobile releases
- Writing a custom audio engine
- Cloud collaboration or account systems

## 4. Functional Requirements

Each requirement is testable. IDs map to [`04-feature-backlog.md`](04-feature-backlog.md).

### FR1 — Audio Engine & Transport `P0`

Audio device selection (WASAPI/ASIO), configurable sample rate and buffer size, transport control
(play, stop, loop, tempo, time signature), and a moving playhead.

> **Acceptance:** Plays a 44.1 kHz stereo WAV for 10 minutes at a 128-sample buffer with zero
> dropouts. Transport responds within 50 ms. Tempo changes take effect without audio interruption.

### FR2 — Track Management `P0`

Create, delete, reorder, and rename audio and MIDI tracks. Per-track mute, solo, gain, pan.
Adjustable track height with collapse/expand.

> **Acceptance:** 32 tracks created and played simultaneously without dropout. Solo correctly mutes
> all non-soloed tracks. Track state survives save and reload.

### FR3 — Timeline & Waveform Display `P0`

Scrollable, zoomable timeline with a time/bar ruler. Audio clips render as waveforms that redraw
correctly at all zoom levels.

> **Acceptance:** Zoom from whole-project to sample level without visual artefacts. Waveform for a
> 10-minute file renders in under 2 seconds. Scrolling stays at 60 fps.

### FR4 — Audio Recording `P0`

Arm tracks, select input, monitor input, record to disk with overdub against existing material.
Automatic latency compensation.

> **Acceptance:** A recorded track aligns with existing material within ±1 ms. A 30-minute
> continuous recording completes with no gaps. Recording survives an application crash.

### FR5 — MIDI Recording `P0`

Record MIDI from an external controller onto MIDI tracks with correct timing.

> **Acceptance:** Recorded MIDI timing is within ±5 ms of played timing. Note-on/note-off pairs and
> velocity are preserved.

### FR6 — Clip Editing `P0`

Move, trim, split, duplicate, delete clips. Fade in/out with adjustable curves. Snap to grid. Full
undo/redo across every operation.

> **Acceptance:** Every edit is undoable and redoable. 100 sequential operations undo cleanly to the
> initial state. Split produces two clips that play back sample-identically to the original.

### FR7 — Destructive Render `P0`

Render any clip or selection to a new audio file in place, applying its effects and edits — the
Audacity-style destructive workflow, on a non-destructive foundation.

> **Acceptance:** Rendered output is null-test identical to the real-time playback of the same
> region.

### FR8 — Plugin Hosting `P0`

Scan for VST3 plugins, present a browsable database, load effects into per-track chains and
instruments onto MIDI tracks, open native plugin editors, and apply plugin delay compensation.

> **Acceptance:** Scans a folder of 50 plugins without hanging. A crashing plugin does not take the
> application with it *(requires B7.9 sandboxing; until then, the crash is logged and reported)*.
> A latency-reporting plugin stays in sample-accurate sync.

### FR9 — Piano Roll `P0`

Draw, move, resize, and delete MIDI notes. Edit velocity. Quantize with adjustable strength. Scale
highlighting.

> **Acceptance:** Notes align to the grid within 1 tick. Quantize at 100% strength lands notes
> exactly on grid positions. Edits are undoable.

### FR10 — Mixer & Automation `P0`

Mixer view with faders, pan, mute/solo, and peak/RMS metering. Send/return buses. Master track.
Automation lanes for volume, pan, and any plugin parameter.

> **Acceptance:** Meters track actual output within 0.1 dB. Automation is sample-accurate on
> playback and identical on export.

### FR11 — Built-in Effects `P0`

At minimum: parametric EQ, compressor, limiter, gate, reverb, delay. All automatable, all with
preset save/load.

> **Acceptance:** Each processes audio without artefacts at a 128-sample buffer. Bypass is
> click-free. Presets restore exactly.

### FR12 — Project Persistence `P0`

Save and load complete project state — tracks, clips, plugins and their parameters, automation,
mixer routing.

> **Acceptance:** A saved and reloaded project renders null-test identical output. Save of a
> 50-track project completes in under 3 seconds. Autosave recovers work after a forced termination.

### FR13 — Export `P0`

Render the project or a selected region to WAV, MP3, FLAC, and OGG at selectable sample rate and
bit depth.

> **Acceptance:** Exported WAV is null-test identical to real-time playback. Export of a 5-minute
> project completes faster than real time.

### FR14 — Step Sequencer & Patterns `P1`

Pattern-based step grid, pattern clips arranged on the playlist, and pattern/song mode switching.

> **Acceptance:** A 16-step pattern triggers an instrument at correct times. Patterns place on the
> timeline and play in arrangement.

## 5. Non-Functional Requirements

| ID | Requirement | Target |
|---|---|---|
| NFR1 | Audio stability | Zero dropouts at 128-sample buffer, 32 tracks, 10 plugins |
| NFR2 | Latency | Round-trip under 20 ms at a 128-sample buffer |
| NFR3 | Startup time | Under 5 seconds excluding plugin scan |
| NFR4 | Project load | 50-track project in under 10 seconds |
| NFR5 | Memory | Under 2 GB for a typical 20-track project |
| NFR6 | Audio-thread safety | No allocation, locking, or I/O on the audio thread — ever |
| NFR7 | Bit-perfect rendering | Export null-tests identical to playback |
| NFR8 | Crash resilience | Autosave at most 60 seconds old |
| NFR9 | Platform | Windows 11 x64; architecture stays portable |

**NFR6 is non-negotiable** and constrains every design decision. See
[`05-architecture.md`](05-architecture.md) §4.

## 6. Out of Scope for the 16-Week Milestone

Not cancelled — sequenced later. See [`04-feature-backlog.md`](04-feature-backlog.md) P2/P3.

Native instrument suite (VST3 hosting covers the capability), time-stretch and warping, spectral
editing, pitch correction, noise reduction, modular plugin routing, video scoring, MIDI scripting,
themes, and the Indian-classical differentiator set.

## 7. Success Metrics

| Metric | Target |
|---|---|
| Author produces a complete track in the application | At least 1 by week 16 |
| Dropouts during a 1-hour session | 0 |
| Third-party VST3 plugins loading successfully | ≥ 90% of a 20-plugin test set |
| Export null-test accuracy | Bit-identical |
| P0 features complete at week 16 | 100% |
| Peer/faculty usability feedback | Positive from ≥ 3 reviewers |

## 8. Dependencies & Risks

Full detail in [`12-risk-register.md`](12-risk-register.md). Headline items:

- **No toolchain installed.** No CMake, MSVC, or Ninja on the development machine.
- **Licence undecided.** AGPLv3 versus commercial JUCE/Tracktion licensing — irreversible once
  released. See [`10-licensing-compliance.md`](10-licensing-compliance.md).
- **Single part-time developer** against a fixed semester deadline.
- **Application Control policy** on the development machine has already blocked one DLL and may
  interfere with compilers, linkers, or plugin scanning.
