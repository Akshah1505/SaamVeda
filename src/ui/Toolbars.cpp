#include "Toolbars.h"

namespace saamveda::ui
{

namespace
{
    void styleToolButton (juce::TextButton& button)
    {
        button.setWantsKeyboardFocus (false);
        button.setColour (juce::TextButton::buttonColourId, colours::headerBackground);
        button.setColour (juce::TextButton::textColourOffId, colours::text);
    }

    void styleReadout (juce::Label& label, float height, juce::Justification justification,
                       juce::Colour colour)
    {
        label.setJustificationType (justification);
        label.setColour (juce::Label::textColourId, colour);
        label.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                                      height, juce::Font::plain)));
    }
}

//==============================================================================
TransportBar::TransportBar()
{
    songModeButton.setClickingTogglesState (true);
    songModeButton.setToggleState (true, juce::dontSendNotification);
    songModeButton.onClick = [this] { if (onToggleSongMode) onToggleSongMode(); };

    playButton.onClick   = [this] { if (onPlayStop) onPlayStop(); };
    stopButton.onClick   = [this] { if (onStop) onStop(); };
    recordButton.onClick = [this] { if (onRecord) onRecord(); };

    // Recording arrives in Phase 4; the control is present so the row's shape
    // does not shift when it starts working.
    recordButton.setEnabled (false);
    recordButton.setTooltip ("Recording arrives in Phase 4");

    for (auto* button : { &songModeButton, &playButton, &stopButton, &recordButton })
        styleToolButton (*button);

    recordButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff5a2f33));

    tempoSlider.setSliderStyle (juce::Slider::LinearBar);
    tempoSlider.setRange (20.0, 400.0, 0.001);
    tempoSlider.setValue (120.0, juce::dontSendNotification);
    tempoSlider.setTextValueSuffix (" BPM");
    tempoSlider.setNumDecimalPlacesToDisplay (3);
    tempoSlider.setColour (juce::Slider::trackColourId, colours::headerBackground);
    tempoSlider.setColour (juce::Slider::textBoxTextColourId, colours::textBright);
    tempoSlider.onDragStart = [this] { tempoDragging = true; };
    tempoSlider.onDragEnd = [this]
    {
        tempoDragging = false;
        if (onTempoDragEnded) onTempoDragEnded();
    };
    tempoSlider.onValueChange = [this]
    {
        if (onTempoChanged) onTempoChanged (tempoSlider.getValue());
    };

    for (int value = 1; value <= 16; ++value)
        numeratorBox.addItem (juce::String (value), value);
    for (auto value : { 1, 2, 4, 8, 16, 32 })
        denominatorBox.addItem (juce::String (value), value);

    const auto timeSignatureChanged = [this]
    {
        if (onTimeSignatureChanged)
            onTimeSignatureChanged (numeratorBox.getSelectedId(), denominatorBox.getSelectedId());
    };
    numeratorBox.onChange = timeSignatureChanged;
    denominatorBox.onChange = timeSignatureChanged;
    setTimeSignature (4, 4);

    styleReadout (timeSignatureCaption, 9.0f, juce::Justification::centredLeft, colours::textDim);
    timeSignatureCaption.setText ("TIME SIG", juce::dontSendNotification);

    styleReadout (positionCaption, 9.0f, juce::Justification::centredLeft, colours::textDim);
    styleReadout (barsLabel, 18.0f, juce::Justification::centredRight, colours::textBright);
    styleReadout (secondsLabel, 10.0f, juce::Justification::centredRight, colours::textDim);
    styleReadout (loadLabel, 11.0f, juce::Justification::centredRight, colours::textDim);

    positionCaption.setText ("BAR:BEAT:TICK", juce::dontSendNotification);
    setPosition (0.0, 1, 1, 0);
    setAudioLoad (0.0);

    for (auto* component : std::initializer_list<juce::Component*>
         { &songModeButton, &playButton, &stopButton, &recordButton, &tempoSlider,
           &numeratorBox, &denominatorBox, &timeSignatureCaption,
           &positionCaption, &barsLabel, &secondsLabel, &loadLabel })
        addAndMakeVisible (component);
}

void TransportBar::setTimeSignature (int numerator, int denominator)
{
    numeratorBox.setSelectedId (juce::jlimit (1, 16, numerator), juce::dontSendNotification);
    denominatorBox.setSelectedId (juce::jlimit (1, 32, denominator), juce::dontSendNotification);
}

