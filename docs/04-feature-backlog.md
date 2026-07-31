# 04 — Feature Backlog: Full Audacity + FL Studio Parity

**Document version:** 1.0
**Date:** 2026-07-25
**Scope commitment:** **Every feature in this document is in scope for the project. Nothing here is
cut.** Priority indicates *build order only* — not whether a feature will exist.

---

## How to read this document

| Priority | Meaning |
|---|---|
| **P0** | Semester core. Required for the 16-week milestone. |
| **P1** | Semester stretch. Built if time allows; otherwise first work after. |
| **P2** | Post-semester, committed. Part of the parity goal, scheduled after the milestone. |
| **P3** | Long tail. Committed to the parity goal, but deep/specialist work. |

Every row is **Planned**. There is no "Won't do" column by design.

### A note on the FL Studio native plugin suite

FL Studio's instruments and effects (Sytrus, Harmor, Edison, Gross Beat, Maximus, and the rest) are
proprietary, trademarked products. We cannot clone them, reuse their names, or copy their DSP.
Where this document lists them, the commitment is to a **functional equivalent of our own design
and naming**. The "Reference" column names the FL feature only to identify the capability being
matched.

The same applies to Audacity's Nyquist-specific effects — the capability is matched, the
implementation is ours. See [`10-licensing-compliance.md`](10-licensing-compliance.md).

### Realistic note on the instrument suite

FL Studio ships roughly 30 native instruments representing decades of DSP work. Matching that
capability has two routes, and we take both: **(a)** host third-party VST3 instruments from day one,
which delivers the *capability* immediately, and **(b)** build our own instrument suite
incrementally, which delivers the *parity*. Route (a) is P0; route (b) is P2/P3 and is the single
largest long-term work item in this document.

---

# PART A — Audacity Parity

## A1. File Import / Export

| ID | Feature | Priority | Phase | Reference |
|---|---|:---:|:---:|---|
| A1.1 | Import WAV, AIFF, FLAC | P0 | 2 | Audacity |
| A1.2 | Import MP3, OGG, Opus | P0 | 2 | Audacity |
| A1.3 | Import M4A/AAC, WMA, AC3, AMR (via FFmpeg) | P2 | — | Audacity |
| A1.4 | Import raw PCM data with parameter dialog | P3 | — | Audacity |
| A1.5 | Import MIDI files | P1 | 11 | Audacity |
| A1.6 | Export WAV, AIFF, FLAC | P0 | 15 | Audacity |
| A1.7 | Export MP3, OGG, Opus | P0 | 15 | Audacity |
| A1.8 | Export M4A/AAC and custom FFmpeg formats | P2 | — | Audacity |
| A1.9 | Export Multiple (split by track or label) | P2 | — | Audacity |
| A1.10 | Export selection / region only | P1 | 15 | Audacity |
| A1.11 | Metadata / ID3 tag editor | P2 | — | Audacity |
| A1.12 | Project save & load | P0 | 15 | Audacity |
| A1.13 | Autosave and crash recovery | P1 | 15 | Audacity |
| A1.14 | Recent files list | P1 | 15 | Audacity |

## A2. Recording

| ID | Feature | Priority | Phase | Reference |
|---|---|:---:|:---:|---|
| A2.1 | Multichannel audio recording | P0 | 5 | Audacity |
| A2.2 | Overdub (play existing tracks while recording) | P0 | 5 | Audacity |
| A2.3 | Input monitoring / software playthrough | P0 | 5 | Audacity |
| A2.4 | Input level meter and gain control | P0 | 5 | Audacity |
| A2.5 | Record to new track vs append to existing | P0 | 5 | Audacity |
| A2.6 | Recording latency compensation | P0 | 5 | Audacity |
| A2.7 | Punch and roll recording | P2 | — | Audacity |
| A2.8 | Timer record (scheduled start/stop) | P3 | — | Audacity |
| A2.9 | Sound-activated recording | P3 | — | Audacity |

## A3. Destructive & Sample-Level Editing

