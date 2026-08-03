#include "MainComponent.h"

namespace saamveda::ui
{

using namespace app;

namespace
{
    constexpr double shortSeekSeconds = 1.0;
    constexpr double longSeekSeconds = 15.0;

    /** Read-only listing of every registered command and its keys, built from
        the command manager so a shortcut cannot drift from its documentation. */
    class ShortcutListComponent final : public juce::Component
    {
    public:
        explicit ShortcutListComponent (juce::ApplicationCommandManager& manager)
        {
            juce::String text;

            for (const auto* category : { CommandCategories::transport, CommandCategories::edit,
                                          CommandCategories::tracks, CommandCategories::view,
                                          CommandCategories::tools })
            {
                text << (text.isEmpty() ? "" : "\n") << juce::String (category).toUpperCase() << "\n";

                for (const auto id : manager.getCommandsInCategory (category))
                {
                    const auto* info = manager.getCommandForID (id);
                    if (info == nullptr)
                        continue;

                    juce::StringArray keys;
                    for (const auto& key : manager.getKeyMappings()->getKeyPressesAssignedToCommand (id))
                        keys.add (key.getTextDescription());

                    text << "  " << info->shortName.paddedRight (' ', 26)
                         << (keys.isEmpty() ? juce::String ("(no shortcut)") : keys.joinIntoString (" / "))
                         << "\n";
                }
            }

            display.setMultiLine (true);
            display.setReadOnly (true);
            display.setScrollbarsShown (true);
            display.setCaretVisible (false);
            display.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                                            13.0f, juce::Font::plain)));
            display.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff181a20));
            display.setColour (juce::TextEditor::textColourId, juce::Colour (0xffd6dae3));
            display.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff323744));
            display.setText (text, false);

            addAndMakeVisible (display);
            setSize (460, 520);
        }

        void resized() override { display.setBounds (getLocalBounds().reduced (8)); }

    private:
        juce::TextEditor display;
    };
}

