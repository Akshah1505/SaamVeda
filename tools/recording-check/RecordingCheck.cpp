/*  Measures the two Phase 4 acceptance criteria that need a real audio device
    and real time, and so cannot live in the unit tests:

      --soak <minutes>   a continuous recording completes with no gaps
      --alignment        a recorded track aligns with existing material
      --record <seconds> records, for the crash test: kill the process partway
                         through and then --analyse what reached the disk
      --analyse <file>   reports what is in a wav

    Run with no arguments to list the audio devices and exit. Results go to
    stdout as `KEY = value` lines and the exit code is 0 only when every
    measured criterion passed, so this can gate a release.

    See docs/02-prd.md FR4 for the criteria and docs/11-testing-strategy.md for
    where this sits relative to the unit tests.
*/

#include <juce_audio_utils/juce_audio_utils.h>

#include <iostream>

#include "../../src/core/Session.h"
#include "../../src/engine/EngineController.h"

namespace
{

/** Runs the message loop for real time. The engine does its work on the message
    thread - device changes, recording completion - so sleeping would hang it. */
void pump (double seconds)
{
    const auto until = juce::Time::getMillisecondCounterHiRes() + seconds * 1000.0;

    while (juce::Time::getMillisecondCounterHiRes() < until)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
}

/** Mirrors tracktion's own log to stdout. tracktion reports recording failures
    through juce::Logger and nowhere else, so without this a recording that
    stops itself looks like a recording that simply produced nothing. */
class EchoingLogger final : public juce::Logger
{
public:
    void logMessage (const juce::String& message) override
    {
        lastMessageAt = juce::Time::getMillisecondCounterHiRes();
        std::cout << "  [engine] " << message.toStdString() << std::endl;
    }

