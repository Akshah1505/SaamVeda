#include <catch2/catch_test_macros.hpp>
#include <juce_data_structures/juce_data_structures.h>

#include "../src/app/CommandBus.h"

using saamveda::app::AddTrackCommand;
using saamveda::app::ApplyDetectedTempoCommand;
using saamveda::app::CommandBus;
using saamveda::app::ImportAudioCommand;
using saamveda::app::MoveClipCommand;
using saamveda::app::RemoveTrackCommand;
using saamveda::app::RenameTrackCommand;
using saamveda::app::SetTapTempoCommand;
using saamveda::app::SetTempoCommand;
using saamveda::app::SetTrackMuteCommand;
using saamveda::app::SetTrackSoloCommand;
using saamveda::app::SetTimeSignatureCommand;
using saamveda::core::Session;

namespace
{
    /** Imports a file and applies a detection result to it, the way
        MainComponent does: only the first import may move the project tempo. */
    juce::String importSongWithDetectedTempo (CommandBus& bus, Session& session,
                                              const juce::String& path, double detectedBpm,
                                              double firstBeatSeconds = 0.0)
    {
        ImportAudioCommand import (juce::File (path), 60.0);
        REQUIRE (bus.dispatch (import));

        const auto isFirstImport = session.clipCount() == 1;
        ApplyDetectedTempoCommand detected (import.createdClipId(), detectedBpm, firstBeatSeconds,
                                            isFirstImport);
        bus.dispatch (detected);
        return import.createdClipId();
    }
}

TEST_CASE ("new session has schema defaults", "[core]")
{
    Session session;
    REQUIRE (session.state().getType() == Session::sessionType());
    REQUIRE (static_cast<int> (session.state().getProperty ("version")) == 1);
    REQUIRE (session.tempo() == 120.0);
    REQUIRE (session.timeSignatureNumerator() == 4);
    REQUIRE (session.timeSignatureDenominator() == 4);
    REQUIRE (session.tracks().getNumChildren() == 0);
}

TEST_CASE ("commands mutate the session and undo restores it", "[core][undo]")
{
    Session session;
    CommandBus bus (session);
    AddTrackCommand add ("audio", "Guitar");

    REQUIRE (bus.dispatch (add));
    REQUIRE (session.tracks().getNumChildren() == 1);
    REQUIRE (session.tracks().getChild (0).getProperty ("name") == "Guitar");

    RenameTrackCommand rename (add.id(), "Lead Guitar");
    REQUIRE (bus.dispatch (rename));
    REQUIRE (session.tracks().getChild (0).getProperty ("name") == "Lead Guitar");

    REQUIRE (bus.undo());
    REQUIRE (session.tracks().getChild (0).getProperty ("name") == "Guitar");
    REQUIRE (bus.undo());
    REQUIRE (session.tracks().getNumChildren() == 0);
    REQUIRE (bus.redo());
    REQUIRE (session.tracks().getNumChildren() == 1);
}

TEST_CASE ("session serialises and reloads", "[core][persistence]")
{
    Session source;
    source.addTrack ("midi", "Bass");
    auto xml = source.state().createXml();
    REQUIRE (xml != nullptr);

    Session restored (juce::ValueTree::fromXml (*xml));
    REQUIRE (restored.tempo() == 120.0);
    REQUIRE (restored.tracks().getNumChildren() == 1);
    REQUIRE (restored.tracks().getChild (0).getProperty ("name") == "Bass");
}

TEST_CASE ("audio import command creates a track and clip", "[core][import]")
{
    Session session;
    CommandBus bus (session);
    ImportAudioCommand import (juce::File ("D:\\Music\\take.wav"), 12.5);

    REQUIRE (bus.dispatch (import));
    REQUIRE (session.tracks().getNumChildren() == 1);

    auto track = session.tracks().getChild (0);
    REQUIRE (track.getProperty ("name").toString() == "take");
    REQUIRE (track.getProperty (Session::idProperty()).toString() == import.createdTrackId());

    auto clips = session.clipsOf (track);
    REQUIRE (clips.getNumChildren() == 1);
    REQUIRE (clips.getChild (0).getProperty (Session::idProperty()).toString()
             == import.createdClipId());
    REQUIRE (static_cast<double> (clips.getChild (0).getProperty ("length")) == 12.5);

    REQUIRE (bus.undo());
    REQUIRE (session.tracks().getNumChildren() == 0);
}

