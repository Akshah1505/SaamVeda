#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include "../core/Session.h"
#include "RealtimeSanityCheck.h"
#include "WaveDeviceLayout.h"

namespace saamveda::engine
{

/** Owns the tracktion_engine Edit and keeps it in step with the session tree.

    The session is authoritative for structure; this class mirrors it. Every
    engine track and clip carries the id of the session node it represents, so
    reconciliation is by identity rather than by index. That is what makes undo
    cheap: restoring a track no longer means tearing the Edit down and decoding
    every file on disk again.
*/
class EngineController : private tracktion::engine::TransportControl::Listener
{
public:
    EngineController();
    ~EngineController() override;

    /** Fired on the message thread when a recording produces a clip. The engine
        creates it; the session has to be told so the clip is not swept away by
        the next synchronise. */
    std::function<void (juce::String trackId, juce::File file,
                        double startSeconds, double lengthSeconds)> onClipRecorded;

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
    void setTapTempo (double bpm);
    void setTimeSignature (int numerator, int denominator);

    /** Applies a detection result to one clip in a single transport cycle.

        @param projectTempoBpm  the new project tempo, or 0 to leave it alone.
                                Only the first import should move it.
        @param sourceTempoBpm   the tempo this clip's audio was recorded at. The
                                playback ratio is project / source, so passing
                                the project tempo plays it at original speed.
                                Held per clip because a project can contain
                                material recorded at several different tempos.
        @param offsetSeconds    lead-in trimmed from the front of this clip only.
    */
    bool applyDetectedTempo (const juce::String& clipId, double projectTempoBpm,
                             double sourceTempoBpm, double offsetSeconds);

    /** Moves one clip along the timeline. */
    bool setClipStart (const juce::String& clipId, double startSeconds);

    /** Reparents a clip onto another track without re-reading its file. */
    bool moveClipToTrack (const juce::String& clipId, const juce::String& targetTrackId);

    //==============================================================================
    // Recording

    /** Names of the wave inputs the device manager currently exposes. */
    juce::StringArray inputDeviceNames() const;

    /** Arms a track: points the first available wave input at it and enables
        recording. Returns false when there is no usable input. */
    bool setTrackArmed (const juce::String& trackId, bool armed);
    bool isTrackArmed (const juce::String& trackId) const;
    int armedTrackCount() const;

    void setInputMonitoring (bool enabled);
    bool isInputMonitoring() const;

    /** Peak input level in dB, or -100 when there is no input. */
    float inputLevelDb();

    /** Tags the clip tracktion just recorded with the session id the host gave
        it, so synchronise treats it as a clip it knows rather than a stray. */
    bool adoptRecordedClip (const juce::String& trackId, const juce::File& file,
                            const juce::String& clipId);

    void startRecording();
    void stopRecording (bool discardRecordings);
    bool isRecording() const;

    /** Bars of count-in before recording starts. 0 disables it. */
    void setCountInBars (int bars);
    int countInBars() const;

    //==============================================================================
    // Structure, mirrored from the session by id
    bool ensureTrack (const juce::String& trackId);
    bool removeTrack (const juce::String& trackId);

    /** Take effect on the next audio block - no transport stop required. */
    bool setTrackMute (const juce::String& trackId, bool muted);
    bool isTrackMuted (const juce::String& trackId) const;
    bool setTrackSolo (const juce::String& trackId, bool soloed);
    double importAudioFile (const juce::File& file, const juce::String& trackId,
                            const juce::String& clipId, double sourceTempoBpm,
                            double offsetSeconds, double startSeconds);
    void synchronise (const core::Session& session);

    // Queries
    bool isPlaying() const;
    bool isLooping() const;
    bool isMetronomeEnabled() const;
    double positionSeconds() const;
    double contentLengthSeconds() const;
    double tempo() const;
    int trackCount() const;

    RealtimeSanityCheck::Report realtimeReport() const { return realtimeCheck.report(); }
    juce::StringArray realtimeOffenders() { return realtimeCheck.describeOffenders(); }

    /** Channel indices tracktion's wave input expects from the device. */
    juce::String inputChannelDiagnostics() const;

    juce::AudioDeviceManager& audioDeviceManager();
    juce::String audioFileWildcard() const;

private:
    // The behaviour has to be supplied at construction: it decides the wave
    // device layout, and that is read the first time a device opens.
    tracktion::engine::Engine engine { "SaamVeda Studio", nullptr,
                                       std::make_unique<ActiveChannelWaveDeviceLayout>() };
    std::unique_ptr<tracktion::engine::Edit> edit;
    RealtimeSanityCheck realtimeCheck;
    tracktion::engine::LevelMeasurer::Client inputLevelClient;
    tracktion::engine::LevelMeasurer* attachedLevelMeasurer = nullptr;

    static const juce::Identifier sessionTrackIdProperty;
    static const juce::Identifier sessionClipIdProperty;
    static const juce::Identifier sourceTempoProperty;
    static const juce::Identifier offsetProperty;
    static const juce::Identifier startProperty;

    void recordingFinished (tracktion::engine::InputDeviceInstance&,
                            tracktion::engine::EditItemID,
                            const juce::ReferenceCountedArray<tracktion::engine::Clip>&) override;

    tracktion::engine::InputDeviceInstance* firstWaveInput() const;
    void disableRetrospectiveRecord();
    void attachLevelClient();

    tracktion::engine::AudioTrack* trackForId (const juce::String& trackId) const;
    tracktion::engine::Clip* clipForId (tracktion::engine::AudioTrack&,
                                        const juce::String& clipId) const;
    tracktion::engine::AudioClipBase* audioClipForId (const juce::String& clipId) const;
    void applyClipSettings (tracktion::engine::AudioClipBase& clip);
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
