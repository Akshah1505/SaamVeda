#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Layout.h"

namespace saamveda::ui
{

/** Top row: pattern/song mode, transport, tempo, and the position readout.

    Holds no state of its own beyond what it displays - the owner pushes values
    in and receives intent through the callbacks, so ui/ stays free of session
    logic as docs/05-architecture.md section 3 requires.
*/
class TransportBar final : public juce::Component
{
public:
    TransportBar();

    std::function<void()> onPlayStop, onStop, onRecord, onToggleSongMode;
    std::function<void (double)> onTempoChanged;
    std::function<void()> onTempoDragEnded;
    std::function<void (int, int)> onTimeSignatureChanged;

    void setPlaying (bool isPlaying);
    void setSongMode (bool isSongMode);
    void setTempo (double bpm);
    void setTimeSignature (int numerator, int denominator);
    void setPosition (double seconds, int bar, int beat, int tick);
    void setAudioLoad (double proportion);

    double tempo() const;
    bool isTempoBeingDragged() const noexcept { return tempoDragging; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::TextButton songModeButton { "SONG" };
    juce::TextButton playButton { "Play" }, stopButton { "Stop" }, recordButton { "Rec" };
    juce::Slider tempoSlider;
    juce::ComboBox numeratorBox, denominatorBox;
    juce::Label barsLabel, secondsLabel, positionCaption, loadLabel, timeSignatureCaption;
    bool tempoDragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
};

//==============================================================================
/** Second row: the hint panel, project actions, and the notification strip.

    The hint panel occupies the same slot as FL Studio's, because that is where
    the eye already goes for "what is selected" and "what just happened".
*/
class ToolBar final : public juce::Component
{
public:
    ToolBar();

    std::function<void()> onAddTrack, onImport, onRemoveTrack, onUndo, onRedo,
                          onTapTempo, onShowShortcuts;
    std::function<void (bool)> onLoopChanged, onMetronomeChanged;

    void setContext (const juce::String& heading, const juce::String& detail);
    void setStatus (const juce::String& message);
    void setHistoryEnabled (bool canUndo, bool canRedo);
    void setLoop (bool shouldLoop);
    void setMetronome (bool enabled);
    void setNotification (const juce::String& message, bool isWarning);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Label contextHeading, contextDetail, statusLabel, notificationLabel;
    juce::TextButton addTrackButton { "Add Track" }, importButton { "Import..." },
                     removeTrackButton { "Remove" }, undoButton { "Undo" },
                     redoButton { "Redo" }, tapTempoButton { "Tap" },
                     shortcutsButton { "Keys" };
    juce::ToggleButton loopButton { "Loop" }, metronomeButton { "Metronome" };
    bool notificationIsWarning = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToolBar)
};

} // namespace saamveda::ui
