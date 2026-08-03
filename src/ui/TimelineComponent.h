#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Layout.h"
#include "ZoomScrollBar.h"
#include "../services/WaveformCache.h"

namespace saamveda::ui
{

/** The arrangement grid: track headers, ruler, and the clip lanes.

    Laid out as four regions that share one scroll state, so the headers can
    never drift out of step with the lanes they label:

        corner  | ruler
        headers | lanes            + vertical scrollbar
                | horizontal scrollbar

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
    std::function<void (int)> onTrackSelected;
    std::function<void (int)> onTrackMuteToggled;
    std::function<void (int)> onTrackSoloToggled;
    std::function<void (int, juce::String)> onTrackRenamed;

    void setWaveformCache (services::WaveformCache* cache) { waveformCache = cache; }

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
    int selectedTrack() const noexcept { return selectedTrackIndex; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    static constexpr double minimumVisibleSeconds = 0.25;
    static constexpr int clipTitleHeight = 15;

    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void seekFromX (int x);
    void zoomAround (double factor, double anchorSeconds);
    void setViewStart (double seconds);
    void setVerticalOffset (double pixels);
    void updateScrollBars();
    void followPlayheadIfNeeded();

    juce::Rectangle<int> zoomBarArea() const;
    juce::Rectangle<int> rulerArea() const;
    juce::Rectangle<int> headerArea() const;
    juce::Rectangle<int> laneArea() const;
    juce::Rectangle<int> rowBounds (int trackIndex, juce::Rectangle<int> column) const;

    /** Clickable targets. Painting and hit-testing both go through these so a
        control and the region that responds to a click cannot drift apart. */
    juce::Rectangle<int> muteButtonBounds (int trackIndex) const;
    juce::Rectangle<int> soloButtonBounds (int trackIndex) const;
    juce::Rectangle<int> nameBounds (int trackIndex) const;

    int trackIndexAt (juce::Point<int> position) const;
    bool isTrackMuted (int trackIndex) const;
    bool isTrackSoloed (int trackIndex) const;
    void beginRename (int trackIndex);
    void commitRename();
    int rowCount() const;
    int contentHeight() const;

    double timeToX (double seconds) const;
    double xToTime (double x) const;

    void paintRuler (juce::Graphics&);
    void paintHeaders (juce::Graphics&);
    void paintLanes (juce::Graphics&);
    void paintClip (juce::Graphics&, const juce::ValueTree& clip, juce::Rectangle<int> row,
                    bool muted);
    void paintPlayhead (juce::Graphics&);

    double lengthSeconds = 60.0;
    double positionSeconds = 0.0;
    double viewStartSeconds = 0.0;
    double visibleSeconds = 60.0;
    double verticalOffset = 0.0;
    double tempoBpm = 120.0;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;
    int selectedTrackIndex = -1;
    int hoveredMuteTrack = -1;
    int hoveredSoloTrack = -1;
    int renamingTrackIndex = -1;
    bool followPlayhead = true;

    juce::ValueTree tracks;
    services::WaveformCache* waveformCache = nullptr;
    ZoomScrollBar zoomBar;
    juce::ScrollBar verticalScrollBar { true };
    juce::TextEditor nameEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineComponent)
};

} // namespace saamveda::ui
