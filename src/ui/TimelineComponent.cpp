#include "TimelineComponent.h"

#include <cmath>

namespace saamveda::ui
{

TimelineComponent::TimelineComponent()
{
    setWantsKeyboardFocus (true);
    addAndMakeVisible (horizontalScrollBar);
    horizontalScrollBar.setAutoHide (false);
    horizontalScrollBar.addListener (this);
    updateScrollBar();
}

//==============================================================================
juce::Rectangle<int> TimelineComponent::laneArea() const
{
    return getLocalBounds()
        .withTrimmedBottom (scrollBarHeight)
        .withTrimmedLeft (headerWidth);
}

double TimelineComponent::timeToX (double secondsPosition) const
{
    const auto area = laneArea();
    return area.getX() + (secondsPosition - viewStartSeconds) / visibleSeconds * area.getWidth();
}

double TimelineComponent::xToTime (double x) const
{
    const auto area = laneArea();
    if (area.getWidth() <= 0)
        return viewStartSeconds;

    return viewStartSeconds + (x - area.getX()) / area.getWidth() * visibleSeconds;
}

//==============================================================================
void TimelineComponent::setLength (double seconds)
{
    lengthSeconds = juce::jmax (1.0, seconds);
    visibleSeconds = juce::jlimit (minimumVisibleSeconds, lengthSeconds, visibleSeconds);
    setViewStart (viewStartSeconds);
    repaint();
}

void TimelineComponent::setPosition (double seconds)
{
    const auto next = juce::jlimit (0.0, lengthSeconds, seconds);
    if (juce::approximatelyEqual (positionSeconds, next))
        return;

    positionSeconds = next;
    followPlayheadIfNeeded();
    repaint();
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

//==============================================================================
void TimelineComponent::zoomAround (double factor, double anchorSeconds)
{
    const auto previous = visibleSeconds;
    visibleSeconds = juce::jlimit (minimumVisibleSeconds, lengthSeconds, visibleSeconds * factor);

    if (juce::approximatelyEqual (previous, visibleSeconds))
        return;

    // Keep whatever the user pointed at under the same pixel.
    const auto anchorProportion = juce::jlimit (0.0, 1.0,
                                                (anchorSeconds - viewStartSeconds) / previous);
    setViewStart (anchorSeconds - anchorProportion * visibleSeconds);
    repaint();
}

void TimelineComponent::zoomIn()
{
    zoomAround (0.5, positionSeconds);
}

void TimelineComponent::zoomOut()
{
    zoomAround (2.0, positionSeconds);
}

void TimelineComponent::zoomNormal()
{
    visibleSeconds = juce::jlimit (minimumVisibleSeconds, lengthSeconds, 60.0);
    setViewStart (positionSeconds - visibleSeconds * 0.5);
    repaint();
}

void TimelineComponent::zoomToFit()
{
    visibleSeconds = lengthSeconds;
    setViewStart (0.0);
    repaint();
}

void TimelineComponent::setFollowPlayhead (bool shouldFollow)
{
    followPlayhead = shouldFollow;
    followPlayheadIfNeeded();
    repaint();
}

void TimelineComponent::setViewStart (double seconds)
{
    viewStartSeconds = juce::jlimit (0.0, juce::jmax (0.0, lengthSeconds - visibleSeconds), seconds);
    updateScrollBar();
}

void TimelineComponent::updateScrollBar()
{
    horizontalScrollBar.setRangeLimits ({ 0.0, lengthSeconds }, juce::dontSendNotification);
    horizontalScrollBar.setCurrentRange ({ viewStartSeconds, viewStartSeconds + visibleSeconds },
                                         juce::dontSendNotification);
}

void TimelineComponent::followPlayheadIfNeeded()
{
    if (! followPlayhead)
        return;

    const auto viewEnd = viewStartSeconds + visibleSeconds;
    if (positionSeconds >= viewStartSeconds && positionSeconds < viewEnd - visibleSeconds * 0.1)
        return;

    // Place the playhead a tenth of the way in so there is context behind it
    // and most of the view ahead of it.
    setViewStart (positionSeconds - visibleSeconds * 0.1);
}

void TimelineComponent::scrollBarMoved (juce::ScrollBar*, double newRangeStart)
{
    viewStartSeconds = juce::jlimit (0.0, juce::jmax (0.0, lengthSeconds - visibleSeconds),
                                     newRangeStart);
    repaint();
}

//==============================================================================
void TimelineComponent::resized()
{
    horizontalScrollBar.setBounds (getLocalBounds()
                                       .removeFromBottom (scrollBarHeight)
                                       .withTrimmedLeft (headerWidth));
    updateScrollBar();
}

void TimelineComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().withTrimmedBottom (scrollBarHeight);
    g.setColour (juce::Colour (0xff272a33));
    g.fillRoundedRectangle (bounds.toFloat(), 5.0f);

    const auto lanes = laneArea();
    const auto pixelsPerSecond = lanes.getWidth() / juce::jmax (1.0e-6, visibleSeconds);

    // ---- musical grid -------------------------------------------------------
    const auto beatSeconds = (60.0 / tempoBpm) * (4.0 / timeSigDenominator);
    const auto barSeconds = beatSeconds * timeSigNumerator;
    const auto drawBeats = beatSeconds * pixelsPerSecond >= 9.0;

    int barStep = 1;
    while (barSeconds * barStep * pixelsPerSecond < 46.0 && barStep < 1024)
        barStep *= 2;

    const auto viewEnd = viewStartSeconds + visibleSeconds;
    auto firstBar = static_cast<long long> (std::floor (viewStartSeconds / barSeconds));
    firstBar -= firstBar % barStep;
    const auto lastBar = static_cast<long long> (std::ceil (viewEnd / barSeconds));

    g.saveState();
    g.reduceClipRegion (lanes);

    for (auto bar = juce::jmax (0LL, firstBar); bar <= lastBar; bar += barStep)
    {
        const auto barTime = static_cast<double> (bar) * barSeconds;
        const auto x = static_cast<int> (timeToX (barTime));

        g.setColour (juce::Colour (0xff8f96a8));
        g.drawVerticalLine (x, static_cast<float> (lanes.getY()),
                            static_cast<float> (lanes.getBottom()));
        g.drawText (juce::String (bar + 1), x + 3, lanes.getY() + 3, 46, 20,
                    juce::Justification::centredLeft);

        if (drawBeats)
        {
            g.setColour (juce::Colour (0xff444955));
            for (int beat = 1; beat < timeSigNumerator * barStep; ++beat)
                g.drawVerticalLine (static_cast<int> (timeToX (barTime + beat * beatSeconds)),
                                    static_cast<float> (lanes.getY() + rulerHeight),
                                    static_cast<float> (lanes.getBottom()));
        }
    }

    g.restoreState();

    // ---- track rows ---------------------------------------------------------
    auto rows = bounds.withTrimmedTop (rulerHeight);
    const auto maximumRows = juce::jmax (1, rows.getHeight() / laneHeight);
    const auto visibleTracks = juce::jmin (tracks.getNumChildren(), maximumRows);

    for (int i = 0; i < visibleTracks; ++i)
    {
        auto row = rows.removeFromTop (laneHeight).reduced (2);
        auto header = row.removeFromLeft (headerWidth - 4);

        g.setColour (i % 2 == 0 ? juce::Colour (0xff323744) : juce::Colour (0xff2d313c));
        g.fillRoundedRectangle (header.toFloat(), 3.0f);
        g.setColour (juce::Colour (0xff68a0e8));
        g.fillRect (header.removeFromLeft (5));
        g.setColour (juce::Colours::white);
        g.drawText (tracks.getChild (i).getProperty ("name").toString(),
                    header.reduced (8, 0), juce::Justification::centredLeft);

        g.saveState();
        g.reduceClipRegion (row);

        const auto clips = tracks.getChild (i).getChildWithName ("CLIPS");
        for (int clipIndex = 0; clipIndex < clips.getNumChildren(); ++clipIndex)
        {
            const auto clip = clips.getChild (clipIndex);
            const auto start = static_cast<double> (clip.getProperty ("start"));
            const auto clipLength = juce::jmax (0.01, static_cast<double> (clip.getProperty ("length")));

            const auto left = timeToX (start);
            const auto right = timeToX (start + clipLength);
            if (right < row.getX() || left > row.getRight())
                continue;

            const juce::Rectangle<float> clipBounds (static_cast<float> (left),
                                                     static_cast<float> (row.getY()),
                                                     juce::jmax (3.0f, static_cast<float> (right - left)),
                                                     static_cast<float> (row.getHeight()));

            g.setColour (juce::Colour (0xff315f8f));
            g.fillRoundedRectangle (clipBounds.reduced (1.0f), 3.0f);
            g.setColour (juce::Colours::white);
            g.drawText (clip.getProperty ("name").toString(),
                        clipBounds.reduced (7.0f, 0.0f).toNearestInt(),
                        juce::Justification::centredLeft);
        }

        g.restoreState();
    }

    if (tracks.getNumChildren() == 0)
    {
        g.setColour (juce::Colour (0xff858b99));
        g.drawText ("No tracks yet", bounds.withTrimmedTop (rulerHeight),
                    juce::Justification::centred);
    }

    // ---- status strip -------------------------------------------------------
    // Sits on top of the ruler, so it needs an opaque backing or bar numbers
    // print through it.
    const auto statusArea = bounds.removeFromTop (rulerHeight).removeFromRight (300);
    g.setColour (juce::Colour (0xff272a33));
    g.fillRect (statusArea);

    g.setColour (juce::Colour (0xffaeb4c3));
    g.drawText (juce::String (tempoBpm, 0) + " BPM  "
                    + juce::String (timeSigNumerator) + "/" + juce::String (timeSigDenominator)
                    + "  |  bar " + juce::String (static_cast<int> (positionSeconds / barSeconds) + 1)
                    + "  |  view " + juce::String (visibleSeconds, 1) + " s"
                    + (followPlayhead ? "  |  follow" : ""),
                statusArea.withTrimmedRight (8),
                juce::Justification::centredRight);

    // ---- playhead -----------------------------------------------------------
    const auto playheadX = static_cast<float> (timeToX (positionSeconds));
    if (playheadX >= lanes.getX() && playheadX <= lanes.getRight())
    {
        g.setColour (juce::Colour (0xffffb74d));
        g.drawLine (playheadX, static_cast<float> (lanes.getY()),
                    playheadX, static_cast<float> (lanes.getBottom()), 2.0f);

        juce::Path marker;
        marker.addTriangle (playheadX - 6.0f, static_cast<float> (lanes.getY()),
                            playheadX + 6.0f, static_cast<float> (lanes.getY()),
                            playheadX, static_cast<float> (lanes.getY()) + 9.0f);
        g.fillPath (marker);
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (juce::Colour (0xff68a0e8).withAlpha (0.5f));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 5.0f, 1.0f);
    }
}

//==============================================================================
void TimelineComponent::mouseDown (const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    seekFromX (event.x);
}

void TimelineComponent::mouseDrag (const juce::MouseEvent& event)
{
    seekFromX (event.x);
}

void TimelineComponent::mouseWheelMove (const juce::MouseEvent& event,
                                        const juce::MouseWheelDetails& wheel)
{
    if (event.mods.isCtrlDown() || event.mods.isCommandDown())
    {
        if (wheel.deltaY > 0.0f)
            zoomAround (0.8, xToTime (event.x));
        else if (wheel.deltaY < 0.0f)
            zoomAround (1.25, xToTime (event.x));

        return;
    }

    const auto delta = (wheel.deltaX != 0.0f ? wheel.deltaX : wheel.deltaY);
    if (delta != 0.0f)
    {
        setViewStart (viewStartSeconds - delta * visibleSeconds * 0.4);
        repaint();
    }
}

void TimelineComponent::seekFromX (int x)
{
    const auto lanes = laneArea();
    if (x < lanes.getX())
        return;

    const auto seconds = juce::jlimit (0.0, lengthSeconds, xToTime (x));
    positionSeconds = seconds;
    repaint();

    if (onSeek)
        onSeek (seconds);
}

} // namespace saamveda::ui
