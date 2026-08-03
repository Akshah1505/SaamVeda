#include "ZoomScrollBar.h"

#include <cmath>

namespace saamveda::ui
{

ZoomScrollBar::ZoomScrollBar()
{
    setWantsKeyboardFocus (false);
}

//==============================================================================
void ZoomScrollBar::setTotalLength (double seconds)
{
    totalSeconds = juce::jmax (0.01, seconds);
    repaint();
}

void ZoomScrollBar::setVisibleRange (double start, double visible)
{
    visibleSeconds = juce::jlimit (minimumVisibleSeconds, totalSeconds, visible);
    startSeconds = juce::jlimit (0.0, juce::jmax (0.0, totalSeconds - visibleSeconds), start);
    repaint();
}

double ZoomScrollBar::xToTime (int x) const
{
    if (getWidth() <= 0)
        return 0.0;

    return juce::jlimit (0.0, totalSeconds,
                         static_cast<double> (x) / getWidth() * totalSeconds);
}

juce::Rectangle<int> ZoomScrollBar::thumbBounds() const
{
    const auto width = static_cast<double> (getWidth());
    if (width <= 0.0)
        return {};

    const auto thumbWidth = juce::jmax (static_cast<double> (minimumThumbWidth),
                                        visibleSeconds / totalSeconds * width);
    const auto x = juce::jlimit (0.0, juce::jmax (0.0, width - thumbWidth),
                                 startSeconds / totalSeconds * width);

    return { static_cast<int> (std::round (x)), 0,
             static_cast<int> (std::round (thumbWidth)), getHeight() };
}

ZoomScrollBar::Zone ZoomScrollBar::zoneAt (int x) const
{
    const auto thumb = thumbBounds();
    if (thumb.isEmpty())
        return Zone::track;

    if (x >= thumb.getX() - 2 && x <= thumb.getX() + edgeGrabWidth)
        return Zone::leftEdge;

    if (x >= thumb.getRight() - edgeGrabWidth && x <= thumb.getRight() + 2)
        return Zone::rightEdge;

    if (x > thumb.getX() && x < thumb.getRight())
        return Zone::body;

    return Zone::track;
}

void ZoomScrollBar::applyCursorFor (Zone zone)
{
    switch (zone)
    {
        case Zone::leftEdge:
        case Zone::rightEdge:
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            break;
        case Zone::body:
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            break;
        case Zone::track:
        default:
            setMouseCursor (juce::MouseCursor::NormalCursor);
            break;
    }
}

void ZoomScrollBar::emitRange (double start, double visible)
{
    const auto clampedVisible = juce::jlimit (minimumVisibleSeconds, totalSeconds, visible);
    const auto clampedStart = juce::jlimit (0.0, juce::jmax (0.0, totalSeconds - clampedVisible),
                                            start);

    startSeconds = clampedStart;
    visibleSeconds = clampedVisible;
    repaint();

    if (onRangeChanged)
        onRangeChanged (clampedStart, clampedVisible);
}

//==============================================================================
void ZoomScrollBar::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (colours::laneBackground);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    const auto thumb = thumbBounds();
    if (thumb.isEmpty())
        return;

    const auto active = hoverZone != Zone::track || dragZone != Zone::track;
    g.setColour (colours::accent.withAlpha (active ? 0.62f : 0.45f));
    g.fillRoundedRectangle (thumb.toFloat().reduced (1.0f), 3.0f);

    // Grip marks at both ends, so the thumb reads as resizable rather than as a
    // plain scrollbar that happens to be draggable.
    const auto drawGrip = [&g, &thumb] (int x, bool emphasised)
    {
        g.setColour (colours::textBright.withAlpha (emphasised ? 0.9f : 0.5f));
        for (int i = 0; i < 2; ++i)
            g.fillRect (static_cast<float> (x + i * 3),
                        static_cast<float> (thumb.getY() + 3),
                        1.0f,
                        static_cast<float> (thumb.getHeight() - 6));
    };

