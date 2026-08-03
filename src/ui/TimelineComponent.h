#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace saamveda::ui
{

/** Arrangement timeline: fixed track headers on the left, a zoomable and
    scrollable lane area on the right.

    The grid is derived from the project tempo and time signature rather than
    drawn at a fixed spacing, so bar lines land where the metronome clicks.
    Phase 6's snap-to-grid will read the same numbers.
*/
class TimelineComponent final : public juce::Component,
                                private juce::ScrollBar::Listener
{
public:
    TimelineComponent();

    std::function<void (double)> onSeek;

    void setLength (double seconds);
    void setPosition (double seconds);
    void setMusicalGrid (double bpm, int numerator, int denominator);
    void setSessionTracks (juce::ValueTree tracks);

    void zoomIn();
    void zoomOut();
    void zoomNormal();
    void zoomToFit();
    void setFollowPlayhead (bool shouldFollow);
    bool isFollowingPlayhead() const noexcept { return followPlayhead; }

    double visibleSpanSeconds() const noexcept { return visibleSeconds; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    static constexpr int headerWidth = 132;
    static constexpr int rulerHeight = 28;
    static constexpr int scrollBarHeight = 12;
    static constexpr int laneHeight = 46;
    static constexpr double minimumVisibleSeconds = 0.25;

    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void seekFromX (int x);
    void zoomAround (double factor, double anchorSeconds);
    void setViewStart (double seconds);
    void updateScrollBar();
    void followPlayheadIfNeeded();

    juce::Rectangle<int> laneArea() const;
    double timeToX (double seconds) const;
    double xToTime (double x) const;

    double lengthSeconds = 60.0;
    double positionSeconds = 0.0;
    double viewStartSeconds = 0.0;
    double visibleSeconds = 60.0;
    double tempoBpm = 120.0;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;
    bool followPlayhead = true;

    juce::ValueTree tracks;
    juce::ScrollBar horizontalScrollBar { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineComponent)
};

} // namespace saamveda::ui
