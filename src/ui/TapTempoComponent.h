#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace saamveda::ui
{

/** FL Studio-style isolated tap-tempo panel.

    Tapping only measures a tempo. The project is changed explicitly with
    Use Tempo, so playback and the timeline remain untouched while measuring.
*/
class TapTempoComponent final : public juce::Component
{
public:
    explicit TapTempoComponent (std::function<void (double)> applyTempo);

    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void registerTap();
    void reset();
    void apply();
    void updateDisplay();

    std::function<void (double)> onApply;
    std::vector<double> tapTimesMs;
    double measuredBpm = 0.0;

    juce::Label titleLabel, bpmLabel, instructionLabel;
    juce::TextButton tapButton { "TAP" };
    juce::TextButton resetButton { "Reset" };
    juce::TextButton useButton { "Use Tempo" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapTempoComponent)
};

} // namespace saamveda::ui