| ID | Feature | Priority | Phase | Reference |
|---|---|:---:|:---:|---|
| A3.1 | Cut, copy, paste, delete, duplicate | P0 | 7 | Audacity |
| A3.2 | Trim, split, join, silence selection | P0 | 7 | Audacity |
| A3.3 | Split-cut, split-delete, split-new | P2 | — | Audacity |
| A3.4 | Full undo/redo with history window | P0 | 7 | Audacity |
| A3.5 | Destructive render of a clip ("freeze to audio") | P0 | 7 | *Our bridge to A3* |
| A3.6 | Draw tool — sample-level waveform editing | P2 | — | Audacity |
| A3.7 | Envelope tool — volume envelopes on clips | P1 | 13 | Audacity |
| A3.8 | Time-shift tool | P0 | 7 | Audacity |
| A3.9 | Multi-tool (context-sensitive) | P2 | — | Audacity |
| A3.10 | Sync-lock tracks | P2 | — | Audacity |
| A3.11 | Mix and render / render to new track | P1 | 15 | Audacity |
| A3.12 | Edge snapping and snap-to grid | P0 | 7 | Audacity |
| A3.13 | Clip naming and clip handles | P1 | 7 | Audacity |

## A4. Tracks & Views

| ID | Feature | Priority | Phase | Reference |
|---|---|:---:|:---:|---|
| A4.1 | Mono and stereo audio tracks | P0 | 2 | Audacity |
| A4.2 | Track mute, solo, gain, pan | P0 | 2 | Audacity |
| A4.3 | Track height adjust, collapse/expand | P0 | 2 | Audacity |
| A4.4 | Waveform view (linear amplitude) | P0 | 2 | Audacity |
| A4.5 | Waveform view (dB scale) | P2 | — | Audacity |
| A4.6 | Spectrogram view | P2 | — | Audacity |
| A4.7 | Multi-view (waveform + spectrogram) | P3 | — | Audacity |
| A4.8 | Label tracks — point and region labels | P2 | — | Audacity |
| A4.9 | Time track (variable playback speed) | P3 | — | Audacity |
| A4.10 | Split stereo to mono / join to stereo | P2 | — | Audacity |
| A4.11 | Per-track sample rate and format conversion | P2 | — | Audacity |
| A4.12 | Zoom in/out/fit/selection/toggle | P0 | 2 | Audacity |

## A5. Built-in Effects

| ID | Feature | Priority | Phase | Reference |
|---|---|:---:|:---:|---|
| A5.1 | Amplify, Normalize | P0 | 9 | Audacity |
| A5.2 | Loudness Normalization (LUFS) | P2 | — | Audacity |
| A5.3 | Compressor, Limiter | P0 | 9 | Audacity |
| A5.4 | Parametric EQ / Filter Curve | P0 | 9 | Audacity |
| A5.5 | Graphic EQ, Bass and Treble | P1 | 9 | Audacity |
| A5.6 | Reverb | P0 | 9 | Audacity |
| A5.7 | Echo, Delay | P0 | 9 | Audacity |
| A5.8 | Phaser, Wahwah, Chorus, Flanger | P1 | 9 | Audacity |
| A5.9 | Distortion, Overdrive | P1 | 9 | Audacity |
| A5.10 | Noise Reduction | P2 | — | Audacity |
| A5.11 | Noise Gate | P1 | 9 | Audacity |
| A5.12 | Click Removal, Clip Fix, Repair | P2 | — | Audacity |
| A5.13 | DeEsser | P2 | — | Audacity |
| A5.14 | Change Pitch (without tempo) | P2 | — | Audacity |
| A5.15 | Change Speed (pitch + tempo) | P1 | — | Audacity |
| A5.16 | Change Tempo (without pitch) | P2 | — | Audacity |
| A5.17 | Sliding Stretch | P3 | — | Audacity |
| A5.18 | Paulstretch (extreme time stretch) | P3 | — | Audacity |
| A5.19 | Fade In / Fade Out / Studio Fade / Adjustable Fade | P0 | 7 | Audacity |
| A5.20 | Crossfade clips and tracks | P1 | 7 | Audacity |
| A5.21 | Invert, Reverse | P1 | 9 | Audacity |
| A5.22 | Repeat | P1 | 9 | Audacity |
| A5.23 | Truncate Silence | P2 | — | Audacity |
| A5.24 | Vocal Reduction and Isolation | P2 | — | Audacity |
| A5.25 | Auto Duck | P2 | — | Audacity |
| A5.26 | Effect presets and user preset management | P1 | 9 | Audacity |

