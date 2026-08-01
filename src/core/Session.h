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
    juce::ValueTree addTrack (juce::String type, juce::String name);
    bool addAudioClip (juce::String trackId, const juce::File& sourceFile, double lengthSeconds);
    bool removeTrack (juce::String trackId);
    bool renameTrack (juce::String trackId, juce::String name);

    static juce::Identifier sessionType();
    static juce::Identifier tracksType();
    static juce::Identifier trackType();
    static juce::Identifier idProperty();

private:
    static juce::String newId();
    void initialiseDefaults();
    juce::ValueTree sessionState;
    juce::UndoManager undo;
};

} // namespace saamveda::core
