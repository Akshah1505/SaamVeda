# 05 — Architecture

**Document version:** 1.0
**Date:** 2026-07-25

---

## 1. Architectural goal

One goal dominates: **feature #50 must cost no more than feature #5.**

The backlog contains 196 features. A design that makes each new feature progressively more expensive
guarantees the project stalls somewhere around feature 30 — which is where most single-developer DAW
projects die. Every decision below is made in service of keeping the marginal cost of a feature flat.

Three properties deliver that:

1. **A headless core.** All state and all operations exist without a UI, so features are testable
   and scriptable from day one.
2. **One undo mechanism.** Undo is structural, not implemented per feature. A new feature gets undo
   for free or it is built wrong.
3. **Strict audio-thread discipline.** Realtime safety is a property of the architecture, not of
   individual authors remembering the rules.

## 2. Layers

```
┌─────────────────────────────────────────────────────────┐
│  ui/            JUCE components. No business logic.     │
│                 Arrangement · PianoRoll · Mixer ·       │
│                 WaveformEditor · Browser · Transport    │
├─────────────────────────────────────────────────────────┤
│  app/           Command bus. Every user action is a     │
│                 command object. Undo lives here.        │
├─────────────────────────────────────────────────────────┤
│  core/          Session model (ValueTree). Pure state.  │
│                 No UI, no engine, no I/O. Fully tested. │
├─────────────────────────────────────────────────────────┤
│  engine/        tracktion_engine wrapper: transport,    │
│                 recording, rendering, plugin hosting    │
├─────────────────────────────────────────────────────────┤
│  services/      Device management · plugin scanning ·   │
│                 file I/O · preset storage · undo store  │
└─────────────────────────────────────────────────────────┘
```

**Dependency rule: arrows point downward only.** `core/` depends on nothing. `ui/` may not touch
`engine/` directly — it issues commands. Violations are caught in review; a link-time check is
added in Phase 2.

### Why `core/` has no engine dependency

It makes the entire session model unit-testable without an audio device, which means the test suite
runs in CI in milliseconds. It also means a future headless render mode, a scripting API, or a
command-line exporter costs almost nothing.

## 3. Module responsibilities

| Module | Owns | Must not |
|---|---|---|
| `core/` | Session state, tracks, clips, automation curves, project schema | Reference JUCE UI or tracktion types |
| `app/` | Command definitions, undo stack, application state | Contain DSP or drawing code |
| `engine/` | tracktion_engine lifecycle, transport, device I/O, render | Be referenced from `ui/` |
| `services/` | Plugin scan, file dialogs, preset persistence, settings | Hold session state |
| `ui/` | Drawing, input handling, layout | Mutate the session directly |

## 4. Threading model

**The audio thread is sacred.** NFR6 in [`02-prd.md`](02-prd.md) forbids allocation, locking, and
I/O on it. This is the rule most likely to be violated accidentally and most expensive to fix later.

| Thread | Responsibility | Constraints |
|---|---|---|
| **Audio** | Process audio callbacks | **No** `new`/`delete`, mutexes, file I/O, logging, or JUCE UI calls |
| **Message (UI)** | All drawing and user input | Never blocks on the audio thread |
| **Disk streaming** | Read/write audio files | Owned by tracktion_engine |
| **Background** | Plugin scan, waveform generation, autosave | Never touches live session state directly |

### Crossing thread boundaries

- **UI → Audio:** lock-free FIFO of parameter changes, or tracktion_engine's own parameter system.
  Never a direct call.
- **Audio → UI:** lock-free FIFO plus a UI-side timer that drains it. Meters and playhead position
  use atomics, not messages.
- **Background → Session:** marshal to the message thread; never mutate session state off-thread.

**Waveform thumbnails, plugin scanning, and autosave all run in the background** — each is a
plausible source of UI stalls if done naively.

## 5. State, commands, and undo

State lives in a single `juce::ValueTree` hierarchy, paired with `juce::UndoManager`.

```
Session
├── Properties: tempo, timeSignature, sampleRate, name
├── Tracks
│   ├── Track (audio) — name, colour, gain, pan, mute, solo, armed
│   │   ├── Clips → Clip: start, length, offset, fadeIn, fadeOut, gain, sourceFile
│   │   ├── PluginChain → Plugin: uid, state (base64), bypassed
│   │   └── AutomationLanes → Lane: parameterId → Points
│   └── Track (MIDI) — as above, plus Patterns → Notes
├── Mixer → MixerTrack, Sends, Master
└── Arrangement → Markers, TempoMap
```

**Why ValueTree.** It gives change notification, serialisation, and undo *for free and uniformly*.
Any new feature that stores its state here inherits save/load and undo without writing a line for
either. That is precisely the flat-marginal-cost property §1 demands. It is also tracktion_engine's
native idiom, so the two compose rather than fight.

