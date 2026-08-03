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
    bool canUndo() const { return session.undoManager().canUndo(); }
    bool canRedo() const { return session.undoManager().canRedo(); }

    juce::String undoDescription() const
    {
        return session.undoManager().getUndoDescription();
    }

    juce::String redoDescription() const
    {
        return session.undoManager().getRedoDescription();
    }

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

class RemoveTrackCommand final : public Command
{
public:
    explicit RemoveTrackCommand (juce::String trackId) : idValue (std::move (trackId)) {}

    bool execute (core::Session& session) override
    {
        return session.removeTrack (idValue);
    }

    juce::String name() const override { return "Remove Track"; }

private:
    juce::String idValue;
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
        trackId = track.getProperty (core::Session::idProperty()).toString();
        if (trackId.isEmpty())
            return false;

        auto clip = session.addAudioClip (trackId, file, duration);
        clipId = clip.getProperty (core::Session::idProperty()).toString();
        return clipId.isNotEmpty();
    }

    juce::String name() const override { return "Import Audio"; }
    const juce::String& createdTrackId() const noexcept { return trackId; }
    const juce::String& createdClipId() const noexcept { return clipId; }

private:
    juce::File file;
    double duration;
    juce::String trackId, clipId;
};

class SetTempoCommand final : public Command
{
public:
    explicit SetTempoCommand (double bpm) : value (bpm) {}

    bool execute (core::Session& session) override
    {
        return session.setTempo (value);
    }

    juce::String name() const override { return "Change Tempo"; }

private:
    double value;
};

class SetTimeSignatureCommand final : public Command
{
public:
    SetTimeSignatureCommand (int num, int denom) : numerator (num), denominator (denom) {}

    bool execute (core::Session& session) override
    {
        return session.setTimeSignature (numerator, denominator);
    }

    juce::String name() const override { return "Change Time Signature"; }

private:
    int numerator, denominator;
};

} // namespace saamveda::app
