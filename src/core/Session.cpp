#include "Session.h"

namespace saamveda::core
{

juce::Identifier Session::sessionType() { return { "SESSION" }; }
juce::Identifier Session::tracksType()  { return { "TRACKS" }; }
juce::Identifier Session::trackType()   { return { "TRACK" }; }
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
    track.addChild (juce::ValueTree ("CLIPS"), -1, &undo);
    tracks().addChild (track, -1, &undo);
    return track;
}

bool Session::addAudioClip (juce::String trackId, const juce::File& sourceFile, double lengthSeconds)
{
    auto children = tracks();
    for (int i = 0; i < children.getNumChildren(); ++i)
    {
        auto track = children.getChild (i);
        if (track.getProperty (idProperty()).toString() == trackId)
        {
            auto clips = track.getChildWithName ("CLIPS");
            auto clip = juce::ValueTree ("CLIP");
            clip.setProperty (idProperty(), newId(), &undo);
            clip.setProperty ("name", sourceFile.getFileNameWithoutExtension(), &undo);
            clip.setProperty ("start", 0.0, &undo);
            clip.setProperty ("length", lengthSeconds, &undo);
            clip.setProperty ("sourceFile", sourceFile.getFullPathName(), &undo);
            clips.addChild (clip, -1, &undo);
            return true;
        }
    }
    return false;
}

bool Session::removeTrack (juce::String trackId)
{
    auto children = tracks();
    for (int i = 0; i < children.getNumChildren(); ++i)
        if (children.getChild (i).getProperty (idProperty()).toString() == trackId)
        {
            children.removeChild (i, &undo);
            return true;
        }
    return false;
}

bool Session::renameTrack (juce::String trackId, juce::String name)
{
    auto children = tracks();
    for (int i = 0; i < children.getNumChildren(); ++i)
    {
        auto track = children.getChild (i);
        if (track.getProperty (idProperty()).toString() == trackId)
        {
            track.setProperty ("name", std::move (name), &undo);
            return true;
        }
    }
    return false;
}

} // namespace saamveda::core
