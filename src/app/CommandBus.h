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

/** Mute is undoable like every other session edit.

    Several DAWs keep mute out of the undo history on the grounds that it is a
    listening control rather than an edit. This project keeps it in: it is a
    property of the track in the session tree, and docs/05-architecture.md
    section 5 is explicit that state living there inherits undo uniformly. One
    toggle is one discrete transaction, not a drag stream, so it cannot flood
    the history.
*/
class SetTrackMuteCommand final : public Command
{
public:
    SetTrackMuteCommand (juce::String trackId, bool shouldMute)
        : idValue (std::move (trackId)), muted (shouldMute) {}

    bool execute (core::Session& session) override
    {
        return session.setTrackMute (idValue, muted);
    }

    juce::String name() const override { return muted ? "Mute Track" : "Unmute Track"; }

private:
    juce::String idValue;
    bool muted;
};

class SetTrackSoloCommand final : public Command
{
public:
    SetTrackSoloCommand (juce::String trackId, bool shouldSolo)
        : idValue (std::move (trackId)), soloed (shouldSolo) {}

    bool execute (core::Session& session) override
    {
        return session.setTrackSolo (idValue, soloed);
    }

    juce::String name() const override { return soloed ? "Solo Track" : "Unsolo Track"; }

private:
    juce::String idValue;
    bool soloed;
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

/** Moves a clip: along the timeline, onto another track, or both at once.

    One command per completed drag, not per mouse move: dragging a clip across
    twenty bars is one thing the user did, and it should be one Ctrl+Z. Both
    axes travel together for the same reason - a diagonal drag is still one
    gesture, and undoing half of it would leave the clip somewhere the user
    never put it.

    The target is a row index rather than a track id because the row may not be
    a track yet: the timeline always draws at least twelve, and dropping on an
    empty one creates it. A negative index means "leave it where it is".
*/
class MoveClipCommand final : public Command
{
public:
    MoveClipCommand (juce::String clipId, double newStartSeconds, int targetRowIndex = -1)
        : idValue (std::move (clipId)), start (newStartSeconds), targetRow (targetRowIndex) {}

    bool execute (core::Session& session) override
    {
        auto changed = false;

        if (targetRow >= 0)
        {
            const auto target = session.ensureTrackAtIndex (targetRow);

            if (target.isValid())
                changed = session.moveClipToTrack (
                    idValue, target.getProperty (core::Session::idProperty()).toString()) || changed;
        }

        return session.setClipStart (idValue, start) || changed;
    }

    juce::String name() const override { return "Move Clip"; }

private:
    juce::String idValue;
    double start;
    int targetRow;
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
