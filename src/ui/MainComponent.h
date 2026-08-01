#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

namespace saamveda::ui
{

/** Phase 1 walking skeleton.

    Proves the toolchain end to end: a window opens, a real audio device can be
    selected, and a WAV file plays audibly.

    This is deliberately throwaway. From Phase 2 the UI issues commands and reads
    from the session model; it never owns transport state like this.
    See docs/05-architecture.md section 8.
*/
class MainComponent : public juce::Component,
                      private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void openFileClicked();
    void playClicked();
    void stopClicked();
    void loadFile (const juce::File&);
    void setStatus (const juce::String&);

    // Device manager must outlive the player, and the player the transport
    // source, so declaration order here is load bearing.
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer sourcePlayer;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioFormatManager formatManager;

    juce::AudioDeviceSelectorComponent deviceSelector;
    juce::TextButton openButton  { "Open WAV..." };
    juce::TextButton playButton  { "Play" };
    juce::TextButton stopButton  { "Stop" };
    juce::Label      statusLabel;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace saamveda::ui
