#include "MainComponent.h"

namespace saamveda::ui
{

MainComponent::MainComponent()
    : deviceSelector (engineController.audioDeviceManager(),
                      0, 2, 0, 2,
                      false, false, true, false)
{
    playButton.onClick = [this] { engineController.play(); };
    stopButton.onClick = [this] { engineController.stop(); };
    loopButton.onClick = [this] { engineController.setLooping (loopButton.getToggleState()); };
    metronomeButton.onClick = [this]
    {
        engineController.setMetronomeEnabled (metronomeButton.getToggleState());
        actionStatusLabel.setText (metronomeButton.getToggleState() ? "Metronome enabled."
                                                                    : "Metronome disabled.",
                                   juce::dontSendNotification);
    };
    tapTempoButton.onClick = [this] { showTapTempo(); };
    timeline.onSeek = [this] (double seconds)
    {
        engineController.seek (seconds);
        actionStatusLabel.setText ("Moved playhead to " + juce::String (seconds, 2) + " s",
                                   juce::dontSendNotification);
    };
    addTrackButton.onClick = [this] { addTrack(); };
    importAudioButton.onClick = [this] { importAudio(); };
    removeTrackButton.onClick = [this] { removeLastTrack(); };
    undoButton.onClick = [this]
    {
        if (commands.undo())
            engineController.synchronise (session);
        refreshTrackSummary();
    };
    redoButton.onClick = [this]
    {
        if (commands.redo())
            engineController.synchronise (session);
        refreshTrackSummary();
    };

    tempoSlider.setRange (20.0, 400.0, 1.0);
    tempoSlider.setValue (engineController.tempo());
    tempoSlider.setTextValueSuffix (" BPM");
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 24);
    tempoSlider.onValueChange = [this]
    {
        // Until automatic beat detection lands, imported recordings use 120 BPM
        // as their reference. Keeping that implementation detail out of the UI
        // gives the normal one-knob DAW workflow requested for this phase.
        engineController.setTempo (tempoSlider.getValue());
        session.state().setProperty ("tempo", tempoSlider.getValue(), nullptr);
        timeline.setMusicalGrid (tempoSlider.getValue(), numeratorBox.getText().getIntValue(),
                                 denominatorBox.getText().getIntValue());
        actionStatusLabel.setText ("Tempo changed to " + juce::String (tempoSlider.getValue(), 0)
                                       + " BPM; transport restarted from 0.",
                                   juce::dontSendNotification);
    };

    for (int value = 1; value <= 16; ++value)
        numeratorBox.addItem (juce::String (value), value);
    for (auto value : { 1, 2, 4, 8, 16, 32 })
        denominatorBox.addItem (juce::String (value), value);
    numeratorBox.setSelectedId (4);
    denominatorBox.setSelectedId (3); // item 3 is denominator 4

    const auto updateTimeSignature = [this]
    {
        const int numerator = numeratorBox.getText().getIntValue();
        const int denominator = denominatorBox.getText().getIntValue();
        engineController.setTimeSignature (numerator, denominator);
        session.state().setProperty ("timeSigNum", numerator, nullptr);
        session.state().setProperty ("timeSigDenom", denominator, nullptr);
        timeline.setMusicalGrid (tempoSlider.getValue(), numerator, denominator);
        actionStatusLabel.setText ("Time signature changed to " + juce::String (numerator)
                                       + "/" + juce::String (denominator)
                                       + "; transport restarted from 0.",
                                   juce::dontSendNotification);
    };
    numeratorBox.onChange = updateTimeSignature;
    denominatorBox.onChange = updateTimeSignature;

