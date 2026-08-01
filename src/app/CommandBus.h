#pragma once

#include "../core/Session.h"

namespace saamveda::app
{

class Command
{
public:
    virtual ~Command() = default;
    virtual bool execute (core::Session&) = 0;
    virtual juce::String name() const = 0;
};

class CommandBus
{
public:
    explicit CommandBus (core::Session& sessionToUse) : session (sessionToUse) {}

    bool dispatch (Command& command)
    {
        session.undoManager().beginNewTransaction (command.name());
        return command.execute (session);
    }

    bool undo() { return session.undoManager().undo(); }
    bool redo() { return session.undoManager().redo(); }
    core::Session& getSession() noexcept { return session; }

private:
    core::Session& session;
};

class AddTrackCommand final : public Command
{
public:
    AddTrackCommand (juce::String trackType, juce::String trackName)
        : type (std::move (trackType)), nameValue (std::move (trackName)) {}

    bool execute (core::Session& session) override
    {
        createdId = session.addTrack (type, nameValue).getProperty (core::Session::idProperty()).toString();
        return createdId.isNotEmpty();
    }

    juce::String name() const override { return "Add Track"; }
    const juce::String& id() const noexcept { return createdId; }

private:
    juce::String type, nameValue, createdId;
};

class RenameTrackCommand final : public Command
{
public:
    RenameTrackCommand (juce::String trackId, juce::String newName)
        : idValue (std::move (trackId)), nameValue (std::move (newName)) {}

    bool execute (core::Session& session) override
    {
        return session.renameTrack (idValue, nameValue);
    }

    juce::String name() const override { return "Rename Track"; }

private:
    juce::String idValue, nameValue;
};

class ImportAudioCommand final : public Command
{
public:
    ImportAudioCommand (juce::File sourceFile, double durationSeconds)
        : file (std::move (sourceFile)), duration (durationSeconds) {}

    bool execute (core::Session& session) override
    {
        auto track = session.addTrack ("audio", file.getFileNameWithoutExtension());
        const auto trackId = track.getProperty (core::Session::idProperty()).toString();
        return trackId.isNotEmpty() && session.addAudioClip (trackId, file, duration);
    }

    juce::String name() const override { return "Import Audio"; }

private:
    juce::File file;
    double duration;
};

} // namespace saamveda::app
