#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace saamveda::app
{

/** Application command identifiers.

    Keyboard bindings live with the command definitions in
    MainComponent::getCommandInfo so a shortcut can never drift from the action
    it triggers. The default map follows Audacity where Audacity has an
    equivalent — see docs/15-keyboard-shortcuts.md.
*/
namespace CommandIDs
{
    enum
    {
        // Transport
        playStop = 0x5a00,
        stopAndReturnToStart,
        toggleLoop,
        loopPlay,
        toggleMetronome,
        toggleRecord,
        skipToStart,
        skipToEnd,
        shortSeekBack,
        shortSeekForward,
        longSeekBack,
        longSeekForward,

        // Edit
        undo,
        redo,

        // Tracks
        newAudioTrack,
        importAudio,
        removeLastTrack,

        // View
        zoomIn,
        zoomOut,
        zoomNormal,
        zoomToFit,
        toggleFollowPlayhead,

        // Tools
        showTapTempo,
        showShortcuts
    };
}

/** Command categories, used by the shortcut list and any future menu bar. */
namespace CommandCategories
{
    inline const char* const transport = "Transport";
    inline const char* const edit      = "Edit";
    inline const char* const tracks    = "Tracks";
    inline const char* const view      = "View";
    inline const char* const tools     = "Tools";
}

} // namespace saamveda::app
