#include "EngineController.h"

namespace saamveda::engine
{

namespace te = tracktion::engine;

const juce::Identifier EngineController::sessionTrackIdProperty { "saamvedaTrackId" };
const juce::Identifier EngineController::sessionClipIdProperty  { "saamvedaClipId" };
const juce::Identifier EngineController::sourceTempoProperty    { "saamvedaSourceTempo" };
const juce::Identifier EngineController::offsetProperty         { "saamvedaOffset" };

namespace
{
    tracktion::core::TimePosition seconds (double value)
    {
        return tracktion::core::TimePosition::fromSeconds (value);
    }

    tracktion::core::TimeDuration duration (double value)
    {
        return tracktion::core::TimeDuration::fromSeconds (value);
    }
}

EngineController::EngineController()
{
    engine.getDeviceManager().initialise (0, 2);
    edit = te::Edit::createSingleTrackEdit (engine, te::Edit::EditRole::forEditing);

    if (edit == nullptr)
        throw std::runtime_error ("tracktion_engine could not create an edit");

    // createSingleTrackEdit hands back one unowned audio track. Removing it
    // establishes the invariant the rest of this class relies on: every audio
    // track in the Edit corresponds to a session track and carries its id.
    for (auto* track : te::getAudioTracks (*edit))
        edit->deleteTrack (track);

    setTempo (120.0);
    setTimeSignature (4, 4);
    updateLoopRange();

    realtimeCheck.attachTo (audioDeviceManager());
}

EngineController::~EngineController()
{
    realtimeCheck.detach();

    if (edit != nullptr)
    {
        edit->getTransport().stop (false, true);
        edit->getTransport().freePlaybackContext();
    }
    edit.reset();
    engine.getDeviceManager().closeDevices();
}

template <typename Operation>
void EngineController::preservingTransport (Operation&& operation)
{
    const auto wasPlaying = isPlaying();
    const auto position = positionSeconds();

    if (wasPlaying)
        stop();

    operation();

    seek (position);

    if (wasPlaying)
        play();
}

//==============================================================================
void EngineController::play()
{
    edit->getTransport().play (false);
}

void EngineController::stop()
{
    edit->getTransport().stop (false, false);
}

void EngineController::togglePlayStop()
{
    if (isPlaying())
        stop();
    else
        play();
}

void EngineController::stopAndReturnToStart()
{
    stop();
    seek (0.0);
}

void EngineController::seek (double secondsPosition)
{
    edit->getTransport().setPosition (seconds (juce::jmax (0.0, secondsPosition)));
}

void EngineController::nudge (double deltaSeconds)
{
    seek (juce::jmax (0.0, positionSeconds() + deltaSeconds));
}

void EngineController::setLooping (bool shouldLoop)
{
    updateLoopRange();
    edit->getTransport().looping = shouldLoop;
}

void EngineController::toggleLooping()
{
    setLooping (! isLooping());
}

void EngineController::setMetronomeEnabled (bool enabled)
{
    edit->clickTrackEnabled = enabled;
    edit->clickTrackEmphasiseBars = true;
}

void EngineController::toggleMetronome()
{
    setMetronomeEnabled (! isMetronomeEnabled());
}

void EngineController::updateLoopRange()
{
    const auto length = juce::jmax (16.0, contentLengthSeconds());
    edit->getTransport().setLoopRange ({ seconds (0.0), duration (length) });
}

//==============================================================================
void EngineController::setTempo (double bpm)
{
    preservingTransport ([this, bpm]
    {
        edit->tempoSequence.getTempo (0)->setBpm (juce::jlimit (20.0, 400.0, bpm));
        applyTempoToAudioClips();
    });
}

void EngineController::setTapTempo (double bpm)
{
    const auto tappedBpm = juce::jlimit (20.0, 400.0, bpm);

    // Re-base every clip onto the new tempo so tapping retunes the click
    // without time-stretching anything already on the timeline.
    preservingTransport ([this, tappedBpm]
    {
        edit->tempoSequence.getTempo (0)->setBpm (tappedBpm);

        for (auto* track : te::getAudioTracks (*edit))
            for (auto* clip : track->getClips())
                if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (clip))
                {
                    audioClip->state.setProperty (sourceTempoProperty, tappedBpm, nullptr);
                    applyClipSettings (*audioClip);
                }
    });
}

