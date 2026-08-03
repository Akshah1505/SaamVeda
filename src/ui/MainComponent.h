#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "../app/CommandBus.h"
#include "../app/CommandIDs.h"
#include "../engine/EngineController.h"
#include "TimelineComponent.h"
#include "TapTempoComponent.h"
#include "../services/TempoDetector.h"

namespace saamveda::ui
{

class MainComponent : public juce::Component,
                      public juce::ApplicationCommandTarget,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void parentHierarchyChanged() override;

    juce::ApplicationCommandManager& getCommandManager() noexcept { return commandManager; }

    // ApplicationCommandTarget
    juce::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands (juce::Array<juce::CommandID>&) override;
    void getCommandInfo (juce::CommandID, juce::ApplicationCommandInfo&) override;
    bool perform (const InvocationInfo&) override;

private:
    void timerCallback() override;

    // Actions
    void addTrack();
    void removeLastTrack();
    void importAudio();
    void showTapTempo();
    void showShortcuts();
    void performUndo();
    void performRedo();
    void commitTempo (double bpm);
    void commitTimeSignature();
    void startLoopPlay();

    // View sync
    void applySessionToUi();
    void refreshTrackSummary();
    void setStatus (const juce::String& message);
    void updateTransportButtons();
    void ensureKeyboardFocus();

    core::Session session;
    app::CommandBus commands { session };
    engine::EngineController engineController;
    juce::ApplicationCommandManager commandManager;

    juce::AudioDeviceSelectorComponent deviceSelector;
    TimelineComponent timeline;
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::ToggleButton loopButton { "Loop" }, metronomeButton { "Metronome" };
    juce::TextButton tapTempoButton { "Tap Tempo" };
    juce::TextButton addTrackButton { "Add Audio Track" };
    juce::TextButton importAudioButton { "Import Audio..." };
    juce::TextButton removeTrackButton { "Remove Last Track" };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::TextButton shortcutsButton { "Keys" };
    juce::Slider tempoSlider;
    juce::ComboBox numeratorBox, denominatorBox;
    juce::Label tempoLabel, timeSignatureLabel, positionLabel, trackSummaryLabel,
                actionStatusLabel, realtimeStatusLabel;

    double timelineLengthSeconds = 60.0;
    bool tempoSliderIsDragging = false;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::ThreadPool analysisPool { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace saamveda::ui
