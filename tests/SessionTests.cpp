#include <catch2/catch_test_macros.hpp>
#include <juce_data_structures/juce_data_structures.h>

#include "../src/app/CommandBus.h"

using saamveda::app::AddTrackCommand;
using saamveda::app::CommandBus;
using saamveda::app::ImportAudioCommand;
using saamveda::app::RemoveTrackCommand;
using saamveda::app::RenameTrackCommand;
using saamveda::app::SetTempoCommand;
using saamveda::app::SetTimeSignatureCommand;
using saamveda::core::Session;

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
