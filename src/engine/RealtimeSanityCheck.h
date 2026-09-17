#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>

namespace saamveda::engine
{

/** Debug-build allocation detector for the audio thread.

    docs/05-architecture.md section 4 forbids allocation on the audio thread and
    section 12 risk 3 requires this detector from Phase 2. It is the cheapest
    possible insurance: an audio-thread allocation that nobody notices in Phase 2
    becomes a dropout nobody can locate in Phase 11.

    How it works. The object registers itself as an extra
    juce::AudioIODeviceCallback purely to learn the audio thread's identity —
    it produces no sound. Every CRT heap allocation is then routed through a
    hook that flags any allocation happening on that thread. Because the check
    is by thread rather than by scope, it covers tracktion_engine's own callback
    as well as ours.

    Limits, stated plainly:
      - MSVC debug CRT only. The hook is _CrtSetAllocHook, which needs /MTd or
        /MDd. On other toolchains the class compiles to a no-op and reports
        itself disabled rather than silently reporting zero violations.
      - It sees CRT heap traffic. Direct HeapAlloc/VirtualAlloc, mutex
        contention, and file I/O are not caught. Those stay a code-review
        concern.
      - The first few blocks after the device opens are ignored; device startup
        legitimately allocates on the callback thread.
*/
class RealtimeSanityCheck final : public juce::AudioIODeviceCallback
{
public:
    struct Report
    {
        bool available = false;    ///< false when built without the hook
        bool armed = false;        ///< true once past device warm-up
        int allocations = 0;
        size_t largestAllocationBytes = 0;

        /** Smallest and largest block the device has delivered. A device that
            varies its block size makes every downstream buffer resize, which is
            an allocation per block on the audio thread. */
        int minimumBlockSize = 0;
        int maximumBlockSize = 0;

        /** Input channels the device actually delivers to the callback. */
        int inputChannels = 0;
    };

    RealtimeSanityCheck() = default;
    ~RealtimeSanityCheck() override;

    /** Starts watching. Safe to call once per device manager. */
    void attachTo (juce::AudioDeviceManager& deviceManager);
    void detach();

    Report report() const;
    void reset();

    /** Symbolised call stacks for the first few offending allocations.

        Counting allocations says something is wrong; only a stack says what. The
        first few are captured inside the hook - raw return addresses, no
        allocation - and symbolised here, on the message thread, when asked.
        Empty until something is caught, and always empty in release builds.
    */
    juce::StringArray describeOffenders();

    /** True when this build can actually detect anything. */
    static bool isAvailable() noexcept;

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

private:
    juce::AudioDeviceManager* manager = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RealtimeSanityCheck)
};

} // namespace saamveda::engine
