#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "../app/CommandBus.h"
#include "../app/CommandIDs.h"
#include "../engine/EngineController.h"
#include "Layout.h"
#include "PlaylistPanel.h"
#include "TapTempoComponent.h"
#include "Toolbars.h"
#include "../services/TempoDetector.h"
#include "../services/WaveformCache.h"

namespace saamveda::ui
{

/** Application shell.

    Owns the session, the engine, and the command layer, and arranges the four
    bands of the window: menu bar and transport, tool row, and the Playlist
    panel. Audio device setup lives in a dialog off the Options menu rather than
    in the main view - it is a setup step, not a working surface.
*/
class MainComponent : public juce::Component,
                      public juce::ApplicationCommandTarget,
                      public juce::MenuBarModel,
                      private juce::Timer,
                      private juce::ChangeListener
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

    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int index, const juce::String& name) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

private:
    /** Menu entries that are not application commands. Kept well below the
        CommandIDs range so the two can never collide in a PopupMenu. */
    enum MenuItemIDs
    {
        newProject = 1,
        openProject,
        saveProject,
        quitApplication,
        showAudioSettings,
        showAbout
    };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    // Actions
    void addTrack();
    void removeLastTrack();
    void toggleTrackMute (int trackIndex);
    void toggleTrackSolo (int trackIndex);
    void toggleTrackArm (int trackIndex);
    void toggleRecording();
    void adoptRecordedClip (const juce::String& trackId, const juce::File& file,
                            double startSeconds, double lengthSeconds);
    void renameTrack (int trackIndex, const juce::String& newName);
    void moveClip (int trackIndex, int clipIndex, int targetTrackIndex, double newStartSeconds);
    void importAudio();
    void showTapTempo();
    void showShortcuts();
    void showAudioSettingsDialog();
    void showAboutDialog();
    void performUndo();
    void performRedo();
    void commitTempo (double bpm);
    void commitTimeSignature (int numerator, int denominator);
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
    services::WaveformCache waveformCache;
    juce::ApplicationCommandManager commandManager;

    // Declared before the widgets it styles: a LookAndFeel must outlive every
    // component pointing at it.
    ChromeLookAndFeel chromeLookAndFeel;
    juce::MenuBarComponent menuBar { this };
    TransportBar transportBar;
    ToolBar toolBar;
    PlaylistPanel playlist;

    double timelineLengthSeconds = 60.0;
    bool realtimeOffendersDumped = false;
    juce::String selectedClipName;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::ThreadPool analysisPool { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace saamveda::ui
