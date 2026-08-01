#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "../app/CommandBus.h"
#include "../engine/EngineController.h"
#include "TimelineComponent.h"
#include "TapTempoComponent.h"
#include "../services/TempoDetector.h"


namespace saamveda::ui
{

class MainComponent : public juce::Component,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void addTrack();
    void removeLastTrack();
    void importAudio();
    void showTapTempo();
    void refreshTrackSummary();

    core::Session session;
    app::CommandBus commands { session };
    engine::EngineController engineController;

    juce::AudioDeviceSelectorComponent deviceSelector;
    TimelineComponent timeline;
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::ToggleButton loopButton { "Loop 0-16s" }, metronomeButton { "Metronome" };
    juce::TextButton tapTempoButton { "Tap Tempo" };
    juce::TextButton addTrackButton { "Add Audio Track" };
    juce::TextButton importAudioButton { "Import Audio..." };
    juce::TextButton removeTrackButton { "Remove Last Track" };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::Slider tempoSlider;
    juce::ComboBox numeratorBox, denominatorBox;
    juce::Label tempoLabel, timeSignatureLabel, positionLabel, trackSummaryLabel, actionStatusLabel;

    double timelineLengthSeconds = 60.0;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::ThreadPool analysisPool { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace saamveda::ui
