#include "TapTempoComponent.h"

#include <algorithm>

namespace saamveda::ui
{

TapTempoComponent::TapTempoComponent (std::function<void (double)> applyTempo)
    : onApply (std::move (applyTempo))
{
    titleLabel.setText ("Tap Tempo", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);

    bpmLabel.setText ("---.- BPM", juce::dontSendNotification);
    bpmLabel.setFont (juce::FontOptions (34.0f, juce::Font::bold));
    bpmLabel.setJustificationType (juce::Justification::centred);
    bpmLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffb74d));

    instructionLabel.setText ("Tap the button or press Space on every beat",
                              juce::dontSendNotification);
    instructionLabel.setJustificationType (juce::Justification::centred);

    tapButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff315f8f));
    tapButton.onClick = [this] { registerTap(); };
    resetButton.onClick = [this] { reset(); };
    useButton.onClick = [this] { apply(); };
    useButton.setEnabled (false);

    for (auto* component : std::initializer_list<juce::Component*>
         { &titleLabel, &bpmLabel, &instructionLabel, &tapButton, &resetButton, &useButton })
        addAndMakeVisible (component);

    setWantsKeyboardFocus (true);
    setSize (360, 300);
}

void TapTempoComponent::resized()
{
    auto area = getLocalBounds().reduced (16);
    titleLabel.setBounds (area.removeFromTop (34));
    bpmLabel.setBounds (area.removeFromTop (54));
    instructionLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (8);
    tapButton.setBounds (area.removeFromTop (92));
    area.removeFromTop (12);
    auto buttons = area.removeFromTop (34);
    resetButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (3));
    useButton.setBounds (buttons.reduced (3));
}

bool TapTempoComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey)
    {
        registerTap();
        return true;
    }
    return false;
}

void TapTempoComponent::registerTap()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    if (! tapTimesMs.empty() && now - tapTimesMs.back() > 2000.0)
        reset();

    tapTimesMs.push_back (now);
    if (tapTimesMs.size() > 16)
        tapTimesMs.erase (tapTimesMs.begin());

    updateDisplay();
}

void TapTempoComponent::reset()
{
    tapTimesMs.clear();
    measuredBpm = 0.0;
    bpmLabel.setText ("---.- BPM", juce::dontSendNotification);
    instructionLabel.setText ("Tap the button or press Space on every beat",
                              juce::dontSendNotification);
    useButton.setEnabled (false);
}

void TapTempoComponent::apply()
{
    if (measuredBpm > 0.0 && onApply)
        onApply (measuredBpm);

    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
        dialog->exitModalState (0);
}

void TapTempoComponent::updateDisplay()
{
    if (tapTimesMs.size() < 2)
    {
        instructionLabel.setText ("Keep tapping...", juce::dontSendNotification);
        return;
    }

    std::vector<double> intervals;
    intervals.reserve (tapTimesMs.size() - 1);
    for (size_t i = 1; i < tapTimesMs.size(); ++i)
        intervals.push_back (tapTimesMs[i] - tapTimesMs[i - 1]);

    std::sort (intervals.begin(), intervals.end());
    const auto median = intervals[intervals.size() / 2];
    measuredBpm = juce::jlimit (20.0, 400.0, 60000.0 / median);

    bpmLabel.setText (juce::String (measuredBpm, 1) + " BPM", juce::dontSendNotification);
    instructionLabel.setText (juce::String (tapTimesMs.size()) + " taps",
                              juce::dontSendNotification);
    useButton.setEnabled (tapTimesMs.size() >= 4);
}

} // namespace saamveda::ui