//==============================================================================
MainComponent::MainComponent()
    : deviceSelector (engineController.audioDeviceManager(),
                      0, 2, 0, 2,
                      false, false, true, false)
{
    playButton.onClick        = [this] { engineController.play(); updateTransportButtons(); };
    stopButton.onClick        = [this] { engineController.stop(); updateTransportButtons(); };
    loopButton.onClick        = [this] { engineController.setLooping (loopButton.getToggleState()); };
    tapTempoButton.onClick    = [this] { showTapTempo(); };
    addTrackButton.onClick    = [this] { addTrack(); };
    importAudioButton.onClick = [this] { importAudio(); };
    removeTrackButton.onClick = [this] { removeLastTrack(); };
    undoButton.onClick        = [this] { performUndo(); };
    redoButton.onClick        = [this] { performRedo(); };
    shortcutsButton.onClick   = [this] { showShortcuts(); };

    metronomeButton.onClick = [this]
    {
        engineController.setMetronomeEnabled (metronomeButton.getToggleState());
        setStatus (metronomeButton.getToggleState() ? "Metronome enabled." : "Metronome disabled.");
    };

    timeline.onSeek = [this] (double seconds)
    {
        engineController.seek (seconds);
        setStatus ("Moved playhead to " + juce::String (seconds, 2) + " s");
    };

    tempoSlider.setRange (20.0, 400.0, 1.0);
    tempoSlider.setValue (session.tempo(), juce::dontSendNotification);
    tempoSlider.setTextValueSuffix (" BPM");
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 24);

    // Dragging updates the engine live but commits one undoable step at the
    // end, so a single gesture is one Ctrl+Z rather than a hundred.
    tempoSlider.onDragStart = [this] { tempoSliderIsDragging = true; };
    tempoSlider.onDragEnd = [this]
    {
        tempoSliderIsDragging = false;
        commitTempo (tempoSlider.getValue());
    };
    tempoSlider.onValueChange = [this]
    {
        if (tempoSliderIsDragging)
        {
            engineController.setTempo (tempoSlider.getValue());
            timeline.setMusicalGrid (tempoSlider.getValue(), numeratorBox.getSelectedId(),
                                     denominatorBox.getSelectedId());
        }
        else
        {
            commitTempo (tempoSlider.getValue());
        }
    };

    for (int value = 1; value <= 16; ++value)
        numeratorBox.addItem (juce::String (value), value);
    for (auto value : { 1, 2, 4, 8, 16, 32 })
        denominatorBox.addItem (juce::String (value), value);
    numeratorBox.setSelectedId (session.timeSignatureNumerator(), juce::dontSendNotification);
    denominatorBox.setSelectedId (session.timeSignatureDenominator(), juce::dontSendNotification);
    numeratorBox.onChange = [this] { commitTimeSignature(); };
    denominatorBox.onChange = [this] { commitTimeSignature(); };

    tempoLabel.setText ("Tempo BPM", juce::dontSendNotification);
    timeSignatureLabel.setText ("Time signature", juce::dontSendNotification);
    positionLabel.setJustificationType (juce::Justification::centredLeft);
    trackSummaryLabel.setJustificationType (juce::Justification::centredLeft);
    actionStatusLabel.setJustificationType (juce::Justification::centredLeft);
    actionStatusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffaeb4c3));
    realtimeStatusLabel.setJustificationType (juce::Justification::centredRight);
    realtimeStatusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff858b99));
    setStatus ("Add a track to begin. Space plays, Enter returns to the start, Ctrl+Z undoes.");

    for (auto* component : std::initializer_list<juce::Component*>
         { &playButton, &stopButton, &loopButton, &metronomeButton, &tapTempoButton,
           &addTrackButton, &importAudioButton, &removeTrackButton,
           &undoButton, &redoButton, &shortcutsButton,
           &tempoSlider, &numeratorBox, &denominatorBox, &tempoLabel,
           &timeSignatureLabel, &positionLabel, &trackSummaryLabel, &actionStatusLabel,
           &realtimeStatusLabel, &timeline, &deviceSelector })
        addAndMakeVisible (component);

    // Buttons must never hold keyboard focus, or Space and Enter would activate
    // whichever one was clicked last instead of driving the transport.
    for (auto* button : std::initializer_list<juce::Button*>
         { &playButton, &stopButton, &loopButton, &metronomeButton, &tapTempoButton,
           &addTrackButton, &importAudioButton, &removeTrackButton,
           &undoButton, &redoButton, &shortcutsButton })
        button->setWantsKeyboardFocus (false);

    commandManager.registerAllCommandsForTarget (this);
    addKeyListener (commandManager.getKeyMappings());
    setWantsKeyboardFocus (true);

    timeline.setLength (timelineLengthSeconds);
    applySessionToUi();
    startTimerHz (30);

    // Wide enough for the transport row's left and right groups not to collide;
    // resized() lays both out from the edges and cannot reflow.
    setSize (1024, 720);
}

MainComponent::~MainComponent()
{
    stopTimer();
    analysisPool.removeAllJobs (true, 5000);
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181a20));
}

void MainComponent::mouseDown (const juce::MouseEvent&)
{
    // Clicking the background takes focus back from the tempo box or a device
    // combo so the transport keys work again.
    grabKeyboardFocus();
}

void MainComponent::parentHierarchyChanged()
{
    ensureKeyboardFocus();
}

