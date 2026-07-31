# SaamVeda Studio

> **Working name — provisional.** The folder name is inherited from an earlier, abandoned project
> direction. See [`docs/01-project-charter.md`](docs/01-project-charter.md) for the naming decision.

A general-purpose Digital Audio Workstation for Windows, built in C++20 on JUCE and
tracktion_engine. Multitrack audio and MIDI recording, non-destructive clip editing, VST3 plugin
hosting, piano roll, mixing, automation, and export.

## Status

**Pre-development.** No code has been written and no toolchain is installed. This repository
currently contains planning and design documentation only.

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

Nothing to build yet. When development begins, start with
[`docs/08-toolchain-setup.md`](docs/08-toolchain-setup.md).