    tempoLabel.setText ("Tempo BPM", juce::dontSendNotification);
    timeSignatureLabel.setText ("Time signature", juce::dontSendNotification);
    positionLabel.setJustificationType (juce::Justification::centredLeft);
    trackSummaryLabel.setJustificationType (juce::Justification::centredLeft);
    actionStatusLabel.setJustificationType (juce::Justification::centredLeft);
    actionStatusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffaeb4c3));
    actionStatusLabel.setText ("Add a track to begin the arrangement.", juce::dontSendNotification);

    for (auto* component : std::initializer_list<juce::Component*>
         { &playButton, &stopButton, &loopButton, &metronomeButton, &tapTempoButton,
           &addTrackButton, &importAudioButton,
           &removeTrackButton,
           &undoButton, &redoButton,
           &tempoSlider, &numeratorBox, &denominatorBox, &tempoLabel,
           &timeSignatureLabel, &positionLabel, &trackSummaryLabel, &actionStatusLabel,
           &timeline, &deviceSelector })
        addAndMakeVisible (component);

    refreshTrackSummary();
    timeline.setMusicalGrid (tempoSlider.getValue(), 4, 4);
    startTimerHz (30);
    setSize (900, 650);
}

MainComponent::~MainComponent()
{
    stopTimer();
    analysisPool.removeAllJobs (true, 5000);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181a20));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (12);
    auto transport = area.removeFromTop (34);
    playButton.setBounds (transport.removeFromLeft (75).reduced (2));
    stopButton.setBounds (transport.removeFromLeft (75).reduced (2));
    loopButton.setBounds (transport.removeFromLeft (115).reduced (2));
    metronomeButton.setBounds (transport.removeFromLeft (105).reduced (2));
    tapTempoButton.setBounds (transport.removeFromLeft (95).reduced (2));
    positionLabel.setBounds (transport.removeFromLeft (150).reduced (4, 2));
    addTrackButton.setBounds (transport.removeFromRight (135).reduced (2));
    importAudioButton.setBounds (transport.removeFromRight (120).reduced (2));
    removeTrackButton.setBounds (transport.removeFromRight (135).reduced (2));
    redoButton.setBounds (transport.removeFromRight (70).reduced (2));
    undoButton.setBounds (transport.removeFromRight (70).reduced (2));

    auto settings = area.removeFromTop (34);
    tempoLabel.setBounds (settings.removeFromLeft (52));
    tempoSlider.setBounds (settings.removeFromLeft (190).reduced (2));
    settings.removeFromLeft (12);
    timeSignatureLabel.setBounds (settings.removeFromLeft (94));
    numeratorBox.setBounds (settings.removeFromLeft (58).reduced (2));
    denominatorBox.setBounds (settings.removeFromLeft (58).reduced (2));
    trackSummaryLabel.setBounds (settings.reduced (8, 2));

    actionStatusLabel.setBounds (area.removeFromTop (32).reduced (4, 2));
    timeline.setBounds (area.removeFromTop (230));
    deviceSelector.setBounds (area);
}

void MainComponent::timerCallback()
{
    const auto seconds = engineController.positionSeconds();
    timeline.setPosition (seconds);
    positionLabel.setText (juce::String (seconds, 3) + " s", juce::dontSendNotification);
    playButton.setEnabled (! engineController.isPlaying());
    stopButton.setEnabled (engineController.isPlaying() || seconds > 0.0);
    repaint();
}

void MainComponent::addTrack()
{
    const auto number = session.tracks().getNumChildren() + 1;
    app::AddTrackCommand command ("audio", "Audio " + juce::String (number));
    const auto added = commands.dispatch (command);
    actionStatusLabel.setText (added ? "Added Audio " + juce::String (number)
                                     : "Could not add the audio track.",
                               juce::dontSendNotification);
    refreshTrackSummary();
}

void MainComponent::removeLastTrack()
{
    const auto trackCount = session.tracks().getNumChildren();
    if (trackCount == 0)
        return;

    const auto lastTrack = session.tracks().getChild (trackCount - 1);
    if (engineController.removeAudioTrack (trackCount - 1)
        && session.removeTrack (lastTrack.getProperty (core::Session::idProperty()).toString()))
    {
        actionStatusLabel.setText ("Removed " + lastTrack.getProperty ("name").toString(),
                                   juce::dontSendNotification);
        refreshTrackSummary();
    }
}

