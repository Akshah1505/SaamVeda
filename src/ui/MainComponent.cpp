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
            display.setColour (juce::TextEditor::backgroundColourId, colours::windowBackground);
            display.setColour (juce::TextEditor::textColourId, colours::text);
            display.setColour (juce::TextEditor::outlineColourId, colours::outline);
            display.setText (text, false);

            addAndMakeVisible (display);
            setSize (460, 520);
        }

        void resized() override { display.setBounds (getLocalBounds().reduced (8)); }

    private:
        juce::TextEditor display;
    };

    /** Static text panel used for the About box. */
    class AboutComponent final : public juce::Component
    {
    public:
        AboutComponent()
        {
            text.setMultiLine (true);
            text.setReadOnly (true);
            text.setCaretVisible (false);
            text.setColour (juce::TextEditor::backgroundColourId, colours::windowBackground);
            text.setColour (juce::TextEditor::textColourId, colours::text);
            text.setColour (juce::TextEditor::outlineColourId, colours::outline);
            text.setText ("SaamVeda Studio 0.1.0\n\n"
                          "A digital audio workstation for Windows, built on JUCE 8 and\n"
                          "tracktion_engine.\n\n"
                          "Phase 2 of 13 - session model, transport, and arrangement view.\n"
                          "Recording, clip editing, plugins, piano roll, mixing, and export\n"
                          "arrive in later phases; see docs/09-roadmap.md.",
                          false);

            addAndMakeVisible (text);
            setSize (440, 220);
        }

        void resized() override { text.setBounds (getLocalBounds().reduced (8)); }

    private:
        juce::TextEditor text;
    };

    void launchDialog (const juce::String& title, juce::Component* content)
    {
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = title;
        options.dialogBackgroundColour = colours::windowBackground;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.content.setOwned (content);
        options.launchAsync();
    }
}

//==============================================================================
MainComponent::MainComponent()
{
    // ---- transport row ------------------------------------------------------
    transportBar.onPlayStop = [this]
    {
        engineController.togglePlayStop();
        updateTransportButtons();
        setStatus (engineController.isPlaying() ? "Playing." : "Stopped.");
    };
    transportBar.onStop = [this]
    {
        engineController.stop();
        updateTransportButtons();
        setStatus ("Stopped.");
    };
    transportBar.onToggleSongMode = [this]
    {
        setStatus ("Pattern mode arrives with the step sequencer in Phase 10.");
        transportBar.setSongMode (true);
    };
    transportBar.onTempoChanged = [this] (double bpm)
    {
        if (transportBar.isTempoBeingDragged())
        {
            engineController.setTempo (bpm);
            playlist.timeline().setMusicalGrid (bpm, session.timeSignatureNumerator(),
                                                session.timeSignatureDenominator());
        }
        else
        {
            commitTempo (bpm);
        }
    };
    transportBar.onTempoDragEnded = [this] { commitTempo (transportBar.tempo()); };
    transportBar.onTimeSignatureChanged = [this] (int numerator, int denominator)
    {
        commitTimeSignature (numerator, denominator);
    };

    // ---- tool row -----------------------------------------------------------
    toolBar.onAddTrack       = [this] { addTrack(); };
    toolBar.onImport         = [this] { importAudio(); };
    toolBar.onRemoveTrack    = [this] { removeLastTrack(); };
    toolBar.onUndo           = [this] { performUndo(); };
    toolBar.onRedo           = [this] { performRedo(); };
    toolBar.onTapTempo       = [this] { showTapTempo(); };
    toolBar.onShowShortcuts  = [this] { showShortcuts(); };
    toolBar.onLoopChanged    = [this] (bool shouldLoop)
    {
        engineController.setLooping (shouldLoop);
        setStatus (shouldLoop ? "Looping enabled." : "Looping disabled.");
    };
    toolBar.onMetronomeChanged = [this] (bool enabled)
    {
        engineController.setMetronomeEnabled (enabled);
        setStatus (enabled ? "Metronome enabled." : "Metronome disabled.");
    };

    // ---- playlist -----------------------------------------------------------
    playlist.timeline().onSeek = [this] (double seconds)
    {
        engineController.seek (seconds);
        setStatus ("Moved playhead to " + juce::String (seconds, 2) + " s");
    };
    playlist.timeline().onTrackSelected = [this] (int index)
    {
        if (index < 0)
        {
            playlist.setBreadcrumb ("Arrangement", selectedClipName);
            return;
        }

        const auto name = session.tracks().getChild (index).getProperty ("name").toString();
        toolBar.setContext ("Track", name);
        playlist.setBreadcrumb ("Arrangement", name);
    };
    playlist.timeline().onTrackMuteToggled = [this] (int index) { toggleTrackMute (index); };
    playlist.timeline().onTrackSoloToggled = [this] (int index) { toggleTrackSolo (index); };
    playlist.timeline().onTrackRenamed = [this] (int index, juce::String name)
    {
        renameTrack (index, name);
    };
    playlist.timeline().onClipMoved = [this] (int trackIndex, int clipIndex, double start)
    {
        moveClip (trackIndex, clipIndex, start);
    };

    playlist.timeline().setWaveformCache (&waveformCache);
    waveformCache.addChangeListener (this);

    playlist.browser().onClipSelected = [this] (const juce::String& name)
    {
        selectedClipName = name;
        toolBar.setContext ("Clip", name.isEmpty() ? "(none)" : name);
        playlist.setBreadcrumb ("Arrangement", name);
    };

    // ---- menus and commands -------------------------------------------------
    setLookAndFeel (&chromeLookAndFeel);
    commandManager.registerAllCommandsForTarget (this);
    addKeyListener (commandManager.getKeyMappings());
    setApplicationCommandManagerToWatch (&commandManager);
    menuBar.setModel (this);

    addAndMakeVisible (menuBar);
    addAndMakeVisible (transportBar);
    addAndMakeVisible (toolBar);
    addAndMakeVisible (playlist);

    setWantsKeyboardFocus (true);

    playlist.timeline().setLength (timelineLengthSeconds);
    applySessionToUi();
    setStatus ("Add a track to begin. Space plays, Enter returns to the start, Ctrl+Z undoes.");
    startTimerHz (30);

    // Wide enough for the transport and tool rows to lay out without their left
    // and right groups colliding; Main.cpp clamps this to the display.
    setSize (1280, 800);
}

