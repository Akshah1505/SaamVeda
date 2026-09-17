#include "TimelineComponent.h"

#include <cmath>

namespace saamveda::ui
{

TimelineComponent::TimelineComponent()
{
    setWantsKeyboardFocus (true);

    zoomBar.setMinimumVisible (minimumVisibleSeconds);
    zoomBar.onRangeChanged = [this] (double start, double visible)
    {
        visibleSeconds = juce::jlimit (minimumVisibleSeconds, lengthSeconds, visible);
        viewStartSeconds = juce::jlimit (0.0, juce::jmax (0.0, lengthSeconds - visibleSeconds),
                                         start);
        repaint();
    };

    verticalScrollBar.setAutoHide (false);
    verticalScrollBar.addListener (this);

    nameEditor.setVisible (false);
    nameEditor.setSelectAllWhenFocused (true);
    nameEditor.setColour (juce::TextEditor::backgroundColourId, colours::windowBackground);
    nameEditor.setColour (juce::TextEditor::textColourId, colours::textBright);
    nameEditor.setFont (juce::Font (juce::FontOptions (12.0f)));
    nameEditor.onReturnKey = [this] { commitRename(); };
    nameEditor.onFocusLost = [this] { commitRename(); };
    nameEditor.onEscapeKey = [this]
    {
        renamingTrackIndex = -1;
        nameEditor.setVisible (false);
        grabKeyboardFocus();
    };

    addAndMakeVisible (zoomBar);
    addAndMakeVisible (verticalScrollBar);
    addChildComponent (nameEditor);
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

juce::Rectangle<int> TimelineComponent::zoomBarArea() const
{
    return getLocalBounds()
        .withTrimmedLeft (layout::trackHeaderWidth)
        .withTrimmedRight (layout::scrollBarThickness)
        .withHeight (layout::zoomBarHeight);
}

juce::Rectangle<int> TimelineComponent::rulerArea() const
{
    return getLocalBounds()
        .withTrimmedLeft (layout::trackHeaderWidth)
        .withTrimmedRight (layout::scrollBarThickness)
        .withTrimmedTop (layout::zoomBarHeight)
        .withHeight (layout::rulerHeight);
}

juce::Rectangle<int> TimelineComponent::headerArea() const
{
    return getLocalBounds()
        .withWidth (layout::trackHeaderWidth)
        .withTrimmedTop (layout::zoomBarHeight + layout::rulerHeight);
}

juce::Rectangle<int> TimelineComponent::laneArea() const
{
    return getLocalBounds()
        .withTrimmedLeft (layout::trackHeaderWidth)
        .withTrimmedTop (layout::zoomBarHeight + layout::rulerHeight)
        .withTrimmedRight (layout::scrollBarThickness);
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

    // The dot is 10px, but a 10px click target is a miss waiting to happen, so
    // the button is 22px square with the dot drawn at its centre.
    return juce::Rectangle<int> (22, 22).withCentre ({ row.getRight() - 15, row.getCentreY() });
}

juce::Rectangle<int> TimelineComponent::soloButtonBounds (int trackIndex) const
{
    const auto mute = muteButtonBounds (trackIndex);
    return juce::Rectangle<int> (20, 20).withCentre ({ mute.getX() - 12, mute.getCentreY() });
}

juce::Rectangle<int> TimelineComponent::armButtonBounds (int trackIndex) const
{
    const auto solo = soloButtonBounds (trackIndex);
    return juce::Rectangle<int> (20, 20).withCentre ({ solo.getX() - 12, solo.getCentreY() });
}

juce::Rectangle<int> TimelineComponent::nameBounds (int trackIndex) const
{
    // Nearly the full row height: this is a double-click target, and a thin
    // band around the text is easy to miss.
    const auto row = rowBounds (trackIndex, headerArea());
    return row.withTrimmedLeft (10)
              .withTrimmedRight (row.getRight() - armButtonBounds (trackIndex).getX() + 4)
              .reduced (0, 6);
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

double TimelineComponent::clipStartOf (int trackIndex, int clipIndex) const
{
    if (! juce::isPositiveAndBelow (trackIndex, tracks.getNumChildren()))
        return 0.0;

    const auto clips = tracks.getChild (trackIndex).getChildWithName ("CLIPS");
    if (! juce::isPositiveAndBelow (clipIndex, clips.getNumChildren()))
        return 0.0;

    return static_cast<double> (clips.getChild (clipIndex).getProperty ("start", 0.0));
}

TimelineComponent::ClipRef TimelineComponent::clipAt (juce::Point<int> position) const
{
    const auto lanes = laneArea();
    if (! lanes.contains (position))
        return {};

    for (int i = 0; i < tracks.getNumChildren(); ++i)
    {
        const auto row = rowBounds (i, lanes);
        if (! row.contains (position))
            continue;

        const auto clips = tracks.getChild (i).getChildWithName ("CLIPS");

        // Back to front, so the topmost clip under the pointer wins once
        // overlapping clips become possible in Phase 6.
        for (int j = clips.getNumChildren(); --j >= 0;)
        {
            const auto clip = clips.getChild (j);
            const auto start = static_cast<double> (clip.getProperty ("start", 0.0));
            const auto length = juce::jmax (0.01, static_cast<double> (
                clip.getProperty ("length", 0.0)));

            if (position.x >= timeToX (start) && position.x <= timeToX (start + length))
                return { i, j };
        }

        return {};
    }

    return {};
}

int TimelineComponent::laneTrackIndexAt (juce::Point<int> position) const
{
    const auto lanes = laneArea();
    const auto index = static_cast<int> (std::floor (
        (position.y - lanes.getY() + verticalOffset) / static_cast<double> (layout::laneHeight)));

    // Clamped to any drawn row, including the empty placeholders - dropping on
    // one of those creates the track. Clamping to the last *real* track instead
    // made a one-track project impossible to drag out of.
    return juce::jlimit (0, juce::jmax (0, rowCount() - 1), index);
}

double TimelineComponent::snapToGrid (double seconds) const
{
    const auto beatSeconds = (60.0 / tempoBpm) * (4.0 / timeSigDenominator);
    if (beatSeconds <= 0.0)
        return seconds;

    return std::round (seconds / beatSeconds) * beatSeconds;
}

bool TimelineComponent::isTrackMuted (int trackIndex) const
{
    return juce::isPositiveAndBelow (trackIndex, tracks.getNumChildren())
        && static_cast<bool> (tracks.getChild (trackIndex).getProperty ("mute", false));
}

bool TimelineComponent::isTrackSoloed (int trackIndex) const
{
    return juce::isPositiveAndBelow (trackIndex, tracks.getNumChildren())
        && static_cast<bool> (tracks.getChild (trackIndex).getProperty ("solo", false));
}

bool TimelineComponent::isTrackArmed (int trackIndex) const
{
    return juce::isPositiveAndBelow (trackIndex, tracks.getNumChildren())
        && static_cast<bool> (tracks.getChild (trackIndex).getProperty ("armed", false));
}

void TimelineComponent::beginRename (int trackIndex)
{
    if (! juce::isPositiveAndBelow (trackIndex, tracks.getNumChildren()))
        return;

    renamingTrackIndex = trackIndex;
    nameEditor.setBounds (nameBounds (trackIndex));
    nameEditor.setText (tracks.getChild (trackIndex).getProperty ("name").toString(), false);
    nameEditor.setVisible (true);
    nameEditor.grabKeyboardFocus();
}

void TimelineComponent::commitRename()
{
    if (renamingTrackIndex < 0)
        return;

    const auto index = renamingTrackIndex;
    const auto text = nameEditor.getText().trim();

    // Clear the state before notifying: the callback repaints, and a visible
    // editor over a row that may have been renumbered is worse than none.
    renamingTrackIndex = -1;
    nameEditor.setVisible (false);
    grabKeyboardFocus();

    if (text.isNotEmpty() && onTrackRenamed)
        onTrackRenamed (index, text);
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

    // An editor left open over a row that no longer exists would rename the
    // wrong track on commit.
    if (renamingTrackIndex >= tracks.getNumChildren())
    {
        renamingTrackIndex = -1;
        nameEditor.setVisible (false);
    }

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
    zoomBar.setTotalLength (lengthSeconds);
    zoomBar.setVisibleRange (viewStartSeconds, visibleSeconds);

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

void TimelineComponent::scrollBarMoved (juce::ScrollBar*, double newRangeStart)
{
    // Only the vertical bar is a JUCE ScrollBar; horizontal scrolling and
    // zooming both come through ZoomScrollBar's callback.
    verticalOffset = juce::jmax (0.0, newRangeStart);
    repaint();
}

//==============================================================================
void TimelineComponent::resized()
{
    zoomBar.setBounds (zoomBarArea().reduced (1, 1));

    verticalScrollBar.setBounds (getLocalBounds()
                                     .removeFromRight (layout::scrollBarThickness)
                                     .withTrimmedTop (layout::zoomBarHeight
                                                      + layout::rulerHeight));

    setVerticalOffset (verticalOffset);
    updateScrollBars();
}

//==============================================================================
void TimelineComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::laneBackground);

    // Corner block spanning the zoom bar's row and the ruler, left of both.
    const auto topHeight = layout::zoomBarHeight + layout::rulerHeight;
    g.setColour (colours::panelTitle);
    g.fillRect (getLocalBounds().withWidth (layout::trackHeaderWidth).withHeight (topHeight));

    paintRuler (g);
    paintHeaders (g);
    paintLanes (g);
    paintPlayhead (g);

    g.setColour (colours::outline);
    g.drawVerticalLine (layout::trackHeaderWidth - 1, 0.0f, static_cast<float> (getHeight()));
    g.drawHorizontalLine (topHeight - 1, 0.0f, static_cast<float> (getWidth()));
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

        if (i != renamingTrackIndex)
        {
            g.setColour (! isRealTrack ? colours::textDim
                                       : (muted ? colours::textDim : colours::text));
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.drawText (name, nameBounds (i), juce::Justification::centredLeft, true);
        }

        // Record-arm, left of solo.
        if (isRealTrack)
        {
            const auto arm = armButtonBounds (i).reduced (2);
            const auto isArmed = isTrackArmed (i);

            g.setColour (isArmed ? colours::armed
                                 : (i == hoveredArmTrack ? colours::headerAlternate.brighter (0.3f)
                                                         : colours::ledOff));
            g.fillRoundedRectangle (arm.toFloat(), 3.0f);

            g.setColour (isArmed ? colours::textBright : colours::textDim);
            g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
            g.drawText ("R", arm, juce::Justification::centred);
        }

        // Solo button, left of the mute LED.
        if (isRealTrack)
        {
            const auto solo = soloButtonBounds (i).reduced (2);
            const auto isSoloed = isTrackSoloed (i);

            g.setColour (isSoloed ? colours::soloed
                                  : (i == hoveredSoloTrack ? colours::headerAlternate.brighter (0.3f)
                                                           : colours::ledOff));
            g.fillRoundedRectangle (solo.toFloat(), 3.0f);

            g.setColour (isSoloed ? colours::windowBackground : colours::textDim);
            g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
            g.drawText ("S", solo, juce::Justification::centred);
        }

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
        {
            // The clip under the pointer is drawn last, on whichever row it is
            // being dragged towards, so it is never hidden behind the lane it
            // is leaving.
            if (clipDragActive && draggedClip.track == i && draggedClip.clip == clipIndex)
                continue;

            const auto clip = clips.getChild (clipIndex);
            paintClip (g, clip, row, muted,
                       static_cast<double> (clip.getProperty ("start", 0.0)), false);
        }
    }

    // The dragged clip, drawn over everything on the row it will land on.
    if (clipDragActive && draggedClip.isValid()
        && juce::isPositiveAndBelow (clipDragTargetTrack, rowCount()))
    {
        const auto sourceClips = tracks.getChild (draggedClip.track).getChildWithName ("CLIPS");
        const auto clip = sourceClips.getChild (draggedClip.clip);

        if (clip.isValid())
        {
            const auto targetRow = rowBounds (clipDragTargetTrack, lanes);

            // Ghost on the target lane so the drop point reads even before the
            // clip gets there.
            g.setColour (colours::accent.withAlpha (0.10f));
            g.fillRect (targetRow);

            paintClip (g, clip, targetRow, isTrackMuted (clipDragTargetTrack),
                       clipDragPreviewStart, true);
        }
    }

    g.restoreState();
}

void TimelineComponent::paintClip (juce::Graphics& g, const juce::ValueTree& clip,
                                   juce::Rectangle<int> row, bool muted, double start,
                                   bool beingDragged)
{
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

    g.setColour (beingDragged ? bodyColour.brighter (0.15f) : bodyColour);
    g.fillRoundedRectangle (bounds.toFloat(), 3.0f);

    if (beingDragged)
    {
        g.setColour (colours::playhead);
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 3.0f, 1.5f);
    }

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

