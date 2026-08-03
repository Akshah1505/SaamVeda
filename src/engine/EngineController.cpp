#include "EngineController.h"

namespace saamveda::engine
{

namespace te = tracktion::engine;

const juce::Identifier EngineController::sessionTrackIdProperty { "saamvedaTrackId" };
const juce::Identifier EngineController::sessionClipIdProperty  { "saamvedaClipId" };

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

void EngineController::setSourceTempo (double bpm)
{
    preservingTransport ([this, bpm]
    {
        currentSourceTempo = juce::jlimit (20.0, 400.0, bpm);
        applyTempoToAudioClips();
    });
}

void EngineController::setDetectedTempo (double bpm)
{
    const auto detectedBpm = juce::jlimit (20.0, 400.0, bpm);

    // Detection describes the audio as imported; it is not a request to alter
    // it. Setting source and project tempo together leaves every clip at 1.0x.
    preservingTransport ([this, detectedBpm]
    {
        currentSourceTempo = detectedBpm;
        edit->tempoSequence.getTempo (0)->setBpm (detectedBpm);
        applyTempoToAudioClips();
    });
}

void EngineController::setTapTempo (double bpm)
{
    const auto tappedBpm = juce::jlimit (20.0, 400.0, bpm);

    // Keep the recording at 1.0x by updating its source tempo together with the
    // click engine's tempo.
    preservingTransport ([this, tappedBpm]
    {
        currentSourceTempo = tappedBpm;
        edit->tempoSequence.getTempo (0)->setBpm (tappedBpm);

        for (auto* track : te::getAudioTracks (*edit))
            for (auto* clip : track->getClips())
                if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (clip))
                {
                    audioClip->setAutoTempo (false);
                    audioClip->setAutoPitch (false);
                    audioClip->setTimeStretchMode (te::TimeStretcher::soundtouchBetter);
                    audioClip->setSpeedRatio (1.0);
                    audioClip->setLength (audioClip->getSourceLength(), true);
                }
    });
}

void EngineController::alignFirstBeat (double firstBeatSeconds)
{
    if (firstBeatSeconds <= 0.0)
        return;

    const auto wasPlaying = isPlaying();
    if (wasPlaying)
        stop();

    // The click grid starts at 0. Skip the song's pre-beat lead-in so its first
    // detected beat is heard at transport zero without altering audio speed.
    const auto shift = duration (firstBeatSeconds);

    for (auto* track : te::getAudioTracks (*edit))
    {
        for (auto* clip : track->getClips())
        {
            const auto position = clip->getPosition();
            if (shift < position.getLength())
                clip->setPosition ({ { position.getStart(), position.getLength() - shift },
                                      position.getOffset() + shift });
        }
    }

    seek (0.0);
    updateLoopRange();

    if (wasPlaying)
        play();
}

void EngineController::alignBeatAtPosition (double beatPositionSeconds)
{
    const auto beatSeconds = 60.0 / juce::jmax (1.0, tempo());
    const auto phase = std::fmod (juce::jmax (0.0, beatPositionSeconds), beatSeconds);
    const auto shift = phase <= beatSeconds * 0.5 ? -phase : beatSeconds - phase;
    if (std::abs (shift) < 0.001)
        return;

    const auto wasPlaying = isPlaying();
    if (wasPlaying)
        stop();

    for (auto* track : te::getAudioTracks (*edit))
    {
        for (auto* clip : track->getClips())
        {
            const auto position = clip->getPosition();
            const auto newStartSeconds = position.getStart().inSeconds() + shift;

            if (newStartSeconds >= 0.0)
            {
                clip->setStart (seconds (newStartSeconds), true, true);
            }
            else
            {
                const auto trim = duration (-newStartSeconds);
                if (trim < position.getLength())
                    clip->setPosition ({ { tracktion::core::TimePosition(),
                                           position.getLength() - trim },
                                         position.getOffset() + trim });
            }
        }
    }

    seek (juce::jmax (0.0, beatPositionSeconds + shift));
    updateLoopRange();

    if (wasPlaying)
        play();
}

void EngineController::applyTempoToAudioClips()
{
    const auto projectTempo = tempo();
    // A 120 BPM recording targeted at 60 BPM uses a 0.5 ratio and therefore
    // becomes twice as long while SoundTouch preserves pitch.
    const auto speedRatio = projectTempo / currentSourceTempo;

    for (auto* track : te::getAudioTracks (*edit))
    {
        for (auto* clip : track->getClips())
        {
            if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (clip))
            {
                audioClip->setAutoTempo (false);
                audioClip->setAutoPitch (false);
                audioClip->setTimeStretchMode (te::TimeStretcher::soundtouchBetter);
                audioClip->setSpeedRatio (speedRatio);
                audioClip->setLength (audioClip->getSourceLength() / speedRatio, true);
            }
        }
    }

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
                                          const juce::String& clipId)
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
    clip->setAutoTempo (false);
    clip->setAutoPitch (false);
    clip->setTimeStretchMode (te::TimeStretcher::soundtouchBetter);

    const auto speedRatio = tempo() / currentSourceTempo;
    clip->setSpeedRatio (speedRatio);
    clip->setLength (clip->getSourceLength() / speedRatio, true);

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
            if (clipForId (*engineTrack, clipId) != nullptr)
                continue;

            const auto path = sessionClip.getProperty ("sourceFile").toString();
            if (path.isNotEmpty())
                importAudioFile (juce::File (path), trackId, clipId);
        }
    }

    // The session owns tempo and time signature, so an undo that restores them
    // has to reach the engine too.
    if (! juce::approximatelyEqual (session.tempo(), tempo()))
        setTempo (session.tempo());

    setTimeSignature (session.timeSignatureNumerator(), session.timeSignatureDenominator());
    updateLoopRange();
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
