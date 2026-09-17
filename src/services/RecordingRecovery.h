#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace saamveda::services
{

struct RecoveredRecording
{
    juce::File file;
    double secondsRecovered = 0.0;
};

/** Repairs takes left behind when the application dies mid-recording.

    SRS-3.6. A WAV written by tracktion carries the length of its audio in the
    `data` chunk header, and that header is only rewritten periodically - when
    the process is killed, the samples already on disk past the last header
    update are unreachable. Measured at roughly three seconds out of twenty on
    this machine: the audio is there, the file simply does not admit to it.

    This scans a folder for files whose `data` chunk is shorter than the audio
    actually present and rewrites the two length fields to match. It is
    deliberately conservative: a file whose `data` chunk is followed by anything
    resembling another chunk is left alone, because in a cleanly closed file
    that trailing content is real and rewriting the length would destroy it.

    What it does not do is put the take back on the timeline. That needs project
    persistence, which is Phase 12; until then the file is recovered and the
    user re-imports it.
*/
juce::Array<RecoveredRecording> recoverInterruptedRecordings (const juce::File& folder);

} // namespace saamveda::services
