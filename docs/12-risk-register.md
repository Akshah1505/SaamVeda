# 12 — Risk Register

**Document version:** 1.0
**Date:** 2026-07-25

---

## Scoring

**Likelihood** and **Impact** are rated Low / Medium / High.
**Severity** = the combination; 🔴 Critical · 🟠 High · 🟡 Medium · 🟢 Low.

---

## R1 🔴 Application Control policy blocks the toolchain

| | |
|---|---|
| **Likelihood** | Medium | 
| **Impact** | High — blocks all development |
| **Evidence** | **Already observed.** A DLL was blocked during environment probing with "An Application Control policy has blocked this file" |

Security software on the development machine has already demonstrated it will block DLL loading.
The same policy could block compilers, linkers, freshly built binaries, or — most likely —
**plugin scanning**, which loads arbitrary third-party DLLs and is precisely the behaviour such
policies exist to prevent.

**Mitigation**
- Run the toolchain verification in [`08-toolchain-setup.md`](08-toolchain-setup.md) §3 **in week 1**,
  before any other work. This is a deliberate go/no-go gate.
- If blocked, resolve with whoever administers the machine before committing to the timeline.
- Test plugin loading early — do not wait for Phase 7 to discover the policy blocks VST3 DLLs.

**Contingency:** develop inside a VM or on a different machine. This would cost days, not hours,
which is why the check happens in week 1.

---

## R2 🔴 Scope expectation versus deliverable

| | |
|---|---|
| **Likelihood** | High |
| **Impact** | High — a correct outcome judged a failure |

The stated goal is full Audacity + FL Studio parity — 196 features against roughly 50 combined years
of commercial development. The 16-week milestone delivers 93 of them. If the project is judged
against "all features," it fails on paper despite succeeding in fact.

**Mitigation**
- [`01-project-charter.md`](01-project-charter.md) §5 states the scope reality explicitly and exists
  to be read before any milestone review.
- [`04-feature-backlog.md`](04-feature-backlog.md) records all 196 features with priorities so that
  nothing appears cancelled — the difference between "later" and "never" is documented.
- Milestone reviews are conducted against [`09-roadmap.md`](09-roadmap.md), not the backlog.

---

## R3 🟠 Single developer, part-time, fixed deadline

| | |
|---|---|
| **Likelihood** | High |
| **Impact** | Medium |

One part-time developer at ~15–20 hours/week against a fixed semester deadline, with no slack for
illness, coursework spikes, or exams. There is no bus factor above one.

**Mitigation**
- [`09-roadmap.md`](09-roadmap.md) §6 defines a pre-agreed cut order, with the decision point at
  week 10 — before panic sets in.
- Highest-risk work is front-loaded (toolchain week 1, plugin hosting weeks 9–10).
- Every phase is independently demonstrable, so an incomplete project still shows working software.

---

## R4 🟠 Licence decision made by accident

| | |
|---|---|
| **Likelihood** | Medium |
| **Impact** | High — irreversible |

Publishing source under AGPLv3 cannot be undone. A casual "let me put this on GitHub" during the
semester would permanently foreclose any commercial option, without the decision ever being
consciously made.

**Mitigation**
- Repository stays **private** until decision D2 is made ([`10-licensing-compliance.md`](10-licensing-compliance.md) §6).
- D2 recorded in the charter's open-decisions table with an explicit "needed by" date.
- Compliance checklist gates any public release.

---

## R5 🟠 GPL contamination from reference codebases

| | |
|---|---|
| **Likelihood** | Medium |
| **Impact** | High |

The project explicitly studies LMMS, Ardour, Audacity, and Zrythm — all GPL or AGPL. Copying even a
small amount of source would place the whole application under copyleft. The realistic failure mode
is not deliberate theft but a "temporary" paste that is never removed.

**Mitigation**
- Working rules in [`10-licensing-compliance.md`](10-licensing-compliance.md) §3.
- Prefer neutral algorithm sources — textbooks, papers — over reading GPL implementations.
- Record in commit messages when a reference codebase informed a component.
- Never paste as a placeholder. Placeholders ship.

---

## R6 🟠 Audio-thread violations discovered late

| | |
|---|---|
| **Likelihood** | Medium |
| **Impact** | High |

