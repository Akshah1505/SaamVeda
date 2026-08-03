#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ClipBrowser.h"
#include "TimelineComponent.h"

namespace saamveda::ui
{

/** The Playlist window: title bar, tool strip, clip browser, and arrangement.

    Composition only - it owns the browser and the timeline and arranges them,
    but forwards every decision to whoever owns it.
*/
class PlaylistPanel final : public juce::Component
{
public:
    PlaylistPanel();

    TimelineComponent& timeline() noexcept { return timelineComponent; }
    ClipBrowser& browser() noexcept { return clipBrowser; }

    void setBreadcrumb (const juce::String& arrangementName, const juce::String& selection);
    void setMusicalReadout (double bpm, int numerator, int denominator, int bar);
    void refreshReadouts();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Label titleLabel, viewLabel, musicalLabel;
    juce::TextButton zoomOutButton { "-" }, zoomInButton { "+" }, zoomFitButton { "Fit" };
    juce::ToggleButton followButton { "Follow" };

    ClipBrowser clipBrowser;
    TimelineComponent timelineComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaylistPanel)
};

} // namespace saamveda::ui