## A6. Generators

| ID | Feature | Priority | Phase | Reference |
|---|---|:---:|:---:|---|
| A6.1 | Tone generator (sine/square/saw/triangle) | P1 | 11 | Audacity |
| A6.2 | Chirp (swept tone) | P2 | — | Audacity |
| A6.3 | Noise (white, pink, brownian) | P1 | 11 | Audacity |
| A6.4 | Silence | P0 | 7 | Audacity |
| A6.5 | DTMF tones | P3 | — | Audacity |
| A6.6 | Rhythm track / click track | P1 | 11 | Audacity |
| A6.7 | Pluck, Risset Drum | P3 | — | Audacity |

## A7. Analysis

| ID | Feature | Priority | Phase | Reference |
|---|---|:---:|:---:|---|
| A7.1 | Plot Spectrum (FFT analysis window) | P1 | 13 | Audacity |
| A7.2 | Find Clipping | P2 | — | Audacity |
| A7.3 | Beat Finder | P2 | — | Audacity |
| A7.4 | Silence Finder / Sound Finder | P2 | — | Audacity |
| A7.5 | Label Sounds | P3 | — | Audacity |
| A7.6 | Regular Interval Labels | P3 | — | Audacity |
| A7.7 | Contrast analysis (accessibility) | P3 | — | Audacity |
| A7.8 | Sample Data Export / Import | P3 | — | Audacity |

## A8. Audacity Workflow & Extensibility

| ID | Feature | Priority | Phase | Reference |
|---|---|:---:|:---:|---|
| A8.1 | Macros / batch processing | P2 | — | Audacity |
| A8.2 | Scripting interface | P3 | — | Audacity (mod-script-pipe) |
| A8.3 | Spectral selection and spectral editing | P3 | — | Audacity |
| A8.4 | Playback speed control (play-at-speed) | P2 | — | Audacity |
| A8.5 | Loop play | P0 | 2 | Audacity |
| A8.6 | Scrub and seek | P2 | — | Audacity |
| A8.7 | Pinned playhead / play region | P1 | 2 | Audacity |
| A8.8 | Customisable keyboard shortcuts | P1 | 15 | Audacity |
| A8.9 | Themes / skinning | P2 | — | Audacity |
| A8.10 | Screen-reader accessibility | P2 | — | Audacity |
| A8.11 | Configurable toolbars | P2 | — | Audacity |

---

# PART B — FL Studio Parity

## B1. Step Sequencer & Patterns

| ID | Feature | Priority | Phase | Reference |
|---|---|:---:|:---:|---|
| B1.1 | Step sequencer grid | P1 | 12 | FL Channel Rack |
| B1.2 | Per-step graph editor (velocity, pan, cutoff, resonance, pitch, shift) | P2 | — | FL |
| B1.3 | Pattern creation, naming, cloning | P1 | 12 | FL |
| B1.4 | Pattern vs Song mode | P1 | 12 | FL |
| B1.5 | Channel rack with per-channel routing | P1 | 12 | FL |
| B1.6 | Variable step count / polyrhythmic patterns | P2 | — | FL |
| B1.7 | Swing / groove templates | P2 | — | FL |

## B2. Piano Roll

FL Studio's piano roll is widely regarded as the best in the industry. Full parity is a substantial
sub-project in its own right.

