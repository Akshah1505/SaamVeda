#include "TimelineComponent.h"

#include <cmath>

namespace saamveda::ui
{

TimelineComponent::TimelineComponent()
{
    setWantsKeyboardFocus (true);

    horizontalScrollBar.setAutoHide (false);
    horizontalScrollBar.addListener (this);
    verticalScrollBar.setAutoHide (false);
    verticalScrollBar.addListener (this);

    addAndMakeVisible (horizontalScrollBar);
    addAndMakeVisible (verticalScrollBar);
    updateScrollBars();
}

//==============================================================================
int TimelineComponent::rowCount() const
{
    return juce::jmax (layout::minimumVisibleTracks, tracks.getNumChildren());
}

int TimelineComponent::contentHeight() const
{
    return rowCount() * layout::laneHeight;
}

juce::Rectangle<int> TimelineComponent::rulerArea() const
{
    return getLocalBounds()
        .withTrimmedLeft (layout::trackHeaderWidth)
        .withTrimmedRight (layout::scrollBarThickness)
        .withHeight (layout::rulerHeight);
}

juce::Rectangle<int> TimelineComponent::headerArea() const
{
    return getLocalBounds()
        .withWidth (layout::trackHeaderWidth)
        .withTrimmedTop (layout::rulerHeight)
        .withTrimmedBottom (layout::scrollBarThickness);
}

juce::Rectangle<int> TimelineComponent::laneArea() const
{
    return getLocalBounds()
        .withTrimmedLeft (layout::trackHeaderWidth)
        .withTrimmedTop (layout::rulerHeight)
        .withTrimmedRight (layout::scrollBarThickness)
        .withTrimmedBottom (layout::scrollBarThickness);
}

juce::Rectangle<int> TimelineComponent::rowBounds (int trackIndex,
                                                   juce::Rectangle<int> column) const
{
    const auto y = column.getY() + trackIndex * layout::laneHeight
                 - static_cast<int> (std::lround (verticalOffset));
    return { column.getX(), y, column.getWidth(), layout::laneHeight };
}

juce::Rectangle<int> TimelineComponent::muteButtonBounds (int trackIndex) const
{
    const auto row = rowBounds (trackIndex, headerArea());

    // The dot is 8px, but an 8px click target is a miss waiting to happen, so
    // the button is 22px square with the dot drawn at its centre.
    return juce::Rectangle<int> (22, 22).withCentre ({ row.getRight() - 14, row.getCentreY() });
}

int TimelineComponent::trackIndexAt (juce::Point<int> position) const
{
    const auto column = headerArea();
    if (! column.contains (position))
        return -1;

    const auto index = static_cast<int> ((position.y - column.getY() + verticalOffset)
                                         / layout::laneHeight);
    return juce::isPositiveAndBelow (index, rowCount()) ? index : -1;
}

bool TimelineComponent::isTrackMuted (int trackIndex) const
{
    return juce::isPositiveAndBelow (trackIndex, tracks.getNumChildren())
        && static_cast<bool> (tracks.getChild (trackIndex).getProperty ("mute", false));
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

    if (selectedTrackIndex >= tracks.getNumChildren())
        selectedTrackIndex = -1;

    updateScrollBars();
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

void TimelineComponent::zoomIn()  { zoomAround (0.5, positionSeconds); }
void TimelineComponent::zoomOut() { zoomAround (2.0, positionSeconds); }

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
    updateScrollBars();
}

void TimelineComponent::setVerticalOffset (double pixels)
{
    const auto maximum = juce::jmax (0, contentHeight() - laneArea().getHeight());
    verticalOffset = juce::jlimit (0.0, static_cast<double> (maximum), pixels);
    updateScrollBars();
}

void TimelineComponent::updateScrollBars()
{
    horizontalScrollBar.setRangeLimits ({ 0.0, lengthSeconds }, juce::dontSendNotification);
    horizontalScrollBar.setCurrentRange ({ viewStartSeconds, viewStartSeconds + visibleSeconds },
                                         juce::dontSendNotification);

    const auto visibleHeight = juce::jmax (1, laneArea().getHeight());
    verticalScrollBar.setRangeLimits ({ 0.0, static_cast<double> (juce::jmax (contentHeight(),
                                                                             visibleHeight)) },
                                      juce::dontSendNotification);
    verticalScrollBar.setCurrentRange ({ verticalOffset, verticalOffset + visibleHeight },
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

void TimelineComponent::scrollBarMoved (juce::ScrollBar* bar, double newRangeStart)
{
    if (bar == &horizontalScrollBar)
        viewStartSeconds = juce::jlimit (0.0, juce::jmax (0.0, lengthSeconds - visibleSeconds),
                                         newRangeStart);
    else
        verticalOffset = juce::jmax (0.0, newRangeStart);

    repaint();
}

//==============================================================================
void TimelineComponent::resized()
{
    horizontalScrollBar.setBounds (getLocalBounds()
                                       .removeFromBottom (layout::scrollBarThickness)
                                       .withTrimmedLeft (layout::trackHeaderWidth)
                                       .withTrimmedRight (layout::scrollBarThickness));

    verticalScrollBar.setBounds (getLocalBounds()
                                     .removeFromRight (layout::scrollBarThickness)
                                     .withTrimmedTop (layout::rulerHeight)
                                     .withTrimmedBottom (layout::scrollBarThickness));

    setVerticalOffset (verticalOffset);
    updateScrollBars();
}

//==============================================================================
void TimelineComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::laneBackground);

    // Corner block between the header column and the ruler.
    g.setColour (colours::panelTitle);
    g.fillRect (getLocalBounds().withWidth (layout::trackHeaderWidth)
                    .withHeight (layout::rulerHeight));

    paintRuler (g);
    paintHeaders (g);
    paintLanes (g);
    paintPlayhead (g);

    g.setColour (colours::outline);
    g.drawVerticalLine (layout::trackHeaderWidth - 1, 0.0f, static_cast<float> (getHeight()));
    g.drawHorizontalLine (layout::rulerHeight - 1, 0.0f, static_cast<float> (getWidth()));
}

void TimelineComponent::paintRuler (juce::Graphics& g)
{
    const auto ruler = rulerArea();
    const auto lanes = laneArea();
    const auto pixelsPerSecond = lanes.getWidth() / juce::jmax (1.0e-6, visibleSeconds);

    g.setColour (colours::panelTitle);
    g.fillRect (ruler);

    const auto beatSeconds = (60.0 / tempoBpm) * (4.0 / timeSigDenominator);
    const auto barSeconds = beatSeconds * timeSigNumerator;

    int barStep = 1;
    while (barSeconds * barStep * pixelsPerSecond < 46.0 && barStep < 1024)
        barStep *= 2;

    auto firstBar = static_cast<long long> (std::floor (viewStartSeconds / barSeconds));
    firstBar -= firstBar % barStep;
    const auto lastBar = static_cast<long long> (std::ceil ((viewStartSeconds + visibleSeconds)
                                                            / barSeconds));

    g.saveState();
    g.reduceClipRegion (ruler);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));

    for (auto bar = juce::jmax (0LL, firstBar); bar <= lastBar; bar += barStep)
    {
        const auto x = static_cast<int> (timeToX (static_cast<double> (bar) * barSeconds));
        g.setColour (colours::gridBar);
        g.drawVerticalLine (x, static_cast<float> (ruler.getY() + 4),
                            static_cast<float> (ruler.getBottom()));
        g.drawText (juce::String (bar + 1), x + 4, ruler.getY(), 48, ruler.getHeight(),
                    juce::Justification::centredLeft);
    }

    g.restoreState();
}