TEST_CASE ("tracks are addressed by id, not index", "[core][identity]")
{
    Session session;
    const auto first = session.addTrack ("audio", "One");
    const auto second = session.addTrack ("audio", "Two");
    const auto firstId = first.getProperty (Session::idProperty()).toString();
    const auto secondId = second.getProperty (Session::idProperty()).toString();

    REQUIRE (firstId != secondId);
    REQUIRE (session.trackWithId (firstId).getProperty ("name") == "One");
    REQUIRE (session.trackWithId (secondId).getProperty ("name") == "Two");
    REQUIRE_FALSE (session.trackWithId ("not-a-real-id").isValid());

    // Removing the first must not make the second answer to the first's id.
    REQUIRE (session.removeTrack (firstId));
    REQUIRE_FALSE (session.trackWithId (firstId).isValid());
    REQUIRE (session.trackWithId (secondId).getProperty ("name") == "Two");
}

TEST_CASE ("remove track is undoable and restores position", "[core][undo]")
{
    Session session;
    CommandBus bus (session);

    AddTrackCommand addFirst ("audio", "One");
    AddTrackCommand addSecond ("audio", "Two");
    AddTrackCommand addThird ("audio", "Three");
    REQUIRE (bus.dispatch (addFirst));
    REQUIRE (bus.dispatch (addSecond));
    REQUIRE (bus.dispatch (addThird));

    RemoveTrackCommand remove (addSecond.id());
    REQUIRE (bus.dispatch (remove));
    REQUIRE (session.tracks().getNumChildren() == 2);
    REQUIRE (session.tracks().getChild (1).getProperty ("name") == "Three");

    REQUIRE (bus.undo());
    REQUIRE (session.tracks().getNumChildren() == 3);
    REQUIRE (session.tracks().getChild (1).getProperty ("name") == "Two");
}

TEST_CASE ("clips move along the timeline and the move is undoable", "[core][clip][undo]")
{
    Session session;
    CommandBus bus (session);

    ImportAudioCommand import (juce::File ("D:\\Music\\take.wav"), 12.5);
    REQUIRE (bus.dispatch (import));
    const auto clipId = import.createdClipId();
    REQUIRE (session.clipStart (clipId) == 0.0);

    MoveClipCommand move (clipId, 8.0);
    REQUIRE (bus.dispatch (move));
    REQUIRE (session.clipStart (clipId) == 8.0);

    // A drag that ends where it started is not an edit.
    MoveClipCommand again (clipId, 8.0);
    REQUIRE_FALSE (bus.dispatch (again));

    // Negative positions are clamped rather than rejected: a drag past zero
    // should stop at zero, not refuse to move.
    MoveClipCommand beforeZero (clipId, -5.0);
    REQUIRE (bus.dispatch (beforeZero));
    REQUIRE (session.clipStart (clipId) == 0.0);

    REQUIRE (bus.undo());
    REQUIRE (session.clipStart (clipId) == 8.0);
    REQUIRE (bus.undo());
    REQUIRE (session.clipStart (clipId) == 0.0);
}

