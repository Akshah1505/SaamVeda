#include "RealtimeSanityCheck.h"

// The hook is MSVC debug-CRT only. _DEBUG is defined by the compiler when /MTd
// or /MDd is selected, which is exactly our Debug configuration.
#if defined (_MSC_VER) && defined (_DEBUG) && SAAMVEDA_REALTIME_CHECKS
 #define SAAMVEDA_ALLOC_HOOK_ACTIVE 1
 #include <crtdbg.h>
#else
 #define SAAMVEDA_ALLOC_HOOK_ACTIVE 0
#endif

namespace saamveda::engine
{

namespace
{
    // Number of device callbacks ignored after the stream starts. Opening a
    // device allocates on the callback thread; flagging that would train the
    // developer to ignore the counter, which defeats the point.
    constexpr int warmUpBlocks = 32;

    std::atomic<juce::Thread::ThreadID> audioThreadId { nullptr };
    std::atomic<int> blocksSeen { 0 };
    std::atomic<bool> armed { false };
    std::atomic<int> allocationCount { 0 };
    std::atomic<size_t> largestAllocation { 0 };

#if SAAMVEDA_ALLOC_HOOK_ACTIVE
    _CRT_ALLOC_HOOK previousHook = nullptr;
    std::atomic<bool> hookInstalled { false };

    // Runs inside the debug CRT's allocation lock. It must not allocate, lock,
    // or log; atomics only.
    int allocHook (int allocType, void* userData, size_t size, int blockType,
                   long requestNumber, const unsigned char* filename, int lineNumber)
    {
        if (blockType != _CRT_BLOCK && allocType != _HOOK_FREE
            && armed.load (std::memory_order_relaxed)
            && juce::Thread::getCurrentThreadId() == audioThreadId.load (std::memory_order_relaxed))
        {
            allocationCount.fetch_add (1, std::memory_order_relaxed);

            auto previous = largestAllocation.load (std::memory_order_relaxed);
            while (size > previous
                   && ! largestAllocation.compare_exchange_weak (previous, size,
                                                                 std::memory_order_relaxed))
            {
            }
        }

        if (previousHook != nullptr)
            return previousHook (allocType, userData, size, blockType, requestNumber,
                                 filename, lineNumber);

        // Non-zero tells the CRT to proceed with the allocation. Spelled as 1
        // rather than TRUE so this file does not have to pull in windows.h.
        return 1;
    }
#endif
}

RealtimeSanityCheck::~RealtimeSanityCheck()
{
    detach();
}

bool RealtimeSanityCheck::isAvailable() noexcept
{
    return SAAMVEDA_ALLOC_HOOK_ACTIVE != 0;
}

void RealtimeSanityCheck::attachTo (juce::AudioDeviceManager& deviceManager)
{
    if (manager != nullptr)
        return;

    manager = &deviceManager;
    reset();

#if SAAMVEDA_ALLOC_HOOK_ACTIVE
    if (! hookInstalled.exchange (true))
        previousHook = _CrtSetAllocHook (allocHook);
#endif

    manager->addAudioCallback (this);
}

void RealtimeSanityCheck::detach()
{
    if (manager == nullptr)
        return;

    manager->removeAudioCallback (this);
    manager = nullptr;
    armed.store (false, std::memory_order_relaxed);

#if SAAMVEDA_ALLOC_HOOK_ACTIVE
    if (hookInstalled.exchange (false))
    {
        _CrtSetAllocHook (previousHook);
        previousHook = nullptr;
    }
#endif
}

void RealtimeSanityCheck::reset()
{
    armed.store (false, std::memory_order_relaxed);
    blocksSeen.store (0, std::memory_order_relaxed);
    allocationCount.store (0, std::memory_order_relaxed);
    largestAllocation.store (0, std::memory_order_relaxed);
}

RealtimeSanityCheck::Report RealtimeSanityCheck::report() const
{
    Report result;
    result.available = isAvailable();
    result.armed = armed.load (std::memory_order_relaxed);
    result.allocations = allocationCount.load (std::memory_order_relaxed);
    result.largestAllocationBytes = largestAllocation.load (std::memory_order_relaxed);
    return result;
}

void RealtimeSanityCheck::audioDeviceAboutToStart (juce::AudioIODevice*)
{
    // Called on the message thread; the audio thread is identified in the
    // callback itself.
    armed.store (false, std::memory_order_relaxed);
    blocksSeen.store (0, std::memory_order_relaxed);
}

void RealtimeSanityCheck::audioDeviceStopped()
{
    armed.store (false, std::memory_order_relaxed);
}

void RealtimeSanityCheck::audioDeviceIOCallbackWithContext (const float* const*,
                                                            int,
                                                            float* const* outputChannelData,
                                                            int numOutputChannels,
                                                            int numSamples,
                                                            const juce::AudioIODeviceCallbackContext&)
{
    audioThreadId.store (juce::Thread::getCurrentThreadId(), std::memory_order_relaxed);

    const auto seen = blocksSeen.fetch_add (1, std::memory_order_relaxed) + 1;
    if (seen == warmUpBlocks)
        armed.store (true, std::memory_order_relaxed);

    // This callback contributes silence. AudioDeviceManager hands the raw
    // output buffer to whichever callback runs first and mixes the rest in, so
    // leaving it untouched could play back uninitialised memory.
    for (int channel = 0; channel < numOutputChannels; ++channel)
        if (auto* destination = outputChannelData[channel])
            juce::FloatVectorOperations::clear (destination, numSamples);
}

} // namespace saamveda::engine