bool EngineController::applyDetectedTempo (const juce::String& clipId, double projectTempoBpm,
                                           double sourceTempoBpm, double offsetSeconds)
{
    auto* clip = audioClipForId (clipId);
    if (clip == nullptr)
        return false;

    // One transport cycle for the whole detection result. Doing it as three
    // separate calls stopped and restarted playback three times.
    preservingTransport ([this, clip, projectTempoBpm, sourceTempoBpm, offsetSeconds]
    {
        if (projectTempoBpm > 0.0)
            edit->tempoSequence.getTempo (0)->setBpm (juce::jlimit (20.0, 400.0, projectTempoBpm));

        clip->state.setProperty (sourceTempoProperty,
                                 juce::jlimit (20.0, 400.0, sourceTempoBpm), nullptr);

        // The click grid starts at 0. Trimming the song's pre-beat lead-in puts
        // its first detected beat at transport zero without altering audio
        // speed - and touches only this clip, so importing a second song cannot
        // shift the first.
        clip->state.setProperty (offsetProperty, juce::jmax (0.0, offsetSeconds), nullptr);

        // A project tempo change re-rates every clip, not just this one.
        if (projectTempoBpm > 0.0)
            applyTempoToAudioClips();
        else
            applyClipSettings (*clip);
    });

    updateLoopRange();
    return true;
}

void EngineController::applyClipSettings (te::AudioClipBase& clip)
{
    // Each clip stretches against the tempo *it* was recorded at. A single
    // engine-wide source tempo cannot describe a project holding two songs, and
    // it survives undo, which is how removing the second song used to leave the
    // first one stretched.
    const auto storedSource = static_cast<double> (clip.state.getProperty (sourceTempoProperty, 0.0));
    const auto sourceTempo = storedSource > 0.0 ? storedSource : tempo();

    // A 120 BPM recording targeted at 60 BPM uses a 0.5 ratio and therefore
    // becomes twice as long while SoundTouch preserves pitch.
    const auto speedRatio = juce::jlimit (0.1, 10.0, tempo() / sourceTempo);

    clip.setAutoTempo (false);
    clip.setAutoPitch (false);
    clip.setTimeStretchMode (te::TimeStretcher::soundtouchBetter);
    clip.setSpeedRatio (speedRatio);

    const auto fullLength = clip.getSourceLength().inSeconds() / speedRatio;
    const auto trim = juce::jlimit (0.0, juce::jmax (0.0, fullLength - 0.05),
                                    juce::jmax (0.0, static_cast<double> (
                                        clip.state.getProperty (offsetProperty, 0.0))));

    // Written absolutely rather than trimmed off the current position: this
    // runs again on every tempo change, and an incremental trim would eat a
    // little more of the clip each time.
    clip.setPosition ({ { clip.getPosition().getStart(), duration (fullLength - trim) },
                        duration (trim) });
}

void EngineController::applyTempoToAudioClips()
{
    for (auto* track : te::getAudioTracks (*edit))
        for (auto* clip : track->getClips())
            if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (clip))
                applyClipSettings (*audioClip);

    updateLoopRange();
}

void EngineController::setTimeSignature (int numerator, int denominator)
{
    preservingTransport ([this, numerator, denominator]
    {
        edit->tempoSequence.getTimeSig (0)->setStringTimeSig (
            juce::String (juce::jlimit (1, 32, numerator)) + "/"
            + juce::String (juce::jlimit (1, 32, denominator)));
    });
}

//==============================================================================
te::AudioTrack* EngineController::trackForId (const juce::String& trackId) const
{
    if (trackId.isEmpty())
        return nullptr;

    for (auto* track : te::getAudioTracks (*edit))
        if (track->state.getProperty (sessionTrackIdProperty).toString() == trackId)
            return track;

    return nullptr;
}

te::Clip* EngineController::clipForId (te::AudioTrack& track, const juce::String& clipId) const
{
    if (clipId.isEmpty())
        return nullptr;

    for (auto* clip : track.getClips())
        if (clip->state.getProperty (sessionClipIdProperty).toString() == clipId)
            return clip;

    return nullptr;
}

