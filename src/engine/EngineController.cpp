#include "EngineController.h"

namespace saamveda::engine
{

namespace te = tracktion::engine;

const juce::Identifier EngineController::sessionTrackIdProperty { "saamvedaTrackId" };
const juce::Identifier EngineController::sessionClipIdProperty  { "saamvedaClipId" };
const juce::Identifier EngineController::sourceTempoProperty    { "saamvedaSourceTempo" };
const juce::Identifier EngineController::offsetProperty         { "saamvedaOffset" };
const juce::Identifier EngineController::startProperty          { "saamvedaStart" };

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
    // A DAW that cannot see an input cannot arm a track, so inputs are opened
    // here. The manifest already declares microphone use.
    engine.getDeviceManager().initialise (2, 2);
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
    edit->getTransport().addListener (this);

    // Allocating the playback context is what creates the input instances that
    // arming works through, so do it up front rather than on the first arm.
    edit->getTransport().ensureContextAllocated();
    attachLevelClient();
}

EngineController::~EngineController()
{
    realtimeCheck.detach();

    if (attachedLevelMeasurer != nullptr)
    {
        attachedLevelMeasurer->removeClient (inputLevelClient);
        attachedLevelMeasurer = nullptr;
    }

    if (edit != nullptr)
        edit->getTransport().removeListener (this);

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

    const auto startSeconds = juce::jmax (0.0, static_cast<double> (
        clip.state.getProperty (startProperty, 0.0)));

    // Written absolutely rather than trimmed off the current position: this
    // runs again on every tempo change, and an incremental trim would eat a
    // little more of the clip each time. The start comes from the stored
    // property for the same reason - reading it back off the clip would let
    // rounding walk the clip along the timeline.
    clip.setPosition ({ { seconds (startSeconds), duration (fullLength - trim) },
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

bool EngineController::setClipStart (const juce::String& clipId, double startSeconds)
{
    auto* clip = audioClipForId (clipId);
    if (clip == nullptr)
        return false;

    preservingTransport ([this, clip, startSeconds]
    {
        clip->state.setProperty (startProperty, juce::jmax (0.0, startSeconds), nullptr);
        applyClipSettings (*clip);
    });

    updateLoopRange();
    return true;
}

bool EngineController::moveClipToTrack (const juce::String& clipId,
                                        const juce::String& targetTrackId)
{
    auto* clip = audioClipForId (clipId);
    auto* target = trackForId (targetTrackId);
    if (clip == nullptr || target == nullptr)
        return false;

    auto moved = false;

    preservingTransport ([this, clip, target, &moved]
    {
        // moveTo reparents the existing clip, so the audio file is not read
        // again - which a delete-and-reimport round trip would do.
        moved = clip->moveTo (*target);

        if (moved)
            applyClipSettings (*clip);
    });

    updateLoopRange();
    return moved;
}

//==============================================================================
te::InputDeviceInstance* EngineController::firstWaveInput() const
{
    auto* context = edit->getTransport().getCurrentPlaybackContext();
    if (context == nullptr)
        return nullptr;

    for (auto* instance : context->getAllInputs())
        if (instance != nullptr
            && instance->getInputDevice().getDeviceType() == te::InputDevice::waveDevice
            && instance->getInputDevice().isEnabled())
            return instance;

    return nullptr;
}

void EngineController::disableRetrospectiveRecord()
{
    auto* context = edit->getTransport().getCurrentPlaybackContext();
    if (context == nullptr)
        return;

    // tracktion keeps a rolling buffer of recent input so a take you forgot to
    // record can be recovered afterwards. Maintaining it happens on the audio
    // thread - a buffer resize, a std::map lookup keyed on the Edit's project
    // item, and a FIFO write, every block - which is exactly the allocation the
    // detector was reporting. We do not offer retrospective record, so we
    // should not pay for it. Setting the lock is what makes
    // WaveInputDevice::consumeNextAudioBlock skip the whole path.
    te::InputDevice::setRetrospectiveLock (engine, context->getAllInputs(), true);
}

void EngineController::attachLevelClient()
{
    auto* instance = firstWaveInput();
    auto* measurer = instance != nullptr ? &instance->getInputDevice().levelMeasurer : nullptr;

    if (measurer == attachedLevelMeasurer)
        return;

    if (attachedLevelMeasurer != nullptr)
        attachedLevelMeasurer->removeClient (inputLevelClient);

    attachedLevelMeasurer = measurer;

    if (attachedLevelMeasurer != nullptr)
    {
        attachedLevelMeasurer->addClient (inputLevelClient);

        // This runs the first time an input instance actually exists, which is
        // the earliest the retrospective lock can be applied - at construction
        // there is nothing to apply it to.
        disableRetrospectiveRecord();
    }
}

juce::StringArray EngineController::inputDeviceNames() const
{
    juce::StringArray names;

    for (auto* device : engine.getDeviceManager().getWaveInputDevices())
        if (device != nullptr && device->isEnabled())
            names.add (device->getName());

    return names;
}

bool EngineController::setTrackArmed (const juce::String& trackId, bool armed)
{
    auto* track = trackForId (trackId);
    if (track == nullptr)
        return false;

    edit->getTransport().ensureContextAllocated();
    disableRetrospectiveRecord();
    attachLevelClient();

    auto* instance = firstWaveInput();
    if (instance == nullptr)
        return false;

    if (armed)
    {
        // The input has to be pointed at the track before recording can be
        // enabled for it; setTarget is what creates that connection.
        if (! instance->setTarget (track->itemID, true, nullptr).has_value())
            return false;
    }

    instance->setRecordingEnabled (track->itemID, armed);

    if (! armed)
        instance->removeTarget (track->itemID, nullptr);

    return true;
}

bool EngineController::isTrackArmed (const juce::String& trackId) const
{
    auto* track = trackForId (trackId);
    auto* instance = firstWaveInput();

    return track != nullptr && instance != nullptr
        && instance->isRecordingEnabled (track->itemID);
}

int EngineController::armedTrackCount() const
{
    auto* instance = firstWaveInput();
    if (instance == nullptr)
        return 0;

    auto count = 0;
    for (auto* track : te::getAudioTracks (*edit))
        if (instance->isRecordingEnabled (track->itemID))
            ++count;

    return count;
}

void EngineController::setInputMonitoring (bool enabled)
{
    if (auto* instance = firstWaveInput())
        instance->getInputDevice().setMonitorMode (
            enabled ? te::InputDevice::MonitorMode::on
                    : te::InputDevice::MonitorMode::automatic);
}

bool EngineController::isInputMonitoring() const
{
    if (auto* instance = firstWaveInput())
        return instance->getInputDevice().getMonitorMode() == te::InputDevice::MonitorMode::on;

    return false;
}

float EngineController::inputLevelDb()
{
    attachLevelClient();

    if (attachedLevelMeasurer == nullptr)
        return -100.0f;

    auto peak = -100.0f;
    const auto channels = juce::jmax (1, inputLevelClient.getNumChannelsUsed());

    for (int channel = 0; channel < channels; ++channel)
        peak = juce::jmax (peak, inputLevelClient.getAndClearAudioLevel (channel).dB);

    return peak;
}

bool EngineController::adoptRecordedClip (const juce::String& trackId, const juce::File& file,
                                          const juce::String& clipId)
{
    auto* track = trackForId (trackId);
    if (track == nullptr || clipId.isEmpty())
        return false;

    for (auto* clip : track->getClips())
    {
        auto* wave = dynamic_cast<te::WaveAudioClip*> (clip);
        if (wave == nullptr)
            continue;

        // Match on the file and on not having been claimed yet: a second take
        // onto the same track must not steal the first take's id.
        if (wave->getSourceFileReference().getFile() != file
            || wave->state.getProperty (sessionClipIdProperty).toString().isNotEmpty())
            continue;

        wave->state.setProperty (sessionClipIdProperty, clipId, nullptr);
        wave->state.setProperty (sourceTempoProperty, tempo(), nullptr);
        wave->state.setProperty (offsetProperty, 0.0, nullptr);
        wave->state.setProperty (startProperty, wave->getPosition().getStart().inSeconds(), nullptr);

        updateLoopRange();
        return true;
    }

    return false;
}

void EngineController::startRecording()
{
    edit->getTransport().ensureContextAllocated();

    // allowRecordingIfNoInputsArmed stays false: recording with nothing armed
    // would look like it worked and produce nothing.
    edit->getTransport().record (false, false);
}

void EngineController::stopRecording (bool discardRecordings)
{
    edit->getTransport().stopRecording (discardRecordings);
    edit->getTransport().stop (false, false);
}

bool EngineController::isRecording() const
{
    return edit->getTransport().isRecording();
}

void EngineController::setCountInBars (int bars)
{
    edit->setCountInMode (bars >= 2 ? te::Edit::CountIn::twoBar
                                    : (bars == 1 ? te::Edit::CountIn::oneBar
                                                 : te::Edit::CountIn::none));
}

int EngineController::countInBars() const
{
    switch (edit->getCountInMode())
    {
        case te::Edit::CountIn::oneBar:  return 1;
        case te::Edit::CountIn::twoBar:  return 2;
        case te::Edit::CountIn::oneBeat:
        case te::Edit::CountIn::twoBeat:
        case te::Edit::CountIn::none:
        default:                         return 0;
    }
}

void EngineController::recordingFinished (te::InputDeviceInstance&, te::EditItemID targetID,
                                          const juce::ReferenceCountedArray<te::Clip>& clips)
{
    if (onClipRecorded == nullptr)
        return;

    // tracktion created these clips itself, so they carry none of our ids. The
    // session has to adopt them or the next synchronise will sweep them away as
    // clips it does not know about.
    for (auto* track : te::getAudioTracks (*edit))
    {
        if (track->itemID != targetID)
            continue;

        const auto trackId = track->state.getProperty (sessionTrackIdProperty).toString();

        for (auto* clip : clips)
        {
            auto* wave = dynamic_cast<te::WaveAudioClip*> (clip);
            if (wave == nullptr)
                continue;

            const auto position = wave->getPosition();
            onClipRecorded (trackId, wave->getSourceFileReference().getFile(),
                            position.getStart().inSeconds(),
                            position.getLength().inSeconds());
        }
    }
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
                                          double offsetSeconds, double startSeconds)
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
    clip->state.setProperty (startProperty, juce::jmax (0.0, startSeconds), nullptr);
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

    // Every clip the session still knows about, wherever it now lives. A clip
    // dragged to another track is not a deletion, so it must not be torn down
    // and read off disk again just because it left the track it started on.
    juce::StringArray wantedClipIdsAnywhere;
    for (int i = 0; i < sessionTracks.getNumChildren(); ++i)
    {
        const auto clips = session.clipsOf (sessionTracks.getChild (i));
        for (int j = 0; j < clips.getNumChildren(); ++j)
            wantedClipIdsAnywhere.add (clips.getChild (j)
                                           .getProperty (core::Session::idProperty()).toString());
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

        // Copy before mutating: removing a clip modifies the track's own array.
        // Only clips the session has dropped entirely are torn down here; one
        // that merely changed track is relocated below.
        const juce::Array<te::Clip*> currentClips (engineTrack->getClips());
        for (auto* clip : currentClips)
        {
            const auto id = clip->state.getProperty (sessionClipIdProperty).toString();
            if (id.isEmpty() || ! wantedClipIdsAnywhere.contains (id))
                clip->removeFromParent();
        }

        for (int i = 0; i < sessionClips.getNumChildren(); ++i)
        {
            const auto sessionClip = sessionClips.getChild (i);
            const auto clipId = sessionClip.getProperty (core::Session::idProperty()).toString();
            const auto sourceTempo = static_cast<double> (
                sessionClip.getProperty ("sourceTempo", session.tempo()));
            const auto offset = static_cast<double> (sessionClip.getProperty ("offset", 0.0));
            const auto clipStart = static_cast<double> (sessionClip.getProperty ("start", 0.0));

            if (auto* existing = audioClipForId (clipId))
            {
                // Present somewhere. Put it on the track the session says it
                // belongs to, then refresh the properties an undo may have
                // restored behind it.
                if (existing->getTrack() != engineTrack)
                    existing->moveTo (*engineTrack);

                existing->state.setProperty (sourceTempoProperty, sourceTempo, nullptr);
                existing->state.setProperty (offsetProperty, offset, nullptr);
                existing->state.setProperty (startProperty, clipStart, nullptr);
                continue;
            }

            const auto path = sessionClip.getProperty ("sourceFile").toString();
            if (path.isNotEmpty())
                importAudioFile (juce::File (path), trackId, clipId, sourceTempo, offset, clipStart);
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
