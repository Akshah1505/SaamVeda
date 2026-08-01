#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace saamveda::ui
{

class TimelineComponent final : public juce::Component
{
public:
    std::function<void (double)> onSeek;

    void setLength (double seconds);
    void setPosition (double seconds);
    void setMusicalGrid (double bpm, int numerator, int denominator);
    void setSessionTracks (juce::ValueTree tracks);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void seekFromX (int x);

    double lengthSeconds = 60.0;
    double positionSeconds = 0.0;
    double tempoBpm = 120.0;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;
    juce::ValueTree tracks;
};

} // namespace saamveda::ui