MainComponent::~MainComponent()
{
    stopTimer();
    waveformCache.removeChangeListener (this);
    playlist.timeline().setWaveformCache (nullptr);
    setApplicationCommandManagerToWatch (nullptr);
    menuBar.setModel (nullptr);
    setLookAndFeel (nullptr);
    analysisPool.removeAllJobs (true, 5000);
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::windowBackground);

    // Backing behind the menu bar, which the menu itself does not fill.
    g.setColour (colours::chromeBackground);
    g.fillRect (getLocalBounds().withWidth (menuBar.getRight())
                    .withHeight (menuBar.getBottom()));
}

void MainComponent::mouseDown (const juce::MouseEvent&)
{
    // Clicking the background takes focus back from the tempo field or a combo
    // box so the transport keys work again.
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
    auto area = getLocalBounds();

    // The menu and transport share a row when there is room, as in the
    // reference layout. When there is not - docked beside another window, say -
    // the menu takes its own row rather than squeezing the transport until its
    // readouts fall off the edge.
    if (getWidth() >= layout::menuBarWidth + TransportBar::minimumUsefulWidth)
    {
        auto topRow = area.removeFromTop (layout::transportRowHeight);
        menuBar.setBounds (topRow.removeFromLeft (layout::menuBarWidth).withHeight (26));
        transportBar.setBounds (topRow);
    }
    else
    {
        menuBar.setBounds (area.removeFromTop (26));
        transportBar.setBounds (area.removeFromTop (layout::transportRowHeight));
    }

    toolBar.setBounds (area.removeFromTop (layout::toolRowHeight));
    playlist.setBounds (area.reduced (6, 6));
}