void MainComponent::ensureKeyboardFocus()
{
    // JUCE dispatches a key press by walking up from the currently focused
    // component. With nothing focused there is no walk and no key listener is
    // ever consulted, so every shortcut silently does nothing. Nothing takes
    // focus by default here: the buttons are explicitly excluded, and the text
    // fields only take it when clicked.
    if (! isShowing() || juce::Component::getCurrentlyFocusedComponent() != nullptr)
        return;

    // Only claim focus when the OS already considers this window active, so
    // this can never pull focus away from another application.
    if (auto* peer = getPeer())
        if (peer->isFocused())
            grabKeyboardFocus();
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (12);

    // Two rows: transport and track operations on the first, musical settings
    // and history on the second. Everything is laid out from both edges, so the
    // two halves of a row must not exceed its width - see the width guard on
    // the default window size in the constructor.
    auto transport = area.removeFromTop (34);
    playButton.setBounds (transport.removeFromLeft (72).reduced (2));
    stopButton.setBounds (transport.removeFromLeft (72).reduced (2));
    loopButton.setBounds (transport.removeFromLeft (66).reduced (2));
    metronomeButton.setBounds (transport.removeFromLeft (102).reduced (2));
    tapTempoButton.setBounds (transport.removeFromLeft (92).reduced (2));
    positionLabel.setBounds (transport.removeFromLeft (96).reduced (4, 2));
    addTrackButton.setBounds (transport.removeFromRight (130).reduced (2));
    importAudioButton.setBounds (transport.removeFromRight (116).reduced (2));
    removeTrackButton.setBounds (transport.removeFromRight (130).reduced (2));

    auto settings = area.removeFromTop (34);
    tempoLabel.setBounds (settings.removeFromLeft (66));
    tempoSlider.setBounds (settings.removeFromLeft (190).reduced (2));
    settings.removeFromLeft (12);
    timeSignatureLabel.setBounds (settings.removeFromLeft (94));
    numeratorBox.setBounds (settings.removeFromLeft (58).reduced (2));
    denominatorBox.setBounds (settings.removeFromLeft (58).reduced (2));
    shortcutsButton.setBounds (settings.removeFromRight (55).reduced (2));
    redoButton.setBounds (settings.removeFromRight (60).reduced (2));
    undoButton.setBounds (settings.removeFromRight (60).reduced (2));
    trackSummaryLabel.setBounds (settings.reduced (8, 2));

    auto status = area.removeFromTop (30);
    realtimeStatusLabel.setBounds (status.removeFromRight (270).reduced (4, 2));
    actionStatusLabel.setBounds (status.reduced (4, 2));

    timeline.setBounds (area.removeFromTop (272));
    area.removeFromTop (8);
    deviceSelector.setBounds (area);
}

void MainComponent::timerCallback()
{
    ensureKeyboardFocus();

    const auto seconds = engineController.positionSeconds();
    timeline.setPosition (seconds);
    positionLabel.setText (juce::String (seconds, 3) + " s", juce::dontSendNotification);
    updateTransportButtons();

    const auto realtime = engineController.realtimeReport();
    if (! realtime.available)
        realtimeStatusLabel.setText ("RT check: release build", juce::dontSendNotification);
    else if (realtime.allocations == 0)
        realtimeStatusLabel.setText (realtime.armed ? "RT check: clean" : "RT check: warming up",
                                     juce::dontSendNotification);
    else
        realtimeStatusLabel.setText ("RT ALLOCATIONS: " + juce::String (realtime.allocations)
                                         + " (max " + juce::String (static_cast<int> (
                                               realtime.largestAllocationBytes)) + " B)",
                                     juce::dontSendNotification);
}

void MainComponent::updateTransportButtons()
{
    const auto playing = engineController.isPlaying();
    playButton.setEnabled (! playing);
    stopButton.setEnabled (playing || engineController.positionSeconds() > 0.0);
    loopButton.setToggleState (engineController.isLooping(), juce::dontSendNotification);
    metronomeButton.setToggleState (engineController.isMetronomeEnabled(),
                                    juce::dontSendNotification);
    commandManager.commandStatusChanged();
}

void MainComponent::setStatus (const juce::String& message)
{
    actionStatusLabel.setText (message, juce::dontSendNotification);
}

//==============================================================================
void MainComponent::addTrack()
{
    const auto number = session.tracks().getNumChildren() + 1;
    AddTrackCommand command ("audio", "Audio " + juce::String (number));

    if (commands.dispatch (command) && engineController.ensureTrack (command.id()))
        setStatus ("Added Audio " + juce::String (number));
    else
        setStatus ("Could not add the audio track.");

    refreshTrackSummary();
}