void MainComponent::importAudio()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Import audio into a new track", juce::File{}, engineController.audioFileWildcard());

    const auto chooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (! file.existsAsFile())
            return;

        const auto trackIndex = session.tracks().getNumChildren();
        const auto duration = engineController.importAudioFile (file, trackIndex);
        if (duration <= 0.0)
        {
            actionStatusLabel.setText ("Could not import " + file.getFileName(),
                                       juce::dontSendNotification);
            return;
        }

        app::ImportAudioCommand command (file, duration);
        if (commands.dispatch (command))
        {
            timelineLengthSeconds = juce::jmax (60.0, duration);
            timeline.setLength (timelineLengthSeconds);
            actionStatusLabel.setText ("Imported " + file.getFileName(),
                                       juce::dontSendNotification);
            refreshTrackSummary();

            actionStatusLabel.setText ("Imported " + file.getFileName() + "; detecting tempo...",
                                       juce::dontSendNotification);
            analysisPool.addJob ([safeThis = juce::Component::SafePointer<MainComponent> (this), file]
            {
                const auto result = services::detectTempo (file);
                juce::MessageManager::callAsync ([safeThis, result]
                {
                    if (safeThis == nullptr)
                        return;

                    if (result.bpm > 0.0)
                    {
                        // Detection synchronizes the metronome to the imported
                        // recording; it must not time-stretch that recording.
                        safeThis->engineController.setDetectedTempo (result.bpm);
                        safeThis->engineController.alignFirstBeat (result.firstBeatSeconds);
                        safeThis->tempoSlider.setValue (result.bpm, juce::dontSendNotification);
                        safeThis->session.state().setProperty ("tempo", result.bpm, nullptr);
                        safeThis->timeline.setMusicalGrid (
                            result.bpm,
                            safeThis->numeratorBox.getText().getIntValue(),
                            safeThis->denominatorBox.getText().getIntValue());
                        safeThis->actionStatusLabel.setText (
                            "Detected " + juce::String (result.bpm, 1)
                                + " BPM; original audio unchanged, metronome synchronized.",
                            juce::dontSendNotification);
                    }
                    else
                    {
                        safeThis->actionStatusLabel.setText (
                            "Tempo could not be detected; using "
                                + juce::String (safeThis->tempoSlider.getValue(), 0) + " BPM.",
                            juce::dontSendNotification);
                    }
                });
            });
        }
    });
}

void MainComponent::showTapTempo()
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Tap Tempo";
    options.dialogBackgroundColour = juce::Colour (0xff181a20);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.content.setOwned (new TapTempoComponent ([safeThis = juce::Component::SafePointer<MainComponent> (this)] (double bpm)
    {
        if (safeThis == nullptr)
            return;

        safeThis->engineController.setTapTempo (bpm);
        safeThis->tempoSlider.setValue (bpm, juce::dontSendNotification);
        safeThis->engineController.setMetronomeEnabled (true);
        safeThis->metronomeButton.setToggleState (true, juce::dontSendNotification);
        safeThis->actionStatusLabel.setText (
            "Tap tempo applied: " + juce::String (bpm, 1)
                + " BPM. Song and playhead unchanged.",
            juce::dontSendNotification);
    }));
    options.launchAsync();
}

void MainComponent::refreshTrackSummary()
{
    timeline.setSessionTracks (session.tracks());
    trackSummaryLabel.setText (juce::String (session.tracks().getNumChildren()) + " session tracks",
                               juce::dontSendNotification);
    undoButton.setEnabled (session.undoManager().canUndo());
    redoButton.setEnabled (session.undoManager().canRedo());
    repaint();
}

} // namespace saamveda::ui