    if (thumb.getWidth() >= minimumThumbWidth)
    {
        drawGrip (thumb.getX() + 3, hoverZone == Zone::leftEdge || dragZone == Zone::leftEdge);
        drawGrip (thumb.getRight() - 6, hoverZone == Zone::rightEdge || dragZone == Zone::rightEdge);
    }
}

//==============================================================================
void ZoomScrollBar::mouseDown (const juce::MouseEvent& event)
{
    dragZone = zoneAt (event.x);
    dragStartSeconds = startSeconds;
    dragVisibleSeconds = visibleSeconds;
    dragStartX = event.x;

    // A click on the empty track jumps the view there, centred.
    if (dragZone == Zone::track)
    {
        emitRange (xToTime (event.x) - visibleSeconds * 0.5, visibleSeconds);
        dragZone = Zone::body;
        dragStartSeconds = startSeconds;
        dragStartX = event.x;
    }

    applyCursorFor (dragZone);
    repaint();
}

void ZoomScrollBar::mouseDrag (const juce::MouseEvent& event)
{
    if (getWidth() <= 0)
        return;

    switch (dragZone)
    {
        case Zone::body:
        {
            const auto delta = static_cast<double> (event.x - dragStartX) / getWidth()
                             * totalSeconds;
            emitRange (dragStartSeconds + delta, dragVisibleSeconds);
            break;
        }

        case Zone::leftEdge:
        {
            // The far end stays put, so dragging this one only changes how much
            // is visible.
            const auto fixedEnd = dragStartSeconds + dragVisibleSeconds;
            const auto newStart = juce::jlimit (0.0, fixedEnd - minimumVisibleSeconds,
                                                xToTime (event.x));
            emitRange (newStart, fixedEnd - newStart);
            break;
        }

        case Zone::rightEdge:
        {
            const auto newEnd = juce::jlimit (dragStartSeconds + minimumVisibleSeconds,
                                              totalSeconds, xToTime (event.x));
            emitRange (dragStartSeconds, newEnd - dragStartSeconds);
            break;
        }

        case Zone::track:
        default:
            break;
    }
}

void ZoomScrollBar::mouseUp (const juce::MouseEvent& event)
{
    dragZone = Zone::track;
    hoverZone = zoneAt (event.x);
    applyCursorFor (hoverZone);
    repaint();
}

void ZoomScrollBar::mouseMove (const juce::MouseEvent& event)
{
    const auto zone = zoneAt (event.x);
    if (zone == hoverZone)
        return;

    hoverZone = zone;
    applyCursorFor (zone);
    repaint();
}

void ZoomScrollBar::mouseExit (const juce::MouseEvent&)
{
    if (hoverZone == Zone::track)
        return;

    hoverZone = Zone::track;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void ZoomScrollBar::mouseDoubleClick (const juce::MouseEvent&)
{
    emitRange (0.0, totalSeconds);
}

void ZoomScrollBar::mouseWheelMove (const juce::MouseEvent& event,
                                    const juce::MouseWheelDetails& wheel)
{
    const auto delta = (wheel.deltaX != 0.0f ? wheel.deltaX : wheel.deltaY);
    if (delta == 0.0f)
        return;

    if (event.mods.isCtrlDown() || event.mods.isCommandDown())
    {
        const auto anchor = xToTime (event.x);
        const auto factor = delta > 0.0f ? 0.8 : 1.25;
        const auto newVisible = juce::jlimit (minimumVisibleSeconds, totalSeconds,
                                              visibleSeconds * factor);
        const auto proportion = juce::jlimit (0.0, 1.0,
                                              (anchor - startSeconds) / visibleSeconds);
        emitRange (anchor - proportion * newVisible, newVisible);
        return;
    }

    emitRange (startSeconds - delta * visibleSeconds * 0.4, visibleSeconds);
}

} // namespace saamveda::ui
