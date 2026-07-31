# 06 — Data Model & Project File Format

**Document version:** 1.0
**Date:** 2026-07-25

---

## 1. Design basis

All session state lives in a single `juce::ValueTree` hierarchy paired with `juce::UndoManager`.

This is the decision that makes the 196-feature backlog tractable. A ValueTree provides change
notification, serialisation, and undo **uniformly and for free**. Any feature that stores its state
in the tree inherits save/load and undo without a line of code written for either. That is the
flat-marginal-cost property demanded by [`05-architecture.md`](05-architecture.md) §1.

It is also tracktion_engine's native idiom, so the two compose rather than fight.

### Consequences

- **Undo is structural.** A feature that stores state outside the tree silently breaks undo — this
  is the single most important invariant in the codebase.
- **Save/load is nearly free.** Serialisation is a tree walk.
- **Change notification is built in.** UI components observe the tree; no manual observer wiring.

## 2. Session hierarchy

```
SESSION
├─ @version, @name, @tempo, @timeSigNum, @timeSigDenom
├─ @sampleRate, @created, @modified
│
├─ TRACKS
│  ├─ TRACK  @id @type(audio|midi) @name @colour @height
│  │         @gain @pan @mute @solo @armed @inputDevice
│  │  ├─ CLIPS
│  │  │  └─ CLIP  @id @name @start @length @offset
│  │  │           @gain @fadeIn @fadeOut @fadeInCurve @fadeOutCurve
│  │  │           @sourceFile        (audio clips)
│  │  │           @patternRef        (midi clips)
│  │  │           └─ NOTES           (midi clips)
│  │  │              └─ NOTE @start @length @pitch @velocity @channel
│  │  ├─ PLUGIN_CHAIN
│  │  │  └─ PLUGIN @id @uid @format @name @bypassed @state(base64)
│  │  └─ AUTOMATION
│  │     └─ LANE @parameterId @visible
│  │        └─ POINT @time @value @curve
│  └─ …
│
├─ PATTERNS                          (step sequencer / reusable MIDI)
│  └─ PATTERN @id @name @length @steps
│     └─ NOTES → NOTE …
│
├─ MIXER
│  ├─ MIXER_TRACK @id @trackRef @gain @pan @mute @solo
│  │  ├─ PLUGIN_CHAIN → PLUGIN …
│  │  └─ SENDS
│  │     └─ SEND @destination @level @preFader
│  └─ MASTER
│     └─ PLUGIN_CHAIN → PLUGIN …
│
├─ ARRANGEMENT
│  ├─ MARKERS   → MARKER @time @name @colour
│  ├─ TEMPO_MAP → TEMPO_POINT @time @bpm @curve
│  └─ TIME_SIGS → TIME_SIG @bar @numerator @denominator
│
└─ VIEW_STATE                        (excluded from undo — see §6)
   @zoomLevel @scrollX @selectedTrack @editorMode
```

## 3. Type conventions

| Concept | Type | Unit | Rationale |
|---|---|---|---|
| Time positions | `double` | seconds | Sample-rate independent; survives rate changes |
| Musical positions | `double` | quarter notes | For tempo-relative content |
| Gain | `float` | linear (not dB) | Avoids repeated conversion in the audio path |
| Pan | `float` | −1.0 to +1.0 | Convention |
| Pitch | `int` | MIDI note 0–127 | Convention |
| Velocity | `int` | 1–127 | Convention |
| IDs | `juce::String` | UUID | Stable across save/load and copy/paste |
| Plugin state | `juce::String` | base64 | Opaque blob owned by the plugin |
| Colour | `juce::String` | ARGB hex | Human-readable in the file |

**Gain is stored linear, displayed in dB.** Storing dB would force a conversion on every audio
callback — a needless cost in the hot path.

**Time is stored in seconds, not samples.** A project opened at a different sample rate must not
shift in time.

## 4. Identity and references

Every entity carries a UUID `@id`. Cross-references (`@trackRef`, `@patternRef`, send
`@destination`) use these, never array indices.

**Why:** indices break the moment anything is reordered, inserted, or deleted — and reordering
tracks is an everyday operation. Index-based references are a bug generator, and undo makes them
worse.

Referential integrity is validated on load; dangling references are reported and repaired rather
than crashing.

## 5. Project file format

**Container:** a single file with a `.zip` structure, so audio can travel with the project.

```
project.rsproj  (zip)
├─ session.xml          ValueTree serialised as XML
├─ meta.json            version, app version, created/modified
└─ media/               recorded audio (imported files are referenced by
   ├─ rec_001.wav       path unless "collect media into project" is chosen)
   └─ …
```

**XML over binary.** Human-readable, diffable, and recoverable by hand if the application has a
bug — worth more than the space saved during the years when the application is least trustworthy.
`ValueTree` supports binary serialisation, so switching later is a late-binding decision
(see [`05-architecture.md`](05-architecture.md) §11).

**Media handling:** recorded audio always lives in `media/`. Imported audio is referenced by
absolute path by default, with an explicit "collect media into project" action for portability.

### Schema versioning

`SESSION @version` is an integer, incremented on every breaking change. Load applies migrations in
sequence. Migration functions live in `core/schema/Migrations.cpp` and are covered by tests using
saved fixture projects from earlier versions.

**Never rename or repurpose an attribute in place.** Add a new one and migrate. Old project files
must keep opening — a DAW that loses a user's work destroys trust permanently.

## 6. Undo boundaries