| ID | Feature | Priority | Phase | Reference |
|---|---|:---:|:---:|---|
| B2.1 | Draw, edit, move, resize, delete notes | P0 | 11 | FL |
| B2.2 | Velocity editing per note | P0 | 11 | FL |
| B2.3 | Quantize (with strength and swing) | P0 | 11 | FL |
| B2.4 | Note properties: pan, filter cutoff/res, pitch, release | P2 | — | FL |
| B2.5 | Scale highlighting and scale snapping | P1 | 11 | FL |
| B2.6 | Ghost notes from other patterns | P2 | — | FL |
| B2.7 | Slide / portamento notes | P2 | — | FL |
| B2.8 | Chop / slice tool | P2 | — | FL |
| B2.9 | Glue, strum, flam, arpeggiate tools | P2 | — | FL |
| B2.10 | Riff generator | P3 | — | FL Riff Machine |
| B2.11 | Paint / brush note entry | P1 | 11 | FL |
| B2.12 | Note colour groups | P2 | — | FL |
| B2.13 | Step entry from MIDI keyboard | P2 | — | FL |

## B3. Playlist / Arrangement

| ID | Feature | Priority | Phase | Reference |
|---|---|:---:|:---:|---|
| B3.1 | Timeline arrangement of audio clips | P0 | 7 | FL Playlist |
| B3.2 | Pattern clips on the playlist | P1 | 12 | FL |
| B3.3 | Automation clips | P1 | 13 | FL |
| B3.4 | Unlimited playlist tracks, any content type | P1 | 12 | FL |
| B3.5 | Multiple arrangements per project | P3 | — | FL |
| B3.6 | Time markers | P1 | 13 | FL |
| B3.7 | Time signature changes mid-project | P2 | — | FL |
| B3.8 | Tempo automation | P2 | — | FL |
| B3.9 | Track grouping / folder tracks | P2 | — | FL |
| B3.10 | Magnetic / adaptive grid snap | P0 | 7 | FL |
| B3.11 | Performance mode (live clip triggering) | P3 | — | FL |

## B4. Mixer & Routing

| ID | Feature | Priority | Phase | Reference |
|---|---|:---:|:---:|---|
| B4.1 | Mixer with per-track fader, pan, mute, solo | P0 | 13 | FL |
| B4.2 | Level meters (peak + RMS) | P0 | 13 | FL |
| B4.3 | Insert FX slots per mixer track | P0 | 9 | FL |
| B4.4 | Send/return buses | P0 | 13 | FL |
| B4.5 | Arbitrary routing matrix between mixer tracks | P1 | 13 | FL |
| B4.6 | Sidechain routing | P1 | 13 | FL |
| B4.7 | Mixer track grouping and linking | P2 | — | FL |
| B4.8 | Per-mixer-track recording arm | P1 | 5 | FL |
| B4.9 | Master track with master FX chain | P0 | 13 | FL |
| B4.10 | Plugin delay compensation | P0 | 9 | FL |

## B5. Automation & Modulation

| ID | Feature | Priority | Phase | Reference |
|---|---|:---:|:---:|---|
| B5.1 | Automation lanes for volume, pan | P0 | 13 | FL |
| B5.2 | Automation of any plugin parameter | P0 | 13 | FL |
| B5.3 | Automation curve types (linear, tension, stepped, hold, smooth) | P1 | 13 | FL |
| B5.4 | Record automation from UI gestures | P1 | 13 | FL |
| B5.5 | Internal LFO controller | P2 | — | FL |
| B5.6 | Envelope controller | P2 | — | FL |
| B5.7 | Peak / envelope-follower controller | P3 | — | FL Peak Controller |
| B5.8 | Formula controller (expression-driven) | P3 | — | FL Formula Controller |
| B5.9 | Link any parameter to any MIDI control | P1 | 13 | FL |
| B5.10 | Multilink to controllers | P2 | — | FL |

## B6. Audio Manipulation