te::AudioClipBase* EngineController::audioClipForId (const juce::String& clipId) const
{
    if (clipId.isEmpty())
        return nullptr;

    for (auto* track : te::getAudioTracks (*edit))
        if (auto* clip = clipForId (*track, clipId))
            return dynamic_cast<te::AudioClipBase*> (clip);

    return nullptr;
}

bool EngineController::ensureTrack (const juce::String& trackId)
{
    if (trackId.isEmpty())
        return false;

    if (trackForId (trackId) != nullptr)
        return true;

    const auto existingCount = te::getAudioTracks (*edit).size();
    edit->ensureNumberOfAudioTracks (existingCount + 1);

    const auto tracks = te::getAudioTracks (*edit);
    if (tracks.size() <= existingCount)
        return false;

    tracks[existingCount]->state.setProperty (sessionTrackIdProperty, trackId, nullptr);
    return true;
}

bool EngineController::setTrackMute (const juce::String& trackId, bool muted)
{
    auto* track = trackForId (trackId);
    if (track == nullptr)
        return false;

    // Deliberately not wrapped in preservingTransport: tracktion applies this
    // on the next block, and stopping playback to mute a track would defeat the
    // point of a mute button.
    track->setMute (muted);
    return true;
}

bool EngineController::isTrackMuted (const juce::String& trackId) const
{
    auto* track = trackForId (trackId);
    return track != nullptr && track->isMuted (false);
}

bool EngineController::setTrackSolo (const juce::String& trackId, bool soloed)
{
    auto* track = trackForId (trackId);
    if (track == nullptr)
        return false;

    // tracktion handles the "everything else goes quiet" part itself, so this
    // stays a per-track flag rather than the host recomputing every other track.
    track->setSolo (soloed);
    return true;
}

bool EngineController::removeTrack (const juce::String& trackId)
{
    if (auto* track = trackForId (trackId))
    {
        edit->deleteTrack (track);
        updateLoopRange();
        return true;
    }

    return false;
}

double EngineController::importAudioFile (const juce::File& file, const juce::String& trackId,
                                          const juce::String& clipId, double sourceTempoBpm,
                                          double offsetSeconds)
{
    te::AudioFile audioFile (engine, file);
    if (! file.existsAsFile() || ! audioFile.isValid() || ! ensureTrack (trackId))
        return 0.0;

    auto* track = trackForId (trackId);
    if (track == nullptr)
        return 0.0;

    const auto lengthSeconds = audioFile.getLength();
    const te::ClipPosition position { { tracktion::core::TimePosition(),
                                        duration (lengthSeconds) },
                                      tracktion::core::TimeDuration() };

    auto clip = track->insertWaveClip (file.getFileNameWithoutExtension(), file, position, false);
    if (clip == nullptr)
        return 0.0;

    clip->state.setProperty (sessionClipIdProperty, clipId, nullptr);
    clip->state.setProperty (sourceTempoProperty,
                             sourceTempoBpm > 0.0 ? sourceTempoBpm : tempo(), nullptr);
    clip->state.setProperty (offsetProperty, juce::jmax (0.0, offsetSeconds), nullptr);
    applyClipSettings (*clip);

    updateLoopRange();
    return lengthSeconds;
}

