# 14 — Diagrams

**Document version:** 1.0
**Date:** 2026-07-25

All diagrams are Mermaid source, renderable on GitHub, in VS Code with a Mermaid extension, or at
<https://mermaid.live>.

---

## 1. Use Case Diagram

```mermaid
flowchart LR
    Producer(["Producer / Musician"])
    Engineer(["Mixing Engineer"])
    Plugin(["VST3 Plugin<br/>(external system)"])
    Hardware(["Audio / MIDI Hardware<br/>(external system)"])

    subgraph System["DAW Application"]
        UC1["Record Audio"]
        UC2["Record MIDI"]
        UC3["Edit Clips"]
        UC4["Sequence MIDI<br/>in Piano Roll"]
        UC5["Host Plugins"]
        UC6["Mix and Automate"]
        UC7["Save / Load Project"]
        UC8["Export Audio"]
        UC9["Undo / Redo"]
    end

    Producer --> UC1
    Producer --> UC2
    Producer --> UC3
    Producer --> UC4
    Producer --> UC7
    Producer --> UC9
    Engineer --> UC5
    Engineer --> UC6
    Engineer --> UC8

    UC1 -.-> Hardware
    UC2 -.-> Hardware
    UC5 -.-> Plugin
    UC6 -.-> Plugin
```

---

## 2. Data Flow Diagram — Level 0 (Context)

```mermaid
flowchart LR
    User(["User"])
    AudioHW(["Audio Hardware"])
    MidiHW(["MIDI Hardware"])
    Plugins(["VST3 Plugins"])
    FS(["File System"])

    DAW{{"DAW Application"}}

    User -->|"commands, edits"| DAW
    DAW -->|"waveforms, meters, UI"| User
    AudioHW -->|"input samples"| DAW
    DAW -->|"output samples"| AudioHW
    MidiHW -->|"MIDI events"| DAW
    Plugins -->|"processed audio"| DAW
    DAW -->|"audio buffers, parameters"| Plugins
    FS -->|"projects, audio files"| DAW
    DAW -->|"projects, exports, recordings"| FS
```

---

## 3. Data Flow Diagram — Level 1

```mermaid
flowchart TD
    User(["User"])
    AudioHW(["Audio Hardware"])
    FS(["File System"])

    P1["1.0<br/>Handle User Input"]
    P2["2.0<br/>Execute Command"]
    P3["3.0<br/>Manage Session State"]
    P4["4.0<br/>Process Audio"]
    P5["5.0<br/>Render / Export"]
    P6["6.0<br/>Persist Project"]

    D1[("Session Model")]
    D2[("Undo Stack")]
    D3[("Audio Files")]

    User --> P1
    P1 --> P2
    P2 --> P3
    P2 --> D2
    D2 --> P2
    P3 <--> D1
    D1 --> P4
    AudioHW --> P4
    P4 --> AudioHW
    P4 --> D3
    D1 --> P5
    P5 --> FS
    D1 --> P6
    P6 --> FS
    FS --> P6
    P6 --> D1
    D3 --> P4
    P4 --> P1
```

---

## 4. Class Diagram — Core Model

```mermaid
classDiagram
    class Session {
        +String name
        +double tempo
        +int timeSigNum
        +int timeSigDenom
        +double sampleRate
        +addTrack(type) Track
        +removeTrack(id)
        +toValueTree() ValueTree
    }

    class Track {
        <<abstract>>
        +String id
        +String name
        +float gain
        +float pan
        +bool mute
        +bool solo
        +bool armed
        +addClip(clip)
        +removeClip(id)
    }

    class AudioTrack {
        +String inputDevice
    }

    class MidiTrack {
        +String instrumentPluginId
    }

    class Clip {
        <<abstract>>
        +String id
        +double start
        +double length
        +double offset
        +float gain
        +double fadeIn
        +double fadeOut
        +split(time) Clip
        +trim(start, end)
    }

    class AudioClip {
        +String sourceFile
    }

    class MidiClip {
        +addNote(note)
        +removeNote(note)
        +quantize(grid, strength)
    }

    class Note {
        +double start
        +double length
        +int pitch
        +int velocity
        +int channel
    }

    class PluginChain {
        +add(plugin)
        +remove(id)
        +getTotalLatency() int
    }

    class Plugin {
        +String id
        +String uid
        +String format
        +bool bypassed
        +String state
    }

    class AutomationLane {
        +String parameterId
        +addPoint(time, value)
        +valueAt(time) float
    }

    class AutomationPoint {
        +double time
        +float value
        +CurveType curve
    }

    Session "1" o-- "*" Track
    Track <|-- AudioTrack
    Track <|-- MidiTrack
    Track "1" o-- "*" Clip
    Track "1" o-- "1" PluginChain
    Track "1" o-- "*" AutomationLane
    Clip <|-- AudioClip
    Clip <|-- MidiClip
    MidiClip "1" o-- "*" Note
    PluginChain "1" o-- "*" Plugin
    AutomationLane "1" o-- "*" AutomationPoint
```