void MainComponent::removeLastTrack()
{
    const auto trackCount = session.tracks().getNumChildren();
    if (trackCount == 0)
    {
        setStatus ("There are no tracks to remove.");
        return;
    }

    const auto lastTrack = session.tracks().getChild (trackCount - 1);
    const auto trackId = lastTrack.getProperty (core::Session::idProperty()).toString();
    const auto trackName = lastTrack.getProperty ("name").toString();

    RemoveTrackCommand command (trackId);
    if (commands.dispatch (command))
    {
        engineController.removeTrack (trackId);
        setStatus ("Removed " + trackName + " (Ctrl+Z to restore).");
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
        grabKeyboardFocus();

        const auto file = chooser.getResult();
        if (! file.existsAsFile())
            return;

        ImportAudioCommand command (file, 0.0);
        if (! commands.dispatch (command))
        {
            setStatus ("Could not import " + file.getFileName());
            return;
        }

        const auto duration = engineController.importAudioFile (file, command.createdTrackId(),
                                                                command.createdClipId());
        if (duration <= 0.0)
        {
            // The engine rejected the file, so roll the session back rather than
            // leaving a track pointing at audio that will not play.
            commands.undo();
            engineController.synchronise (session);
            setStatus ("Could not import " + file.getFileName());
            refreshTrackSummary();
            return;
        }

        // The command recorded a placeholder length; the engine knows the real
        // one, so write it back through the same undo transaction.
        auto clip = session.clipsOf (session.trackWithId (command.createdTrackId()))
                        .getChildWithProperty (core::Session::idProperty(), command.createdClipId());
        if (clip.isValid())
            clip.setProperty ("length", duration, &session.undoManager());

        timelineLengthSeconds = juce::jmax (60.0, engineController.contentLengthSeconds());
        timeline.setLength (timelineLengthSeconds);
        refreshTrackSummary();
        setStatus ("Imported " + file.getFileName() + "; detecting tempo...");

        analysisPool.addJob ([safeThis = juce::Component::SafePointer<MainComponent> (this), file]
        {
            const auto result = services::detectTempo (file);

            juce::MessageManager::callAsync ([safeThis, result]
            {
                if (safeThis == nullptr)
                    return;

                if (result.bpm <= 0.0)
                {
                    safeThis->setStatus ("Tempo could not be detected; using "
                                         + juce::String (safeThis->session.tempo(), 0) + " BPM.");
                    return;
                }

                // Detection synchronizes the metronome to the imported
                // recording; it must not time-stretch that recording.
                safeThis->engineController.setDetectedTempo (result.bpm);
                safeThis->engineController.alignFirstBeat (result.firstBeatSeconds);

                // This lands long after the import transaction closed, and the
                // user may have edited since. Going through the bus gives it
                // its own undo step instead of silently joining whatever
                // transaction happens to be open.
                SetTempoCommand tempoCommand (result.bpm);
                safeThis->commands.dispatch (tempoCommand);
                safeThis->applySessionToUi();
                safeThis->setStatus ("Detected " + juce::String (result.bpm, 1)
                                     + " BPM; original audio unchanged, metronome synchronized.");
            });
        });
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
    options.content.setOwned (new TapTempoComponent (
        [safeThis = juce::Component::SafePointer<MainComponent> (this)] (double bpm)
        {
            if (safeThis == nullptr)
                return;

            safeThis->engineController.setTapTempo (bpm);
            SetTempoCommand tempoCommand (bpm);
            safeThis->commands.dispatch (tempoCommand);
            safeThis->engineController.setMetronomeEnabled (true);
            safeThis->applySessionToUi();
            safeThis->setStatus ("Tap tempo applied: " + juce::String (bpm, 1)
                                 + " BPM. Song and playhead unchanged.");
            safeThis->grabKeyboardFocus();
        }));

    options.launchAsync();
}

void MainComponent::showShortcuts()
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Keyboard Shortcuts";
    options.dialogBackgroundColour = juce::Colour (0xff181a20);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.content.setOwned (new ShortcutListComponent (commandManager));
    options.launchAsync();
}

//==============================================================================
void MainComponent::performUndo()
{
    const auto description = commands.undoDescription();
    if (! commands.undo())
    {
        setStatus ("Nothing to undo.");
        return;
    }

    engineController.synchronise (session);
    applySessionToUi();
    setStatus ("Undo: " + (description.isEmpty() ? juce::String ("last change") : description));
}

void MainComponent::performRedo()
{
    const auto description = commands.redoDescription();
    if (! commands.redo())
    {
        setStatus ("Nothing to redo.");
        return;
    }

    engineController.synchronise (session);
    applySessionToUi();
    setStatus ("Redo: " + (description.isEmpty() ? juce::String ("last change") : description));
}