void TimelineComponent::paintHeaders (juce::Graphics& g)
{
    const auto column = headerArea();

    g.saveState();
    g.reduceClipRegion (column);

    for (int i = 0; i < rowCount(); ++i)
    {
        auto row = rowBounds (i, column);
        if (row.getBottom() < column.getY() || row.getY() > column.getBottom())
            continue;

        const auto isRealTrack = i < tracks.getNumChildren();
        const auto isSelected = i == selectedTrackIndex;
        const auto muted = isTrackMuted (i);

        g.setColour (isSelected ? colours::accent.withAlpha (0.25f)
                                : (i % 2 == 0 ? colours::headerBackground
                                              : colours::headerAlternate));
        g.fillRect (row.reduced (0, 1));

        const auto bottom = row.getBottom();

        // Colour tag, drawn only for tracks that exist, so empty rows read as
        // placeholders rather than as silent tracks.
        if (isRealTrack)
        {
            g.setColour (muted ? colours::accent.withAlpha (0.3f) : colours::accent);
            g.fillRect (row.removeFromLeft (4).reduced (0, 3));
        }
        else
        {
            row.removeFromLeft (4);
        }

        const auto name = isRealTrack ? tracks.getChild (i).getProperty ("name").toString()
                                      : "Track " + juce::String (i + 1);

        g.setColour (! isRealTrack ? colours::textDim
                                   : (muted ? colours::textDim : colours::text));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText (name, row.reduced (8, 0).withTrimmedRight (26),
                    juce::Justification::centredLeft, true);

        // Mute button, in the same place FL Studio puts its track LED.
        const auto button = muteButtonBounds (i);
        const auto dot = juce::Rectangle<float> (10.0f, 10.0f)
                             .withCentre (button.getCentre().toFloat());

        if (isRealTrack && i == hoveredMuteTrack)
        {
            g.setColour (colours::textBright.withAlpha (0.12f));
            g.fillEllipse (button.reduced (2).toFloat());
        }

        if (! isRealTrack)
        {
            g.setColour (colours::ledOff);
            g.fillEllipse (dot);
        }
        else if (muted)
        {
            // Hollow ring rather than a dimmer dot: "off" has to be readable at
            // a glance across twelve rows, and a filled-but-darker circle is not.
            g.setColour (colours::ledOff);
            g.fillEllipse (dot);
            g.setColour (colours::muted);
            g.drawEllipse (dot.reduced (0.5f), 1.6f);
        }
        else
        {
            g.setColour (colours::ledOn);
            g.fillEllipse (dot);
        }

        g.setColour (colours::outline);
        g.drawHorizontalLine (bottom - 1, static_cast<float> (column.getX()),
                              static_cast<float> (column.getRight()));
    }

    g.restoreState();
}