TEST_CASE ("clips move between tracks and the move is undoable", "[core][clip][undo]")
{
    Session session;
    CommandBus bus (session);

    AddTrackCommand addFirst ("audio", "One");
    AddTrackCommand addSecond ("audio", "Two");
    REQUIRE (bus.dispatch (addFirst));
    REQUIRE (bus.dispatch (addSecond));

    const auto clip = session.addAudioClip (addFirst.id(), juce::File ("D:/Music/take.wav"), 9.0);
    const auto clipId = clip.getProperty (Session::idProperty()).toString();
    REQUIRE (session.trackIdContainingClip (clipId) == addFirst.id());

    // A diagonal drag is one gesture: new track and new position together, so
    // one Ctrl+Z puts both back.
    MoveClipCommand move (clipId, 4.0, addSecond.id());
    REQUIRE (bus.dispatch (move));
    REQUIRE (session.trackIdContainingClip (clipId) == addSecond.id());
    REQUIRE (session.clipStart (clipId) == 4.0);
    REQUIRE (session.clipsOf (session.trackWithId (addFirst.id())).getNumChildren() == 0);
    REQUIRE (session.clipsOf (session.trackWithId (addSecond.id())).getNumChildren() == 1);

    REQUIRE (bus.undo());
    REQUIRE (session.trackIdContainingClip (clipId) == addFirst.id());
    REQUIRE (session.clipStart (clipId) == 0.0);

    // The clip keeps its identity across the move - it is the same node, not a
    // copy, so its length and source survive.
    REQUIRE (static_cast<double> (session.clipWithId (clipId).getProperty ("length")) == 9.0);
}

TEST_CASE ("moving a clip onto the track it is already on is not an edit", "[core][clip]")
{
    Session session;
    CommandBus bus (session);

    AddTrackCommand add ("audio", "One");
    REQUIRE (bus.dispatch (add));
    const auto clip = session.addAudioClip (add.id(), juce::File ("D:/Music/a.wav"), 3.0);
    const auto clipId = clip.getProperty (Session::idProperty()).toString();

    REQUIRE_FALSE (session.moveClipToTrack (clipId, add.id()));
    REQUIRE_FALSE (session.moveClipToTrack (clipId, "not-a-real-track"));
    REQUIRE (session.trackIdContainingClip (clipId) == add.id());
}

TEST_CASE ("clip position survives serialisation", "[core][clip][persistence]")
{
    Session source;
    const auto track = source.addTrack ("audio", "Guitar");
    const auto trackId = track.getProperty (Session::idProperty()).toString();
    const auto clip = source.addAudioClip (trackId, juce::File ("D:\\Music\\a.wav"), 4.0);
    const auto clipId = clip.getProperty (Session::idProperty()).toString();
    REQUIRE (source.setClipStart (clipId, 12.25));

    auto xml = source.state().createXml();
    REQUIRE (xml != nullptr);

    Session restored (juce::ValueTree::fromXml (*xml));
    REQUIRE (restored.clipStart (clipId) == 12.25);
}

TEST_CASE ("track mute toggles and is undoable", "[core][mute]")
{
    Session session;
    CommandBus bus (session);

    AddTrackCommand addFirst ("audio", "One");
    AddTrackCommand addSecond ("audio", "Two");
    REQUIRE (bus.dispatch (addFirst));
    REQUIRE (bus.dispatch (addSecond));

    REQUIRE_FALSE (session.isTrackMuted (addFirst.id()));

    SetTrackMuteCommand mute (addFirst.id(), true);
    REQUIRE (bus.dispatch (mute));
    REQUIRE (session.isTrackMuted (addFirst.id()));

    // Muting one track must not touch its neighbour.
    REQUIRE_FALSE (session.isTrackMuted (addSecond.id()));

    REQUIRE (bus.undo());
    REQUIRE_FALSE (session.isTrackMuted (addFirst.id()));

    REQUIRE (bus.redo());
    REQUIRE (session.isTrackMuted (addFirst.id()));

    // Muting an already-muted track is not an edit, so it must not add a step.
    SetTrackMuteCommand again (addFirst.id(), true);
    REQUIRE_FALSE (bus.dispatch (again));

    SetTrackMuteCommand unmute (addFirst.id(), false);
    REQUIRE (bus.dispatch (unmute));
    REQUIRE_FALSE (session.isTrackMuted (addFirst.id()));
}