//==============================================================================
void MainComponent::timerCallback()
{
    ensureKeyboardFocus();

    const auto seconds = engineController.positionSeconds();
    playlist.timeline().setPosition (seconds);

    const auto beatSeconds = (60.0 / juce::jmax (1.0, session.tempo()))
                           * (4.0 / juce::jmax (1, session.timeSignatureDenominator()));
    const auto barSeconds = beatSeconds * juce::jmax (1, session.timeSignatureNumerator());
    const auto bar = static_cast<int> (seconds / barSeconds);
    const auto beatInBar = static_cast<int> (std::fmod (seconds, barSeconds) / beatSeconds);
    const auto tick = static_cast<int> (std::fmod (seconds, beatSeconds) / beatSeconds * 100.0);

    transportBar.setPosition (seconds, bar + 1, beatInBar + 1, tick);
    playlist.setMusicalReadout (session.tempo(), session.timeSignatureNumerator(),
                                session.timeSignatureDenominator(), bar + 1);
    playlist.refreshReadouts();
    updateTransportButtons();

    transportBar.setAudioLoad (engineController.audioDeviceManager().getCpuUsage());

    const auto realtime = engineController.realtimeReport();
    if (! realtime.available)
        toolBar.setNotification ("RT check: release build", false);
    else if (realtime.allocations == 0)
        toolBar.setNotification (realtime.armed ? "RT check: clean" : "RT check: warming up", false);
    else
        toolBar.setNotification ("RT allocations: " + juce::String (realtime.allocations)
                                     + " (max " + juce::String (static_cast<int> (
                                           realtime.largestAllocationBytes)) + " B)",
                                 true);
}

void MainComponent::updateTransportButtons()
{
    transportBar.setPlaying (engineController.isPlaying());
    transportBar.setSongMode (true);
    toolBar.setLoop (engineController.isLooping());
    toolBar.setMetronome (engineController.isMetronomeEnabled());
    toolBar.setHistoryEnabled (commands.canUndo(), commands.canRedo());
    commandManager.commandStatusChanged();
}

void MainComponent::setStatus (const juce::String& message)
{
    toolBar.setStatus (message);
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

void MainComponent::toggleTrackMute (int trackIndex)
{
    const auto track = session.tracks().getChild (trackIndex);
    if (! track.isValid())
        return;

    const auto trackId = track.getProperty (core::Session::idProperty()).toString();
    const auto trackName = track.getProperty ("name").toString();
    const auto shouldMute = ! session.isTrackMuted (trackId);

    SetTrackMuteCommand command (trackId, shouldMute);
    if (! commands.dispatch (command))
        return;

    engineController.setTrackMute (trackId, shouldMute);
    setStatus ((shouldMute ? "Muted " : "Unmuted ") + trackName + ".");
    refreshTrackSummary();
}

void MainComponent::toggleTrackSolo (int trackIndex)
{
    const auto track = session.tracks().getChild (trackIndex);
    if (! track.isValid())
        return;

    const auto trackId = track.getProperty (core::Session::idProperty()).toString();
    const auto trackName = track.getProperty ("name").toString();
    const auto shouldSolo = ! session.isTrackSoloed (trackId);

    SetTrackSoloCommand command (trackId, shouldSolo);
    if (! commands.dispatch (command))
        return;

    engineController.setTrackSolo (trackId, shouldSolo);
    setStatus (shouldSolo ? "Soloed " + trackName + "; everything else is silent."
                          : "Unsoloed " + trackName + ".");
    refreshTrackSummary();
}

void MainComponent::renameTrack (int trackIndex, const juce::String& newName)
{
    const auto track = session.tracks().getChild (trackIndex);
    if (! track.isValid())
        return;

    const auto trackId = track.getProperty (core::Session::idProperty()).toString();
    const auto previous = track.getProperty ("name").toString();
    if (previous == newName)
        return;

    RenameTrackCommand command (trackId, newName);
    if (! commands.dispatch (command))
        return;

    setStatus ("Renamed " + previous + " to " + newName + ".");
    refreshTrackSummary();
}

void MainComponent::moveClip (int trackIndex, int clipIndex, double newStartSeconds)
{
    const auto track = session.tracks().getChild (trackIndex);
    const auto clip = session.clipsOf (track).getChild (clipIndex);
    if (! clip.isValid())
        return;

    const auto clipId = clip.getProperty (core::Session::idProperty()).toString();
    const auto clipName = clip.getProperty ("name").toString();

    MoveClipCommand command (clipId, newStartSeconds);
    if (! commands.dispatch (command))
        return;

    engineController.setClipStart (clipId, session.clipStart (clipId));
    setStatus ("Moved " + clipName + " to " + juce::String (session.clipStart (clipId), 2) + " s.");
    refreshTrackSummary();
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // A thumbnail finished another chunk of its file; the lanes are the only
    // thing that shows it.
    playlist.timeline().repaint();
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

        // Decided now, not when detection returns: two imports in quick
        // succession would otherwise race and both believe they were first.
        const auto isFirstImport = session.clipCount() == 1;

        const auto duration = engineController.importAudioFile (
            file, command.createdTrackId(), command.createdClipId(),
            session.clipSourceTempo (command.createdClipId()), 0.0, 0.0);
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

        refreshTrackSummary();
        setStatus ("Imported " + file.getFileName() + "; detecting tempo...");

        const auto clipId = command.createdClipId();

        analysisPool.addJob ([safeThis = juce::Component::SafePointer<MainComponent> (this),
                              file, clipId, isFirstImport]
        {
            const auto result = services::detectTempo (file);

            juce::MessageManager::callAsync ([safeThis, result, clipId, isFirstImport]
            {
                if (safeThis == nullptr)
                    return;

                if (result.bpm <= 0.0)
                {
                    safeThis->setStatus ("Tempo could not be detected; using "
                                         + juce::String (safeThis->session.tempo(), 0) + " BPM.");
                    return;
                }

                // This lands long after the import transaction closed, and the
                // user may have edited since. Going through the bus gives it
                // its own undo step instead of silently joining whatever
                // transaction happens to be open.
                ApplyDetectedTempoCommand detectedCommand (clipId, result.bpm,
                                                           result.firstBeatSeconds, isFirstImport);
                safeThis->commands.dispatch (detectedCommand);

                // Detection describes the audio as imported; it is not a request
                // to alter it. Recording the project tempo as this clip's source
                // tempo leaves it at 1.0x either way.
                safeThis->engineController.applyDetectedTempo (
                    clipId,
                    isFirstImport ? safeThis->session.tempo() : 0.0,
                    safeThis->session.tempo(),
                    result.firstBeatSeconds);

                safeThis->applySessionToUi();

                safeThis->setStatus (
                    isFirstImport
                        ? "Detected " + juce::String (result.bpm, 1)
                              + " BPM; project tempo set, audio unchanged."
                        : "Detected " + juce::String (result.bpm, 1) + " BPM; project stays at "
                              + juce::String (safeThis->session.tempo(), 1)
                              + " BPM, audio unchanged.");
            });
        });
    });
}

