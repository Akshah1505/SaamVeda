#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace saamveda::engine
{

/** The engine-wide policy decisions tracktion asks the host to make.

    Two of tracktion's defaults are wrong for this application, and both of them
    are wrong quietly.
*/
class StudioBehaviour final : public tracktion::engine::EngineBehaviour
{
public:
    //==============================================================================
    /** Where recordings are written.

        tracktion's default returns an invalid File, so `%projectdir%` expands to
        nothing and takes are written to a relative path - which lands in the
        process's current working directory. That scattered recordings into
        whatever folder the application happened to be launched from, and would
        silently produce no clip at all when that folder was not writable.
    */
    juce::File getDefaultFolderForAudioRecordings (tracktion::engine::Edit&) override
    {
        auto music = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

        if (! music.isDirectory())
            music = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

        auto folder = music.getChildFile ("SaamVeda Studio").getChildFile ("Recordings");
        folder.createDirectory();

        return folder;
    }

    //==============================================================================
    /** Describes the wave devices from the channels that are actually active.

        tracktion's default layout pairs up every channel the driver *names*. On
        the WASAPI endpoint this machine reports, that is 64 named inputs and 32
        stereo wave input devices, while only 2 channels reach the audio
        callback. The other 31 devices index past the end of the callback's
        channel array on every block, and each one costs twice:

          - `WaveInputDeviceInstance::copyIncomingDataIntoBuffer` trips
            `jassertfalse`, which formats a string. Debug only, but it was 40,000
            allocations a second on the audio thread.
          - `WaveInputDevice::consumeNextAudioBlock` heap-allocates a scratch
            AudioBuffer for every enabled device with no instance attached. That
            one allocates in Release too.

        Describing only the active channels removes both. The indices here are
        positions in the callback's channel array - which is how tracktion uses
        them - so they count active channels, not named ones.
    */
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