void TransportBar::setPlaying (bool isPlaying)
{
    playButton.setEnabled (! isPlaying);
    stopButton.setEnabled (isPlaying);
}

void TransportBar::setSongMode (bool isSongMode)
{
    songModeButton.setToggleState (isSongMode, juce::dontSendNotification);
    songModeButton.setButtonText (isSongMode ? "SONG" : "PAT");
}

void TransportBar::setTempo (double bpm)
{
    tempoSlider.setValue (bpm, juce::dontSendNotification);
}

double TransportBar::tempo() const
{
    return tempoSlider.getValue();
}

void TransportBar::setPosition (double seconds, int bar, int beat, int tick)
{
    barsLabel.setText (juce::String (bar) + ":"
                           + juce::String (beat).paddedLeft ('0', 2) + ":"
                           + juce::String (tick).paddedLeft ('0', 2),
                       juce::dontSendNotification);

    const auto totalSeconds = juce::jmax (0, static_cast<int> (seconds));
    secondsLabel.setText (juce::String (totalSeconds / 60) + ":"
                              + juce::String (totalSeconds % 60).paddedLeft ('0', 2) + "."
                              + juce::String (static_cast<int> ((seconds - std::floor (seconds))
                                                                * 100.0)).paddedLeft ('0', 2),
                          juce::dontSendNotification);
}

void TransportBar::setAudioLoad (double proportion)
{
    // Audio-callback load, not process CPU: it is the number that predicts a
    // dropout, which is the one that matters in a DAW.
    loadLabel.setText (juce::String (juce::jlimit (0, 999,
                                                   static_cast<int> (proportion * 100.0)))
                           + "% audio",
                       juce::dontSendNotification);
}

void TransportBar::paint (juce::Graphics& g)
{
    g.fillAll (colours::chromeBackground);

    // Recess the position readout so it reads as an instrument display rather
    // than another button.
    auto readout = getLocalBounds().reduced (6, 4).removeFromRight (280).removeFromLeft (176);
    g.setColour (colours::windowBackground);
    g.fillRoundedRectangle (readout.toFloat(), 3.0f);

    g.setColour (colours::outline);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
}

void TransportBar::resized()
{
    auto area = getLocalBounds().reduced (6, 4);

    songModeButton.setBounds (area.removeFromLeft (56).reduced (1));
    area.removeFromLeft (6);
    playButton.setBounds (area.removeFromLeft (58).reduced (1));
    stopButton.setBounds (area.removeFromLeft (58).reduced (1));
    recordButton.setBounds (area.removeFromLeft (48).reduced (1));
    area.removeFromLeft (10);
    tempoSlider.setBounds (area.removeFromLeft (118).reduced (1));
    area.removeFromLeft (10);

    auto timeSignature = area.removeFromLeft (108);
    timeSignatureCaption.setBounds (timeSignature.removeFromTop (11));
    numeratorBox.setBounds (timeSignature.removeFromLeft (52).reduced (1));
    denominatorBox.setBounds (timeSignature.removeFromLeft (52).reduced (1));

    loadLabel.setBounds (area.removeFromRight (104).reduced (4, 0));

    // Caption and elapsed time share the top line so the bar counter gets the
    // whole remaining height; stacking all three overflowed the row.
    auto readout = area.removeFromRight (176).reduced (8, 3);
    auto captionRow = readout.removeFromTop (11);
    positionCaption.setBounds (captionRow.removeFromLeft (80));
    secondsLabel.setBounds (captionRow);
    barsLabel.setBounds (readout);
}