Allocation or locking on the audio thread causes dropouts that are intermittent, unreproducible
under a debugger, and expensive to fix once the design depends on them.

**Mitigation**
- Debug-build allocation/lock detector added in **Phase 2**, not later
  ([`11-testing-strategy.md`](11-testing-strategy.md) §5). A detector added in Phase 11 would surface
  an untriageable backlog and get disabled.
- Architectural rule: the audio thread never reads the ValueTree
  ([`06-data-model.md`](06-data-model.md) §7).
- AddressSanitizer enabled in debug builds.

---

## R7 🟡 tracktion_engine learning curve

| | |
|---|---|
| **Likelihood** | High |
| **Impact** | Medium |

Documentation is thinner than JUCE's; the examples are the real reference. Early phases may run
slower than planned while the API is learned.

**Mitigation**
- Phase 1 is deliberately a walking skeleton, so the first contact is small.
- Budget extra time in Phases 2–3; the roadmap allocates 3 weeks for what is nominally 2 weeks of work.
- The wrapper in `engine/` contains the API surface, so learning is concentrated in one place.

---

## R8 🟡 Two sources of truth (our model vs tracktion's)

| | |
|---|---|
| **Likelihood** | Medium |
| **Impact** | Medium |

`core/` holds the session model; tracktion_engine holds its own `Edit`. Divergence causes bugs that
are hard to diagnose because both look correct in isolation.

**Mitigation**
- `engine/EngineController` solely owns synchronisation.
- Where tracktion is authoritative (plugin state, transport position), we do not duplicate.
- Save→load→render null tests catch divergence objectively.

---

## R9 🟡 Third-party plugin instability

| | |
|---|---|
| **Likelihood** | High |
| **Impact** | Medium |

Plugins are arbitrary third-party code. Some crash, leak, or misreport latency. Until out-of-process
hosting (B7.9, P2) lands, a bad plugin can take the application down — including during a demo.

**Mitigation**
- Scanning is out-of-process from Phase 7 (SRS-6.2), covering the most common crash point.
- A vetted plugin set is used for the demo; nothing unknown is loaded live.
- B7.9 is the first post-semester work item.

**Accepted:** full hosting isolation is out of scope for the semester. This is a known gap.

---

## R10 🟡 Disk space

| | |
|---|---|
| **Likelihood** | Medium |
| **Impact** | Medium |

Visual Studio with the C++ workload needs 20–40 GB. C: has 63 GB free. Adding build artefacts, JUCE,
tracktion, plugins, and recorded audio, this gets tight.

**Mitigation**
- Relocate the VS download cache and shared components to D: (75 GB free) during installation.
- Keep the repository and build output on D:.
- `.gitignore` excludes audio and build directories.

---

## R11 🟡 Demo failure on the day

| | |
|---|---|
| **Likelihood** | Medium |
| **Impact** | High |

Live audio demonstrations fail for reasons outside your control — a different room, borrowed
hardware, a driver that changed, no audio interface.

**Mitigation**
- **Record a complete backup demo video in week 15**, before the presentation
  ([`09-roadmap.md`](09-roadmap.md) Phase 13). Not optional.
- Prepare a pre-built project so the demo does not depend on live recording succeeding.
- Test on the actual presentation hardware beforehand if possible.
- Have WASAPI as a fallback if ASIO misbehaves on unfamiliar hardware.

---

## R12 🟢 Framework licensing terms change

| | |
|---|---|
| **Likelihood** | Low |
| **Impact** | High |

JUCE or Tracktion could alter their licensing. JUCE's terms have changed across major versions
before.

**Mitigation**
- Submodules pinned to specific commits, so the current terms apply to the pinned version.
- The `engine/` wrapper limits how deeply tracktion's API pervades the codebase.

---

## R13 🟢 Hardware failure

| | |
|---|---|
| **Likelihood** | Low |
| **Impact** | High |

A single development machine holding all work.

**Mitigation**
- Git repository with frequent commits.
- Off-machine backup — a private remote once D2 permits, or an encrypted external drive until then.
- **Do not let the only copy of a semester's work live on one laptop.**

---

## Summary