| ID | Feature | Priority | Phase | Reference |
|---|---|:---:|:---:|---|
| B6.1 | Audio clip time stretching | P1 | — | FL / elastique |
| B6.2 | Independent pitch shifting | P1 | — | FL |
| B6.3 | Beat detection and warping | P2 | — | FL |
| B6.4 | Beat slicing to sequencer | P2 | — | FL Slicex |
| B6.5 | Dedicated audio editor window (destructive) | P1 | — | FL Edison |
| B6.6 | Spectral view and spectral repair | P3 | — | FL Edison |
| B6.7 | Convolution / impulse response loading | P3 | — | FL Convolver |
| B6.8 | Pitch correction and note-level pitch editing | P3 | — | FL Newtone |
| B6.9 | Clip declick, normalize, reverse, swap stereo | P1 | 7 | FL |
| B6.10 | Sample-accurate loop points | P1 | — | FL |

## B7. Plugin Hosting

| ID | Feature | Priority | Phase | Reference |
|---|---|:---:|:---:|---|
| B7.1 | VST3 effect hosting | P0 | 9 | FL |
| B7.2 | VST3 instrument hosting | P0 | 11 | FL |
| B7.3 | Plugin scanning and database | P0 | 9 | FL |
| B7.4 | Plugin editor windows (native UI) | P0 | 9 | FL |
| B7.5 | Plugin preset save/load | P1 | 9 | FL |
| B7.6 | VST2 hosting | P2 | — | FL |
| B7.7 | CLAP hosting | P2 | — | Zrythm/modern hosts |
| B7.8 | LV2 hosting | P3 | — | Ardour/Zrythm |
| B7.9 | Out-of-process plugin sandboxing (crash isolation) | P2 | — | FL bridging |
| B7.10 | 32-bit plugin bridging | P3 | — | FL |
| B7.11 | Modular plugin routing graph | P3 | — | FL Patcher |

## B8. Native Instruments (our own designs)

Delivered as capability via VST3 hosting (P0) and as parity via our own suite (P2/P3).

| ID | Feature | Priority | Phase | Reference capability |
|---|---|:---:|:---:|---|
| B8.1 | Basic subtractive synth (multi-osc, filter, ADSR) | P1 | 12 | FL 3xOsc |
| B8.2 | Sampler with envelopes, filters, loop modes | P1 | 12 | FL Sampler |
| B8.3 | Drum pad / kit sampler | P2 | — | FL FPC |
| B8.4 | Sliced-loop instrument | P2 | — | FL Slicex |
| B8.5 | FM synthesizer | P3 | — | FL Sytrus |
| B8.6 | Additive / resynthesis engine | P3 | — | FL Harmor |
| B8.7 | Wavetable synthesizer | P3 | — | FL Flex |
| B8.8 | Granular synthesizer | P3 | — | FL Granulizer |
| B8.9 | Virtual analog bass synth | P3 | — | FL Transistor Bass |
| B8.10 | SoundFont / SFZ player | P2 | — | LMMS SF2 |
| B8.11 | Physical modelling instrument | P3 | — | FL PlucKed! |

## B9. Native Effects (our own designs)

| ID | Feature | Priority | Phase | Reference capability |
|---|---|:---:|:---:|---|
| B9.1 | Parametric EQ with spectrum display | P0 | 9 | FL Parametric EQ 2 |
| B9.2 | Compressor, Limiter, Gate | P0 | 9 | FL |
| B9.3 | Multiband compressor | P2 | — | FL Multiband / Maximus |
| B9.4 | Reverb (algorithmic) | P0 | 9 | FL Reverb 2 |
| B9.5 | Delay (tempo-synced, ping-pong, feedback) | P0 | 9 | FL Delay 3 |
| B9.6 | Chorus, Flanger, Phaser | P1 | 9 | FL |
| B9.7 | Distortion, Overdrive, Waveshaper, Bitcrush | P1 | 9 | FL |
| B9.8 | Stereo enhancer / width control | P1 | 9 | FL Stereo Enhancer |
| B9.9 | Vocoder | P3 | — | FL Vocodex |
| B9.10 | Time-gating / stutter effect | P3 | — | FL Gross Beat |
| B9.11 | Filter with modulation | P1 | 9 | FL Love Philter |
| B9.12 | Frequency shifter, ring modulator | P3 | — | FL |
| B9.13 | Convolution reverb | P3 | — | FL Convolver |
| B9.14 | Metering suite (scope, spectrum, LUFS, correlation) | P1 | 13 | FL Wave Candy |
| B9.15 | Tuner | P2 | — | FL |

