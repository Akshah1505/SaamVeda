# 11 — Testing Strategy

**Document version:** 1.0
**Date:** 2026-07-25

---

## 1. What makes testing a DAW unusual

Three properties shape everything below:

1. **Correctness is measurable to the bit.** Audio either matches or it does not. The null test —
   invert one signal, sum with the other, confirm silence — gives an objective pass/fail that most
   software cannot obtain. Use it heavily.
2. **The worst bugs are timing bugs.** Dropouts, clicks, and race conditions on the audio thread do
   not reproduce reliably and rarely survive a debugger. They must be prevented structurally, not
   caught in review.
3. **Data loss is unforgivable.** A DAW that corrupts a project destroys trust permanently. A
   musician who loses a session does not come back.

The strategy follows from these: automate the objective parts ruthlessly, make realtime violations
impossible by construction, and treat persistence as safety-critical.

## 2. Test levels

| Level | Scope | Tooling | Speed |
|---|---|---|---|
| **Unit** | `core/` logic — model, commands, undo | Catch2 | Milliseconds |
| **Integration** | `engine/` + `core/` together | Catch2, offline rendering | Seconds |
| **Null tests** | Render vs playback correctness | Catch2 + audio comparison | Seconds |
| **Realtime safety** | Audio-thread discipline | Debug-build detector | Continuous |
| **Smoke** | Plugin scanning and loading | Scripted, plugin corpus | Minutes |
| **Manual QA** | UI, workflow, feel | Per-phase checklist | Per phase |
| **Soak** | Long-session stability | Extended manual runs | Hours |

### Why `core/` has no dependencies

[`05-architecture.md`](05-architecture.md) requires `core/` to compile without UI or engine. The
payoff is here: the entire session model, command bus, and undo system test in milliseconds with no
audio device, so the suite can run on every save without friction.

## 3. Unit testing

**Target: `core/` — the session model, commands, undo, schema migration.**

Priority cases:

| Area | Cases |
|---|---|
| Session model | Add/remove/reorder tracks; clip insertion; referential integrity |
| Commands | Each command's execute and undo; 100-deep undo/redo returns to the initial state |
| Schema | Validation rules from [`06-data-model.md`](06-data-model.md) §9; each migration path |
| Automation | Point insertion, ordering, interpolation, curve evaluation |
| Time | Seconds↔beats conversion across tempo changes |
| IDs | Uniqueness; reference resolution; behaviour on dangling references |

**The undo invariant deserves a dedicated property test:** apply N random operations, undo N times,
and assert the serialised tree is byte-identical to the start state. This catches the single most
common structural bug — a feature that mutates state outside the command bus.

## 4. Null testing — the core correctness tool

Three properties, all of which must hold exactly:

| Test | Assertion |
|---|---|
| **Render = playback** | Exported file is bit-identical to real-time output (SRS-NF3.1) |
| **Save/load = identity** | A saved and reloaded project renders identically (SRS-NF3.2) |
| **PDC correctness** | A latency-reporting plugin stays sample-aligned |

```
render(project) ⊕ invert(playback(project)) → peak amplitude must be exactly 0
```

Applied per phase:

| Phase | Null test added |
|:---:|---|
| 6 | Split/trim produce sample-identical audio; destructive render matches playback |
| 7 | Plugin chains render identically with PDC applied |
| 11 | Automation is sample-accurate in render |
| 12 | Save→load→render is identical |

**A non-zero null test is never "close enough."** Any residual indicates a real defect — a rounding
error, an off-by-one in buffer handling, or a missed latency compensation.

## 5. Realtime safety

NFR6 forbids allocation, locking, and I/O on the audio thread. Reviews will not reliably catch
violations, so this is enforced mechanically.

**Debug-build audio-thread detector, added in Phase 2** — not later. A detector added in Phase 11
would report a backlog of violations too large to triage, which is how such checks end up disabled.

| Violation | Detection |
|---|---|
| Heap allocation | Overridden `operator new`, asserts if called on the audio thread |
| Lock acquisition | Instrumented mutex wrapper asserting on the audio thread |
| File I/O | Wrapper assertions |
| Logging | Assert on the audio thread |
| Blocking JUCE calls | Review checklist + assertions where feasible |

Supplemented by AddressSanitizer (available in the VS 2022 C++ workload) and ThreadSanitizer where
supported.

## 6. Performance testing

Measured, not estimated, against the NFRs in [`02-prd.md`](02-prd.md) §5.

| Metric | Target | Method |
|---|---|---|
| Dropouts | 0 at 128 samples / 32 tracks / 10 plugins | Instrumented callback-overrun counter |
| Round-trip latency | < 20 ms at 128 samples | Loopback measurement |
| Startup | < 5 s excluding plugin scan | Timed |
| Project load | 50 tracks < 10 s | Timed with a generated fixture |
| Waveform display | 10-min file < 2 s | Timed |
| UI frame rate | 60 fps during scroll/zoom | Frame timing instrumentation |
| Export | Faster than real time | Timed |
| Memory | < 2 GB for 20 tracks | Process monitoring |

