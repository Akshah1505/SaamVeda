#include "RealtimeSanityCheck.h"

// The hook is MSVC debug-CRT only. _DEBUG is defined by the compiler when /MTd
// or /MDd is selected, which is exactly our Debug configuration.
#if defined (_MSC_VER) && defined (_DEBUG) && SAAMVEDA_REALTIME_CHECKS
 #define SAAMVEDA_ALLOC_HOOK_ACTIVE 1
 #include <crtdbg.h>
 #include <windows.h>
 #include <dbghelp.h>
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
    std::atomic<int> minimumBlockSize { 0 };
    std::atomic<int> maximumBlockSize { 0 };
    std::atomic<int> deliveredInputChannels { -1 };

#if SAAMVEDA_ALLOC_HOOK_ACTIVE
    // Stacks for the first few offenders. Fixed storage, because the whole
    // point is to record an allocation without making one.
    constexpr int maxOffenders = 6;
    constexpr int maxFrames = 40;
    // Capture from the very first offender. This was 200,000 while the wave
    // input channel mismatch was flooding the hook - capturing early then only
    // recorded the flood, and the interesting stacks came from its steady
    // state. With that fixed the baseline is a handful of allocations, so the
    // first one caught is the one worth seeing.
    constexpr int captureAfterAllocations = 0;
    std::atomic<int> capturedOffenders { 0 };
    void* offenderFrames[maxOffenders][maxFrames] {};
    unsigned short offenderFrameCount[maxOffenders] {};
    size_t offenderSize[maxOffenders] {};
#endif

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

            // Start-up allocations are already excluded by the warm-up block
            // count, so anything reaching here is worth a stack.
            const auto seenSoFar = allocationCount.load (std::memory_order_relaxed);
            const auto slot = capturedOffenders.load (std::memory_order_relaxed);

            if (seenSoFar > captureAfterAllocations && slot < maxOffenders)
            {
                // Return addresses only - symbolising here would allocate, and
                // this runs inside the CRT's allocation lock.
                offenderFrameCount[slot] = RtlCaptureStackBackTrace (
                    2, maxFrames, offenderFrames[slot], nullptr);
                offenderSize[slot] = size;
                capturedOffenders.store (slot + 1, std::memory_order_relaxed);
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
    minimumBlockSize.store (0, std::memory_order_relaxed);
    maximumBlockSize.store (0, std::memory_order_relaxed);

#if SAAMVEDA_ALLOC_HOOK_ACTIVE
    capturedOffenders.store (0, std::memory_order_relaxed);
#endif
}

juce::StringArray RealtimeSanityCheck::describeOffenders()
{
#if SAAMVEDA_ALLOC_HOOK_ACTIVE
    juce::StringArray result;

    static auto symbolsReady = []
    {
        SymSetOptions (SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
        return SymInitialize (GetCurrentProcess(), nullptr, TRUE) != FALSE;
    }();

    const auto process = GetCurrentProcess();
    const auto count = juce::jmin (capturedOffenders.load(), maxOffenders);

    for (int i = 0; i < count; ++i)
    {
        juce::String trace;
        trace << "--- audio-thread allocation #" << (i + 1)
              << " (" << static_cast<int> (offenderSize[i]) << " bytes) ---";

        for (int f = 0; f < offenderFrameCount[i]; ++f)
        {
            const auto address = reinterpret_cast<DWORD64> (offenderFrames[i][f]);
            juce::String line = "    " + juce::String::toHexString (static_cast<juce::int64> (address));

            if (symbolsReady)
            {
                alignas (SYMBOL_INFO) char buffer[sizeof (SYMBOL_INFO) + 512] {};
                auto* symbol = reinterpret_cast<SYMBOL_INFO*> (buffer);
                symbol->SizeOfStruct = sizeof (SYMBOL_INFO);
                symbol->MaxNameLen = 500;

                DWORD64 displacement = 0;
                if (SymFromAddr (process, address, &displacement, symbol))
                {
                    line = "    " + juce::String (symbol->Name);

                    IMAGEHLP_LINE64 lineInfo {};
                    lineInfo.SizeOfStruct = sizeof (IMAGEHLP_LINE64);
                    DWORD lineDisplacement = 0;

                    if (SymGetLineFromAddr64 (process, address, &lineDisplacement, &lineInfo))
                        line << "  (" << juce::File (lineInfo.FileName).getFileName()
                             << ":" << static_cast<int> (lineInfo.LineNumber) << ")";
                }
            }

            trace << juce::newLine << line;
        }

        result.add (trace);
    }

    return result;
#else
    return {};
#endif
}

RealtimeSanityCheck::Report RealtimeSanityCheck::report() const
{
    Report result;
    result.available = isAvailable();
    result.armed = armed.load (std::memory_order_relaxed);
    result.allocations = allocationCount.load (std::memory_order_relaxed);
    result.largestAllocationBytes = largestAllocation.load (std::memory_order_relaxed);
    result.minimumBlockSize = minimumBlockSize.load (std::memory_order_relaxed);
    result.maximumBlockSize = maximumBlockSize.load (std::memory_order_relaxed);
    result.inputChannels = deliveredInputChannels.load (std::memory_order_relaxed);
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
                                                            int numInputChannels,
                                                            float* const* outputChannelData,
                                                            int numOutputChannels,
                                                            int numSamples,
                                                            const juce::AudioIODeviceCallbackContext&)
{
    audioThreadId.store (juce::Thread::getCurrentThreadId(), std::memory_order_relaxed);
    deliveredInputChannels.store (numInputChannels, std::memory_order_relaxed);

    // A device that varies its block size forces every downstream buffer to
    // resize, so this is worth knowing before blaming the code downstream.
    auto smallest = minimumBlockSize.load (std::memory_order_relaxed);
    while ((smallest == 0 || numSamples < smallest)
           && ! minimumBlockSize.compare_exchange_weak (smallest, numSamples,
                                                        std::memory_order_relaxed))
    {
    }

    auto largest = maximumBlockSize.load (std::memory_order_relaxed);
    while (numSamples > largest
           && ! maximumBlockSize.compare_exchange_weak (largest, numSamples,
                                                        std::memory_order_relaxed))
    {
    }

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