void MainComponent::commitTempo (double bpm)
{
    SetTempoCommand command (bpm);
    if (! commands.dispatch (command))
        return;

    engineController.setTempo (session.tempo());
    applySessionToUi();
    setStatus ("Tempo changed to " + juce::String (session.tempo(), 0) + " BPM.");
}

void MainComponent::commitTimeSignature()
{
    SetTimeSignatureCommand command (numeratorBox.getSelectedId(), denominatorBox.getSelectedId());
    if (! commands.dispatch (command))
        return;

    engineController.setTimeSignature (session.timeSignatureNumerator(),
                                       session.timeSignatureDenominator());
    applySessionToUi();
    setStatus ("Time signature changed to "
               + juce::String (session.timeSignatureNumerator()) + "/"
               + juce::String (session.timeSignatureDenominator()) + ".");
}

void MainComponent::startLoopPlay()
{
    engineController.setLooping (true);
    if (! engineController.isPlaying())
        engineController.play();

    updateTransportButtons();
    setStatus ("Loop play.");
}

//==============================================================================
void MainComponent::applySessionToUi()
{
    tempoSlider.setValue (session.tempo(), juce::dontSendNotification);
    numeratorBox.setSelectedId (session.timeSignatureNumerator(), juce::dontSendNotification);
    denominatorBox.setSelectedId (session.timeSignatureDenominator(), juce::dontSendNotification);
    timeline.setMusicalGrid (session.tempo(), session.timeSignatureNumerator(),
                             session.timeSignatureDenominator());
    refreshTrackSummary();
}

void MainComponent::refreshTrackSummary()
{
    timeline.setSessionTracks (session.tracks());
    timelineLengthSeconds = juce::jmax (60.0, engineController.contentLengthSeconds());
    timeline.setLength (timelineLengthSeconds);
    trackSummaryLabel.setText (juce::String (session.tracks().getNumChildren()) + " session tracks",
                               juce::dontSendNotification);
    undoButton.setEnabled (commands.canUndo());
    redoButton.setEnabled (commands.canRedo());
    commandManager.commandStatusChanged();
    repaint();
}

//==============================================================================
void MainComponent::getAllCommands (juce::Array<juce::CommandID>& target)
{
    target.addArray ({
        CommandIDs::playStop, CommandIDs::stopAndReturnToStart, CommandIDs::toggleLoop,
        CommandIDs::loopPlay, CommandIDs::toggleMetronome, CommandIDs::skipToStart,
        CommandIDs::skipToEnd, CommandIDs::shortSeekBack, CommandIDs::shortSeekForward,
        CommandIDs::longSeekBack, CommandIDs::longSeekForward,
        CommandIDs::undo, CommandIDs::redo,
        CommandIDs::newAudioTrack, CommandIDs::importAudio, CommandIDs::removeLastTrack,
        CommandIDs::zoomIn, CommandIDs::zoomOut, CommandIDs::zoomNormal, CommandIDs::zoomToFit,
        CommandIDs::toggleFollowPlayhead,
        CommandIDs::showTapTempo });
}

