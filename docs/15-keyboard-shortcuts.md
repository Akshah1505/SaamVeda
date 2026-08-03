# 15 — Keyboard Shortcuts

**Document version:** 1.0
**Date:** 2026-08-03
**Status:** implemented in Phase 2

---

## 1. Principle

The default map follows **Audacity** wherever Audacity has an equivalent action. Anyone arriving from
Audacity should be able to drive the transport without reading anything. Where Audacity has no
equivalent — a metronome, a tap-tempo panel — the binding follows the wider DAW convention.

Bindings are declared next to the actions they trigger, in `MainComponent::getCommandInfo`
([src/ui/MainComponent.cpp](../src/ui/MainComponent.cpp)), on top of JUCE's
`ApplicationCommandManager`. A shortcut therefore cannot drift from its command, and the in-app
**Keys** button prints this table straight from the live command map rather than from a copy.

## 2. The map

### Transport

| Key | Action | Audacity |
|---|---|:---:|
| `Space` | Play / Stop | same |
| `Shift+Space` | Loop play | same |
| `Enter` | Stop and return to start | — |
| `L` | Enable looping | same |
| `M` | Metronome on/off | — |
| `Home` | Skip to start | same |
| `End` | Skip to end | same |
| `←` / `→` | Seek 1 second | same |
| `Shift+←` / `Shift+→` | Seek 15 seconds | same |

### Edit

| Key | Action | Audacity |
|---|---|:---:|
| `Ctrl+Z` | Undo | same |
| `Ctrl+Shift+Z` | Redo | same |
| `Ctrl+Y` | Redo (alternative) | same |

### Tracks

| Key | Action | Audacity |
|---|---|:---:|
| `Ctrl+Shift+N` | New audio track | same |
| `Ctrl+Shift+I` | Import audio | same |
| *(unbound)* | Remove last track | — |

### View

| Key | Action | Audacity |
|---|---|:---:|
| `Ctrl+1` | Zoom in | same |
| `Ctrl+2` | Zoom normal | same |
| `Ctrl+3` | Zoom out | same |
| `Ctrl+F` | Fit project to window | same |
| `Ctrl+Shift+F` | Follow playhead on/off | — |

### Tools

| Key | Action | Audacity |
|---|---|:---:|
| `T` | Tap tempo panel | — |

### Mouse

| Gesture | Action |
|---|---|
| Click or drag in the lane area | Move the playhead |
| Wheel | Scroll the timeline horizontally |
| `Ctrl`+wheel | Zoom around the pointer |

## 3. Three deliberate departures from Audacity

**`Space` stops without rewinding.** Audacity returns the cursor to where playback started. This
keeps the playhead where it stopped, matching commit `7d167dc` and every arrangement-first DAW.
`Enter` is bound to the rewinding stop for anyone who wants Audacity's behaviour explicitly.

**`Enter` is bound at all.** Audacity leaves it free. Stop-and-return-to-zero is common enough in
arrangement work to deserve a key, and `Enter` is where Pro Tools and Cubase users reach for it.

**Remove Last Track has no key.** It is undoable, but a destructive action one stray keystroke away
is a bad trade in a tool people record into. It stays a button.

## 4. Why the buttons do not take focus

Every transport button is created with `setWantsKeyboardFocus (false)`. Without it, JUCE gives the
clicked button keyboard focus and `Space` re-triggers *that button* instead of the transport — so
after clicking **Stop** once, `Space` would stop forever. Text fields (tempo, time signature, the
audio device selectors) do keep focus, because typing into them requires it; clicking the timeline
or the window background hands focus back.

## 5. Adding a shortcut

1. Add an identifier to `CommandIDs` in [src/app/CommandIDs.h](../src/app/CommandIDs.h).
2. Add it to `MainComponent::getAllCommands`.
3. Describe it in `getCommandInfo`, including `addDefaultKeypress`.
4. Handle it in `MainComponent::perform`.
5. Add the row to this table.

Steps 1–4 are enforced by the compiler and the command manager; step 5 is not, but the in-app
**Keys** dialog reads the live map, so a missed row here shows up as a discrepancy rather than a
silent gap.

## 6. Not yet bound

Reserved for the phases that introduce them, so nothing here gets claimed by a lesser action:
`R` record (Phase 4), `Ctrl+S` save and `Ctrl+O` open (Phase 12), `Ctrl+E` export (Phase 12),
`Ctrl+X/C/V` clip clipboard and `Ctrl+I` split (Phase 6), `Ctrl+A` select all (Phase 6).