    // ---- waveform -----------------------------------------------------------
    const auto visible = bounds.getIntersection (laneArea());
    if (visible.isEmpty())
        return;

    juce::AudioThumbnail* thumbnail = nullptr;
    const auto path = clip.getProperty ("sourceFile").toString();

    if (waveformCache != nullptr && path.isNotEmpty())
        thumbnail = waveformCache->thumbnailFor (juce::File (path));

    if (thumbnail == nullptr || thumbnail->getTotalLength() <= 0.0)
    {
        // Still scanning, or unreadable. A centre line keeps the clip looking
        // like a clip instead of an empty box.
        g.setColour (colours::textBright.withAlpha (0.25f));
        g.drawHorizontalLine (bounds.getCentreY(), static_cast<float> (bounds.getX() + 3),
                              static_cast<float> (bounds.getRight() - 3));
        return;
    }

    // Draw only the slice that is actually on screen, at the resolution it is
    // being shown at. Handing the whole clip to drawChannels and letting it
    // squeeze into a small rect wastes the thumbnail's detail.
    const auto offset = juce::jmax (0.0, static_cast<double> (clip.getProperty ("offset", 0.0)));
    const auto sourceTempo = juce::jmax (1.0, static_cast<double> (
        clip.getProperty ("sourceTempo", tempoBpm)));
    const auto speedRatio = tempoBpm / sourceTempo;

