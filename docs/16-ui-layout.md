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
│ │ CLIPS      │          │ ║▓▓▓▓▓▓▓▓▓▓▓▓║                                    │ │ │  zoom bar
│ │            │          │ 1      3      5      7      9     11     13       │ │ │  ruler
│ │            ├──────────┼──────────────────────────────────────────────────┤ │ │
│ │ clip list  │ Audio 1 ●│ ▓▓▓▓▓▓▓▓ clip                                     │ │ │
│ │            │ Track 2 ○│                                                   │▓│ │  lanes
│ │            │ Track 3 ○│                                                   │ │ │
│ └────────────┴──────────┴──────────────────────────────────────────────────┴─┘ │
└───────────────────────────────────────────────────────────────────────────────┘
```

| Band | Component | Source |
|---|---|---|
| Menu + transport | `juce::MenuBarComponent`, `TransportBar` | [Toolbars.h](../src/ui/Toolbars.h) |
| Tool row | `ToolBar` | [Toolbars.h](../src/ui/Toolbars.h) |
| Playlist panel | `PlaylistPanel` | [PlaylistPanel.h](../src/ui/PlaylistPanel.h) |
| Clip browser | `ClipBrowser` | [ClipBrowser.h](../src/ui/ClipBrowser.h) |
| Headers, ruler, lanes | `TimelineComponent` | [TimelineComponent.h](../src/ui/TimelineComponent.h) |
| Scroll + zoom bar | `ZoomScrollBar` | [ZoomScrollBar.h](../src/ui/ZoomScrollBar.h) |
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

**The horizontal scrollbar is above the ruler, and it is also the zoom control.**
The thumb is the visible slice of the project: drag its middle to scroll, drag either end to
resize the slice, which is zooming. Combining them is not a space saving — "where am I" and "how
much am I looking at" are the same question, and answering it with two separate widgets makes the
user reconcile them. Both ends carry grip marks and a resize cursor so the thumb reads as
resizable rather than as a plain scrollbar. Double-click fits the project. The `−`/`+`/`Fit`
buttons and `Ctrl+1`/`2`/`3`/`F` remain, driving the same state, so the bar always reflects the
current view however it was changed. There is no bottom scrollbar; the vertical one stays on the
right.

**Waveforms draw only the slice that is on screen.** `paintClip` intersects the clip with the lane
area and converts those two pixel columns back to source time, so `AudioThumbnail::drawChannels`
renders at the resolution it is being displayed at. Handing it the whole clip and letting it squeeze
into a narrow rectangle throws away the detail the thumbnail was built for. The conversion applies
the clip's speed ratio, so a stretched clip still lines its waveform up with its audio.

**The track header's LED is the mute button.** It is where FL Studio puts its
track LED, so it is where the hand goes. The dot itself is 10px, but the click target is 22px
square — an 8px target is a miss waiting to happen. Lit green means the track sounds; a hollow red
ring means muted, and the track name and its clips desaturate to match. Off has to be readable at a
glance down twelve rows, which a merely dimmer dot is not. Painting and hit-testing share one
`muteButtonBounds()` so the dot and the region that responds cannot drift apart.

**The track header column scrolls with the lanes, in the same component.** Splitting them into
sibling components means two scroll states that must be kept in sync, and they will eventually fall
out of sync. `TimelineComponent` owns headers, ruler and lanes together for that reason.

**The hint panel sits top-left of the tool row**, where FL Studio puts it, showing the current
selection above the last action's result. It is the first place the eye lands.

**The transport carries an input level meter.** It sits beside `Rec`, because the question it
answers - "is signal arriving?" - is the one asked immediately before pressing record. It is in
the group that never gets dropped when the window narrows, for the same reason.

**Each track header carries R, S and the mute LED**, in that order, right of the name. Record-arm
is leftmost because it is the least-used of the three and the most costly to hit by accident.

## 4. What is not there yet

Reserved regions, so later phases do not have to re-cut the layout:

| Phase | Feature | Where it goes |
|:---:|---|---|
| 3 | Per-track gain and pan | track header, left of the solo button |
| 3 | Track reorder and colour | header drag handle; colour tag opens a picker |
| 6 | Edit tools (draw, slice, select) | playlist tool strip, left of the zoom controls |
| 9 | Piano roll | its own panel, peer of the Playlist panel |
| 10 | Pattern picker | second row, right of the transport |
| 11 | Mixer | its own panel, peer of the Playlist panel |

## 5. Keyboard

The layout is fully keyboard-driven; see [15-keyboard-shortcuts.md](15-keyboard-shortcuts.md).
Note that toolbar buttons are created with `setWantsKeyboardFocus (false)` — without it, `Space`
re-triggers the last-clicked button instead of the transport.
