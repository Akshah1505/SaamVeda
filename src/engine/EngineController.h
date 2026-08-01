#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include "../core/Session.h"

namespace saamveda::engine
{

class EngineController
{
public:
    EngineController();
    ~EngineController();

    void play();
    void stop();
    void seek (double seconds);
    void setLooping (bool shouldLoop);
    void setTempo (double bpm);
    void setSourceTempo (double bpm);
    void setDetectedTempo (double bpm);
    void setTapTempo (double bpm);
    void alignFirstBeat (double firstBeatSeconds);
    void alignBeatAtPosition (double beatPositionSeconds);
    void setMetronomeEnabled (bool enabled);
    void setTimeSignature (int numerator, int denominator);
    double importAudioFile (const juce::File& file, int trackIndex);
    bool removeAudioTrack (int trackIndex);
    void synchronise (const core::Session& session);

    bool isPlaying() const;
    bool isLooping() const;
    double positionSeconds() const;
    double tempo() const;
    double sourceTempo() const noexcept { return currentSourceTempo; }

    juce::AudioDeviceManager& audioDeviceManager();
    juce::String audioFileWildcard() const;

private:
    tracktion::engine::Engine engine { "SaamVeda Studio" };
    std::unique_ptr<tracktion::engine::Edit> edit;
    double currentSourceTempo = 120.0;

    void applyTempoToAudioClips();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineController)
};

} // namespace saamveda::engine
