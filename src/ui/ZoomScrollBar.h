#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Layout.h"

namespace saamveda::ui
{

/** Combined scroll and zoom bar, as FL Studio puts above its playlist.

    The thumb is the visible slice of the project. Dragging its middle scrolls;
    dragging either end resizes the slice, which is zooming. One control for
    both is not a space saving - it is that "where am I" and "how much am I
    looking at" are the same question, and answering it with two widgets makes
    the user do the reconciliation.

    It reports intent and holds no view state of its own: the owner pushes the
    range back in through setVisibleRange, which never re-fires the callback.
*/
class ZoomScrollBar final : public juce::Component
{
public:
    ZoomScrollBar();

    /** Fired while dragging, with the requested view. The owner clamps. */
    std::function<void (double startSeconds, double visibleSeconds)> onRangeChanged;

    void setTotalLength (double seconds);
    void setVisibleRange (double startSeconds, double visibleSeconds);
    void setMinimumVisible (double seconds) { minimumVisibleSeconds = juce::jmax (0.01, seconds); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    /** Grab width at each end of the thumb. Generous, because a 2px edge is not
        a control anyone can hit on the first try. */
    static constexpr int edgeGrabWidth = 7;
    static constexpr int minimumThumbWidth = 22;

    enum class Zone { track, body, leftEdge, rightEdge };

    Zone zoneAt (int x) const;
    juce::Rectangle<int> thumbBounds() const;
    double xToTime (int x) const;
    void applyCursorFor (Zone zone);
    void emitRange (double start, double visible);

    double totalSeconds = 60.0;
    double startSeconds = 0.0;
    double visibleSeconds = 60.0;
    double minimumVisibleSeconds = 0.25;

    Zone dragZone = Zone::track;
    Zone hoverZone = Zone::track;
    double dragStartSeconds = 0.0;
    double dragVisibleSeconds = 0.0;
    int dragStartX = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZoomScrollBar)
};

} // namespace saamveda::ui
