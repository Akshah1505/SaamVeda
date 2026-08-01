#include <catch2/catch_test_macros.hpp>
#include <juce_data_structures/juce_data_structures.h>

#include "../src/app/CommandBus.h"

using saamveda::app::AddTrackCommand;
using saamveda::app::CommandBus;
using saamveda::app::RenameTrackCommand;

TEST_CASE ("new session has schema defaults", "[core]")
{
    saamveda::core::Session session;
    REQUIRE (session.state().getType() == saamveda::core::Session::sessionType());
    REQUIRE (static_cast<int> (session.state().getProperty ("version")) == 1);
    REQUIRE (static_cast<double> (session.state().getProperty ("tempo")) == 120.0);
    REQUIRE (session.tracks().getNumChildren() == 0);
}

TEST_CASE ("commands mutate the session and undo restores it", "[core][undo]")
{
    saamveda::core::Session session;
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
    saamveda::core::Session source;
    source.addTrack ("midi", "Bass");
    auto xml = source.state().createXml();
    REQUIRE (xml != nullptr);

    saamveda::core::Session restored (juce::ValueTree::fromXml (*xml));
    REQUIRE (static_cast<double> (restored.state().getProperty ("tempo")) == 120.0);
    REQUIRE (restored.tracks().getNumChildren() == 1);
    REQUIRE (restored.tracks().getChild (0).getProperty ("name") == "Bass");
}

TEST_CASE ("audio import command creates a track and clip", "[core][import]")
{
    saamveda::core::Session session;
    CommandBus bus (session);
    saamveda::app::ImportAudioCommand import (
        juce::File ("D:\\Music\\take.wav"), 12.5);

    REQUIRE (bus.dispatch (import));
    REQUIRE (session.tracks().getNumChildren() == 1);
    auto track = session.tracks().getChild (0);
    REQUIRE (track.getProperty ("name").toString() == "take");
    auto clips = track.getChildWithName ("CLIPS");
    REQUIRE (clips.getNumChildren() == 1);
    REQUIRE (static_cast<double> (clips.getChild (0).getProperty ("length")) == 12.5);
    REQUIRE (bus.undo());
    REQUIRE (session.tracks().getNumChildren() == 0);
}
