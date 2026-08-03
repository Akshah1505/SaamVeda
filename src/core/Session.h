#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace saamveda::core
{

class Session
{
public:
    static constexpr int currentSchemaVersion = 1;

    Session();
    explicit Session (juce::ValueTree state);

    juce::ValueTree& state() noexcept { return sessionState; }
    const juce::ValueTree& state() const noexcept { return sessionState; }
    juce::UndoManager& undoManager() noexcept { return undo; }

    juce::ValueTree tracks() const;
    juce::ValueTree trackWithId (const juce::String& trackId) const;
    juce::ValueTree clipsOf (const juce::ValueTree& track) const;
    juce::ValueTree clipWithId (const juce::String& clipId) const;
    int clipCount() const;

    juce::ValueTree addTrack (juce::String type, juce::String name);
    juce::ValueTree addAudioClip (juce::String trackId, const juce::File& sourceFile, double lengthSeconds);
    bool removeTrack (juce::String trackId);
    bool renameTrack (juce::String trackId, juce::String name);

    /** Tempo the clip's audio was recorded at.

        This lives per clip, not per project, because a project can hold two
        songs at different tempos and one global value cannot be right for both.
        It is session state so undo restores it: an engine-side member would
        survive an undo and leave the surviving clip stretched by the tempo of
        the clip that was just removed. */
    bool setClipSourceTempo (const juce::String& clipId, double bpm);
    void setAllClipSourceTempos (double bpm);
    double clipSourceTempo (const juce::String& clipId) const;

    /** Lead-in trimmed from the front of a clip, in seconds. */
    bool setClipOffset (const juce::String& clipId, double seconds);

    // Tempo and time signature are undoable session state, not view state. The
    // UI writes them through these rather than touching the tree, so Ctrl+Z
    // covers them like every other edit.
    bool setTempo (double bpm);
    bool setTimeSignature (int numerator, int denominator);

    double tempo() const;
    int timeSignatureNumerator() const;
    int timeSignatureDenominator() const;

    static juce::Identifier sessionType();
    static juce::Identifier tracksType();
    static juce::Identifier trackType();
    static juce::Identifier clipsType();
    static juce::Identifier clipType();
    static juce::Identifier idProperty();

    static juce::String newId();

    /** Undo history depth. JUCE's default is 30000 units with a floor of 30
        transactions, which in practice prunes the history after roughly fifty
        track edits — short of the 100 operations docs/09-roadmap.md Phase 6
        requires. The floor is what actually guarantees the requirement; the
        unit budget only caps memory when individual edits are large. */
    static constexpr int undoUnitBudget = 100000;
    static constexpr int undoTransactionFloor = 200;

private:
    void initialiseDefaults();
    juce::ValueTree sessionState;
    juce::UndoManager undo { undoUnitBudget, undoTransactionFloor };
};

} // namespace saamveda::core
