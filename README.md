# SaamVeda Studio

> **Working name — provisional.** The folder name is inherited from an earlier, abandoned project
> direction. See [`docs/01-project-charter.md`](docs/01-project-charter.md) for the naming decision.

A general-purpose Digital Audio Workstation for Windows, built in C++20 on JUCE and
tracktion_engine. Multitrack audio and MIDI recording, non-destructive clip editing, VST3 plugin
hosting, piano roll, mixing, automation, and export.

## Status

**Phase 2 of 13 complete** — see the [roadmap](docs/09-roadmap.md).

The application builds and runs. It opens an audio device, plays audio, imports files onto tracks,
detects their tempo, and drives a zoomable timeline with a synchronised playhead. Session state
lives in a `ValueTree` behind a command bus, so every structural edit — tracks, clips, tempo, time
signature — is undoable. Keyboard control follows Audacity
([document 15](docs/15-keyboard-shortcuts.md)); the window follows FL Studio's arrangement
([document 16](docs/16-ui-layout.md)).

Not yet built: recording, waveform display, clip editing, plugin hosting, piano roll, mixing,
save/load, export. Those are Phases 3–12.

## Documentation

| # | Document | Purpose |
|---|---|---|
| 01 | [Project Charter](docs/01-project-charter.md) | Vision, goals, non-goals, differentiation |
| 02 | [PRD](docs/02-prd.md) | Product requirements with acceptance criteria |
| 03 | [SRS](docs/03-srs.md) | Formal software requirements specification |
| 04 | [Feature Backlog](docs/04-feature-backlog.md) | Audacity + FL Studio parity matrix, prioritized |
| 05 | [Architecture](docs/05-architecture.md) | Layers, modules, threading model |
| 06 | [Data Model](docs/06-data-model.md) | Session schema and project file format |
| 07 | [Tech Stack](docs/07-tech-stack.md) | Framework selection and rationale |
| 08 | [Toolchain Setup](docs/08-toolchain-setup.md) | Build environment (documented, not yet executed) |
| 09 | [Roadmap](docs/09-roadmap.md) | 16-week phased delivery plan |
| 10 | [Licensing & Compliance](docs/10-licensing-compliance.md) | AGPLv3 vs commercial; GPL study rules |
| 11 | [Testing Strategy](docs/11-testing-strategy.md) | Unit, null-test, and QA approach |
| 12 | [Risk Register](docs/12-risk-register.md) | Identified risks and mitigations |
| 13 | [Synopsis](docs/13-synopsis.md) | College submission abstract |
| 14 | [Diagrams](docs/14-diagrams.md) | Use-case, DFD, class, sequence (Mermaid) |
| 15 | [Keyboard Shortcuts](docs/15-keyboard-shortcuts.md) | Audacity-compatible key map |
| 16 | [UI Layout](docs/16-ui-layout.md) | Window structure and reserved regions |

## Quick facts

- **Language:** C++20
- **Frameworks:** JUCE 8 + tracktion_engine
- **Build:** CMake + MSVC (Visual Studio 2022)
- **Target platform:** Windows 11 (architecture kept cross-platform-capable)
- **Licence:** Undecided — see [document 10](docs/10-licensing-compliance.md). This choice is
  consequential and should be settled before first public release.

## Important constraints

**This project reads open-source DAWs for architectural reference but does not copy their code.**
LMMS and Ardour are GPL-2.0; incorporating their source would make this project GPL-2.0 in its
entirety and irreversibly foreclose a proprietary release. See
[document 10](docs/10-licensing-compliance.md) before reading any reference codebase.

## Getting started

Install the toolchain per [`docs/08-toolchain-setup.md`](docs/08-toolchain-setup.md), then:

```bash
git submodule update --init --recursive
```

If that fails with `Permission denied (publickey)`, tracktion_engine's nested JUCE submodule is
using an SSH URL. Redirect it to HTTPS and retry — see
[Step 4](docs/08-toolchain-setup.md#step-4--repository-and-submodules).

```bash
powershell -File tools\build.ps1 build
```

The script locates CMake, Ninja, and MSVC itself; override with `SAAMVEDA_CMAKE`, `SAAMVEDA_NINJA`,
or `SAAMVEDA_VCVARS` if it guesses wrong. Then:

```bash
powershell -File tools\build.ps1 test
```

```bash
powershell -File tools\build.ps1 run
```
