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
    track.addChild (juce::ValueTree (clipsType()), -1, &undo);
    tracks().addChild (track, -1, &undo);
    return track;
}

juce::ValueTree Session::addAudioClip (juce::String trackId, const juce::File& sourceFile,
                                       double lengthSeconds)
{
    auto track = trackWithId (trackId);
    if (! track.isValid())
        return {};

    auto clip = juce::ValueTree (clipType());
    clip.setProperty (idProperty(), newId(), &undo);
    clip.setProperty ("name", sourceFile.getFileNameWithoutExtension(), &undo);
    clip.setProperty ("start", 0.0, &undo);
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
