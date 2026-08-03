#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <map>
#include <memory>

namespace saamveda::services
{

/** Background waveform thumbnails, one per source file.

    Thumbnails are read on juce::AudioThumbnailCache's own background thread and
    stored at a fixed source-samples-per-point resolution; AudioThumbnail then
    downsamples from that for whatever zoom the timeline is at, which is the
    multi-resolution behaviour docs/09-roadmap.md Phase 3 asks for. Nothing here
    touches the message thread except the change broadcast, and nothing touches
    the audio thread at all.

    Keyed by absolute path, so two clips pointing at the same file share one
    thumbnail and one read.
*/
class WaveformCache : public juce::ChangeBroadcaster,
                      private juce::ChangeListener
{
public:
    WaveformCache();
    ~WaveformCache() override;

    /** Returns the thumbnail for a file, starting a background read the first
        time it is asked for. Null only if the file cannot be opened at all. */
    juce::AudioThumbnail* thumbnailFor (const juce::File& file);

    /** Drops thumbnails for files no longer referenced by anything. */
    void retainOnly (const juce::StringArray& absolutePaths);

    int thumbnailCount() const { return static_cast<int> (thumbnails.size()); }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    /** One stored point per this many source samples. 512 keeps a ten-minute
        44.1 kHz file around 50k points - fast to scan, fine at full zoom. */
    static constexpr int samplesPerThumbnailSample = 512;

    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache cache { 128 };
    std::map<juce::String, std::unique_ptr<juce::AudioThumbnail>> thumbnails;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformCache)
};

} // namespace saamveda::services