## B10. MIDI & Control

| ID | Feature | Priority | Phase | Reference |
|---|---|:---:|:---:|---|
| B10.1 | MIDI input recording | P0 | 5 | FL |
| B10.2 | MIDI controller mapping (MIDI learn) | P1 | 13 | FL |
| B10.3 | MIDI output to external gear | P2 | — | FL |
| B10.4 | MIDI clock sync | P3 | — | FL |
| B10.5 | MIDI controller scripting API | P3 | — | FL MIDI Scripting |
| B10.6 | MPE support | P3 | — | Modern hosts |

## B11. Workflow & UI

| ID | Feature | Priority | Phase | Reference |
|---|---|:---:|:---:|---|
| B11.1 | File / sample browser | P1 | 15 | FL Browser |
| B11.2 | Plugin and preset browser | P1 | 9 | FL |
| B11.3 | Undo history panel | P0 | 7 | FL |
| B11.4 | Project templates | P2 | — | FL |
| B11.5 | Detachable windows / multi-monitor | P2 | — | FL |
| B11.6 | Themes and skinning | P2 | — | FL |
| B11.7 | Project snapshots | P3 | — | FL |
| B11.8 | Metronome and count-in | P0 | 5 | FL |
| B11.9 | Export stems (split mixer tracks) | P1 | 15 | FL |
| B11.10 | Export MIDI | P2 | — | FL |
| B11.11 | Render selection to audio clip (consolidate) | P1 | 15 | FL |
| B11.12 | Touch / high-DPI support | P2 | — | FL |
| B11.13 | Video player for scoring to picture | P3 | — | FL |

---

# PART C — Beyond Parity (Differentiation)

Carried forward from the abandoned Raga Sur direction. Not required for parity; this is where the
project could become something neither reference tool is.

| ID | Feature | Priority | Reference |
|---|---|:---:|---|
| C1.1 | Tanpura / drone generator with tuning control | P3 | — |
| C1.2 | Sa (tonic) calibration from a sustained note | P3 | Raga Sur PRD |
| C1.3 | Swara notation view (Sa Re Ga Ma Pa Dha Ni) | P3 | Raga Sur PRD |
| C1.4 | Raga-aware scale snapping in the piano roll | P3 | — |
| C1.5 | Tala cycle metronome (teentaal, jhaptaal, rupak) | P3 | — |
| C1.6 | Pitch/intonation analysis against a reference | P3 | Raga Sur PRD F3 |
| C1.7 | Microtonal / shruti tuning support | P3 | — |

---

# Summary counts

| Priority | Count | Timing |
|---|---:|---|
| **P0** — semester core | 48 | Weeks 1–16 |
| **P1** — semester stretch | 45 | Weeks 1–16 if possible, else immediately after |
| **P2** — post-semester, committed | 58 | Following 6–12 months |
| **P3** — long tail, committed | 45 | Ongoing |
| **Total tracked** | **196** | — |

**Nothing in this document is cancelled.** The P0/P1 set (93 features) constitutes a genuinely
usable DAW and is what the 16-week milestone is judged against — see
[`09-roadmap.md`](09-roadmap.md). The P2/P3 set (103 features) is the parity programme that
continues after.

For honest planning: the P3 tier alone — particularly the instrument suite (B8) and the advanced
audio manipulation (B6) — represents the majority of the remaining work by effort, and much of it
is specialist DSP. Hosting VST3 plugins (B7.1/B7.2, both P0) delivers those *capabilities* from
week 9 onward, which is why the native suite can be sequenced late without the DAW being unusable
in the meantime.