void TimelineComponent::paintLanes (juce::Graphics& g)
{
    const auto lanes = laneArea();
    const auto pixelsPerSecond = lanes.getWidth() / juce::jmax (1.0e-6, visibleSeconds);

    g.saveState();
    g.reduceClipRegion (lanes);

    // Row striping first, so the grid draws on top of it.
    for (int i = 0; i < rowCount(); ++i)
    {
        const auto row = rowBounds (i, lanes);
        if (row.getBottom() < lanes.getY() || row.getY() > lanes.getBottom())
            continue;

        g.setColour (i % 2 == 0 ? colours::laneBackground : colours::laneAlternate);
        g.fillRect (row);
        g.setColour (colours::outline.withAlpha (0.5f));
        g.drawHorizontalLine (row.getBottom() - 1, static_cast<float> (lanes.getX()),
                              static_cast<float> (lanes.getRight()));
    }

    // Musical grid.
    const auto beatSeconds = (60.0 / tempoBpm) * (4.0 / timeSigDenominator);
    const auto barSeconds = beatSeconds * timeSigNumerator;
    const auto drawBeats = beatSeconds * pixelsPerSecond >= 9.0;

    int barStep = 1;
    while (barSeconds * barStep * pixelsPerSecond < 46.0 && barStep < 1024)
        barStep *= 2;

    auto firstBar = static_cast<long long> (std::floor (viewStartSeconds / barSeconds));
    firstBar -= firstBar % barStep;
    const auto lastBar = static_cast<long long> (std::ceil ((viewStartSeconds + visibleSeconds)
                                                            / barSeconds));

    for (auto bar = juce::jmax (0LL, firstBar); bar <= lastBar; bar += barStep)
    {
        const auto barTime = static_cast<double> (bar) * barSeconds;

        g.setColour (colours::gridBar.withAlpha (0.65f));
        g.drawVerticalLine (static_cast<int> (timeToX (barTime)),
                            static_cast<float> (lanes.getY()),
                            static_cast<float> (lanes.getBottom()));

        if (drawBeats)
        {
            g.setColour (colours::gridBeat);
            for (int beat = 1; beat < timeSigNumerator * barStep; ++beat)
                g.drawVerticalLine (static_cast<int> (timeToX (barTime + beat * beatSeconds)),
                                    static_cast<float> (lanes.getY()),
                                    static_cast<float> (lanes.getBottom()));
        }
    }

    // Clips.
    for (int i = 0; i < tracks.getNumChildren(); ++i)
    {
        const auto row = rowBounds (i, lanes);
        if (row.getBottom() < lanes.getY() || row.getY() > lanes.getBottom())
            continue;

        const auto muted = isTrackMuted (i);
        const auto clips = tracks.getChild (i).getChildWithName ("CLIPS");
        for (int clipIndex = 0; clipIndex < clips.getNumChildren(); ++clipIndex)
            paintClip (g, clips.getChild (clipIndex), row, muted);
    }

    g.restoreState();
}

