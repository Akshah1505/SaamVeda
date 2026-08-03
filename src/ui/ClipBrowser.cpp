#include "ClipBrowser.h"

namespace saamveda::ui
{

ClipBrowser::ClipBrowser()
{
    list.setRowHeight (26);
    // Transparent so the panel's own empty-state hint shows through when there
    // is nothing to list.
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    list.setWantsKeyboardFocus (false);
    addAndMakeVisible (list);
}

void ClipBrowser::setSessionTracks (juce::ValueTree tracks)
{
    entries.clear();
    entryTrackIndex.clear();

    for (int trackIndex = 0; trackIndex < tracks.getNumChildren(); ++trackIndex)
    {
        const auto track = tracks.getChild (trackIndex);
        const auto clips = track.getChildWithName ("CLIPS");

        for (int clipIndex = 0; clipIndex < clips.getNumChildren(); ++clipIndex)
        {
            entries.add (clips.getChild (clipIndex).getProperty ("name").toString());
            entryTrackIndex.add (trackIndex);
        }
    }

    list.updateContent();
    repaint();
}

juce::String ClipBrowser::selectedClipName() const
{
    const auto row = list.getSelectedRow();
    return juce::isPositiveAndBelow (row, entries.size()) ? entries[row] : juce::String();
}

int ClipBrowser::getNumRows()
{
    return entries.size();
}

void ClipBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height,
                                    bool selected)
{
    if (! juce::isPositiveAndBelow (row, entries.size()))
        return;

    auto bounds = juce::Rectangle<int> (0, 0, width, height).reduced (2, 1);

    g.setColour (selected ? colours::accent.withAlpha (0.35f)
                          : (row % 2 == 0 ? colours::headerBackground : colours::headerAlternate));
    g.fillRoundedRectangle (bounds.toFloat(), 3.0f);

    // Colour tag on the left edge, mirroring the clip colour in the lanes.
    g.setColour (colours::clipTitle);
    g.fillRect (bounds.removeFromLeft (4));

    g.setColour (selected ? colours::textBright : colours::text);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText (entries[row], bounds.reduced (6, 0), juce::Justification::centredLeft, true);
}

void ClipBrowser::selectedRowsChanged (int)
{
    if (onClipSelected)
        onClipSelected (selectedClipName());
}

void ClipBrowser::paint (juce::Graphics& g)
{
    g.fillAll (colours::panelBackground);

    auto header = getLocalBounds().removeFromTop (layout::rulerHeight);
    g.setColour (colours::panelTitle);
    g.fillRect (header);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("CLIPS", header.reduced (8, 0), juce::Justification::centredLeft);

    if (entries.isEmpty())
    {
        g.setColour (colours::textDim);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawFittedText ("No clips.\nImport audio to\nfill this list.",
                          getLocalBounds().withTrimmedTop (layout::rulerHeight + 12).reduced (10, 0),
                          juce::Justification::centredTop, 3);
    }
}

void ClipBrowser::resized()
{
    list.setBounds (getLocalBounds().withTrimmedTop (layout::rulerHeight));
}

} // namespace saamveda::ui
