#include "RecordingRecovery.h"

namespace saamveda::services
{

namespace
{
    struct WaveLayout
    {
        bool valid = false;
        juce::int64 dataSizeFieldOffset = 0;  ///< where the data chunk's length is stored
        juce::int64 dataStart = 0;            ///< first audio byte
        juce::int64 declaredBytes = 0;
        int bytesPerFrame = 0;
        int sampleRate = 0;
    };

    /** Walks the RIFF chunk list far enough to find `fmt ` and `data`. */
    WaveLayout readLayout (juce::FileInputStream& stream)
    {
        WaveLayout layout;

        if (stream.readInt() != static_cast<int> (juce::ByteOrder::littleEndianInt ("RIFF")))
            return layout;

        stream.readInt(); // RIFF size, deliberately not trusted

        if (stream.readInt() != static_cast<int> (juce::ByteOrder::littleEndianInt ("WAVE")))
            return layout;

        const auto fileLength = stream.getTotalLength();

        while (stream.getPosition() + 8 <= fileLength)
        {
            const auto chunkId = stream.readInt();
            const auto chunkSize = static_cast<juce::int64> (static_cast<juce::uint32> (stream.readInt()));
            const auto chunkStart = stream.getPosition();

            if (chunkId == static_cast<int> (juce::ByteOrder::littleEndianInt ("fmt ")))
            {
                stream.skipNextBytes (4);              // format tag, channel count
                layout.sampleRate = stream.readInt();
                stream.skipNextBytes (4);              // byte rate
                layout.bytesPerFrame = stream.readShort();
            }
            else if (chunkId == static_cast<int> (juce::ByteOrder::littleEndianInt ("data")))
            {
                layout.dataSizeFieldOffset = chunkStart - 4;
                layout.dataStart = chunkStart;
                layout.declaredBytes = chunkSize;
                layout.valid = layout.bytesPerFrame > 0;
                return layout;
            }

            // Chunks are word-aligned, so an odd size is followed by a pad byte.
            stream.setPosition (chunkStart + chunkSize + (chunkSize & 1));
        }

        return layout;
    }

    /** True when the bytes at `offset` look like the start of a real chunk.

        A cleanly closed file can carry chunks after `data`, and in that case the
        space beyond the declared audio is not unwritten audio - rewriting the
        length would swallow them. Audio samples that happen to spell four
        printable characters make this say yes when it should say no, which
        costs a recovery that was possible. That is the right way round.
    */
    bool looksLikeAChunkHeader (juce::FileInputStream& stream, juce::int64 offset)
    {
        if (offset + 8 > stream.getTotalLength())
            return false;

        stream.setPosition (offset);

        char id[4] {};
        if (stream.read (id, 4) != 4)
            return false;

        for (const auto character : id)
            if (character < 32 || character > 126)
                return false;

        return true;
    }

    bool patchLengths (const juce::File& file, const WaveLayout& layout, juce::int64 audioBytes)
    {
        juce::FileOutputStream out (file);

        if (! out.openedOk())
            return false;

        out.setPosition (layout.dataSizeFieldOffset);
        out.writeInt (static_cast<int> (static_cast<juce::uint32> (audioBytes)));

        // The RIFF size covers everything after its own 8-byte header.
        out.setPosition (4);
        out.writeInt (static_cast<int> (static_cast<juce::uint32> (layout.dataStart + audioBytes - 8)));

        return out.getStatus().wasOk();
    }
}

juce::Array<RecoveredRecording> recoverInterruptedRecordings (const juce::File& folder)
{
    juce::Array<RecoveredRecording> recovered;

    if (! folder.isDirectory())
        return recovered;

    for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*.wav",
                                                            juce::File::findFiles))
    {
        const auto file = entry.getFile();

        WaveLayout layout;
        juce::int64 audioBytes = 0;

        {
            juce::FileInputStream in (file);

            if (! in.openedOk())
                continue;

            layout = readLayout (in);

            if (! layout.valid)
                continue;

            // Whole frames only. A kill mid-frame leaves a partial one behind.
            audioBytes = in.getTotalLength() - layout.dataStart;
            audioBytes -= audioBytes % layout.bytesPerFrame;

            if (audioBytes <= layout.declaredBytes)
                continue;

            const auto trailing = layout.dataStart + layout.declaredBytes + (layout.declaredBytes & 1);

            if (looksLikeAChunkHeader (in, trailing))
                continue;
        }

        // The stream is closed before writing: the same file cannot be open for
        // reading and writing at once on Windows.
        if (! patchLengths (file, layout, audioBytes))
            continue;

        const auto gainedFrames = (audioBytes - layout.declaredBytes) / layout.bytesPerFrame;
        recovered.add ({ file, layout.sampleRate > 0
                                   ? static_cast<double> (gainedFrames) / layout.sampleRate : 0.0 });
    }

    return recovered;
}

} // namespace saamveda::services
