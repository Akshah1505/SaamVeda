#include "WaveformCache.h"

namespace saamveda::services
{

WaveformCache::WaveformCache()
{
    formatManager.registerBasicFormats();
}

WaveformCache::~WaveformCache()
{
    for (auto& entry : thumbnails)
        entry.second->removeChangeListener (this);

    thumbnails.clear();
}

juce::AudioThumbnail* WaveformCache::thumbnailFor (const juce::File& file)
{
    if (! file.existsAsFile())
        return nullptr;

    const auto key = file.getFullPathName();
    const auto existing = thumbnails.find (key);
    if (existing != thumbnails.end())
        return existing->second.get();

    auto thumbnail = std::make_unique<juce::AudioThumbnail> (samplesPerThumbnailSample,
                                                             formatManager, cache);
    thumbnail->addChangeListener (this);

    // Hands the read to the cache's background thread and returns immediately,
    // so the first paint after an import is not blocked on disk.
    thumbnail->setSource (new juce::FileInputSource (file));

    auto* raw = thumbnail.get();
    thumbnails.emplace (key, std::move (thumbnail));
    return raw;
}

void WaveformCache::retainOnly (const juce::StringArray& absolutePaths)
{
    for (auto it = thumbnails.begin(); it != thumbnails.end();)
    {
        if (absolutePaths.contains (it->first))
        {
            ++it;
            continue;
        }

        it->second->removeChangeListener (this);
        it = thumbnails.erase (it);
    }
}

void WaveformCache::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // AudioThumbnail broadcasts as more of the file is scanned. Forwarding it
    // as one signal lets the timeline simply repaint; ChangeBroadcaster already
    // delivers on the message thread.
    sendChangeMessage();
}

} // namespace saamveda::services