    const auto toSourceTime = [&] (double x)
    {
        return offset + (xToTime (x) - start) * speedRatio;
    };

    g.setColour (muted ? colours::waveform.withAlpha (0.35f) : colours::waveform);
    thumbnail->drawChannels (g, visible,
                             toSourceTime (visible.getX()),
                             toSourceTime (visible.getRight()),
                             0.92f);
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

        // The mute and solo buttons win over selection: clicking one should do
        // that one thing, not also drag the selection around under the pointer.
        if (juce::isPositiveAndBelow (index, tracks.getNumChildren()))
        {
            if (muteButtonBounds (index).contains (event.getPosition()))
            {
                if (onTrackMuteToggled)
                    onTrackMuteToggled (index);

                return;
            }

            if (soloButtonBounds (index).contains (event.getPosition()))
            {
                if (onTrackSoloToggled)
                    onTrackSoloToggled (index);

                return;
            }

            if (armButtonBounds (index).contains (event.getPosition()))
            {
                if (onTrackArmToggled)
                    onTrackArmToggled (index);

                return;
            }
        }

        selectedTrackIndex = juce::isPositiveAndBelow (index, tracks.getNumChildren()) ? index : -1;

        if (onTrackSelected)
            onTrackSelected (selectedTrackIndex);

        repaint();
        return;
    }

    // A click on a clip picks it up. A click on empty lane still moves the
    // playhead, so the timeline keeps its most common use.
    if (const auto hit = clipAt (event.getPosition()); hit.isValid())
    {
        draggedClip = hit;
        clipDragActive = true;
        clipDragGrabTime = xToTime (event.x);
        clipDragOriginalStart = clipStartOf (hit.track, hit.clip);
        clipDragPreviewStart = clipDragOriginalStart;
        clipDragTargetTrack = hit.track;
        selectedTrackIndex = hit.track;

        if (onTrackSelected)
            onTrackSelected (selectedTrackIndex);

        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        repaint();
        return;
    }

    seekFromX (event.x);
}

void TimelineComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (clipDragActive)
    {
        const auto moved = xToTime (event.x) - clipDragGrabTime;
        const auto raw = juce::jmax (0.0, clipDragOriginalStart + moved);

        // Snapped to the beat by default, free while Alt is held - the usual
        // arrangement of these two in a DAW.
        clipDragPreviewStart = event.mods.isAltDown() ? raw
                                                      : juce::jmax (0.0, snapToGrid (raw));

        // Vertical travel picks the track it will land on. Tracks are discrete,
        // so this snaps by nature - there is no free-placement equivalent.
        const auto row = laneTrackIndexAt (event.getPosition());
        if (row >= 0)
            clipDragTargetTrack = row;

        repaint();
        return;
    }

    if (! headerArea().contains (event.getPosition()))
        seekFromX (event.x);
}

void TimelineComponent::mouseUp (const juce::MouseEvent&)
{
    if (! clipDragActive)
        return;

    const auto moved = draggedClip;
    const auto newStart = clipDragPreviewStart;
    const auto targetTrack = clipDragTargetTrack;
    const auto changed = ! juce::approximatelyEqual (newStart, clipDragOriginalStart)
                      || targetTrack != moved.track;

    clipDragActive = false;
    draggedClip = {};
    clipDragTargetTrack = -1;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();

    if (changed && onClipMoved)
        onClipMoved (moved.track, moved.clip, targetTrack, newStart);
}