---

## 5. Class Diagram — Command & Undo

```mermaid
classDiagram
    class Command {
        <<interface>>
        +execute(session, undoManager) bool
        +getName() String
    }

    class CommandBus {
        -UndoManager undoManager
        +dispatch(command) bool
        +undo() bool
        +redo() bool
        +getHistory() List
    }

    class UndoManager {
        +beginNewTransaction(name)
        +undo() bool
        +redo() bool
    }

    class AddTrackCommand
    class SplitClipCommand
    class MoveClipCommand
    class AddPluginCommand
    class EditNoteCommand

    Command <|.. AddTrackCommand
    Command <|.. SplitClipCommand
    Command <|.. MoveClipCommand
    Command <|.. AddPluginCommand
    Command <|.. EditNoteCommand
    CommandBus --> Command : dispatches
    CommandBus --> UndoManager : uses
```

---

## 6. Sequence — Recording Audio

```mermaid
sequenceDiagram
    actor User
    participant UI as Transport UI
    participant Bus as CommandBus
    participant Core as Session Model
    participant Eng as EngineController
    participant TE as tracktion_engine
    participant Disk as File System

    User->>UI: Arm track, press Record
    UI->>Bus: dispatch(ArmTrackCommand)
    Bus->>Core: set armed = true
    Core-->>Eng: state changed
    Eng->>TE: configure input device

    UI->>Bus: dispatch(StartRecordCommand)
    Bus->>Eng: startRecording()
    Eng->>TE: transport.record()

    loop Audio thread (realtime)
        TE->>TE: capture input buffer
        TE->>Disk: stream to file
        TE-->>Eng: level (atomic)
    end

    loop UI thread (timer)
        Eng-->>UI: read level atomically
        UI-->>User: update meter and playhead
    end

    User->>UI: Press Stop
    UI->>Bus: dispatch(StopRecordCommand)
    Bus->>Eng: stopRecording()
    Eng->>TE: transport.stop()
    TE->>Disk: finalise file
    Eng->>Bus: dispatch(CreateClipCommand)
    Bus->>Core: add clip to track
    Core-->>UI: notify change
    UI-->>User: display new clip waveform
```

---

## 7. Sequence — Loading and Using a Plugin

```mermaid
sequenceDiagram
    actor User
    participant UI as Plugin Browser
    participant Svc as PluginScanner
    participant Proc as Scan Subprocess
    participant Bus as CommandBus
    participant Core as Session Model
    participant Eng as EngineController

    User->>UI: Open plugin browser
    UI->>Svc: getPlugins()

    alt Cache is stale
        Svc->>Proc: scan folders (out of process)
        Note over Proc: Isolated so a faulty<br/>plugin cannot crash the host
        Proc-->>Svc: plugin descriptions
        Svc->>Svc: write cache to disk
    end

    Svc-->>UI: plugin list
    User->>UI: Select plugin, drop on track
    UI->>Bus: dispatch(AddPluginCommand)
    Bus->>Core: add to PluginChain
    Core-->>Eng: state changed
    Eng->>Eng: instantiate plugin
    Eng->>Eng: recalculate delay compensation
    Eng-->>UI: ready
    UI-->>User: show plugin editor

    User->>UI: Adjust a parameter
    UI->>Eng: setParameter (lock-free FIFO)
    Note over Eng: Never a direct call<br/>onto the audio thread
```

---

## 8. Sequence — Export with Null-Test Verification

```mermaid
sequenceDiagram
    actor User
    participant UI as Export Dialog
    participant Eng as EngineController
    participant TE as tracktion_engine
    participant FS as File System
    participant Test as Null Test

    User->>UI: Export to WAV
    UI->>Eng: render(range, format)
    Eng->>TE: offline render
    TE->>TE: process all tracks, plugins, automation
    TE-->>Eng: rendered buffer
    Eng->>FS: write file
    FS-->>UI: complete
    UI-->>User: export finished

    Note over Test: Automated verification
    Test->>FS: read exported file
    Test->>Eng: capture realtime playback
    Test->>Test: invert one, sum both
    Test-->>Test: peak must equal exactly 0
```

---

## 9. Architecture Layers