//==============================================================================
ToolBar::ToolBar()
{
    addTrackButton.onClick    = [this] { if (onAddTrack) onAddTrack(); };
    importButton.onClick      = [this] { if (onImport) onImport(); };
    removeTrackButton.onClick = [this] { if (onRemoveTrack) onRemoveTrack(); };
    undoButton.onClick        = [this] { if (onUndo) onUndo(); };
    redoButton.onClick        = [this] { if (onRedo) onRedo(); };
    tapTempoButton.onClick    = [this] { if (onTapTempo) onTapTempo(); };
    shortcutsButton.onClick   = [this] { if (onShowShortcuts) onShowShortcuts(); };

    loopButton.onClick = [this]
    {
        if (onLoopChanged) onLoopChanged (loopButton.getToggleState());
    };
    metronomeButton.onClick = [this]
    {
        if (onMetronomeChanged) onMetronomeChanged (metronomeButton.getToggleState());
    };

    for (auto* button : { &addTrackButton, &importButton, &removeTrackButton, &undoButton,
                          &redoButton, &tapTempoButton, &shortcutsButton })
        styleToolButton (*button);

    for (auto* button : { &loopButton, &metronomeButton })
        button->setWantsKeyboardFocus (false);

    contextHeading.setJustificationType (juce::Justification::centredLeft);
    contextHeading.setColour (juce::Label::textColourId, colours::textDim);
    contextHeading.setFont (juce::Font (juce::FontOptions (11.0f)));

    contextDetail.setJustificationType (juce::Justification::centredLeft);
    contextDetail.setColour (juce::Label::textColourId, colours::text);
    contextDetail.setFont (juce::Font (juce::FontOptions (13.0f)));

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, colours::textDim);
    statusLabel.setFont (juce::Font (juce::FontOptions (12.0f)));

    notificationLabel.setJustificationType (juce::Justification::centredRight);
    notificationLabel.setColour (juce::Label::textColourId, colours::textDim);
    notificationLabel.setFont (juce::Font (juce::FontOptions (11.0f)));

    setContext ("Project", "Untitled");

    for (auto* component : std::initializer_list<juce::Component*>
         { &contextHeading, &contextDetail, &statusLabel, &notificationLabel,
           &loopButton, &metronomeButton, &tapTempoButton, &addTrackButton, &importButton,
           &removeTrackButton, &undoButton, &redoButton, &shortcutsButton })
        addAndMakeVisible (component);
}

void ToolBar::setContext (const juce::String& heading, const juce::String& detail)
{
    contextHeading.setText (heading, juce::dontSendNotification);
    contextDetail.setText (detail, juce::dontSendNotification);
}

void ToolBar::setStatus (const juce::String& message)
{
    statusLabel.setText (message, juce::dontSendNotification);
}

void ToolBar::setHistoryEnabled (bool canUndo, bool canRedo)
{
    undoButton.setEnabled (canUndo);
    redoButton.setEnabled (canRedo);
}

void ToolBar::setLoop (bool shouldLoop)
{
    loopButton.setToggleState (shouldLoop, juce::dontSendNotification);
}

void ToolBar::setMetronome (bool enabled)
{
    metronomeButton.setToggleState (enabled, juce::dontSendNotification);
}

void ToolBar::setNotification (const juce::String& message, bool isWarning)
{
    notificationIsWarning = isWarning;
    notificationLabel.setText (message, juce::dontSendNotification);
    notificationLabel.setColour (juce::Label::textColourId,
                                 isWarning ? colours::warning : colours::textDim);
}

void ToolBar::paint (juce::Graphics& g)
{
    g.fillAll (colours::chromeBackground);

    auto hint = getLocalBounds().removeFromLeft (layout::hintPanelWidth).reduced (6, 4);
    g.setColour (colours::windowBackground);
    g.fillRoundedRectangle (hint.toFloat(), 3.0f);

    g.setColour (colours::outline);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
}

void ToolBar::resized()
{
    auto area = getLocalBounds().reduced (6, 4);

    auto hint = area.removeFromLeft (layout::hintPanelWidth - 12).reduced (6, 2);
    contextHeading.setBounds (hint.removeFromTop (13));
    contextDetail.setBounds (hint.removeFromTop (16));
    statusLabel.setBounds (getLocalBounds().withTrimmedLeft (layout::hintPanelWidth + 4)
                               .withTrimmedTop (getHeight() - 15).withHeight (14));

    area.removeFromLeft (12);
    auto buttons = area.removeFromTop (getHeight() - 20);

    addTrackButton.setBounds (buttons.removeFromLeft (86).reduced (2));
    importButton.setBounds (buttons.removeFromLeft (80).reduced (2));
    removeTrackButton.setBounds (buttons.removeFromLeft (72).reduced (2));
    buttons.removeFromLeft (8);
    undoButton.setBounds (buttons.removeFromLeft (58).reduced (2));
    redoButton.setBounds (buttons.removeFromLeft (58).reduced (2));
    buttons.removeFromLeft (8);
    loopButton.setBounds (buttons.removeFromLeft (60).reduced (2));
    metronomeButton.setBounds (buttons.removeFromLeft (96).reduced (2));
    tapTempoButton.setBounds (buttons.removeFromLeft (50).reduced (2));

    shortcutsButton.setBounds (buttons.removeFromRight (52).reduced (2));
    notificationLabel.setBounds (buttons.removeFromRight (210).reduced (4, 2));
}

} // namespace saamveda::ui
