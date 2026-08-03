#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include "../core/Session.h"
#include "RealtimeSanityCheck.h"

namespace saamveda::engine
{

/** Owns the tracktion_engine Edit and keeps it in step with the session tree.

    The session is authoritative for structure; this class mirrors it. Every
    engine track and clip carries the id of the session node it represents, so
    reconciliation is by identity rather than by index. That is what makes undo
    cheap: restoring a track no longer means tearing the Edit down and decoding
    every file on disk again.
*/
class EngineController
{
public:
    EngineController();
    ~EngineController();

    // Transport
    void play();
    void stop();
    void togglePlayStop();
    void stopAndReturnToStart();
    void seek (double seconds);
    void nudge (double deltaSeconds);
    void setLooping (bool shouldLoop);
    void toggleLooping();
    void setMetronomeEnabled (bool enabled);
    void toggleMetronome();

    // Musical settings
    void setTempo (double bpm);
    void setSourceTempo (double bpm);
    void setDetectedTempo (double bpm);
    void setTapTempo (double bpm);
    void alignFirstBeat (double firstBeatSeconds);
    void alignBeatAtPosition (double beatPositionSeconds);
    void setTimeSignature (int numerator, int denominator);

    // Structure, mirrored from the session by id
    bool ensureTrack (const juce::String& trackId);
    bool removeTrack (const juce::String& trackId);
    double importAudioFile (const juce::File& file, const juce::String& trackId,
                            const juce::String& clipId);
    void synchronise (const core::Session& session);

    // Queries
    bool isPlaying() const;
    bool isLooping() const;
    bool isMetronomeEnabled() const;
    double positionSeconds() const;
    double contentLengthSeconds() const;
    double tempo() const;
    double sourceTempo() const noexcept { return currentSourceTempo; }
    int trackCount() const;

    RealtimeSanityCheck::Report realtimeReport() const { return realtimeCheck.report(); }

    juce::AudioDeviceManager& audioDeviceManager();
    juce::String audioFileWildcard() const;

private:
    tracktion::engine::Engine engine { "SaamVeda Studio" };
    std::unique_ptr<tracktion::engine::Edit> edit;
    RealtimeSanityCheck realtimeCheck;
    double currentSourceTempo = 120.0;

    static const juce::Identifier sessionTrackIdProperty;
    static const juce::Identifier sessionClipIdProperty;

    tracktion::engine::AudioTrack* trackForId (const juce::String& trackId) const;
    tracktion::engine::Clip* clipForId (tracktion::engine::AudioTrack&,
                                        const juce::String& clipId) const;
    void applyTempoToAudioClips();
    void updateLoopRange();

    /** Runs an operation that requires a stopped transport, then puts the
        playhead and play state back. Tempo and time-signature edits go through
        this so Ctrl+Z never moves the playhead out from under the user. */
    template <typename Operation>
    void preservingTransport (Operation&& operation);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineController)
};

} // namespace saamveda::engine
