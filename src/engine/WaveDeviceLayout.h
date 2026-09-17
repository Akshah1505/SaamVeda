#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace saamveda::engine
{

/** Describes tracktion's wave devices from the channels that are actually active.

    tracktion's default layout pairs up every channel the driver *names*. On the
    64-channel WASAPI endpoint this machine reports, that is 32 stereo inputs,
    while only 2 channels reach the audio callback. The other 31 devices index
    past the end of the callback's channel array on every block, and each one
    costs twice:

      - `WaveInputDeviceInstance::copyIncomingDataIntoBuffer` trips `jassertfalse`,
        which formats a string. Debug only, but it was 40,000 allocations a second
        on the audio thread.
      - `WaveInputDevice::consumeNextAudioBlock` heap-allocates a scratch
        AudioBuffer for every enabled device that has no instance attached. That
        one allocates in Release too.

    Describing only the active channels removes both. The indices here are
    positions in the callback's channel array - which is how tracktion uses them -
    so they count active channels, not named ones.
*/
class ActiveChannelWaveDeviceLayout final : public tracktion::engine::EngineBehaviour
{
public:
    bool isDescriptionOfWaveDevicesSupported() override { return true; }

    void describeWaveDevices (std::vector<tracktion::engine::WaveDeviceDescription>& descriptions,
                              juce::AudioIODevice& device, bool isInput) override
    {
        // tracktion does not clear this for us, and initialise() can run more
        // than once as devices come and go.
        descriptions.clear();

        const auto active = isInput ? device.getActiveInputChannels()
                                    : device.getActiveOutputChannels();

        const auto channelCount = active.countNumberOfSetBits();

        for (int index = 0; index < channelCount; ++index)
        {
            // Pairs, with a trailing odd channel left mono - the same shape
            // tracktion's own default builds, over a shorter list.
            const auto width = (index + 1 < channelCount) ? 2 : 1;

            descriptions.push_back (tracktion::engine::WaveDeviceDescription::withNumChannels (
                {}, static_cast<uint32_t> (index), static_cast<uint32_t> (width), true));

            index += width - 1;
        }
    }
};

} // namespace saamveda::engine