void MainComponent::getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result)
{
    using juce::KeyPress;
    using Mods = juce::ModifierKeys;

    switch (commandID)
    {
        case CommandIDs::playStop:
            result.setInfo ("Play / Stop", "Start or stop playback at the playhead",
                            CommandCategories::transport, 0);
            result.addDefaultKeypress (KeyPress::spaceKey, Mods::noModifiers);
            break;

        case CommandIDs::stopAndReturnToStart:
            result.setInfo ("Stop and Return to Start", "Stop playback and move the playhead to 0",
                            CommandCategories::transport, 0);
            result.addDefaultKeypress (KeyPress::returnKey, Mods::noModifiers);
            break;

        case CommandIDs::toggleLoop:
            result.setInfo ("Enable Looping", "Toggle transport looping",
                            CommandCategories::transport, 0);
            result.addDefaultKeypress ('l', Mods::noModifiers);
            result.setTicked (engineController.isLooping());
            break;

        case CommandIDs::loopPlay:
            result.setInfo ("Loop Play", "Play with looping enabled",
                            CommandCategories::transport, 0);
            result.addDefaultKeypress (KeyPress::spaceKey, Mods::shiftModifier);
            break;

        case CommandIDs::toggleMetronome:
            result.setInfo ("Metronome", "Toggle the click track",
                            CommandCategories::transport, 0);
            result.addDefaultKeypress ('m', Mods::noModifiers);
            result.setTicked (engineController.isMetronomeEnabled());
            break;

        case CommandIDs::skipToStart:
            result.setInfo ("Skip to Start", "Move the playhead to the beginning",
                            CommandCategories::transport, 0);
            result.addDefaultKeypress (KeyPress::homeKey, Mods::noModifiers);
            break;

        case CommandIDs::skipToEnd:
            result.setInfo ("Skip to End", "Move the playhead to the end of the audio",
                            CommandCategories::transport, 0);
            result.addDefaultKeypress (KeyPress::endKey, Mods::noModifiers);
            break;

        case CommandIDs::shortSeekBack:
            result.setInfo ("Short Seek Left", "Move the playhead back one second",
                            CommandCategories::transport, 0);
            result.addDefaultKeypress (KeyPress::leftKey, Mods::noModifiers);
            break;

        case CommandIDs::shortSeekForward:
            result.setInfo ("Short Seek Right", "Move the playhead forward one second",
                            CommandCategories::transport, 0);
            result.addDefaultKeypress (KeyPress::rightKey, Mods::noModifiers);
            break;

        case CommandIDs::longSeekBack:
            result.setInfo ("Long Seek Left", "Move the playhead back fifteen seconds",
                            CommandCategories::transport, 0);
            result.addDefaultKeypress (KeyPress::leftKey, Mods::shiftModifier);
            break;

        case CommandIDs::longSeekForward:
            result.setInfo ("Long Seek Right", "Move the playhead forward fifteen seconds",
                            CommandCategories::transport, 0);
            result.addDefaultKeypress (KeyPress::rightKey, Mods::shiftModifier);
            break;

        case CommandIDs::undo:
            result.setInfo ("Undo", "Undo the last change", CommandCategories::edit, 0);
            result.addDefaultKeypress ('z', Mods::ctrlModifier);
            result.setActive (commands.canUndo());
            break;

        case CommandIDs::redo:
            result.setInfo ("Redo", "Redo the last undone change", CommandCategories::edit, 0);
            result.addDefaultKeypress ('z', Mods::ctrlModifier | Mods::shiftModifier);
            result.addDefaultKeypress ('y', Mods::ctrlModifier);
            result.setActive (commands.canRedo());
            break;

        case CommandIDs::newAudioTrack:
            result.setInfo ("New Audio Track", "Add an empty audio track",
                            CommandCategories::tracks, 0);
            result.addDefaultKeypress ('n', Mods::ctrlModifier | Mods::shiftModifier);
            break;

        case CommandIDs::importAudio:
            result.setInfo ("Import Audio...", "Import an audio file onto a new track",
                            CommandCategories::tracks, 0);
            result.addDefaultKeypress ('i', Mods::ctrlModifier | Mods::shiftModifier);
            break;

        case CommandIDs::removeLastTrack:
            // Deliberately unbound. It is undoable, but a destructive action a
            // single stray keystroke away is a bad trade in a recording tool.
            result.setInfo ("Remove Last Track", "Delete the most recently added track",
                            CommandCategories::tracks, 0);
            result.setActive (session.tracks().getNumChildren() > 0);
            break;

        case CommandIDs::zoomIn:
            result.setInfo ("Zoom In", "Show a shorter span of time",
                            CommandCategories::view, 0);
            result.addDefaultKeypress ('1', Mods::ctrlModifier);
            break;

        case CommandIDs::zoomOut:
            result.setInfo ("Zoom Out", "Show a longer span of time",
                            CommandCategories::view, 0);
            result.addDefaultKeypress ('3', Mods::ctrlModifier);
            break;

        case CommandIDs::zoomNormal:
            result.setInfo ("Zoom Normal", "Return to the default zoom",
                            CommandCategories::view, 0);
            result.addDefaultKeypress ('2', Mods::ctrlModifier);
            break;

        case CommandIDs::zoomToFit:
            result.setInfo ("Fit to Window", "Show the whole project",
                            CommandCategories::view, 0);
            result.addDefaultKeypress ('f', Mods::ctrlModifier);
            break;

        case CommandIDs::toggleFollowPlayhead:
            result.setInfo ("Follow Playhead", "Scroll the view to keep the playhead visible",
                            CommandCategories::view, 0);
            result.addDefaultKeypress ('f', Mods::ctrlModifier | Mods::shiftModifier);
            result.setTicked (timeline.isFollowingPlayhead());
            break;

        case CommandIDs::showTapTempo:
            result.setInfo ("Tap Tempo...", "Open the tap tempo panel",
                            CommandCategories::tools, 0);
            result.addDefaultKeypress ('t', Mods::noModifiers);
            break;

        default:
            break;
    }
}

