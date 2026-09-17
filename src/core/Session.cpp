#include "Session.h"

namespace saamveda::core
{

juce::Identifier Session::sessionType() { return { "SESSION" }; }
juce::Identifier Session::tracksType()  { return { "TRACKS" }; }
juce::Identifier Session::trackType()   { return { "TRACK" }; }
juce::Identifier Session::clipsType()   { return { "CLIPS" }; }
juce::Identifier Session::clipType()    { return { "CLIP" }; }
juce::Identifier Session::idProperty()  { return { "id" }; }

juce::String Session::newId()
{
    return juce::Uuid().toString();
}

Session::Session()
    : sessionState (sessionType())
{
    initialiseDefaults();
}

void Session::initialiseDefaults()
{
    sessionState.setProperty ("version", currentSchemaVersion, nullptr);
    sessionState.setProperty ("name", "Untitled", nullptr);
    sessionState.setProperty ("tempo", 120.0, nullptr);
    sessionState.setProperty ("timeSigNum", 4, nullptr);
    sessionState.setProperty ("timeSigDenom", 4, nullptr);
    sessionState.setProperty ("sampleRate", 44100.0, nullptr);
    sessionState.addChild (juce::ValueTree (tracksType()), -1, nullptr);
}

Session::Session (juce::ValueTree state)
    : sessionState (std::move (state))
{
    if (! sessionState.isValid() || sessionState.getType() != sessionType())
    {
        sessionState = juce::ValueTree (sessionType());
        initialiseDefaults();
    }

    if (! sessionState.getChildWithName (tracksType()).isValid())
        sessionState.addChild (juce::ValueTree (tracksType()), -1, nullptr);
}

juce::ValueTree Session::tracks() const
{
    return sessionState.getChildWithName (tracksType());
}

juce::ValueTree Session::trackWithId (const juce::String& trackId) const
{
    return tracks().getChildWithProperty (idProperty(), trackId);
}

juce::ValueTree Session::clipsOf (const juce::ValueTree& track) const
{
    return track.getChildWithName (clipsType());
}

juce::ValueTree Session::addTrack (juce::String type, juce::String name)
{
    auto track = juce::ValueTree (trackType());
    track.setProperty (idProperty(), newId(), &undo);
    track.setProperty ("type", std::move (type), &undo);
    track.setProperty ("name", std::move (name), &undo);
    track.setProperty ("gain", 1.0f, &undo);
    track.setProperty ("pan", 0.0f, &undo);
    track.setProperty ("mute", false, &undo);
    track.setProperty ("solo", false, &undo);
    track.setProperty ("armed", false, &undo);
    track.addChild (juce::ValueTree (clipsType()), -1, &undo);
    tracks().addChild (track, -1, &undo);
    return track;
}

juce::ValueTree Session::addAudioClip (juce::String trackId, const juce::File& sourceFile,
                                       double lengthSeconds, double startSeconds)
{
    auto track = trackWithId (trackId);
    if (! track.isValid())
        return {};

    auto clip = juce::ValueTree (clipType());
    clip.setProperty (idProperty(), newId(), &undo);
    clip.setProperty ("name", sourceFile.getFileNameWithoutExtension(), &undo);
    clip.setProperty ("start", juce::jmax (0.0, startSeconds), &undo);
    clip.setProperty ("length", lengthSeconds, &undo);
    clip.setProperty ("offset", 0.0, &undo);
    clip.setProperty ("gainDb", 0.0, &undo);
    // Defaults to the project tempo, which makes the clip play at its recorded
    // speed until something says otherwise.
    clip.setProperty ("sourceTempo", tempo(), &undo);
    clip.setProperty ("sourceFile", sourceFile.getFullPathName(), &undo);
    clipsOf (track).addChild (clip, -1, &undo);
    return clip;
}

juce::ValueTree Session::clipWithId (const juce::String& clipId) const
{
    if (clipId.isEmpty())
        return {};

    const auto trackList = tracks();
    for (int i = 0; i < trackList.getNumChildren(); ++i)
    {
        auto clip = clipsOf (trackList.getChild (i)).getChildWithProperty (idProperty(), clipId);
        if (clip.isValid())
            return clip;
    }

    return {};
}

int Session::clipCount() const
{
    int total = 0;
    const auto trackList = tracks();

    for (int i = 0; i < trackList.getNumChildren(); ++i)
        total += clipsOf (trackList.getChild (i)).getNumChildren();

    return total;
}

bool Session::setClipSourceTempo (const juce::String& clipId, double bpm)
{
    auto clip = clipWithId (clipId);
    if (! clip.isValid())
        return false;

    clip.setProperty ("sourceTempo", juce::jlimit (20.0, 400.0, bpm), &undo);
    return true;
}

void Session::setAllClipSourceTempos (double bpm)
{
    const auto clamped = juce::jlimit (20.0, 400.0, bpm);
    const auto trackList = tracks();

    for (int i = 0; i < trackList.getNumChildren(); ++i)
    {
        auto clips = clipsOf (trackList.getChild (i));
        for (int j = 0; j < clips.getNumChildren(); ++j)
            clips.getChild (j).setProperty ("sourceTempo", clamped, &undo);
    }
}

double Session::clipSourceTempo (const juce::String& clipId) const
{
    const auto clip = clipWithId (clipId);
    return clip.isValid() ? static_cast<double> (clip.getProperty ("sourceTempo", tempo()))
                          : tempo();
}

bool Session::setClipOffset (const juce::String& clipId, double seconds)
{
    auto clip = clipWithId (clipId);
    if (! clip.isValid())
        return false;

    clip.setProperty ("offset", juce::jmax (0.0, seconds), &undo);
    return true;
}

bool Session::removeTrack (juce::String trackId)
{
    auto track = trackWithId (trackId);
    if (! track.isValid())
        return false;

    tracks().removeChild (track, &undo);
    return true;
}

bool Session::renameTrack (juce::String trackId, juce::String name)
{
    auto track = trackWithId (trackId);
    if (! track.isValid())
        return false;

    track.setProperty ("name", std::move (name), &undo);
    return true;
}

bool Session::setClipStart (const juce::String& clipId, double seconds)
{
    auto clip = clipWithId (clipId);
    if (! clip.isValid())
        return false;

    const auto clamped = juce::jmax (0.0, seconds);
    if (juce::approximatelyEqual (clamped, static_cast<double> (clip.getProperty ("start", 0.0))))
        return false;

    clip.setProperty ("start", clamped, &undo);
    return true;
}

double Session::clipStart (const juce::String& clipId) const
{
    const auto clip = clipWithId (clipId);
    return clip.isValid() ? static_cast<double> (clip.getProperty ("start", 0.0)) : 0.0;
}

juce::String Session::trackIdContainingClip (const juce::String& clipId) const
{
    if (clipId.isEmpty())
        return {};

    const auto trackList = tracks();
    for (int i = 0; i < trackList.getNumChildren(); ++i)
    {
        const auto track = trackList.getChild (i);
        if (clipsOf (track).getChildWithProperty (idProperty(), clipId).isValid())
            return track.getProperty (idProperty()).toString();
    }

    return {};
}

bool Session::moveClipToTrack (const juce::String& clipId, const juce::String& targetTrackId)
{
    const auto sourceTrackId = trackIdContainingClip (clipId);
    if (sourceTrackId.isEmpty() || sourceTrackId == targetTrackId)
        return false;

    auto targetTrack = trackWithId (targetTrackId);
    if (! targetTrack.isValid())
        return false;

    auto clip = clipWithId (clipId);
    auto sourceClips = clipsOf (trackWithId (sourceTrackId));

    // The node is reference counted, so it survives being detached and keeps
    // its id, length and source file - the clip stays the same clip.
    sourceClips.removeChild (clip, &undo);
    clipsOf (targetTrack).addChild (clip, -1, &undo);
    return true;
}

juce::ValueTree Session::ensureTrackAtIndex (int index)
{
    if (index < 0)
        return {};

    auto trackList = tracks();

    // Dropping a clip on row 5 of a one-track project should put it on row 5,
    // where the user let go - not bounce it back to the last real track. The
    // rows in between become real tracks too, because that is what the timeline
    // was already showing.
    while (trackList.getNumChildren() <= index)
        addTrack ("audio", "Audio " + juce::String (trackList.getNumChildren() + 1));

    return trackList.getChild (index);
}

bool Session::setTrackMute (const juce::String& trackId, bool muted)
{
    auto track = trackWithId (trackId);
    if (! track.isValid())
        return false;

    if (static_cast<bool> (track.getProperty ("mute", false)) == muted)
        return false;

    track.setProperty ("mute", muted, &undo);
    return true;
}

bool Session::isTrackMuted (const juce::String& trackId) const
{
    const auto track = trackWithId (trackId);
    return track.isValid() && static_cast<bool> (track.getProperty ("mute", false));
}

bool Session::setTrackSolo (const juce::String& trackId, bool soloed)
{
    auto track = trackWithId (trackId);
    if (! track.isValid())
        return false;

    if (static_cast<bool> (track.getProperty ("solo", false)) == soloed)
        return false;

    track.setProperty ("solo", soloed, &undo);
    return true;
}

bool Session::isTrackSoloed (const juce::String& trackId) const
{
    const auto track = trackWithId (trackId);
    return track.isValid() && static_cast<bool> (track.getProperty ("solo", false));
}

bool Session::hasAnySoloedTrack() const
{
    const auto trackList = tracks();

    for (int i = 0; i < trackList.getNumChildren(); ++i)
        if (static_cast<bool> (trackList.getChild (i).getProperty ("solo", false)))
            return true;

    return false;
}

bool Session::setTrackArmed (const juce::String& trackId, bool armed)
{
    auto track = trackWithId (trackId);
    if (! track.isValid())
        return false;

    if (static_cast<bool> (track.getProperty ("armed", false)) == armed)
        return false;

    track.setProperty ("armed", armed, &undo);
    return true;
}

bool Session::isTrackArmed (const juce::String& trackId) const
{
    const auto track = trackWithId (trackId);
    return track.isValid() && static_cast<bool> (track.getProperty ("armed", false));
}

bool Session::setTempo (double bpm)
{
    const auto clamped = juce::jlimit (20.0, 400.0, bpm);
    if (juce::approximatelyEqual (clamped, tempo()))
        return false;

    sessionState.setProperty ("tempo", clamped, &undo);
    return true;
}

bool Session::setTimeSignature (int numerator, int denominator)
{
    const auto num = juce::jlimit (1, 32, numerator);
    const auto denom = juce::jlimit (1, 32, denominator);
    if (num == timeSignatureNumerator() && denom == timeSignatureDenominator())
        return false;

    sessionState.setProperty ("timeSigNum", num, &undo);
    sessionState.setProperty ("timeSigDenom", denom, &undo);
    return true;
}

double Session::tempo() const
{
    return static_cast<double> (sessionState.getProperty ("tempo", 120.0));
}

int Session::timeSignatureNumerator() const
{
    return static_cast<int> (sessionState.getProperty ("timeSigNum", 4));
}

int Session::timeSignatureDenominator() const
{
    return static_cast<int> (sessionState.getProperty ("timeSigDenom", 4));
}

} // namespace saamveda::core
