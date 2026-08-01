#include "MainComponent.h"

namespace saamveda::ui
{

MainComponent::MainComponent()
    : deviceSelector (deviceManager,
                      0, 2,    // min/max input channels
                      0, 2,    // min/max output channels
                      false,   // no MIDI input selector yet (Phase 5)
                      false,   // no MIDI output selector
                      true,    // stereo pairs
                      false)   // show advanced options
{
    formatManager.registerBasicFormats();

    // 0 inputs for now: Phase 1 only plays back. Recording arrives in Phase 4,
    // at which point this needs revisiting along with input permissions.
    if (auto error = deviceManager.initialiseWithDefaultDevices (0, 2); error.isNotEmpty())
        setStatus ("Audio device error: " + error);
    else
        setStatus ("Ready. Open a WAV file to play.");

    sourcePlayer.setSource (&transport);
    deviceManager.addAudioCallback (&sourcePlayer);
    transport.addChangeListener (this);

    openButton.onClick = [this] { openFileClicked(); };
    playButton.onClick = [this] { playClicked(); };
    stopButton.onClick = [this] { stopClicked(); };

    playButton.setEnabled (false);
    stopButton.setEnabled (false);

    statusLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (deviceSelector);
    addAndMakeVisible (openButton);
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (statusLabel);

    setSize (760, 560);
}

MainComponent::~MainComponent()
{
    // Tear down in reverse order of setup, or the audio thread can call into
    // a partially destroyed object.
    transport.removeChangeListener (this);
    deviceManager.removeAudioCallback (&sourcePlayer);
    sourcePlayer.setSource (nullptr);
    transport.setSource (nullptr);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (8);

    auto controls = area.removeFromTop (32);
    openButton.setBounds (controls.removeFromLeft (120).reduced (2));
    playButton.setBounds (controls.removeFromLeft (90).reduced (2));
    stopButton.setBounds (controls.removeFromLeft (90).reduced (2));

    statusLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (8);
    deviceSelector.setBounds (area);
}

void MainComponent::openFileClicked()
{
    chooser = std::make_unique<juce::FileChooser> ("Select an audio file",
                                                   juce::File{},
                                                   formatManager.getWildcardForAllFormats());

    const auto chooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        if (auto file = fc.getResult(); file != juce::File{})
            loadFile (file);
    });
}

void MainComponent::loadFile (const juce::File& file)
{
    // Reader is owned by the source, which is swapped in only after it is fully
    // prepared. The old source is released after the transport has let it go.
    if (auto* reader = formatManager.createReaderFor (file))
    {
        auto newSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);

        transport.setSource (newSource.get(),
                             0, nullptr,
                             reader->sampleRate);

        readerSource = std::move (newSource);

        playButton.setEnabled (true);
        setStatus (file.getFileName()
                   + "  |  " + juce::String (reader->sampleRate, 0) + " Hz"
                   + "  |  " + juce::String (reader->numChannels) + " ch"
                   + "  |  " + juce::String (transport.getLengthInSeconds(), 2) + " s");
    }
    else
    {
        setStatus ("Could not read: " + file.getFileName());
    }
}

void MainComponent::playClicked()
{
    transport.start();
}

void MainComponent::stopClicked()
{
    transport.stop();
    transport.setPosition (0.0);
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &transport)
    {
        const bool playing = transport.isPlaying();
        playButton.setEnabled (! playing && readerSource != nullptr);
        stopButton.setEnabled (playing);
    }
}

void MainComponent::setStatus (const juce::String& text)
{
    statusLabel.setText (text, juce::dontSendNotification);
}

} // namespace saamveda::ui
