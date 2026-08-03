#include "PlaylistPanel.h"

namespace saamveda::ui
{

PlaylistPanel::PlaylistPanel()
{
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setColour (juce::Label::textColourId, colours::text);
    titleLabel.setFont (juce::Font (juce::FontOptions (12.0f)));

    for (auto* label : { &viewLabel, &musicalLabel })
    {
        label->setJustificationType (juce::Justification::centredRight);
        label->setColour (juce::Label::textColourId, colours::textDim);
        label->setFont (juce::Font (juce::FontOptions (11.0f)));
    }

    zoomOutButton.onClick = [this] { timelineComponent.zoomOut(); refreshReadouts(); };
    zoomInButton.onClick  = [this] { timelineComponent.zoomIn();  refreshReadouts(); };
    zoomFitButton.onClick = [this] { timelineComponent.zoomToFit(); refreshReadouts(); };
    followButton.onClick  = [this]
    {
        timelineComponent.setFollowPlayhead (followButton.getToggleState());
    };

    for (auto* button : { &zoomOutButton, &zoomInButton, &zoomFitButton })
    {
        button->setWantsKeyboardFocus (false);
        button->setColour (juce::TextButton::buttonColourId, colours::headerBackground);
        button->setColour (juce::TextButton::textColourOffId, colours::text);
    }

    followButton.setWantsKeyboardFocus (false);
    followButton.setToggleState (timelineComponent.isFollowingPlayhead(),
                                 juce::dontSendNotification);

    setBreadcrumb ("Arrangement", {});

    for (auto* component : std::initializer_list<juce::Component*>
         { &titleLabel, &viewLabel, &musicalLabel, &zoomOutButton, &zoomInButton,
           &zoomFitButton, &followButton, &clipBrowser, &timelineComponent })
        addAndMakeVisible (component);
}

void PlaylistPanel::setBreadcrumb (const juce::String& arrangementName,
                                   const juce::String& selection)
{
    auto text = "Playlist  -  " + arrangementName;
    if (selection.isNotEmpty())
        text << "  >  " << selection;

    titleLabel.setText (text, juce::dontSendNotification);
}

void PlaylistPanel::setMusicalReadout (double bpm, int numerator, int denominator, int bar)
{
    musicalLabel.setText (juce::String (bpm, 0) + " BPM   "
                              + juce::String (numerator) + "/" + juce::String (denominator)
                              + "   bar " + juce::String (bar),
                          juce::dontSendNotification);
}

void PlaylistPanel::refreshReadouts()
{
    viewLabel.setText ("view " + juce::String (timelineComponent.visibleSpanSeconds(), 1) + " s",
                       juce::dontSendNotification);
    followButton.setToggleState (timelineComponent.isFollowingPlayhead(),
                                 juce::dontSendNotification);
}

void PlaylistPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour (colours::panelTitle);
    g.fillRect (bounds.removeFromTop (layout::panelTitleHeight));

    g.setColour (colours::chromeBackground);
    g.fillRect (bounds.removeFromTop (layout::panelToolStripHeight));

    g.setColour (colours::outline);
    g.drawRect (getLocalBounds(), 1);
    g.drawHorizontalLine (layout::panelTitleHeight, 0.0f, static_cast<float> (getWidth()));
    g.drawHorizontalLine (layout::panelTitleHeight + layout::panelToolStripHeight,
                          0.0f, static_cast<float> (getWidth()));
}

void PlaylistPanel::resized()
{
    auto bounds = getLocalBounds().reduced (1);

    auto title = bounds.removeFromTop (layout::panelTitleHeight);
    titleLabel.setBounds (title.reduced (8, 0));

    auto tools = bounds.removeFromTop (layout::panelToolStripHeight).reduced (4, 2);
    zoomOutButton.setBounds (tools.removeFromLeft (26).reduced (1));
    zoomInButton.setBounds (tools.removeFromLeft (26).reduced (1));
    zoomFitButton.setBounds (tools.removeFromLeft (40).reduced (1));
    tools.removeFromLeft (8);
    followButton.setBounds (tools.removeFromLeft (78).reduced (1));

    musicalLabel.setBounds (tools.removeFromRight (190));
    viewLabel.setBounds (tools.removeFromRight (90));

    clipBrowser.setBounds (bounds.removeFromLeft (layout::browserWidth));
    timelineComponent.setBounds (bounds);
}

} // namespace saamveda::ui