TEST_CASE ("track solo toggles independently of mute", "[core][solo]")
{
    Session session;
    CommandBus bus (session);

    AddTrackCommand addFirst ("audio", "One");
    AddTrackCommand addSecond ("audio", "Two");
    REQUIRE (bus.dispatch (addFirst));
    REQUIRE (bus.dispatch (addSecond));

    REQUIRE_FALSE (session.hasAnySoloedTrack());

    SetTrackSoloCommand solo (addSecond.id(), true);
    REQUIRE (bus.dispatch (solo));
    REQUIRE (session.isTrackSoloed (addSecond.id()));
    REQUIRE_FALSE (session.isTrackSoloed (addFirst.id()));
    REQUIRE (session.hasAnySoloedTrack());

    // Mute and solo are separate flags; setting one must not disturb the other.
    SetTrackMuteCommand mute (addSecond.id(), true);
    REQUIRE (bus.dispatch (mute));
    REQUIRE (session.isTrackSoloed (addSecond.id()));
    REQUIRE (session.isTrackMuted (addSecond.id()));

    REQUIRE (bus.undo());
    REQUIRE_FALSE (session.isTrackMuted (addSecond.id()));
    REQUIRE (session.isTrackSoloed (addSecond.id()));

    REQUIRE (bus.undo());
    REQUIRE_FALSE (session.hasAnySoloedTrack());
}

TEST_CASE ("mute survives serialisation", "[core][mute][persistence]")
{
    Session source;
    const auto track = source.addTrack ("audio", "Guitar");
    const auto trackId = track.getProperty (Session::idProperty()).toString();
    REQUIRE (source.setTrackMute (trackId, true));

    auto xml = source.state().createXml();
    REQUIRE (xml != nullptr);

    Session restored (juce::ValueTree::fromXml (*xml));
    REQUIRE (restored.isTrackMuted (trackId));
}

TEST_CASE ("tempo changes are undoable", "[core][undo][tempo]")
{
    Session session;
    CommandBus bus (session);

    SetTempoCommand toNinety (90.0);
    REQUIRE (bus.dispatch (toNinety));
    REQUIRE (session.tempo() == 90.0);

    SetTempoCommand toOneForty (140.0);
    REQUIRE (bus.dispatch (toOneForty));
    REQUIRE (session.tempo() == 140.0);

    REQUIRE (bus.undo());
    REQUIRE (session.tempo() == 90.0);
    REQUIRE (bus.undo());
    REQUIRE (session.tempo() == 120.0);
    REQUIRE (bus.redo());
    REQUIRE (session.tempo() == 90.0);
}

TEST_CASE ("tempo is clamped and no-ops do not enter the undo history", "[core][tempo]")
{
    Session session;
    CommandBus bus (session);

    SetTempoCommand tooFast (10000.0);
    REQUIRE (bus.dispatch (tooFast));
    REQUIRE (session.tempo() == 400.0);

    // Setting the value it already has is not an edit.
    SetTempoCommand same (400.0);
    REQUIRE_FALSE (bus.dispatch (same));

    REQUIRE (bus.undo());
    REQUIRE (session.tempo() == 120.0);
}

TEST_CASE ("time signature changes are undoable", "[core][undo][tempo]")
{
    Session session;
    CommandBus bus (session);

    SetTimeSignatureCommand toSixEight (6, 8);
    REQUIRE (bus.dispatch (toSixEight));
    REQUIRE (session.timeSignatureNumerator() == 6);
    REQUIRE (session.timeSignatureDenominator() == 8);

    REQUIRE (bus.undo());
    REQUIRE (session.timeSignatureNumerator() == 4);
    REQUIRE (session.timeSignatureDenominator() == 4);
}

TEST_CASE ("the first import sets the project tempo, later imports do not",
           "[core][tempo][import]")
{
    Session session;
    CommandBus bus (session);

    const auto firstClip = importSongWithDetectedTempo (bus, session, "D:\\Music\\first.wav", 90.0);
    REQUIRE (session.tempo() == 90.0);
    REQUIRE (session.clipSourceTempo (firstClip) == 90.0);

    // A second song at a different tempo must not hijack the project. If it
    // did, the first song would be re-stretched by 130/90 behind the user's
    // back.
    const auto secondClip = importSongWithDetectedTempo (bus, session, "D:\\Music\\second.wav",
                                                         130.0);
    REQUIRE (session.tempo() == 90.0);
    REQUIRE (session.clipSourceTempo (firstClip) == 90.0);
    REQUIRE (session.clipSourceTempo (secondClip) == 90.0);
}