void MainComponent::showTapTempo()
{
    launchDialog ("Tap Tempo", new TapTempoComponent (
        [safeThis = juce::Component::SafePointer<MainComponent> (this)] (double bpm)
        {
            if (safeThis == nullptr)
                return;

            safeThis->engineController.setTapTempo (bpm);
            SetTapTempoCommand tempoCommand (bpm);
            safeThis->commands.dispatch (tempoCommand);
            safeThis->engineController.setMetronomeEnabled (true);
            safeThis->applySessionToUi();
            safeThis->setStatus ("Tap tempo applied: " + juce::String (bpm, 1)
                                 + " BPM. Song and playhead unchanged.");
            safeThis->grabKeyboardFocus();
        }));
}

void MainComponent::showShortcuts()
{
    launchDialog ("Keyboard Shortcuts", new ShortcutListComponent (commandManager));
}

void MainComponent::showAudioSettingsDialog()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (
        engineController.audioDeviceManager(), 0, 2, 0, 2, false, false, true, false);
    selector->setSize (580, 420);
    launchDialog ("Audio Settings", selector);
}

void MainComponent::showAboutDialog()
{
    launchDialog ("About SaamVeda Studio", new AboutComponent());
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

void MainComponent::commitTimeSignature (int numerator, int denominator)
{
    SetTimeSignatureCommand command (numerator, denominator);
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
    transportBar.setTempo (session.tempo());
    transportBar.setTimeSignature (session.timeSignatureNumerator(),
                                   session.timeSignatureDenominator());
    playlist.timeline().setMusicalGrid (session.tempo(), session.timeSignatureNumerator(),
                                        session.timeSignatureDenominator());
    refreshTrackSummary();
}

void MainComponent::refreshTrackSummary()
{
    playlist.timeline().setSessionTracks (session.tracks());
    playlist.browser().setSessionTracks (session.tracks());

    // Release thumbnails for files no longer on the timeline, so undoing a long
    // series of imports does not leave their scans resident.
    juce::StringArray livePaths;
    const auto trackList = session.tracks();
    for (int i = 0; i < trackList.getNumChildren(); ++i)
    {
        const auto clips = session.clipsOf (trackList.getChild (i));
        for (int j = 0; j < clips.getNumChildren(); ++j)
            livePaths.addIfNotAlreadyThere (clips.getChild (j).getProperty ("sourceFile").toString());
    }
    waveformCache.retainOnly (livePaths);

    timelineLengthSeconds = juce::jmax (60.0, engineController.contentLengthSeconds());
    playlist.timeline().setLength (timelineLengthSeconds);

    toolBar.setHistoryEnabled (commands.canUndo(), commands.canRedo());
    commandManager.commandStatusChanged();
    repaint();
}

//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "FILE", "EDIT", "ADD", "VIEW", "OPTIONS", "TOOLS", "HELP" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int index, const juce::String&)
{
    juce::PopupMenu menu;

    switch (index)
    {
        case 0: // FILE
            menu.addItem (newProject, "New Project", false);
            menu.addItem (openProject, "Open Project...", false);
            menu.addItem (saveProject, "Save Project", false);
            menu.addSeparator();
            menu.addCommandItem (&commandManager, CommandIDs::importAudio);
            menu.addSeparator();
            menu.addItem (quitApplication, "Exit");
            break;

        case 1: // EDIT
            menu.addCommandItem (&commandManager, CommandIDs::undo);
            menu.addCommandItem (&commandManager, CommandIDs::redo);
            break;

        case 2: // ADD
            menu.addCommandItem (&commandManager, CommandIDs::newAudioTrack);
            menu.addCommandItem (&commandManager, CommandIDs::importAudio);
            menu.addSeparator();
            menu.addCommandItem (&commandManager, CommandIDs::removeLastTrack);
            break;

        case 3: // VIEW
            menu.addCommandItem (&commandManager, CommandIDs::zoomIn);
            menu.addCommandItem (&commandManager, CommandIDs::zoomOut);
            menu.addCommandItem (&commandManager, CommandIDs::zoomNormal);
            menu.addCommandItem (&commandManager, CommandIDs::zoomToFit);
            menu.addSeparator();
            menu.addCommandItem (&commandManager, CommandIDs::toggleFollowPlayhead);
            break;

        case 4: // OPTIONS
            menu.addItem (showAudioSettings, "Audio Settings...");
            menu.addSeparator();
            menu.addCommandItem (&commandManager, CommandIDs::toggleMetronome);
            menu.addCommandItem (&commandManager, CommandIDs::toggleLoop);
            break;

        case 5: // TOOLS
            menu.addCommandItem (&commandManager, CommandIDs::showTapTempo);
            break;

        case 6: // HELP
            menu.addCommandItem (&commandManager, CommandIDs::showShortcuts);
            menu.addSeparator();
            menu.addItem (showAbout, "About SaamVeda Studio");
            break;

        default:
            break;
    }

    return menu;
}

void MainComponent::menuItemSelected (int menuItemID, int)
{
    switch (menuItemID)
    {
        case newProject:
        case openProject:
        case saveProject:
            setStatus ("Project save and load arrive in Phase 12.");
            break;

        case quitApplication:
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            break;

        case showAudioSettings:
            showAudioSettingsDialog();
            break;

        case showAbout:
            showAboutDialog();
            break;

        default:
            break;
    }
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
        CommandIDs::showTapTempo, CommandIDs::showShortcuts });
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
            result.setTicked (playlist.timeline().isFollowingPlayhead());
            break;

        case CommandIDs::showTapTempo:
            result.setInfo ("Tap Tempo...", "Open the tap tempo panel",
                            CommandCategories::tools, 0);
            result.addDefaultKeypress ('t', Mods::noModifiers);
            break;

        case CommandIDs::showShortcuts:
            result.setInfo ("Keyboard Shortcuts...", "List every command and its keys",
                            CommandCategories::tools, 0);
            result.addDefaultKeypress (KeyPress::F1Key, Mods::noModifiers);
            break;

        default:
            break;
    }
}

bool MainComponent::perform (const InvocationInfo& info)
{
    auto& timeline = playlist.timeline();

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

        case CommandIDs::newAudioTrack:     addTrack();        break;
        case CommandIDs::importAudio:       importAudio();     break;
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

        case CommandIDs::showTapTempo:      showTapTempo();  break;
        case CommandIDs::showShortcuts:     showShortcuts(); break;

        default:
            return false;
    }

    return true;
}

} // namespace saamveda::ui
