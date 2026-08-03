# 16 — UI Layout

**Document version:** 1.0
**Date:** 2026-08-04
**Status:** implemented in Phase 2

---

## 1. Why this shape

The arrangement follows FL Studio's window structure. Not for imitation's sake: it is the layout the
author already works in, it is the layout the reference screenshot in the project notes was taken
from, and every DAW that arranges audio on a timeline converges on roughly the same regions anyway.
Adopting a known-good arrangement early means the later phases — mixer, piano roll, browser — have
an obvious place to land instead of forcing a re-layout each time.

**Visual styling is deliberately unfinished.** This document fixes *where things are*, not what they
look like. Colours and control styling live in `ui/Layout.h` and are expected to change.

## 2. The bands

```
┌───────────────────────────────────────────────────────────────────────────────┐
│ FILE EDIT ADD VIEW OPTIONS TOOLS HELP │ SONG Play Stop Rec │ tempo │ time sig │ position │ load │
├───────────────────────────────────────────────────────────────────────────────┤
│ ┌───────────────┐                                                             │
│ │ Project       │  Add Track  Import  Remove │ Undo Redo │ Loop Metronome Tap │ RT check │ Keys │
│ │ Untitled      │  status line                                                │
│ └───────────────┘                                                             │
├───────────────────────────────────────────────────────────────────────────────┤
│ Playlist  -  Arrangement  >  <selection>                                       │  title
│ − + Fit  ☑ Follow                                     view 60.0 s   120 BPM 4/4 │  tools
│ ┌────────────┬──────────┬──────────────────────────────────────────────────┬─┐ │
│ │ CLIPS      │          │ 1      3      5      7      9     11     13       │ │ │  ruler
│ │            ├──────────┼──────────────────────────────────────────────────┤ │ │
│ │ clip list  │ Audio 1 ●│ ▓▓▓▓▓▓▓▓ clip                                     │ │ │
│ │            │ Track 2 ○│                                                   │▓│ │  lanes
│ │            │ Track 3 ○│                                                   │ │ │
│ └────────────┴──────────┴──────────────────────────────────────────────────┴─┘ │
│                         └──────────────── horizontal scrollbar ──────────────┘ │
└───────────────────────────────────────────────────────────────────────────────┘
```

| Band | Component | Source |
|---|---|---|
| Menu + transport | `juce::MenuBarComponent`, `TransportBar` | [Toolbars.h](../src/ui/Toolbars.h) |
| Tool row | `ToolBar` | [Toolbars.h](../src/ui/Toolbars.h) |
| Playlist panel | `PlaylistPanel` | [PlaylistPanel.h](../src/ui/PlaylistPanel.h) |
| Clip browser | `ClipBrowser` | [ClipBrowser.h](../src/ui/ClipBrowser.h) |
| Headers, ruler, lanes | `TimelineComponent` | [TimelineComponent.h](../src/ui/TimelineComponent.h) |
| Metrics and colours | `layout::`, `colours::`, `ChromeLookAndFeel` | [Layout.h](../src/ui/Layout.h) |

Every dimension is a named constant in `layout::`. Components read from it rather than hardcoding,
because the rows have to line up across four independent paint routines and they drift otherwise.

## 3. Decisions worth recording

**Audio device setup moved off the main window.** It previously occupied the lower half of the
window permanently. It is a setup step performed once, not a working surface, so it lives behind
**Options → Audio Settings...**. That change alone roughly doubled the arrangement area.

**The lane area always draws at least twelve rows.** An empty project shows `Track 1`…`Track 12` in
grey with dimmed LEDs. A blank rectangle gives the user nothing to aim at; a track sheet does. Rows
backed by a real session track get an accent tag, a live name, and a lit LED.

**The track header column scrolls with the lanes, in the same component.** Splitting them into
sibling components means two scroll states that must be kept in sync, and they will eventually fall
out of sync. `TimelineComponent` owns headers, ruler and lanes together for that reason.

**The hint panel sits top-left of the tool row**, where FL Studio puts it, showing the current
selection above the last action's result. It is the first place the eye lands.

**The `Rec` button is present but disabled.** Recording arrives in Phase 4. Leaving the control in
place means the transport group does not reshuffle when it starts working.

## 4. What is not there yet

Reserved regions, so later phases do not have to re-cut the layout:

| Phase | Feature | Where it goes |
|:---:|---|---|
| 3 | Waveform thumbnails | inside the clip body, replacing the placeholder centre line |
| 3 | Per-track gain, pan, mute, solo | track header, right of the name |
| 6 | Edit tools (draw, slice, select) | playlist tool strip, left of the zoom controls |
| 9 | Piano roll | its own panel, peer of the Playlist panel |
| 10 | Pattern picker | second row, right of the transport |
| 11 | Mixer | its own panel, peer of the Playlist panel |

## 5. Keyboard

The layout is fully keyboard-driven; see [15-keyboard-shortcuts.md](15-keyboard-shortcuts.md).
Note that toolbar buttons are created with `setWantsKeyboardFocus (false)` — without it, `Space`
re-triggers the last-clicked button instead of the transport.
