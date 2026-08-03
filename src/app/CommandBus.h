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

/** Applies the result of tempo detection to one imported clip.

    Whether the project tempo follows the detection is decided by the caller at
    import time, not here: only the first import may set it. A later import that
    hijacked the project tempo would re-stretch every clip already on the
    timeline against a tempo they were never recorded at.

    Both effects land in one transaction so a single Ctrl+Z undoes the whole
    detection rather than half of it.
*/
class ApplyDetectedTempoCommand final : public Command
{
public:
    ApplyDetectedTempoCommand (juce::String clipId, double detectedBpm,
                               double firstBeatSeconds, bool shouldSetProjectTempo)
        : idValue (std::move (clipId)), detected (detectedBpm),
          firstBeat (firstBeatSeconds), setsProjectTempo (shouldSetProjectTempo) {}

    bool execute (core::Session& session) override
    {
        if (setsProjectTempo)
            session.setTempo (detected);

        // Recording the project tempo as the clip's source tempo is what keeps
        // it at its original speed: the engine's ratio is project / source.
        const auto changed = session.setClipSourceTempo (idValue, session.tempo());
        session.setClipOffset (idValue, firstBeat);
        return changed;
    }

    juce::String name() const override { return "Detect Tempo"; }

private:
    juce::String idValue;
    double detected, firstBeat;
    bool setsProjectTempo;
};

/** Tap tempo: retunes the project and re-bases every clip onto the new tempo so
    nothing is time-stretched by the change. */
class SetTapTempoCommand final : public Command
{
public:
    explicit SetTapTempoCommand (double bpm) : value (bpm) {}

    bool execute (core::Session& session) override
    {
        const auto changed = session.setTempo (value);
        session.setAllClipSourceTempos (session.tempo());
        return changed;
    }

    juce::String name() const override { return "Tap Tempo"; }

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