void EngineController::synchronise (const core::Session& session)
{
    const auto sessionTracks = session.tracks();

    juce::StringArray wantedTrackIds;
    for (int i = 0; i < sessionTracks.getNumChildren(); ++i)
        wantedTrackIds.add (sessionTracks.getChild (i)
                                .getProperty (core::Session::idProperty()).toString());

    // Drop engine tracks the session no longer has. Iterating backwards keeps
    // the indices valid as tracks are removed.
    const auto engineTracks = te::getAudioTracks (*edit);
    for (int i = engineTracks.size(); --i >= 0;)
    {
        const auto id = engineTracks[i]->state.getProperty (sessionTrackIdProperty).toString();
        if (id.isEmpty() || ! wantedTrackIds.contains (id))
            edit->deleteTrack (engineTracks[i]);
    }

    for (int trackIndex = 0; trackIndex < sessionTracks.getNumChildren(); ++trackIndex)
    {
        const auto sessionTrack = sessionTracks.getChild (trackIndex);
        const auto trackId = sessionTrack.getProperty (core::Session::idProperty()).toString();
        if (! ensureTrack (trackId))
            continue;

        auto* engineTrack = trackForId (trackId);
        if (engineTrack == nullptr)
            continue;

        // Mute and solo are session state, so an undo that restores them has to
        // reach the engine like tempo does.
        engineTrack->setMute (static_cast<bool> (sessionTrack.getProperty ("mute", false)));
        engineTrack->setSolo (static_cast<bool> (sessionTrack.getProperty ("solo", false)));

        const auto sessionClips = session.clipsOf (sessionTrack);

        juce::StringArray wantedClipIds;
        for (int i = 0; i < sessionClips.getNumChildren(); ++i)
            wantedClipIds.add (sessionClips.getChild (i)
                                   .getProperty (core::Session::idProperty()).toString());

        // Copy before mutating: removing a clip modifies the track's own array.
        const juce::Array<te::Clip*> currentClips (engineTrack->getClips());
        for (auto* clip : currentClips)
        {
            const auto id = clip->state.getProperty (sessionClipIdProperty).toString();
            if (id.isEmpty() || ! wantedClipIds.contains (id))
                clip->removeFromParent();
        }

        for (int i = 0; i < sessionClips.getNumChildren(); ++i)
        {
            const auto sessionClip = sessionClips.getChild (i);
            const auto clipId = sessionClip.getProperty (core::Session::idProperty()).toString();
            const auto sourceTempo = static_cast<double> (
                sessionClip.getProperty ("sourceTempo", session.tempo()));
            const auto offset = static_cast<double> (sessionClip.getProperty ("offset", 0.0));

            if (auto* existing = audioClipForId (clipId))
            {
                // Already present, but undo may have restored a different
                // source tempo or lead-in behind it.
                existing->state.setProperty (sourceTempoProperty, sourceTempo, nullptr);
                existing->state.setProperty (offsetProperty, offset, nullptr);
                continue;
            }

            const auto path = sessionClip.getProperty ("sourceFile").toString();
            if (path.isNotEmpty())
                importAudioFile (juce::File (path), trackId, clipId, sourceTempo, offset);
        }
    }

    // The session owns tempo and time signature, so an undo that restores them
    // has to reach the engine too.
    if (! juce::approximatelyEqual (session.tempo(), tempo()))
        edit->tempoSequence.getTempo (0)->setBpm (juce::jlimit (20.0, 400.0, session.tempo()));

    setTimeSignature (session.timeSignatureNumerator(), session.timeSignatureDenominator());

    // Recompute every ratio from the restored per-clip source tempos. Without
    // this an undo leaves surviving clips stretched against the tempo of the
    // clip that was just removed.
    applyTempoToAudioClips();
}

//==============================================================================
bool EngineController::isPlaying() const
{
    return edit->getTransport().isPlaying();
}

bool EngineController::isLooping() const
{
    return edit->getTransport().looping.get();
}

bool EngineController::isMetronomeEnabled() const
{
    return edit->clickTrackEnabled.get();
}

double EngineController::positionSeconds() const
{
    return edit->getTransport().getPosition().inSeconds();
}

double EngineController::contentLengthSeconds() const
{
    double longest = 0.0;

    for (auto* track : te::getAudioTracks (*edit))
        for (auto* clip : track->getClips())
            longest = juce::jmax (longest, clip->getPosition().getEnd().inSeconds());

    return longest;
}

double EngineController::tempo() const
{
    return edit->tempoSequence.getTempo (0)->getBpm();
}

int EngineController::trackCount() const
{
    return te::getAudioTracks (*edit).size();
}

juce::AudioDeviceManager& EngineController::audioDeviceManager()
{
    return engine.getDeviceManager().deviceManager;
}

juce::String EngineController::audioFileWildcard() const
{
    return engine.getAudioFileFormatManager().readFormatManager.getWildcardForAllFormats();
}

} // namespace saamveda::engine
