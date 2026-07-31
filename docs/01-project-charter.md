# 01 — Project Charter

**Document version:** 1.0
**Date:** 2026-07-25
**Status:** Approved direction, pre-development

---

## 1. Background

This project began as *Raga Sur*, a React/PHP/Python web application for Indian classical flute
practice. That direction was abandoned during planning. The original PRD explicitly listed "Full
Digital Audio Workstation (DAW) features" under **Non-Goals** — the current direction inverts that
premise entirely, which is why a complete replacement document set exists rather than an amendment.

The motivating observation was a feature comparison showing that no single free tool covers the
whole workflow:

| Feature | Audacity | LMMS | Ardour |
|---|:---:|:---:|:---:|
| Step-sequencer grid (FL-style) | No | **Yes** | No |
| Piano roll MIDI editing | No | **Yes** | **Yes** |
| Direct microphone recording | **Yes** | No | **Yes** |
| Destructive waveform editing | **Yes** | No | Non-destructive |
| Built-in VST synthesizers | No | **Yes** | FX only |

The initial instinct was to merge the LMMS and Ardour codebases. That was rejected — see §6.

## 2. Vision

A Windows-native DAW that covers both halves of the music-production workflow — **audio recording
and editing** (the Audacity/Ardour strength) and **MIDI sequencing and instruments** (the FL Studio
strength) — in one coherent application, at a quality level suitable for genuine production use.

## 3. Goals

| # | Goal | Measure |
|---|---|---|
| G1 | Usable for real music production by the author | Author records and mixes a complete track in it |
| G2 | Demonstrable as a college project within one semester | Working demo by end of semester (~Nov 2026) |
| G3 | Production-grade audio quality | No dropouts, glitches, or clicks at 128-sample buffer |
| G4 | Host industry-standard plugins | Scans and runs third-party VST3 plugins reliably |
| G5 | Architecture that scales to a large feature set | Feature #50 costs no more than feature #5 |

**G5 is the load-bearing goal.** The stated long-term ambition is full Audacity + FL Studio feature
parity. No architecture decision should be made that trades long-term extensibility for short-term
demo speed.

## 4. Non-Goals

Explicitly out of scope, permanently or for the foreseeable term:

- **Merging or forking existing DAW codebases.** See §6.
- **Linux and macOS releases.** The architecture stays portable, but only Windows is built and tested.
- **Mobile versions.**
- **Cloud collaboration, project sharing, or account systems.**
- **Writing a custom audio engine.** tracktion_engine provides this; re-implementing it is where
  DAW projects die and it is invisible to users.
- **Notation/score engraving.** Deferred to the long-tail backlog.
- **Video editing or scoring-to-picture.**

## 5. Scope reality statement

This section exists to be read before any milestone is judged.

**Full Audacity + FL Studio feature parity is a direction, not a semester milestone.** FL Studio
has roughly 28 years of full-time commercial development behind it; Audacity roughly 25. That gap
does not close in four months, or by one developer in any timeframe.

What is achievable in one semester, on the chosen foundation, is a DAW that genuinely records,
edits, hosts plugins, sequences MIDI, mixes, and exports — usable for real work. The remaining
parity items are tracked in [`04-feature-backlog.md`](04-feature-backlog.md) as post-semester work
and are not counted against the semester deliverable.

Judging the 16-week milestone against "all features" would guarantee a false failure. Judge it
against [`09-roadmap.md`](09-roadmap.md).

## 6. Rejected approach: merging LMMS and Ardour

The original proposal was to combine the two repositories. Rejected for four independent reasons,
any one of which is disqualifying:

1. **No shared foundation.** LMMS is C++/Qt. Ardour is C++ on its own GTK2 fork. Different widget
   toolkits, audio engines, plugin layers, session formats, and data models. There is no seam.
2. **Scale.** Between them, several hundred person-years of accumulated work. Merging is
   effectively rewriting one application inside the other's architecture.
3. **Ardour is not buildable on Windows.** No current Ardour developer builds on Windows; releases
   are cross-compiled from Linux via MinGW. The team advises against attempting it and provides no
   build support. This is decisive on a Windows-only development machine.
4. **The premise was flawed.** Ardour is a multitrack DAW, not an Audacity analogue — it overlaps
   heavily with LMMS rather than complementing it. And the headline gap, "Ardour has no built-in
   synths," is solved by installing a free plugin, not by merging codebases.

## 7. Differentiation — why not just use Zrythm?

**Zrythm** (AGPL-3.0, C++23, Qt/QML + JUCE) already offers piano roll, audio editor, audio and MIDI
recording with takes, and hosts VST3/CLAP/LV2/LADSPA/AU. It is the closest existing analogue and
this project must have an honest answer to it.

The honest answer, stated plainly:

> As a *product*, this project does not currently beat Zrythm or Reaper, and pretending otherwise
> would be dishonest. Its justification is (a) as a **learning and academic artefact** — the author
> builds and understands a complete DAW architecture, which is the actual assessed deliverable; and
> (b) as a **tool shaped to the author's own workflow**, where fitting one user perfectly beats
> fitting everyone adequately.

If a genuine product differentiator is wanted later, the strongest available one is **first-class
Indian classical music support** — raga/sur analysis, swara notation, tanpura drone, and tala
cycles. No mainstream DAW offers this. That capability is recorded in the backlog as a P3
opportunity, carried forward from the abandoned Raga Sur direction. It is not part of the semester
scope.

## 8. Success criteria

**Semester (≈16 weeks):**
- Records multitrack audio and MIDI without dropouts
- Edits clips non-destructively with working undo throughout
- Scans and hosts at least one third-party VST3 plugin
- Piano roll edits MIDI and drives an instrument plugin
- Saves, reloads, and exports a project to WAV
- Author has produced at least one complete piece of music using it

**Long term:**
- Feature backlog advances without core rewrites
- Author uses it in preference to other DAWs for their own work

## 9. Constraints

| Constraint | Detail |
|---|---|
| Timeline | ~4 months to demo (semester ending ~November 2026) |
| Team | Single developer, part-time alongside coursework |
| Platform | Windows 11, 16 cores / 16 GB RAM |
| Toolchain | **Not yet installed** — no CMake, MSVC, or Ninja present |
| Licensing | Undecided; see [`10-licensing-compliance.md`](10-licensing-compliance.md) |

## 10. Open decisions

| # | Decision | Owner | Needed by |
|---|---|---|---|
| D1 | Product name — "SaamVeda" no longer fits a general-purpose DAW | Author | Before public release |
| D2 | AGPLv3 vs planning for commercial JUCE/Tracktion licences | Author | Before first distribution |
| D3 | Whether to pursue the Indian-classical differentiator | Author | Post-semester |

**On D1:** the folder name can stay indefinitely; only the product name matters. Deferring costs
nothing now and a rename later is a find-and-replace.

**On D2:** this decision is genuinely irreversible in one direction. Releasing under AGPLv3 cannot
be undone for code already published. Read [`10-licensing-compliance.md`](10-licensing-compliance.md)
before first release, not after.
