# 09 — Roadmap

**Document version:** 1.0
**Date:** 2026-07-25
**Milestone target:** ~16 weeks from development start (semester ending ~November 2026)

---

## 1. Principles

1. **Every phase ends in something runnable.** No phase produces only internal scaffolding.
2. **Usable, not just demoable.** The author should be able to make real music with it from Phase 5
   onward. This is why the engine layer is bought rather than built.
3. **Risk first.** The scariest things — toolchain, audio stability, plugin hosting — come early,
   while there is time to recover.
4. **Nothing is cut, only sequenced.** Features not in these 16 weeks are in
   [`04-feature-backlog.md`](04-feature-backlog.md) with a priority, not deleted.

## 2. What "done" means at week 16

A DAW that records multitrack audio and MIDI, edits clips with full undo, hosts VST3 plugins, edits
MIDI in a piano roll, mixes with automation, saves and reloads, and exports to standard formats.

**The 93 P0/P1 features.** Not the 196. Judging week 16 against full Audacity + FL Studio parity
would be judging it against roughly 50 combined years of commercial development — see
[`01-project-charter.md`](01-project-charter.md) §5.

---

## 3. Phases

### Phase 1 — Foundation `Week 1`

Toolchain and a proving skeleton.

- Install Visual Studio 2022 (Desktop C++ workload), CMake, Ninja *(see [`08-toolchain-setup.md`](08-toolchain-setup.md))*
- `git init`; add JUCE and tracktion_engine as pinned submodules
- CMake project building a JUCE GUI application
- Window opens; audio device settings panel; play a WAV end to end
- Catch2 wired up with one passing test

> **Demo:** application opens, selects an audio device, plays a file.
> **Proves:** the entire toolchain works on this machine — the largest unknown, since nothing is
> currently installed.

**Exit criteria:** clean build from scratch; audio audible; test suite runs.

---

### Phase 2 — Session model & transport `Weeks 2–3`

- `core/` session ValueTree schema ([`06-data-model.md`](06-data-model.md))
- Command bus and undo infrastructure
- `engine/EngineController` wrapping tracktion_engine
- Transport: play, stop, loop, tempo, time signature
- Timeline with ruler, playhead, zoom, scroll
- Debug-build audio-thread allocation detector

> **Demo:** timeline with a moving playhead; transport responds; tempo editable.
> **Proves:** the state/command/undo foundation everything else depends on.

**Exit criteria:** SRS-1.1–1.7; undo works on a trivial operation; playhead sample-accurate.

---

### Phase 3 — Tracks & waveforms `Week 4`

- Track create, delete, rename, reorder, colour
- Per-track gain, pan, mute, solo
- Audio file import
- Background waveform thumbnail generation with multi-resolution caching
- Clips render on the timeline

> **Demo:** import audio onto several tracks; see waveforms; mute/solo; play back.
> **Proves:** the UI/state separation holds under real data.

**Exit criteria:** SRS-2.1–2.5; 10-minute file displays within 2 s (SRS-NF1.5); 32 tracks play
without dropout.

---

### Phase 4 — Audio recording `Weeks 5–6`

- Input selection and track arming
- Input monitoring with level metering
- Record to disk while playing back existing material
- Latency compensation
- Crash-safe recording (data survives termination)
- Metronome and count-in

> **Demo:** record a live instrument over an existing track; play both back in sync.
> **Proves:** the core value proposition, and the first point at which the application is genuinely
> useful for real work.

**Exit criteria:** SRS-3.1–3.6; alignment within ±1 ms; 30-minute continuous recording; a killed
process loses no audio.

---

### Phase 5 — MIDI recording `Week 6`

- MIDI device input, MIDI track type, MIDI clips
- Record MIDI with correct timing
- Basic note display

> **Demo:** record from a MIDI keyboard; see notes on the timeline.
> **Exit criteria:** SRS-3.5; timing within ±5 ms.

---

### Phase 6 — Clip editing `Weeks 7–8`

- Move, trim, split, duplicate, delete
- Fades with selectable curves; crossfades
- Snap to grid; clip gain
- Undo history panel
- **Destructive render** of a clip or selection (the Audacity workflow)

> **Demo:** arrange a piece by cutting and trimming takes; undo 50 operations back.
> **Proves:** the non-destructive model with a destructive escape hatch — a differentiator over both
> reference tools.

**Exit criteria:** SRS-4.1–4.6; 100 operations undo cleanly; split is sample-identical; rendered
output null-tests against playback (SRS-NF3.1).

---

### Phase 7 — Plugin hosting `Weeks 9–10`

- Out-of-process VST3 scanning with disk cache
- Plugin database and browser
- Per-track effect chains; native plugin editor windows
- Plugin delay compensation
- Plugin state persisted into the session

> **Demo:** load a third-party VST3 reverb onto a recorded track; tweak it; hear it.
> **Proves:** the highest-risk integration. A DAW that cannot host plugins is not a DAW.

**Exit criteria:** SRS-6.1–6.8; 50-plugin folder scans without hanging; a plugin that crashes during
scan does not take the host with it; PDC verified by null-test.

---

### Phase 8 — Built-in effects `Week 10`

