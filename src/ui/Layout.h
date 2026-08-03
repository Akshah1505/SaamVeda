#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace saamveda::ui
{

/** Shared metrics and colours for the main window.

    The arrangement follows the reference layout in docs/16-ui-layout.md: a menu
    bar and two toolbar rows above a titled Playlist panel, which itself holds a
    clip browser, a track header column, a ruler and the lane area.

    Sizes live here rather than in each component so the rows actually line up,
    and so a later visual pass can restyle without hunting through paint code.
*/
namespace layout
{
    // Top chrome
    constexpr int menuBarWidth        = 430;
    constexpr int transportRowHeight  = 46;
    constexpr int toolRowHeight       = 42;
    constexpr int hintPanelWidth      = 300;

    // Playlist panel chrome
    constexpr int panelTitleHeight    = 22;
    constexpr int panelToolStripHeight = 26;

    // Playlist panel contents
    constexpr int browserWidth        = 150;
    constexpr int trackHeaderWidth    = 116;
    constexpr int rulerHeight         = 20;
    constexpr int laneHeight          = 58;
    constexpr int scrollBarThickness  = 12;

    // The lane area always draws at least this many rows, so an empty project
    // still reads as a track sheet rather than a blank rectangle.
    constexpr int minimumVisibleTracks = 12;
}

namespace colours
{
    const juce::Colour windowBackground   { 0xff181a20 };
    const juce::Colour chromeBackground   { 0xff232730 };
    const juce::Colour panelBackground    { 0xff272a33 };
    const juce::Colour panelTitle         { 0xff2f333e };
    const juce::Colour laneBackground     { 0xff21242c };
    const juce::Colour laneAlternate      { 0xff262a33 };
    const juce::Colour headerBackground   { 0xff2d313c };
    const juce::Colour headerAlternate    { 0xff323744 };
    const juce::Colour gridBar            { 0xff8f96a8 };
    const juce::Colour gridBeat           { 0xff444955 };
    const juce::Colour outline            { 0xff3a3f4b };
    const juce::Colour text               { 0xffd6dae3 };
    const juce::Colour textDim            { 0xff858b99 };
    const juce::Colour textBright         { 0xffffffff };
    const juce::Colour accent             { 0xff68a0e8 };
    const juce::Colour clipBody           { 0xff315f8f };
    const juce::Colour clipTitle          { 0xff3d74ac };
    const juce::Colour playhead           { 0xffffb74d };
    const juce::Colour ledOn              { 0xff7ddc7d };
    const juce::Colour ledOff             { 0xff3c4450 };
    const juce::Colour warning            { 0xffe8a15c };
}

/** Shared look for the window chrome.

    Set once on MainComponent so every child inherits it; the alternative is
    colour ids scattered across each component's constructor, which drifts the
    moment someone adds a widget.
*/
class ChromeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChromeLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, colours::panelBackground);
        setColour (juce::PopupMenu::textColourId, colours::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId,
                   colours::accent.withAlpha (0.35f));
        setColour (juce::PopupMenu::highlightedTextColourId, colours::textBright);
        setColour (juce::PopupMenu::headerTextColourId, colours::textDim);

        setColour (juce::TextButton::buttonColourId, colours::headerBackground);
        setColour (juce::TextButton::textColourOffId, colours::text);
        setColour (juce::ToggleButton::textColourId, colours::text);
        setColour (juce::ToggleButton::tickColourId, colours::accent);

        setColour (juce::ComboBox::backgroundColourId, colours::headerBackground);
        setColour (juce::ComboBox::textColourId, colours::text);
        setColour (juce::ComboBox::outlineColourId, colours::outline);
        setColour (juce::ComboBox::arrowColourId, colours::textDim);

        setColour (juce::ScrollBar::thumbColourId, colours::accent.withAlpha (0.55f));
        setColour (juce::ScrollBar::trackColourId, colours::panelBackground);
    }

    /** JUCE's default menu font is sized for a full-width menu bar. This one
        shares its row with the transport, so it has to be compact enough for
        all seven titles to fit. */
    juce::Font getMenuBarFont (juce::MenuBarComponent&, int, const juce::String&) override
    {
        return juce::Font (juce::FontOptions (13.0f));
    }
};

} // namespace saamveda::ui
