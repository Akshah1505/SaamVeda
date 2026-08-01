#include "EngineController.h"

namespace saamveda::engine
{

namespace te = tracktion::engine;

EngineController::EngineController()
{
    engine.getDeviceManager().initialise (0, 2);
    edit = te::Edit::createSingleTrackEdit (engine, te::Edit::EditRole::forEditing);

    if (edit == nullptr)
        throw std::runtime_error ("tracktion_engine could not create an edit");

    setTempo (120.0);
    setTimeSignature (4, 4);
    edit->getTransport().setLoopRange (tracktion::core::TimeRange (
        tracktion::core::TimePosition::fromSeconds (0.0),
        tracktion::core::TimeDuration::fromSeconds (16.0)));
}

EngineController::~EngineController()
{
    if (edit != nullptr)
    {
        edit->getTransport().stop (false, true);
        edit->getTransport().freePlaybackContext();
    }
    edit.reset();
    engine.getDeviceManager().closeDevices();
}

void EngineController::play()
{
    edit->getTransport().play (false);
}

void EngineController::stop()
{
    edit->getTransport().stop (false, false);
}

void EngineController::seek (double seconds)
{
    edit->getTransport().setPosition (
        tracktion::core::TimePosition::fromSeconds (juce::jmax (0.0, seconds)));
}

void EngineController::setLooping (bool shouldLoop)
{
    edit->getTransport().looping = shouldLoop;
}

void EngineController::setTempo (double bpm)
{
    const auto wasPlaying = isPlaying();
    if (wasPlaying)
        stop();

    edit->tempoSequence.getTempo (0)->setBpm (juce::jlimit (20.0, 400.0, bpm));
    applyTempoToAudioClips();
    seek (0.0);

    if (wasPlaying)
        play();
}

void EngineController::setSourceTempo (double bpm)
{
    currentSourceTempo = juce::jlimit (20.0, 400.0, bpm);
    applyTempoToAudioClips();
}

void EngineController::setDetectedTempo (double bpm)
{
    const auto detectedBpm = juce::jlimit (20.0, 400.0, bpm);

    // Detection describes the audio as imported; it is not a request to alter
    // it. Setting source and project tempo together leaves every clip at 1.0x.
    currentSourceTempo = detectedBpm;
    edit->tempoSequence.getTempo (0)->setBpm (detectedBpm);
    applyTempoToAudioClips();
}

void EngineController::setTapTempo (double bpm)
{
    const auto tappedBpm = juce::jlimit (20.0, 400.0, bpm);
    const auto position = positionSeconds();
    const auto wasPlaying = isPlaying();

    // Keep the recording at 1.0x by updating its source tempo together with
    // the click engine's tempo. Restore the transport position afterwards.
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

    seek (position);
    if (wasPlaying && ! isPlaying())
        play();
}

void EngineController::setMetronomeEnabled (bool enabled)
{
    edit->clickTrackEnabled = enabled;
    edit->clickTrackEmphasiseBars = true;
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
    const auto shift = tracktion::core::TimeDuration::fromSeconds (firstBeatSeconds);

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
                clip->setStart (tracktion::core::TimePosition::fromSeconds (newStartSeconds),
                                true, true);
            }
            else
            {
                const auto trim = tracktion::core::TimeDuration::fromSeconds (-newStartSeconds);
                if (trim < position.getLength())
                    clip->setPosition ({ { tracktion::core::TimePosition(),
                                           position.getLength() - trim },
                                         position.getOffset() + trim });
            }
        }
    }

    seek (juce::jmax (0.0, beatPositionSeconds + shift));
    if (wasPlaying)
        play();
}

void EngineController::applyTempoToAudioClips()
{
    const auto wasPlaying = isPlaying();
    if (wasPlaying)
        stop();

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

    seek (0.0);
    if (wasPlaying)
        play();
}

void EngineController::setTimeSignature (int numerator, int denominator)
{
    const auto wasPlaying = isPlaying();
    if (wasPlaying)
        stop();

    edit->tempoSequence.getTimeSig (0)->setStringTimeSig (
        juce::String (juce::jlimit (1, 32, numerator)) + "/"
        + juce::String (juce::jlimit (1, 32, denominator)));
    seek (0.0);

    if (wasPlaying)
        play();
}

double EngineController::importAudioFile (const juce::File& file, int trackIndex)
{
    te::AudioFile audioFile (engine, file);
    if (! file.existsAsFile() || ! audioFile.isValid())
        return 0.0;

    edit->ensureNumberOfAudioTracks (trackIndex + 1);
    const auto tracks = te::getAudioTracks (*edit);
    if (! juce::isPositiveAndBelow (trackIndex, tracks.size()))
        return 0.0;

    const auto duration = audioFile.getLength();
    const te::ClipPosition position {
        { tracktion::core::TimePosition(),
          tracktion::core::TimeDuration::fromSeconds (duration) },
        tracktion::core::TimeDuration()
    };

    auto clip = tracks[trackIndex]->insertWaveClip (file.getFileNameWithoutExtension(),
                                                    file, position, false);
    if (clip != nullptr)
    {
        clip->setAutoTempo (false);
        clip->setAutoPitch (false);
        clip->setTimeStretchMode (te::TimeStretcher::soundtouchBetter);
        const auto speedRatio = tempo() / currentSourceTempo;
        clip->setSpeedRatio (speedRatio);
        clip->setLength (clip->getSourceLength() / speedRatio, true);
    }
    return clip != nullptr ? duration : 0.0;
}

bool EngineController::removeAudioTrack (int trackIndex)
{
    const auto tracks = te::getAudioTracks (*edit);
    if (! juce::isPositiveAndBelow (trackIndex, tracks.size()))
        return false;

    edit->deleteTrack (tracks[trackIndex]);
    return true;
}

void EngineController::synchronise (const core::Session& session)
{
    const auto wasPlaying = isPlaying();
    stop();

    auto engineTracks = te::getAudioTracks (*edit);
    for (int i = engineTracks.size(); --i >= 0;)
        edit->deleteTrack (engineTracks[i]);

    const auto sessionTracks = session.tracks();
    for (int trackIndex = 0; trackIndex < sessionTracks.getNumChildren(); ++trackIndex)
    {
        edit->ensureNumberOfAudioTracks (trackIndex + 1);
        const auto clips = sessionTracks.getChild (trackIndex).getChildWithName ("CLIPS");
        for (int clipIndex = 0; clipIndex < clips.getNumChildren(); ++clipIndex)
        {
            const auto path = clips.getChild (clipIndex).getProperty ("sourceFile").toString();
            if (path.isNotEmpty())
                importAudioFile (juce::File (path), trackIndex);
        }
    }

    if (wasPlaying)
        play();
}

bool EngineController::isPlaying() const
{
    return edit->getTransport().isPlaying();
}

bool EngineController::isLooping() const
{
    return edit->getTransport().looping.get();
}

double EngineController::positionSeconds() const
{
    return edit->getTransport().getPosition().inSeconds();
}

double EngineController::tempo() const
{
    return edit->tempoSequence.getTempo (0)->getBpm();
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
