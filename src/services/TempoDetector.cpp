#include "TempoDetector.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace saamveda::services
{

TempoDetectionResult detectTempo (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        return {};

    constexpr int hopSize = 1024;
    constexpr double maxAnalysisSeconds = 120.0;
    const auto samplesToAnalyse = juce::jmin (
        reader->lengthInSamples,
        static_cast<juce::int64> (reader->sampleRate * maxAnalysisSeconds));
    const auto frameCount = static_cast<int> (samplesToAnalyse / hopSize);
    if (frameCount < 32)
        return {};

    juce::AudioBuffer<float> buffer (static_cast<int> (juce::jmin (reader->numChannels, 2u)), hopSize);
    std::vector<double> onset (static_cast<size_t> (frameCount), 0.0);
    double previousEnergy = 0.0;

    for (int frame = 0; frame < frameCount; ++frame)
    {
        reader->read (&buffer, 0, hopSize, static_cast<juce::int64> (frame) * hopSize, true, true);
        double energy = 0.0;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);
            for (int sample = 0; sample < hopSize; ++sample)
                energy += std::abs (data[sample]);
        }
        energy /= hopSize * buffer.getNumChannels();
        onset[static_cast<size_t> (frame)] = juce::jmax (0.0, energy - previousEnergy);
        previousEnergy = energy;
    }

    const auto mean = std::accumulate (onset.begin(), onset.end(), 0.0) / onset.size();
    for (auto& value : onset)
        value = juce::jmax (0.0, value - mean);

    constexpr int minBpm = 60;
    constexpr int maxBpm = 200;
    double bestScore = 0.0;
    double secondScore = 0.0;
    double bestBpm = 0.0;

    for (int bpmTimesTwo = minBpm * 2; bpmTimesTwo <= maxBpm * 2; ++bpmTimesTwo)
    {
        const auto bpm = bpmTimesTwo / 2.0;
        const auto lag = static_cast<int> (std::lround (60.0 * reader->sampleRate / (bpm * hopSize)));
        if (lag <= 0 || lag >= frameCount)
            continue;

        double score = 0.0;
        for (int i = lag; i < frameCount; ++i)
            score += onset[static_cast<size_t> (i)] * onset[static_cast<size_t> (i - lag)];

        // Favor the practical centre slightly to reduce half/double-tempo errors.
        score *= std::exp (-std::abs (std::log2 (bpm / 120.0)) * 0.12);
        if (score > bestScore)
        {
            secondScore = bestScore;
            bestScore = score;
            bestBpm = bpm;
        }
        else if (score > secondScore)
        {
            secondScore = score;
        }
    }

    if (bestScore <= 0.0)
        return {};

    // Tempo fixes the beat *rate*; this finds the beat *phase*. Without it the
    // click runs at the right speed but lands between the song's beats.
    const auto beatFrames = 60.0 * reader->sampleRate / (bestBpm * hopSize);
    const auto searchFrames = juce::jmax (1, static_cast<int> (std::lround (beatFrames)));
    double bestPhaseScore = -1.0;
    int bestPhase = 0;

    for (int phase = 0; phase < searchFrames; ++phase)
    {
        double score = 0.0;
        for (double position = phase; position < frameCount; position += beatFrames)
        {
            const auto index = static_cast<int> (std::lround (position));
            if (index >= 0 && index < frameCount)
                score += onset[static_cast<size_t> (index)];
        }

        if (score > bestPhaseScore)
        {
            bestPhaseScore = score;
            bestPhase = phase;
        }
    }

    const auto firstBeatSeconds = bestPhase * hopSize / reader->sampleRate;

    return { bestBpm,
             juce::jlimit (0.0, 1.0, (bestScore - secondScore) / bestScore),
             firstBeatSeconds };
}

} // namespace saamveda::services
