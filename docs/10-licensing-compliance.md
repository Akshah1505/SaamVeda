# 10 — Licensing & Compliance

**Document version:** 1.0
**Date:** 2026-07-25
**Status:** ⚠️ **Decision D2 is open and must be made before first distribution.**

> This document is written by a software engineer, not a lawyer. It reflects verified public
> licence terms and is sufficient for planning. Before commercial distribution, obtain professional
> legal advice.

---

## 1. Why this document exists early

Licensing is normally deferred as paperwork. Here it cannot be, for one reason:

> **Publishing source under a copyleft licence cannot be undone.** Code released under AGPLv3 stays
> available under AGPLv3 to everyone who received it. If a proprietary or commercial release is ever
> wanted, that decision must be made *before* the first public release, not after.

The project brief included "production level," which may or may not mean commercial. Until that is
settled, the safest posture is the one that preserves both options — see §6.

## 2. The two GPL exposures

There are two entirely separate ways this project could become obligated to copyleft. They are
often confused; keep them distinct.

| # | Exposure | Trigger | Under our control? |
|:---:|---|---|---|
| **1** | **Reference codebases** (LMMS, Ardour, Audacity, Zrythm) | Copying their source into ours | **Yes** — simply never copy |
| **2** | **Framework licence** (JUCE, tracktion_engine) | Using them under their free licence | **Yes** — choose free or paid |

Exposure 1 is an engineering discipline problem. Exposure 2 is a budget decision.

## 3. Exposure 1 — the reference codebases

| Project | Licence |
|---|---|
| LMMS | GPL-2.0 |
| Ardour | GPL-2.0-or-later |
| Audacity | GPL-2.0-or-later |
| Zrythm | AGPL-3.0 |

### What GPL-2.0 requires

A derivative work incorporating GPL-2.0 code must, **in its entirety**, be licensed under GPL-2.0,
with source made available to anyone who receives a binary. Any work that is distributed and which
in whole or in part includes or derives from the GPL programme must be licensed as a whole, free of
charge, to all third parties.

**Consequence for us:** copying even a modest amount of LMMS or Ardour source would place the entire
application under GPL-2.0, permanently.

### The line between study and copying

There is a real and well-established distinction:

| ✅ Permitted | ❌ Not permitted |
|---|---|
| Reading the source to understand an approach | Copy-pasting functions or classes |
| Learning an algorithm's structure and re-implementing independently | Translating their code line-by-line into our style |
| Studying UI layout and interaction patterns | Copying resource files, icons, or presets |
| Reading documentation and design discussion | Copying distinctive comments or identifier schemes |
| Noting *which* DSP technique is used | Copying their coefficient tables or tuned constants |

Where a work is separate and independent from GPL code with which it could be combined, copyleft
obligations do not extend to it. Independence is the thing to protect.

### Working rules

1. **Never copy-paste from a GPL codebase into this project.** Not as a placeholder, not "to fix
   later." Placeholder code has a way of shipping.
2. **Prefer neutral sources for algorithms** — textbooks, papers, public-domain references — over
   reading a GPL implementation, wherever a choice exists.
3. **When a reference codebase has been consulted for a component, note it in the commit message.**
   Provenance is cheap to record and expensive to reconstruct.
4. **Do not copy proprietary designs either.** FL Studio's plugins (Sytrus, Harmor, Edison, Gross
   Beat, and the rest) are trademarked commercial products. We build functional equivalents with our
   own names and our own DSP — never clones, and never their names. See
   [`04-feature-backlog.md`](04-feature-backlog.md).

## 4. Exposure 2 — JUCE and tracktion_engine

### JUCE

Dual-licensed: **AGPLv3** or a **commercial subscription**.

- Under AGPLv3: free to use, but software you convey must itself be AGPLv3 with source available.
- Not using AGPLv3 requires a JUCE licence, maintained for at least as long as you distribute
  closed-source binaries containing JUCE.
- An **Educational licence** exists for universities, schools, and colleges, but may not be used for
  commercial, professional, promotional, or any other for-profit activity.
- Commercial licensing is subscription-based (monthly); perpetual licences are not offered.

> AGPLv3 has a network clause: if users interact with the software over a network, source must be
> offered to them. For a desktop DAW this is unlikely to bite — but it would if a networked or
> collaborative feature were ever added.

### tracktion_engine

Dual-licensed: **GPL** or **commercial**, with multiple commercial tiers.

Critically: distributing a product built on it requires licences for **both JUCE and Tracktion
Engine** separately. Budget accordingly — this is a common and expensive surprise.

## 5. The three viable paths

| | **A — Open source** | **B — Commercial** | **C — Academic only** |
|---|---|---|---|
| Licence | AGPLv3 | Paid JUCE + Tracktion | Educational |
| Cost | Free | Two subscriptions | Free |
| Source disclosure | Required | Not required | N/A (not distributed) |
| Can sell it | Effectively no | Yes | **No** |
| Reversible | **No** | Yes | Yes — decide later |
| Fits "production level" | If that means quality | If that means commercial | Only for coursework |

## 6. Recommendation

**Adopt Path C now, and defer the choice between A and B.**

Rationale: while the project is unreleased coursework, no distribution occurs, so no licence
obligation is triggered and **both A and B remain open**. Choosing early gains nothing and could
foreclose an option irreversibly.

What this requires in the meantime:

- Do not publish the source or binaries publicly until D2 is decided
- Keep the repository private
- Follow §3's rules absolutely, so the *reference* exposure never becomes the deciding factor
- Confirm the educational licence terms cover a graded college project

**When D2 must be answered:** before the first public release, before any paid distribution, and
before open-sourcing the repository. Not before then.

## 7. Third-party components

| Component | Licence | Notes |
|---|---|---|
| JUCE | AGPLv3 / commercial | See §4 |
| tracktion_engine | GPL / commercial | Separate licence from JUCE |
| Catch2 | BSL-1.0 | Permissive; test-only |
| VST3 SDK | Proprietary (Steinberg) / GPLv3 | Dual-licensed; terms apply to hosting |
| ASIO SDK | Proprietary (Steinberg) | Redistribution restrictions — see [`07-tech-stack.md`](07-tech-stack.md) Q3 |
| MP3 encoding | Patents expired; library terms vary | Confirm the chosen library's licence — [`07-tech-stack.md`](07-tech-stack.md) Q4 |
| FLAC / Ogg Vorbis | BSD-like | Permissive |

**The VST3 and ASIO SDKs carry their own terms** independent of JUCE's. Read them before shipping
ASIO support in particular.

## 8. Attribution obligations

Regardless of the path chosen, the application must include an "About / Licences" dialog listing
every third-party component, its licence, and required notices. This is a requirement of most of the
licences above, not a courtesy.

Add this in Phase 12 alongside project persistence, so it is never forgotten at release time.

## 9. Compliance checklist

Before **any** public release:

- [ ] D2 decided and recorded in [`01-project-charter.md`](01-project-charter.md)
- [ ] No GPL-derived source in the codebase — verified by review
- [ ] No trademarked names from FL Studio, Audacity, or others used for our features
- [ ] Third-party licence notices bundled and displayed
- [ ] VST3 SDK terms reviewed and complied with
- [ ] ASIO SDK terms reviewed, if ASIO ships
- [ ] MP3 encoder licence confirmed, if MP3 export ships
- [ ] If Path B: both JUCE and Tracktion licences active and current
- [ ] If Path A: complete corresponding source published
- [ ] Legal review, if distributing commercially
