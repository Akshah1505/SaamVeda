#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Layout.h"

namespace saamveda::ui
{

/** Left-hand source list: every clip the session knows about.

    This is the equivalent of FL Studio's channel/clip strip beside the
    playlist. It reads the session tree and never writes to it; selecting a row
    only reports the selection upward.
*/
class ClipBrowser final : public juce::Component,
                          private juce::ListBoxModel
{
public:
    ClipBrowser();

    std::function<void (const juce::String&)> onClipSelected;

    void setSessionTracks (juce::ValueTree tracks);
    juce::String selectedClipName() const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;

    juce::ListBox list { "clips", this };
    juce::StringArray entries;
    juce::Array<int> entryTrackIndex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipBrowser)
};

} // namespace saamveda::ui