    double lastMessageAt = juce::Time::getMillisecondCounterHiRes();
};

EchoingLogger* engineLogger = nullptr;

/** Waits until the engine stops reconfiguring itself.

    The MIDI device scan finishes a second or two after startup and triggers a
    device-list reload, which rebuilds the playback context and kills any
    recording in progress. Starting to record before that has settled measures
    the scan, not the recording.
*/
void waitForDeviceScan()
{
    // The MIDI scan is kicked off lazily and reports itself as taking 0 ms, so
    // there is no point waiting only for quiet - it has not started yet. Sit
    // through a fixed warm-up first, then wait for the log to go quiet.
    const auto startedAt = juce::Time::getMillisecondCounterHiRes();
    const auto warmUpUntil = startedAt + 12000.0;
    const auto giveUpAt = startedAt + 45000.0;

    while (juce::Time::getMillisecondCounterHiRes() < giveUpAt)
    {
        pump (0.5);

        if (juce::Time::getMillisecondCounterHiRes() < warmUpUntil)
            continue;

        if (engineLogger == nullptr
             || juce::Time::getMillisecondCounterHiRes() - engineLogger->lastMessageAt > 3000.0)
            return;
    }
}

struct RecordedTake
{
    juce::File file;
    double startSeconds = 0.0;
    double lengthSeconds = 0.0;
    bool received = false;
};

void listDevices (saamveda::engine::EngineController& engine)
{
    auto& manager = engine.audioDeviceManager();

    std::cout << "device.type = "
              << manager.getCurrentAudioDeviceType().toStdString() << "\n";

    if (auto* device = manager.getCurrentAudioDevice())
    {
        std::cout << "device.name = " << device->getName().toStdString() << "\n";
        std::cout << "device.sampleRate = " << device->getCurrentSampleRate() << "\n";
        std::cout << "device.blockSize = " << device->getCurrentBufferSizeSamples() << "\n";
        std::cout << "device.inputLatency = " << device->getInputLatencyInSamples() << "\n";
        std::cout << "device.outputLatency = " << device->getOutputLatencyInSamples() << "\n";
        std::cout << "device.namedInputs = " << device->getInputChannelNames().size() << "\n";
        std::cout << "device.activeInputs = "
                  << device->getActiveInputChannels().countNumberOfSetBits() << "\n";
    }

    std::cout << "engine.waveInputs = "
              << engine.inputChannelDiagnostics().toStdString() << "\n";

    for (const auto& name : engine.inputDeviceNames())
        std::cout << "engine.input = " << name.toStdString() << "\n";
}

/** What is actually in a recorded take, read back off disk. */
struct TakeAnalysis
{
    bool valid = false;
    double sampleRate = 0.0;
    juce::int64 samples = 0;
    double lengthSeconds = 0.0;
    double peak = 0.0;
    juce::int64 longestSilentRun = 0;
    juce::int64 firstTransient = -1;
};

TakeAnalysis analyse (const juce::File& file, float silenceThreshold)
{
    TakeAnalysis result;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
        return result;

    result.valid = true;
    result.sampleRate = reader->sampleRate;
    result.samples = (juce::int64) reader->lengthInSamples;
    result.lengthSeconds = reader->sampleRate > 0.0
        ? (double) reader->lengthInSamples / reader->sampleRate : 0.0;

    // Streamed in blocks: a 30-minute stereo take at 48 kHz is ~330 MB and does
    // not want to be resident.
    const int blockSize = 1 << 16;
    juce::AudioBuffer<float> block ((int) reader->numChannels, blockSize);

    juce::int64 position = 0;
    juce::int64 silentRun = 0;

    while (position < (juce::int64) reader->lengthInSamples)
    {
        const auto toRead = (int) juce::jmin ((juce::int64) blockSize,
                                              (juce::int64) reader->lengthInSamples - position);
        reader->read (&block, 0, toRead, position, true, true);

        for (int i = 0; i < toRead; ++i)
        {
            auto magnitude = 0.0f;

            for (int channel = 0; channel < block.getNumChannels(); ++channel)
                magnitude = juce::jmax (magnitude, std::abs (block.getSample (channel, i)));

            result.peak = juce::jmax (result.peak, (double) magnitude);

            if (magnitude < silenceThreshold)
            {
                ++silentRun;
                result.longestSilentRun = juce::jmax (result.longestSilentRun, silentRun);
            }
            else
            {
                silentRun = 0;

                if (result.firstTransient < 0)
                    result.firstTransient = position + i;
            }
        }

        position += toRead;
    }

    return result;
}

int runSoak (saamveda::engine::EngineController& engine, saamveda::core::Session& session,
             double minutes)
{
    using saamveda::core::Session;

    RecordedTake take;
    engine.onClipRecorded = [&take] (juce::String, juce::File file, double start, double length)
    {
        take.file = file;
        take.startSeconds = start;
        take.lengthSeconds = length;
        take.received = true;
    };

    const auto track = session.addTrack ("audio", "Soak");
    const auto trackId = track.getProperty (Session::idProperty()).toString();
    engine.synchronise (session);

    if (! engine.setTrackArmed (trackId, true))
    {
        std::cout << "FAIL = could not arm a track; no usable input\n";
        return 1;
    }

    auto* device = engine.audioDeviceManager().getCurrentAudioDevice();
    const auto xrunsBefore = device != nullptr ? device->getXRunCount() : -1;

    const auto startedAt = juce::Time::getMillisecondCounterHiRes();
    engine.startRecording();
    pump (1.0);

    if (! engine.isRecording())
    {
        std::cout << "FAIL = transport did not enter record\n";
        return 1;
    }

    auto nextReport = 60.0;

    while (true)
    {
        const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0;

        if (elapsed >= minutes * 60.0)
            break;

        if (elapsed >= nextReport)
        {
            std::cout << "progress = " << (int) (elapsed / 60.0) << " min, recording="
                      << (engine.isRecording() ? "yes" : "NO") << ", allocations="
                      << engine.realtimeReport().allocations << std::endl;
            nextReport += 60.0;
        }

        pump (1.0);
    }

    const auto wallSeconds = (juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000.0;
    const auto stillRecording = engine.isRecording();

    engine.stopRecording (false);

    // Writing and flushing the take is asynchronous.
    for (auto waited = 0; waited < 60 && ! take.received; ++waited)
        pump (1.0);

    device = engine.audioDeviceManager().getCurrentAudioDevice();
    const auto xrunsAfter = device != nullptr ? device->getXRunCount() : -1;

    std::cout << "soak.requestedMinutes = " << minutes << "\n";
    std::cout << "soak.wallSeconds = " << wallSeconds << "\n";
    std::cout << "soak.stayedInRecord = " << (stillRecording ? "yes" : "no") << "\n";
    std::cout << "soak.clipReceived = " << (take.received ? "yes" : "no") << "\n";
    std::cout << "soak.xruns = " << (xrunsAfter - xrunsBefore) << "\n";
    std::cout << "soak.allocations = " << engine.realtimeReport().allocations << "\n";

    if (! take.received)
    {
        std::cout << "FAIL = no clip was produced\n";
        return 1;
    }

    const auto analysis = analyse (take.file, 1.0e-5f);

    if (! analysis.valid)
    {
        std::cout << "FAIL = recorded file could not be read: "
                  << take.file.getFullPathName().toStdString() << "\n";
        return 1;
    }

    // A dropout inside a WAV shows up as length, not as a hole: the writer
    // simply receives fewer samples than elapsed time called for.
    const auto expectedSamples = (juce::int64) std::llround (wallSeconds * analysis.sampleRate);
    const auto shortfall = expectedSamples - analysis.samples;
    const auto shortfallMs = 1000.0 * (double) shortfall / analysis.sampleRate;

    std::cout << "soak.file = " << take.file.getFullPathName().toStdString() << "\n";
    std::cout << "soak.sampleRate = " << analysis.sampleRate << "\n";
    std::cout << "soak.recordedSeconds = " << analysis.lengthSeconds << "\n";
    std::cout << "soak.shortfallMs = " << shortfallMs << "\n";
    std::cout << "soak.peak = " << analysis.peak << "\n";
    std::cout << "soak.longestSilenceSeconds = "
              << (double) analysis.longestSilentRun / analysis.sampleRate << "\n";

    // The take is allowed to be a little shorter than wall time - stopping is
    // not instantaneous - but not by anything a listener would hear as a gap.
    const auto lengthOk = shortfallMs > -250.0 && shortfallMs < 250.0;
    const auto passed = stillRecording && lengthOk && (xrunsAfter - xrunsBefore) == 0;

    std::cout << "soak.result = " << (passed ? "PASS" : "FAIL") << "\n";
    return passed ? 0 : 1;
}

int runAlignment (saamveda::engine::EngineController& engine, saamveda::core::Session& session,
                  const juce::String& startOption)
{
    using saamveda::core::Session;

    auto* device = engine.audioDeviceManager().getCurrentAudioDevice();

    if (device == nullptr)
    {
        std::cout << "FAIL = no audio device\n";
        return 1;
    }

    const auto inputLatency = device->getInputLatencyInSamples();
    const auto outputLatency = device->getOutputLatencyInSamples();
    const auto sampleRate = device->getCurrentSampleRate();

    std::cout << "alignment.sampleRate = " << sampleRate << "\n";
    std::cout << "alignment.inputLatencySamples = " << inputLatency << "\n";
    std::cout << "alignment.outputLatencySamples = " << outputLatency << "\n";
    std::cout << "alignment.reportedRoundTripMs = "
              << 1000.0 * (inputLatency + outputLatency) / sampleRate << "\n";

    RecordedTake take;
    engine.onClipRecorded = [&take] (juce::String, juce::File file, double start, double length)
    {
        take.file = file;
        take.startSeconds = start;
        take.lengthSeconds = length;
        take.received = true;
    };

    const auto track = session.addTrack ("audio", "Alignment");
    const auto trackId = track.getProperty (Session::idProperty()).toString();
    engine.synchronise (session);

    if (! engine.setTrackArmed (trackId, true))
    {
        std::cout << "FAIL = could not arm a track; no usable input\n";
        return 1;
    }

    // Record from a start position that is not zero: an offset applied once at
    // the origin is invisible there.
    const auto requestedStart = startOption.isNotEmpty() ? startOption.getDoubleValue() : 4.0;
    engine.seek (requestedStart);
    pump (0.5);

    std::cout << "alignment.positionAfterSeek = " << engine.positionSeconds() << "\n";
    std::cout << "alignment.contentBefore = " << engine.contentLengthSeconds() << "\n";

    engine.startRecording();
    pump (1.0);

    std::cout << "alignment.armedTracks = " << engine.armedTrackCount() << "\n";
    std::cout << "alignment.enteredRecord = " << (engine.isRecording() ? "yes" : "no") << "\n";

    for (auto tick = 0; tick < 5; ++tick)
    {
        pump (1.0);
        const auto rt = engine.realtimeReport();
        std::cout << "  t+" << (tick + 1) << "s pos=" << engine.positionSeconds()
                  << " playing=" << (engine.isPlaying() ? 1 : 0)
                  << " recording=" << (engine.isRecording() ? 1 : 0)
                  << " block=" << rt.minimumBlockSize << ".." << rt.maximumBlockSize
                  << " in=" << rt.inputChannels << std::endl;
    }

    std::cout << "alignment.positionBeforeStop = " << engine.positionSeconds() << "\n";

    engine.stopRecording (false);

    for (auto waited = 0; waited < 30 && ! take.received; ++waited)
        pump (1.0);

    if (! take.received)
    {
        std::cout << "FAIL = no clip was produced\n";
        return 1;
    }

    const auto placementErrorMs = 1000.0 * (take.startSeconds - requestedStart);
    const auto analysis = analyse (take.file, 1.0e-5f);

    std::cout << "alignment.requestedStart = " << requestedStart << "\n";
    std::cout << "alignment.clipStart = " << take.startSeconds << "\n";
    std::cout << "alignment.placementErrorMs = " << placementErrorMs << "\n";
    std::cout << "alignment.file = " << take.file.getFullPathName().toStdString() << "\n";
    std::cout << "alignment.clipLength = " << take.lengthSeconds << "\n";
    std::cout << "alignment.recordedSeconds = " << analysis.lengthSeconds << "\n";
    std::cout << "alignment.peak = " << analysis.peak << "\n";

    // A clip placed correctly but holding nothing would pass a placement check
    // and fail a listener, so the take has to contain audio and has to be as
    // long as the clip says it is.
    const auto lengthErrorMs = 1000.0 * (analysis.lengthSeconds - take.lengthSeconds);
    std::cout << "alignment.clipVersusFileMs = " << lengthErrorMs << "\n";

    const auto placementOk = std::abs (placementErrorMs) <= 1.0
                          && analysis.valid
                          && analysis.peak > 0.0
                          && std::abs (lengthErrorMs) <= 1.0;
    std::cout << "alignment.placement = " << (placementOk ? "PASS" : "FAIL") << "\n";

    // Alignment is a round trip: something known leaves the outputs, comes back
    // in, and has to land where it started. Without the outputs wired back to
    // the inputs there is nothing to measure, so that half is reported as
    // unmeasured rather than folded into the result above.
    std::cout << "alignment.roundTrip = UNMEASURED (needs output looped back to input)\n";

    return placementOk ? 0 : 1;
}

/** Reports what is in a file that already exists.

    Used after killing a recording process: FR4 requires that a crash loses no
    audio, and the only way to know is to read back what reached the disk.
*/
int runAnalyse (const juce::File& file)
{
    const auto analysis = analyse (file, 1.0e-5f);

    std::cout << "analyse.file = " << file.getFullPathName().toStdString() << "\n";
    std::cout << "analyse.readable = " << (analysis.valid ? "yes" : "no") << "\n";

    if (! analysis.valid)
    {
        std::cout << "analyse.result = FAIL\n";
        return 1;
    }

    std::cout << "analyse.sampleRate = " << analysis.sampleRate << "\n";
    std::cout << "analyse.seconds = " << analysis.lengthSeconds << "\n";
    std::cout << "analyse.peak = " << analysis.peak << "\n";
    std::cout << "analyse.longestSilenceSeconds = "
              << (double) analysis.longestSilentRun / analysis.sampleRate << "\n";

    const auto passed = analysis.lengthSeconds > 0.0 && analysis.peak > 0.0;
    std::cout << "analyse.result = " << (passed ? "PASS" : "FAIL") << "\n";
    return passed ? 0 : 1;
}

/** Records for a while, then stops normally.

    The crash test kills this process partway through, so the destination is
    printed before any audio is written - after the kill there is nobody left
    to report it.
*/
int runRecord (saamveda::engine::EngineController& engine, saamveda::core::Session& session,
               double seconds)
{
    using saamveda::core::Session;

    const auto track = session.addTrack ("audio", "Crash");
    const auto trackId = track.getProperty (Session::idProperty()).toString();
    engine.synchronise (session);

    if (! engine.setTrackArmed (trackId, true))
    {
        std::cout << "FAIL = could not arm a track; no usable input\n";
        return 1;
    }

    engine.startRecording();
    pump (1.0);

    if (! engine.isRecording())
    {
        std::cout << "FAIL = transport did not enter record\n";
        return 1;
    }

    std::cout << "record.started = yes" << std::endl;
    pump (juce::jmax (0.0, seconds - 1.0));

    engine.stopRecording (false);
    pump (2.0);

    std::cout << "record.finished = yes" << std::endl;
    return 0;
}

/** Reads `--option value` and `--option=value` alike.

    juce::ArgumentList::getValueForOption handled neither form here - it
    returned an empty string and a 30-minute soak silently ran for one second -
    so the parsing is explicit rather than trusted.
*/
bool hasOption (const juce::StringArray& arguments, const juce::String& option)
{
    for (const auto& argument : arguments)
        if (argument == option || argument.startsWith (option + "="))
            return true;

    return false;
}

juce::String valueFor (const juce::StringArray& arguments, const juce::String& option)
{
    for (int i = 0; i < arguments.size(); ++i)
    {
        const auto& argument = arguments[i];

        if (argument == option)
            return i + 1 < arguments.size() ? arguments[i + 1] : juce::String();

        if (argument.startsWith (option + "="))
            return argument.fromFirstOccurrenceOf ("=", false, false);
    }

    return {};
}

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    EchoingLogger logger;
    engineLogger = &logger;
    juce::Logger::setCurrentLogger (&logger);

    juce::StringArray arguments;
    for (int i = 1; i < argc; ++i)
        arguments.add (juce::String::fromUTF8 (argv[i]));

    const auto result = [&]
    {
        // Reading a file back needs no audio device, so it runs before one is
        // opened - the crash test analyses a file while nothing else is live.
        if (hasOption (arguments, "--analyse"))
            return runAnalyse (juce::File::getCurrentWorkingDirectory()
                                   .getChildFile (valueFor (arguments, "--analyse")));

        saamveda::core::Session session;
        saamveda::engine::EngineController engine;

        waitForDeviceScan();
        listDevices (engine);

        if (hasOption (arguments, "--soak"))
            return runSoak (engine, session, valueFor (arguments, "--soak").getDoubleValue());

        if (hasOption (arguments, "--record"))
            return runRecord (engine, session, valueFor (arguments, "--record").getDoubleValue());

        if (hasOption (arguments, "--alignment"))
            return runAlignment (engine, session, valueFor (arguments, "--from"));

        std::cout << "\nusage: RecordingCheck --soak <minutes> | --alignment"
                     " | --record <seconds> | --analyse <file>\n";
        return 0;
    }();

    juce::Logger::setCurrentLogger (nullptr);
    engineLogger = nullptr;
    return result;
}