**Fixture projects are generated, not hand-built** — a script produces 8/32/50/100-track projects so
performance is measured against consistent load across phases.

## 7. Plugin compatibility

The highest-variance area: third-party plugins are arbitrary code of highly variable quality.

**A reference corpus of at least 20 VST3 plugins** — mixing well-known commercial plugins and free
ones (Surge XT, Vital, and similar), deliberately including at least one known-problematic plugin.

| Test | Pass condition |
|---|---|
| Scan | Full corpus scans without hanging |
| Crash isolation | A plugin crashing during scan does not kill the host (SRS-6.2) |
| Load/unload | Repeated cycling leaks no memory |
| State | Save/reload restores parameters exactly |
| PDC | Latency-reporting plugin stays aligned (null test) |
| Editor | Native UI opens, resizes, and closes cleanly |

Target: ≥ 90% of the corpus loads successfully.

## 8. Persistence and data safety

Treated as safety-critical.

| Test | Assertion |
|---|---|
| Round-trip | Save→load→render is null-identical |
| Migration | Every prior schema version still opens (fixture projects retained per version) |
| Crash recovery | Killed mid-session, autosave recovers within 60 s of work |
| Recording durability | Killed mid-recording, audio already captured survives |
| Missing media | Clips marked offline and relinkable — **never silently deleted** |
| Corrupt file | Reports an error; never crashes; never destroys the original |

**Keep a fixture project per schema version, permanently, from Phase 12 onward.** Reconstructing an
old project file after the fact is far harder than saving one at the time.

## 9. Manual QA

Automation cannot judge whether a fade *feels* right or a drag is responsive. Each phase ends with a
manual checklist covering:

- Every feature in the phase, exercised through the UI
- Undo/redo on each new operation
- Save, reload, and verify
- Keyboard shortcuts
- Behaviour at boundaries — empty project, single track, 100 tracks
- Deliberate misuse — cancel mid-operation, delete while playing, unplug the audio device

**A defect log is kept per phase.** Defects are fixed before the next phase begins; carrying them
forward is how a project arrives at week 16 with an unshippable backlog.

## 10. Soak testing

Timing bugs surface over hours, not minutes.

| Test | Duration | Pass condition |
|---|---|---|
| Continuous playback | 4 hours | No dropouts, no memory growth |
| Continuous recording | 2 hours | No gaps (SRS-NF2.3) |
| Edit session | 2 hours active use | No crash, no leak, no undo corruption |
| Plugin cycling | 1000 load/unload cycles | Stable memory |

Run at the end of Phase 11 and again in Phase 13, before the demo.

### The recording check harness

`tools/recording-check/` builds `SaamVedaRecordingCheck`, a console target that drives
`EngineController` against a real audio device. It exists because the FR4 acceptance criteria cannot
be expressed as unit tests: they need an open device, an input signal, and real elapsed time.

| Mode | Measures |
|---|---|
| `--soak=<minutes>` | a continuous recording completes with no gaps |
| `--alignment` | a recorded clip lands where the transport was, and the file matches the clip |
| `--overdub <file>` | recording onto one track while another plays existing material (SRS-3.2) |
| `--record=<seconds>` | records, to be killed partway through |
| `--analyse <file>` | what actually reached the disk, used after that kill |

`crash-test.ps1` beside it drives the last two together: it starts a recording, terminates the
process outright with no chance to flush, and reads back what survived (SRS-3.6).

Every mode prints `key = value` lines and exits non-zero on failure, so it can gate a release.
It is deliberately **not** part of `ctest`: half an hour is too long for a commit gate, and a build
machine with no input would fail it for the wrong reason.

Two things worth knowing before reading its output. tracktion reports recording failures only
through `juce::Logger`, so the harness mirrors that log to stdout - without it, a recording that
stops itself looks exactly like one that produced nothing. And the MIDI device scan finishes a
second or two after startup and reloads the device list, which rebuilds the playback context and
kills any recording already in progress; the harness waits that out before it starts.

## 11. CI

Local at minimum; GitHub Actions if the repository moves to a remote.

On every commit: build Debug and Release, run unit tests, run null tests, and run the realtime-safety
detector. Performance benchmarks run nightly rather than per-commit, since they are slower and
noisier.

## 12. Phase exit gates

No phase is complete until:

- [ ] All SRS requirements for the phase pass
- [ ] Unit tests written and green
- [ ] Applicable null tests pass exactly
- [ ] Realtime detector reports no violations
- [ ] Manual QA checklist complete
- [ ] Defect log empty or explicitly deferred with reasons
- [ ] Performance targets met

## 13. Deliberately not tested (yet)

Honest scope limits, so their absence is a decision rather than an oversight:

| Not covered | Why | When |
|---|---|---|
| Automated UI tests | Brittle and expensive relative to value at this stage | Post-semester |
| Cross-platform | Windows only for now | Post-semester |
| Localisation | English only | Post-semester |
| Accessibility | Committed as A8.10 (P2), untested until built | With A8.10 |
| Fuzzing project files | Valuable for robustness; not affordable now | Post-semester |