bool MainComponent::perform (const InvocationInfo& info)
{
    switch (info.commandID)
    {
        case CommandIDs::playStop:
            engineController.togglePlayStop();
            updateTransportButtons();
            setStatus (engineController.isPlaying() ? "Playing." : "Stopped.");
            break;

        case CommandIDs::stopAndReturnToStart:
            engineController.stopAndReturnToStart();
            timeline.setPosition (0.0);
            updateTransportButtons();
            setStatus ("Stopped; playhead returned to the start.");
            break;

        case CommandIDs::toggleLoop:
            engineController.toggleLooping();
            updateTransportButtons();
            setStatus (engineController.isLooping() ? "Looping enabled." : "Looping disabled.");
            break;

        case CommandIDs::loopPlay:
            startLoopPlay();
            break;

        case CommandIDs::toggleMetronome:
            engineController.toggleMetronome();
            updateTransportButtons();
            setStatus (engineController.isMetronomeEnabled() ? "Metronome enabled."
                                                             : "Metronome disabled.");
            break;

        case CommandIDs::skipToStart:
            engineController.seek (0.0);
            timeline.setPosition (0.0);
            setStatus ("Playhead at the start.");
            break;

        case CommandIDs::skipToEnd:
        {
            const auto end = engineController.contentLengthSeconds();
            engineController.seek (end);
            timeline.setPosition (end);
            setStatus ("Playhead at " + juce::String (end, 2) + " s.");
            break;
        }

        case CommandIDs::shortSeekBack:     engineController.nudge (-shortSeekSeconds); break;
        case CommandIDs::shortSeekForward:  engineController.nudge (shortSeekSeconds);  break;
        case CommandIDs::longSeekBack:      engineController.nudge (-longSeekSeconds);  break;
        case CommandIDs::longSeekForward:   engineController.nudge (longSeekSeconds);   break;

        case CommandIDs::undo:              performUndo(); break;
        case CommandIDs::redo:              performRedo(); break;

        case CommandIDs::newAudioTrack:     addTrack();       break;
        case CommandIDs::importAudio:       importAudio();    break;
        case CommandIDs::removeLastTrack:   removeLastTrack(); break;

        case CommandIDs::zoomIn:
            timeline.zoomIn();
            setStatus ("Zoom: " + juce::String (timeline.visibleSpanSeconds(), 2) + " s visible.");
            break;

        case CommandIDs::zoomOut:
            timeline.zoomOut();
            setStatus ("Zoom: " + juce::String (timeline.visibleSpanSeconds(), 2) + " s visible.");
            break;

        case CommandIDs::zoomNormal:
            timeline.zoomNormal();
            setStatus ("Zoom reset.");
            break;

        case CommandIDs::zoomToFit:
            timeline.zoomToFit();
            setStatus ("Fitted the project to the window.");
            break;

        case CommandIDs::toggleFollowPlayhead:
            timeline.setFollowPlayhead (! timeline.isFollowingPlayhead());
            commandManager.commandStatusChanged();
            setStatus (timeline.isFollowingPlayhead() ? "Following the playhead."
                                                      : "Playhead following off.");
            break;

        case CommandIDs::showTapTempo:      showTapTempo(); break;

        default:
            return false;
    }

    return true;
}

} // namespace saamveda::ui