> **Status update 2026-08-04 — the Application Control risk has materialised.**
> Smart App Control is **Enforced** on the development machine
> (`HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy\VerifiedAndReputablePolicyState = 1`) and blocks
> freshly linked, unsigned executables with CodeIntegrity event 3077. It is intermittent: the test
> binary usually runs, the application binary is blocked most of the time, and a given build is
> sometimes allowed and sometimes not. Both Debug and Release builds are affected.
>
> This does not stop development — compiling and unit testing are unaffected — but it does stop
> running the application on demand, which makes visual verification unreliable and would make a
> live demo a gamble.
>
> The only real fix is turning Smart App Control off, in Windows Security → App & browser control.
> **That decision belongs to the machine's owner, and it is one-way: Smart App Control cannot be
> switched back on without reinstalling Windows.** Signing the binary does not help, because Smart
> App Control wants a signature it already trusts, not merely a valid one.
>
> **Closed 2026-09-17.** The machine's owner turned Smart App Control off
> (`VerifiedAndReputablePolicyState = 0`) and freshly linked binaries have run on demand since.
> R1 stays on the register: the policy state is worth re-checking before the demo, and a different
> presentation machine brings it straight back. Record the demo video early as roadmap §13 already
> requires.

> **Resolved 2026-09-17 — audio-thread allocations once inputs are open.**
> With input channels open, the Debug allocation detector reported roughly 40,000 allocations per
> second on the audio thread — 1,025,457 over a short session, with audio load at 12–13%.
>
> Captured stacks pointed at `jassertfalse` in
> `WaveInputDeviceInstance::copyIncomingDataIntoBuffer` (`tracktion_WaveInputDevice.cpp:1197`),
> which calls `juce::logAssertion` and formats a string. The assertion was a symptom. The cause was
> the wave device layout: tracktion's default pairs up every channel the driver *names*, and this
> machine's WASAPI endpoint names **64** inputs while only **2** are active. That produced 32 stereo
> wave input devices, of which 31 index past the end of the callback's channel array on every block.
>
> Each stray device cost twice. The assertion is Debug-only, but
> `WaveInputDevice::consumeNextAudioBlock` also heap-allocates a scratch `AudioBuffer` for every
> enabled device with no instance attached — **that one allocates in Release too**, so this was never
> purely a Debug artifact.
>
> Fixed in [`src/engine/WaveDeviceLayout.h`](../src/engine/WaveDeviceLayout.h) by implementing
> `EngineBehaviour::describeWaveDevices`, which builds the layout from the device's *active*
> channels rather than its named ones. tracktion treats a host-supplied layout as authoritative and
> skips both its channel-coverage pass and persisting the layout to settings, so there is no stale
> state to clear.
>
> | | before | after |
> |---|---:|---:|
> | audio-thread allocations | 1,025,457 | 14 |
> | audio load | 12% | 1% |
> | wave input devices | 32 | 1 |
>
> Recording was re-verified after the change: arm, record, stop, and a clip with signal on the
> timeline. An earlier attempt — activating all 64 channels and calling `rescanWaveDeviceList()` —
> also removed the allocations but silenced the input, and is not what shipped.
>
> One trap worth keeping: `setAudioDeviceSetup` **persists** to
> `%APPDATA%/SaamVeda Studio/Settings.xml`. A bad `audioDeviceInChans` mask survives restarts and a
> code revert, and has to be cleared in that file.

| ID | Risk | Severity |
|---|---|:---:|
| R1 | Application Control blocks the toolchain | 🔴 |
| R2 | Scope expectation versus deliverable | 🔴 |
| R3 | Single part-time developer | 🟠 |
| R4 | Licence decision made by accident | 🟠 |
| R5 | GPL contamination | 🟠 |
| R6 | Audio-thread violations found late | 🟠 |
| R7 | tracktion_engine learning curve | 🟡 |
| R8 | Two sources of truth | 🟡 |
| R9 | Third-party plugin instability | 🟡 |
| R10 | Disk space | 🟡 |
| R11 | Demo failure | 🟡 |
| R12 | Licensing terms change | 🟢 |
| R13 | Hardware failure | 🟢 |

**The two critical risks are addressable this week.** R1 needs a 5-minute toolchain test; R2 needs
the charter's scope statement read and agreed by whoever grades the project. Both are far cheaper to
handle now than in November.