- Parametric EQ with spectrum display
- Compressor, limiter, gate
- Reverb, delay
- Click-free bypass; preset save/load

> **Demo:** mix a recording using only built-in effects.
> **Exit criteria:** SRS-8.1–8.4, 8.7; no artefacts at a 128-sample buffer.

---

### Phase 9 — Piano roll `Weeks 11–12`

- Draw, move, resize, delete notes; paint entry
- Velocity editing; quantize with strength
- Scale highlighting
- VST3 **instrument** hosting on MIDI tracks

> **Demo:** write a bassline in the piano roll driving a VST3 synth.
> **Proves:** the FL Studio half of the product.

**Exit criteria:** SRS-5.1–5.5, 6.5; notes grid-align within 1 tick; all edits undoable.

---

### Phase 10 — Patterns & step sequencer `Week 12` `P1`

- Pattern entity; step sequencer grid
- Pattern clips on the timeline; pattern/song mode

> **Demo:** build a drum pattern on the step grid and arrange it.
> **Note:** first candidate to slip if earlier phases overrun. It is P1, not P0.

---

### Phase 11 — Mixer & automation `Weeks 13–14`

- Mixer view: faders, pan, mute/solo, peak+RMS meters
- Send/return buses; master track
- Automation lanes for volume, pan, and plugin parameters
- Automation curve types; MIDI learn `P1`

> **Demo:** mix a multitrack project with automated fades and a reverb send.
> **Exit criteria:** SRS-7.1–7.6; meters accurate within 0.1 dB; automation sample-accurate and
> identical on export.

---

### Phase 12 — Persistence & export `Week 15`

- Project save/load (`.rsproj`); schema migration framework
- Autosave and crash recovery
- Export WAV, FLAC, MP3, OGG; region export; stems `P1`

> **Demo:** save a project, restart, reload, export a finished mix.
> **Exit criteria:** SRS-9.1–9.5; reloaded project renders null-identical; save under 3 s for 50
> tracks; export faster than real time.

---

### Phase 13 — Stabilisation & demo `Week 16`

- Bug fixing against the phase exit criteria
- Keyboard shortcuts; UI polish
- **Produce a complete piece of music in the application** (success metric G1)
- Presentation materials; recorded backup demo video

> **Demo:** the full workflow, record to export, in one unbroken session.

**A recorded backup demo is not optional.** Live audio demonstrations fail for reasons outside your
control — driver changes, a different room, borrowed hardware. Record it in week 15.

---

## 4. Schedule summary

| Weeks | Phase | Outcome |
|:---:|---|---|
| 1 | Foundation | Toolchain proven; audio plays |
| 2–3 | Session & transport | Timeline, undo, playhead |
| 4 | Tracks & waveforms | Import, display, mix basics |
| 5–6 | Audio recording | **Genuinely usable** |
| 6 | MIDI recording | MIDI capture |
| 7–8 | Clip editing | Full editing + undo |
| 9–10 | Plugin hosting | VST3 effects |
| 10 | Built-in effects | Mix without third-party plugins |
| 11–12 | Piano roll | MIDI composition |
| 12 | Patterns `P1` | Step sequencing |
| 13–14 | Mixer & automation | Full mixing |
| 15 | Persistence & export | Save and ship a track |
| 16 | Stabilisation | Demo-ready |

## 5. Post-semester

Ordered by value, from [`04-feature-backlog.md`](04-feature-backlog.md):

| Order | Work | Backlog |
|:---:|---|---|
| 1 | Out-of-process plugin sandboxing | B7.9 |
| 2 | Time-stretch, pitch-shift, warping | B6.1–B6.3 |
| 3 | Native instrument suite | B8.1–B8.11 |
| 4 | Advanced piano roll tools | B2.4–B2.13 |
| 5 | Audacity analysis and restoration effects | A5.10–A5.25, A7.x |
| 6 | Dedicated destructive audio editor | B6.5 |
| 7 | VST2, CLAP, LV2 hosting | B7.6–B7.8 |
| 8 | Themes, templates, browser | B11.x |
| 9 | Indian classical differentiator | C1.x |

## 6. Slip plan

If the schedule slips, cut in this order. Decide by week 10 — a decision made late is a decision
made badly.

| Cut order | What | Why it is safe |
|:---:|---|---|
| 1 | Phase 10 patterns/step sequencer | P1; piano roll already covers MIDI |
| 2 | Built-in effects beyond EQ + compressor | VST3 hosting covers the capability |
| 3 | MP3/OGG export | WAV export satisfies the requirement |
| 4 | MIDI learn, automation curve types | Straight-line automation still works |
| 5 | Scale highlighting | Convenience only |

**Never cut:** recording, editing, undo, plugin hosting, save/load, WAV export. Those five are what
make it a DAW rather than a demo.

## 7. Assumptions

- Development starts immediately after toolchain installation
- Roughly 15–20 hours per week alongside coursework
- No hardware failure or driver incompatibility
- The Application Control policy on the development machine does not block the toolchain — **this
  is unverified and is the largest single schedule risk** (see [`12-risk-register.md`](12-risk-register.md))