```mermaid
flowchart TD
    subgraph L5["ui/ — JUCE components"]
        direction LR
        Arr["Arrangement"]
        Piano["Piano Roll"]
        Mix["Mixer"]
        Wave["Waveform Editor"]
    end

    subgraph L4["app/ — command bus and undo"]
        Cmd["Commands"]
        Undo["UndoManager"]
    end

    subgraph L3["core/ — pure state, no dependencies"]
        Model["Session ValueTree"]
        Schema["Schema and Migration"]
    end

    subgraph L2["engine/ — tracktion wrapper"]
        Ctrl["EngineController"]
    end

    subgraph L1["services/"]
        Scan["Plugin Scanner"]
        IO["File I/O"]
        Dev["Device Manager"]
    end

    L5 -->|"dispatch commands"| L4
    L4 -->|"mutate"| L3
    L3 -->|"observed by"| L2
    L2 --> L1
    L5 -.->|"read only"| L3

    style L3 fill:#2d5016,color:#fff
    style L2 fill:#1a4d5c,color:#fff
```

> The dependency rule is one-way and downward. `ui/` never calls `engine/` directly — it dispatches
> commands. `core/` depends on nothing, which is what makes it testable without audio hardware.

---

## 10. Threading Model

```mermaid
flowchart LR
    subgraph Audio["🔴 Audio Thread — realtime"]
        direction TB
        CB["Audio callback"]
        DSP["Process DSP and plugins"]
        CB --> DSP
    end

    subgraph Message["Message Thread — UI"]
        direction TB
        Draw["Draw components"]
        Input["Handle input"]
    end

    subgraph Background["Background Threads"]
        direction TB
        Thumb["Waveform thumbnails"]
        PScan["Plugin scanning"]
        Auto["Autosave"]
    end

    subgraph Disk["Disk Streaming"]
        Stream["Read / write audio"]
    end

    Message -->|"lock-free FIFO"| Audio
    Audio -->|"atomics, lock-free FIFO"| Message
    Audio <-->|"buffered"| Disk
    Background -->|"marshal to message thread"| Message

    style Audio fill:#5c1a1a,color:#fff
```

> **The audio thread performs no allocation, no locking, and no I/O.** All communication crosses
> thread boundaries through lock-free structures or atomics. This constraint (NFR6) is enforced by a
> debug-build detector added in Phase 2 — see [`11-testing-strategy.md`](11-testing-strategy.md) §5.

---

## 11. Entity Relationship — Project File

```mermaid
erDiagram
    SESSION ||--o{ TRACK : contains
    SESSION ||--o{ PATTERN : contains
    SESSION ||--|| MIXER : has
    SESSION ||--|| ARRANGEMENT : has

    TRACK ||--o{ CLIP : contains
    TRACK ||--|| PLUGIN_CHAIN : has
    TRACK ||--o{ AUTOMATION_LANE : has

    CLIP ||--o{ NOTE : "contains (MIDI only)"
    CLIP }o--|| AUDIO_FILE : "references (audio only)"

    PLUGIN_CHAIN ||--o{ PLUGIN : contains
    AUTOMATION_LANE ||--o{ AUTOMATION_POINT : contains

    MIXER ||--o{ MIXER_TRACK : contains
    MIXER ||--|| MASTER : has
    MIXER_TRACK ||--o{ SEND : has
    MIXER_TRACK }o--|| TRACK : references

    ARRANGEMENT ||--o{ MARKER : contains
    ARRANGEMENT ||--o{ TEMPO_POINT : contains

    PATTERN ||--o{ NOTE : contains
```

---

## 12. Development Phases

```mermaid
flowchart LR
    P1["1<br/>Foundation<br/>wk 1"] --> P2["2<br/>Session and<br/>Transport<br/>wk 2-3"]
    P2 --> P3["3<br/>Tracks and<br/>Waveforms<br/>wk 4"]
    P3 --> P4["4<br/>Audio<br/>Recording<br/>wk 5-6"]
    P4 --> P5["5<br/>MIDI<br/>Recording<br/>wk 6"]
    P5 --> P6["6<br/>Clip<br/>Editing<br/>wk 7-8"]
    P6 --> P7["7<br/>Plugin<br/>Hosting<br/>wk 9-10"]
    P7 --> P8["8<br/>Built-in<br/>Effects<br/>wk 10"]
    P8 --> P9["9<br/>Piano Roll<br/>wk 11-12"]
    P9 --> P10["10<br/>Patterns<br/>wk 12"]
    P10 --> P11["11<br/>Mixer and<br/>Automation<br/>wk 13-14"]
    P11 --> P12["12<br/>Persistence<br/>and Export<br/>wk 15"]
    P12 --> P13["13<br/>Stabilise<br/>and Demo<br/>wk 16"]

    style P4 fill:#2d5016,color:#fff
    style P7 fill:#5c4a1a,color:#fff
    style P10 fill:#4a4a4a,color:#fff
```

> Green marks the point at which the application becomes genuinely usable for real work.
> Amber marks the highest-risk integration. Grey marks the first phase to cut if the schedule slips.