**The rule:** all mutations go through the command bus. No component writes to the tree directly.

```cpp
// Every user action is a command. Undo is automatic.
class SplitClipCommand : public Command {
    bool execute (Session&, UndoManager&) override;
    juce::String getName() const override { return "Split Clip"; }
};
```

A feature that cannot express itself as a command is a design smell worth stopping for.

## 6. Engine integration

tracktion_engine is wrapped, not exposed. `engine/EngineController` is the only class that includes
tracktion headers.

**Rationale.** It keeps the option of replacing or supplementing the engine later, keeps `core/`
testable, and — practically — contains the licensing surface in one place, since the JUCE/Tracktion
licence question (see [`10-licensing-compliance.md`](10-licensing-compliance.md)) attaches to this
layer.

The session model in `core/` is the source of truth; `engine/` synchronises the tracktion `Edit`
from it. Where tracktion's own model is authoritative (plugin state, transport position), the
wrapper mediates.

## 7. Plugin architecture

| Concern | Approach |
|---|---|
| Scanning | Background thread, out-of-process, results cached to disk |
| Crash isolation | Scanning is out-of-process from Phase 9. Full hosting sandbox is B7.9 (P2) |
| Formats | VST3 first (P0); VST2, CLAP, LV2 follow (P2/P3) |
| State | Plugin state serialised into the ValueTree as base64 |
| Delay compensation | Handled by tracktion_engine; verified by null-test |

Until B7.9 lands, a misbehaving plugin can crash the host. This is a known, accepted gap for the
semester milestone and is logged in [`12-risk-register.md`](12-risk-register.md).

## 8. UI architecture

JUCE components, with a strict rule: **components read from the session and issue commands; they
never mutate.** Each major view is independently constructible for testing.

Views: Arrangement (timeline + clips), Piano Roll, Mixer, Waveform Editor, Browser, Transport bar.

Waveform rendering uses cached multi-resolution thumbnails generated in the background, so zoom
stays responsive on long files (FR3 requires 10-minute files in under 2 seconds).

## 9. Extensibility

Features earn their low marginal cost by fitting existing seams rather than adding new ones:

| To add… | Do this |
|---|---|
| A new effect | Implement the plugin interface; registry handles the rest |
| A new editor view | New component reading session, emitting commands |
| A new file format | Register an importer/exporter in `services/` |
| A new automatable parameter | Declare it; automation and undo come free |
| A scripting API | Bind the command bus — deliberately already possible |

## 10. Directory layout

```
D:\SaamVeda\
├── CMakeLists.txt
├── modules/          JUCE + tracktion_engine (git submodules)
├── src/
│   ├── core/         session model, schema, pure logic
│   ├── app/          commands, undo, application state
│   ├── engine/       tracktion wrapper
│   ├── services/     plugin scan, file I/O, settings
│   ├── ui/           all JUCE components
│   └── Main.cpp
├── tests/            unit + null-tests
├── resources/        icons, fonts, factory presets
└── docs/             this document set
```

## 11. Decisions deliberately deferred

| Decision | Deferred until | Why it is safe to defer |
|---|---|---|
| Project file format (binary vs XML) | Phase 15 | ValueTree serialises to both; choice is late-binding |
| Plugin sandbox IPC design | P2 | Scanning is already out-of-process; hosting isolation is additive |
| Scripting language | P3 | Command bus is the binding surface regardless |
| Cross-platform build | Post-semester | JUCE and tracktion are already portable |

## 12. Known architectural risks

1. **tracktion_engine's model versus ours.** Two sources of truth risk divergence. Mitigation: the
   wrapper owns synchronisation and is covered by tests; where tracktion is authoritative, we do not
   duplicate.
2. **ValueTree performance at scale.** Very large sessions may stress change notification.
   Mitigation: measure at Phase 13 with a 100-track project before it becomes structural.
3. **Audio-thread violations creeping in.** Mitigation: code review checklist, and a debug-build
   allocation-detector on the audio thread from Phase 2 — implemented in
   [`src/engine/RealtimeSanityCheck.cpp`](../src/engine/RealtimeSanityCheck.cpp), reported live in
   the main window's status strip. It identifies the audio thread from a device callback and flags
   any CRT heap allocation on it. Read the header for what it does *not* catch (`HeapAlloc`,
   locking, file I/O) — those remain a review concern.
4. **Wrapper becoming a bottleneck.** If every feature must pass through `EngineController`, it can
   grow into a god-class. Mitigation: split by concern (transport, recording, render, plugins) as
   soon as it exceeds roughly 500 lines.