void TimelineComponent::paintClip (juce::Graphics& g, const juce::ValueTree& clip,
                                   juce::Rectangle<int> row, bool muted)
{
    const auto start = static_cast<double> (clip.getProperty ("start"));
    const auto clipLength = juce::jmax (0.01, static_cast<double> (clip.getProperty ("length")));

    const auto left = timeToX (start);
    const auto right = timeToX (start + clipLength);
    if (right < row.getX() || left > row.getRight())
        return;

    auto bounds = juce::Rectangle<int> (static_cast<int> (left),
                                        row.getY() + 2,
                                        juce::jmax (4, static_cast<int> (right - left)),
                                        row.getHeight() - 5);

    // A muted clip stays in place and stays readable, but must not compete for
    // attention with the tracks that are actually sounding.
    const auto bodyColour = muted ? colours::clipBody.withMultipliedSaturation (0.25f)
                                                     .withMultipliedBrightness (0.7f)
                                  : colours::clipBody;
    const auto titleColour = muted ? colours::clipTitle.withMultipliedSaturation (0.25f)
                                                       .withMultipliedBrightness (0.7f)
                                   : colours::clipTitle;

    g.setColour (bodyColour);
    g.fillRoundedRectangle (bounds.toFloat(), 3.0f);

    auto title = bounds.removeFromTop (clipTitleHeight);
    g.setColour (titleColour);
    g.fillRect (title);

    g.setColour (muted ? colours::textDim : colours::textBright);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (clip.getProperty ("name").toString(), title.reduced (6, 0),
                juce::Justification::centredLeft, true);

    // Gain readout, in the same corner FL Studio uses.
    if (title.getWidth() > 120)
    {
        const auto gainDb = static_cast<double> (clip.getProperty ("gainDb", 0.0));
        g.setColour (colours::text);
        g.drawText (juce::String (gainDb, 1) + " dB", title.reduced (6, 0),
                    juce::Justification::centredRight, false);
    }

    // Placeholder for the waveform thumbnail that arrives in Phase 3.
    g.setColour (colours::textBright.withAlpha (0.25f));
    g.drawHorizontalLine (bounds.getCentreY(), static_cast<float> (bounds.getX() + 3),
                          static_cast<float> (bounds.getRight() - 3));
}

void TimelineComponent::paintPlayhead (juce::Graphics& g)
{
    const auto lanes = laneArea();
    const auto x = static_cast<float> (timeToX (positionSeconds));
    if (x < lanes.getX() || x > lanes.getRight())
        return;

    g.setColour (colours::playhead);
    g.drawLine (x, static_cast<float> (rulerArea().getY()), x,
                static_cast<float> (lanes.getBottom()), 1.5f);

    juce::Path marker;
    marker.addTriangle (x - 5.0f, static_cast<float> (rulerArea().getY()),
                        x + 5.0f, static_cast<float> (rulerArea().getY()),
                        x, static_cast<float> (rulerArea().getY()) + 8.0f);
    g.fillPath (marker);
}

//==============================================================================
void TimelineComponent::mouseDown (const juce::MouseEvent& event)
{
    grabKeyboardFocus();

    if (headerArea().contains (event.getPosition()))
    {
        const auto index = trackIndexAt (event.getPosition());

        // The mute button wins over selection: clicking it should silence the
        // track, not also drag the selection around under the pointer.
        if (juce::isPositiveAndBelow (index, tracks.getNumChildren())
            && muteButtonBounds (index).contains (event.getPosition()))
        {
            if (onTrackMuteToggled)
                onTrackMuteToggled (index);

            return;
        }

        selectedTrackIndex = juce::isPositiveAndBelow (index, tracks.getNumChildren()) ? index : -1;

        if (onTrackSelected)
            onTrackSelected (selectedTrackIndex);

        repaint();
        return;
    }

    seekFromX (event.x);
}

void TimelineComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (! headerArea().contains (event.getPosition()))
        seekFromX (event.x);
}

void TimelineComponent::mouseMove (const juce::MouseEvent& event)
{
    const auto index = trackIndexAt (event.getPosition());
    const auto overMute = juce::isPositiveAndBelow (index, tracks.getNumChildren())
                       && muteButtonBounds (index).contains (event.getPosition());
    const auto hovered = overMute ? index : -1;

    if (hovered == hoveredMuteTrack)
        return;

    hoveredMuteTrack = hovered;
    setMouseCursor (overMute ? juce::MouseCursor::PointingHandCursor
                             : juce::MouseCursor::NormalCursor);
    repaint();
}

void TimelineComponent::mouseExit (const juce::MouseEvent&)
{
    if (hoveredMuteTrack < 0)
        return;

    hoveredMuteTrack = -1;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
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

    if (event.mods.isShiftDown())
    {
        const auto delta = (wheel.deltaX != 0.0f ? wheel.deltaX : wheel.deltaY);
        if (delta != 0.0f)
        {
            setViewStart (viewStartSeconds - delta * visibleSeconds * 0.4);
            repaint();
        }

        return;
    }

    if (wheel.deltaY != 0.0f)
    {
        setVerticalOffset (verticalOffset - wheel.deltaY * layout::laneHeight * 2.0);
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