void TimelineComponent::mouseMove (const juce::MouseEvent& event)
{
    const auto index = trackIndexAt (event.getPosition());
    const auto isReal = juce::isPositiveAndBelow (index, tracks.getNumChildren());

    // A grab cursor over clips advertises that they can be dragged.
    if (laneArea().contains (event.getPosition()))
    {
        setMouseCursor (clipAt (event.getPosition()).isValid()
                            ? juce::MouseCursor::DraggingHandCursor
                            : juce::MouseCursor::NormalCursor);
    }

    const auto overMute = isReal && muteButtonBounds (index).contains (event.getPosition());
    const auto overSolo = isReal && soloButtonBounds (index).contains (event.getPosition());
    const auto overArm = isReal && armButtonBounds (index).contains (event.getPosition());
    const auto overName = isReal && nameBounds (index).contains (event.getPosition());

    const auto mute = overMute ? index : -1;
    const auto solo = overSolo ? index : -1;
    const auto arm = overArm ? index : -1;

    if (mute == hoveredMuteTrack && solo == hoveredSoloTrack && arm == hoveredArmTrack)
        return;

    hoveredMuteTrack = mute;
    hoveredSoloTrack = solo;
    hoveredArmTrack = arm;
    setMouseCursor (overMute || overSolo || overArm
                        ? juce::MouseCursor::PointingHandCursor
                        : (overName ? juce::MouseCursor::IBeamCursor
                                    : juce::MouseCursor::NormalCursor));
    repaint();
}

void TimelineComponent::mouseExit (const juce::MouseEvent&)
{
    if (hoveredMuteTrack < 0 && hoveredSoloTrack < 0 && hoveredArmTrack < 0)
        return;

    hoveredMuteTrack = -1;
    hoveredSoloTrack = -1;
    hoveredArmTrack = -1;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void TimelineComponent::mouseDoubleClick (const juce::MouseEvent& event)
{
    const auto index = trackIndexAt (event.getPosition());
    if (juce::isPositiveAndBelow (index, tracks.getNumChildren())
        && nameBounds (index).contains (event.getPosition()))
    {
        beginRename (index);
    }
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
