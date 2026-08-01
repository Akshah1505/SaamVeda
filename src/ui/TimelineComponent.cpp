#include "TimelineComponent.h"

namespace saamveda::ui
{

void TimelineComponent::setLength (double seconds)
{
    lengthSeconds = juce::jmax (1.0, seconds);
    repaint();
}

void TimelineComponent::setPosition (double seconds)
{
    const auto next = juce::jlimit (0.0, lengthSeconds, seconds);
    if (! juce::approximatelyEqual (positionSeconds, next))
    {
        positionSeconds = next;
        repaint();
    }
}

void TimelineComponent::setMusicalGrid (double bpm, int numerator, int denominator)
{
    tempoBpm = juce::jlimit (20.0, 400.0, bpm);
    timeSigNumerator = juce::jlimit (1, 32, numerator);
    timeSigDenominator = juce::jlimit (1, 32, denominator);
    repaint();
}

void TimelineComponent::setSessionTracks (juce::ValueTree tracksToUse)
{
    tracks = std::move (tracksToUse);
    repaint();
}

void TimelineComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.setColour (juce::Colour (0xff272a33));
    g.fillRoundedRectangle (bounds.toFloat(), 5.0f);

    auto ruler = bounds.removeFromTop (28);
    // Keep the visible grid stable when tempo changes. BPM controls playback and
    // the metronome; the time signature alone controls grid spacing/grouping.
    // A quarter-note uses a fixed 0.5 s visual reference (120 BPM).
    const auto beatSeconds = 0.5 * (4.0 / timeSigDenominator);
    const auto barSeconds = beatSeconds * timeSigNumerator;
    const auto maxGridLines = 400;

    for (int beat = 0; beat < maxGridLines; ++beat)
    {
        const auto time = beat * beatSeconds;
        if (time > lengthSeconds)
            break;

        const bool barStart = beat % timeSigNumerator == 0;
        const auto x = ruler.getX() + static_cast<float> (time / lengthSeconds) * ruler.getWidth();
        g.setColour (barStart ? juce::Colour (0xff8f96a8) : juce::Colour (0xff444955));
        g.drawVerticalLine (static_cast<int> (x), static_cast<float> (ruler.getY()),
                            static_cast<float> (bounds.getBottom()));

        if (barStart)
            g.drawText (juce::String (beat / timeSigNumerator + 1), static_cast<int> (x) + 3,
                        ruler.getY() + 3, 32, 20, juce::Justification::centredLeft);
    }

    g.setColour (juce::Colour (0xffaeb4c3));
    g.drawText (juce::String (tempoBpm, 0) + " BPM  "
                    + juce::String (timeSigNumerator) + "/" + juce::String (timeSigDenominator)
                    + "  |  bar " + juce::String (static_cast<int> (positionSeconds / barSeconds) + 1),
                ruler.removeFromRight (210), juce::Justification::centredRight);

    const auto visibleTracks = juce::jmin (tracks.getNumChildren(), 4);
    for (int i = 0; i < visibleTracks; ++i)
    {
        auto row = bounds.removeFromTop (46).reduced (2);
        g.setColour (i % 2 == 0 ? juce::Colour (0xff323744) : juce::Colour (0xff2d313c));
        g.fillRoundedRectangle (row.toFloat(), 3.0f);
        g.setColour (juce::Colour (0xff68a0e8));
        g.fillRect (row.removeFromLeft (5));
        g.setColour (juce::Colours::white);
        g.drawText (tracks.getChild (i).getProperty ("name").toString(),
                    row.reduced (10, 0), juce::Justification::centredLeft);

        auto clips = tracks.getChild (i).getChildWithName ("CLIPS");
        if (clips.getNumChildren() > 0)
        {
            auto clip = clips.getChild (0);
            const auto duration = static_cast<double> (clip.getProperty ("length"));
            const auto width = juce::jmax (80.0f,
                static_cast<float> (duration / lengthSeconds) * row.getWidth());
            auto clipBounds = row.withTrimmedLeft (145).withWidth (
                juce::jmin (static_cast<int> (width), row.getWidth() - 145)).reduced (2);
            g.setColour (juce::Colour (0xff315f8f));
            g.fillRoundedRectangle (clipBounds.toFloat(), 3.0f);
            g.setColour (juce::Colours::white);
            g.drawText (clip.getProperty ("name").toString(), clipBounds.reduced (7, 0),
                        juce::Justification::centredLeft);
        }
    }

    if (tracks.getNumChildren() == 0)
    {
        g.setColour (juce::Colour (0xff858b99));
        g.drawText ("No tracks yet", bounds, juce::Justification::centred);
    }

    const auto playheadX = getLocalBounds().getX()
        + static_cast<float> (positionSeconds / lengthSeconds) * getWidth();
    g.setColour (juce::Colour (0xffffb74d));
    g.drawLine (playheadX, 0.0f, playheadX, static_cast<float> (getHeight()), 2.0f);
    juce::Path marker;
    marker.addTriangle (playheadX - 6.0f, 0.0f, playheadX + 6.0f, 0.0f,
                        playheadX, 9.0f);
    g.fillPath (marker);
}

void TimelineComponent::mouseDown (const juce::MouseEvent& event)
{
    seekFromX (event.x);
}

void TimelineComponent::mouseDrag (const juce::MouseEvent& event)
{
    seekFromX (event.x);
}

void TimelineComponent::seekFromX (int x)
{
    const auto proportion = juce::jlimit (0.0, 1.0, static_cast<double> (x) / getWidth());
    setPosition (proportion * lengthSeconds);
    if (onSeek)
        onSeek (positionSeconds);
}

} // namespace saamveda::ui
