#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace saamveda::services
{

struct TempoDetectionResult
{
    double bpm = 0.0;
    double confidence = 0.0;

    /** Seconds from the start of the file to the first detected beat.

        Tempo alone only fixes the click *rate*; without this the metronome runs
        at the right speed but lands between the song's beats.
    */
    double firstBeatSeconds = 0.0;
};

/** Estimates tempo from an onset-strength envelope.

    This intentionally returns a confidence score: arbitrary recordings can be
    rubato, silent, or rhythmically ambiguous, so an estimate must never be
    presented as ground truth.
*/
TempoDetectionResult detectTempo (const juce::File& file);

} // namespace saamveda::services