Not every change belongs in the undo history. Scrolling the timeline should not be undoable.

| Undoable | Not undoable |
|---|---|
| Clip and note edits | Zoom and scroll position |
| Track add/remove/reorder | Selection changes |
| Parameter and automation changes | Solo/mute *(under review — see below)* |
| Plugin add/remove | Transport position |
| Mixer routing | Window layout, panel sizes |

`VIEW_STATE` is a separate subtree, mutated **without** the `UndoManager`. It is saved with the
project but excluded from history.

> **Open question:** whether mute and solo should be undoable. FL Studio and Ardour differ. Deferred
> to Phase 13 when the mixer is built; the model supports either.

## 7. Audio-thread access

The audio thread **never reads the ValueTree.** Change notification, string handling, and reference
counting all violate realtime safety (NFR6).

Instead: the message thread observes the tree and pushes plain-old-data snapshots to the audio
thread through a lock-free FIFO.

```
ValueTree (message thread) → change listener → POD snapshot → lock-free FIFO → audio thread
```

Where tracktion_engine already owns a parameter (plugin values, transport position), its own
realtime-safe mechanism is used rather than duplicating one.

## 8. Worked example

A two-track session — recorded guitar with reverb, and a MIDI bass line:

```xml
<SESSION version="1" name="Demo" tempo="120" timeSigNum="4" timeSigDenom="4"
         sampleRate="48000">
  <TRACKS>
    <TRACK id="t-a1b2" type="audio" name="Guitar" colour="ff4a90d9"
           gain="0.891" pan="-0.2" mute="0" solo="0">
      <CLIPS>
        <CLIP id="c-9f3e" name="Take 1" start="4.0" length="16.0" offset="0.0"
              gain="1.0" fadeIn="0.01" fadeOut="0.05"
              sourceFile="media/rec_001.wav"/>
      </CLIPS>
      <PLUGIN_CHAIN>
        <PLUGIN id="p-77c1" uid="ValhallaRoom" format="VST3"
                name="Room Reverb" bypassed="0" state="eJyLrjYw..."/>
      </PLUGIN_CHAIN>
      <AUTOMATION>
        <LANE parameterId="gain" visible="1">
          <POINT time="4.0"  value="0.0"   curve="linear"/>
          <POINT time="6.0"  value="0.891" curve="linear"/>
          <POINT time="18.0" value="0.891" curve="linear"/>
          <POINT time="20.0" value="0.0"   curve="exponential"/>
        </LANE>
      </AUTOMATION>
    </TRACK>

    <TRACK id="t-c3d4" type="midi" name="Bass" colour="ffd94a4a"
           gain="1.0" pan="0.0" mute="0" solo="0">
      <CLIPS>
        <CLIP id="c-4b8a" name="Bassline" start="0.0" length="8.0" offset="0.0">
          <NOTES>
            <NOTE start="0.0" length="0.45" pitch="40" velocity="100" channel="1"/>
            <NOTE start="0.5" length="0.45" pitch="40" velocity="88"  channel="1"/>
            <NOTE start="1.0" length="0.95" pitch="43" velocity="105" channel="1"/>
          </NOTES>
        </CLIP>
      </CLIPS>
      <PLUGIN_CHAIN>
        <PLUGIN id="p-2e5f" uid="Surge XT" format="VST3" name="Surge XT"
                bypassed="0" state="eJxdU8tu..."/>
      </PLUGIN_CHAIN>
    </TRACK>
  </TRACKS>

  <MIXER>
    <MIXER_TRACK id="m-01" trackRef="t-a1b2" gain="0.891" pan="-0.2"/>
    <MIXER_TRACK id="m-02" trackRef="t-c3d4" gain="1.0" pan="0.0">
      <SENDS><SEND destination="m-rev" level="0.25" preFader="0"/></SENDS>
    </MIXER_TRACK>
    <MASTER><PLUGIN_CHAIN/></MASTER>
  </MIXER>

  <ARRANGEMENT>
    <MARKERS><MARKER time="0.0" name="Intro"/><MARKER time="16.0" name="Verse"/></MARKERS>
    <TEMPO_MAP><TEMPO_POINT time="0.0" bpm="120.0"/></TEMPO_MAP>
  </ARRANGEMENT>

  <VIEW_STATE zoomLevel="1.0" scrollX="0.0" selectedTrack="t-a1b2"/>
</SESSION>
```

## 9. Validation rules

Enforced on load; violations are repaired and logged rather than fatal.

| Rule | On violation |
|---|---|
| All `@id` values unique | Regenerate the duplicate |
| All references resolve | Drop the reference, warn |
| `@length` > 0 | Drop the clip, warn |
| `@start` ≥ 0 | Clamp to 0 |
| `@offset` within source bounds | Clamp |
| Automation points ordered by time | Sort |
| `@pitch` and `@velocity` in range | Clamp |
| Missing media file | Keep the clip, mark offline, prompt to relink |

**Missing media never deletes a clip.** The user must be able to relink and recover — silently
discarding their work is the worst possible failure mode.

## 10. Extension points

Adding a feature should mean adding to this schema, not changing it:

| To add… | Do this |
|---|---|
| A new clip property | New attribute + a migration |
| A new track type | New `@type` value |
| A new automatable parameter | New `LANE @parameterId` — no schema change |
| Per-clip effects | `PLUGIN_CHAIN` under `CLIP` — same element |
| Raga/swara metadata (C1.x) | New optional subtree under `SESSION` |