TEST_CASE ("undoing the second song leaves the first at its own tempo",
           "[core][tempo][undo][import]")
{
    Session session;
    CommandBus bus (session);

    const auto firstClip = importSongWithDetectedTempo (bus, session, "D:\\Music\\first.wav",
                                                        90.0, 0.4);
    importSongWithDetectedTempo (bus, session, "D:\\Music\\second.wav", 130.0, 0.7);
    REQUIRE (session.clipCount() == 2);
    REQUIRE (session.tempo() == 90.0);

    // Undo back to one song. The number of steps is deliberately not asserted:
    // a detection that changed nothing records no transaction, so it varies.
    // What must hold is the state the user is left in.
    while (session.clipCount() > 1)
        REQUIRE (bus.undo());

    REQUIRE (session.clipCount() == 1);
    REQUIRE (session.tempo() == 90.0);

    // Source tempo equal to project tempo is what makes the surviving song play
    // at its recorded speed rather than stretched by the removed song's tempo.
    REQUIRE (session.clipSourceTempo (firstClip) == 90.0);
    REQUIRE (session.clipSourceTempo (firstClip) == session.tempo());
}

TEST_CASE ("clip source tempo survives a full undo and redo", "[core][tempo][undo]")
{
    Session session;
    CommandBus bus (session);

    const auto clipId = importSongWithDetectedTempo (bus, session, "D:\\Music\\only.wav", 75.0);
    REQUIRE (session.tempo() == 75.0);

    REQUIRE (bus.undo());
    REQUIRE (session.tempo() == 120.0);

    REQUIRE (bus.redo());
    REQUIRE (session.tempo() == 75.0);
    REQUIRE (session.clipSourceTempo (clipId) == 75.0);
}

TEST_CASE ("tap tempo re-bases every clip so nothing is stretched", "[core][tempo]")
{
    Session session;
    CommandBus bus (session);

    const auto firstClip = importSongWithDetectedTempo (bus, session, "D:\\Music\\a.wav", 90.0);
    const auto secondClip = importSongWithDetectedTempo (bus, session, "D:\\Music\\b.wav", 130.0);

    SetTapTempoCommand tap (100.0);
    REQUIRE (bus.dispatch (tap));

    REQUIRE (session.tempo() == 100.0);
    REQUIRE (session.clipSourceTempo (firstClip) == 100.0);
    REQUIRE (session.clipSourceTempo (secondClip) == 100.0);

    REQUIRE (bus.undo());
    REQUIRE (session.tempo() == 90.0);
    REQUIRE (session.clipSourceTempo (firstClip) == 90.0);
}

TEST_CASE ("a hundred operations undo cleanly", "[core][undo]")
{
    // Phase 6 requires 100 undos; proving the invariant now costs nothing and
    // catches a broken transaction boundary the moment it appears.
    Session session;
    CommandBus bus (session);

    juce::StringArray ids;
    for (int i = 0; i < 100; ++i)
    {
        AddTrackCommand add ("audio", "Track " + juce::String (i));
        REQUIRE (bus.dispatch (add));
        ids.add (add.id());
    }

    REQUIRE (session.tracks().getNumChildren() == 100);

    for (int i = 0; i < 100; ++i)
        REQUIRE (bus.undo());

    REQUIRE (session.tracks().getNumChildren() == 0);
    REQUIRE_FALSE (bus.canUndo());

    for (int i = 0; i < 100; ++i)
        REQUIRE (bus.redo());

    REQUIRE (session.tracks().getNumChildren() == 100);
    for (int i = 0; i < 100; ++i)
        REQUIRE (session.tracks().getChild (i).getProperty (Session::idProperty()).toString()
                 == ids[i]);
}
