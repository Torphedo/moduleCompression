#include "rewriteXM.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <opus/opus.h>
#include <opusfile.h>
#include "opusenc_helpers.h"
#include "xm.h"
#include "pcm.h"
#include "VirtualIO.h"

bool writeOpusSampleXM(VirtualIO* io, const void* data, uint32_t size, bool is16Bit) {
    // The OGG container seems to have ~1KiB minimum overhead
    if (size < 1024) {
        io->write(io, data, size); // Just keep raw samples
        return true;
    }

    const uint8_t sampleSize = is16Bit ? 2 : 1;
    const uint32_t sampleCount = size / sampleSize;
    void* pcmData = malloc(sampleCount * sizeof(int16_t));
    if (!pcmData) {
        printf("Failed to allocate %d bytes for decoded PCM data\n", size);
        return false;
    }
    if (!is16Bit) {
        // Convert 8-bit delta-coded PCM to 16-bit raw PCM
        pcmDelta8to16(data, pcmData, sampleCount);
    } else {
        // Just convert to PCM
        pcmDeltaDecode16(data, pcmData, sampleCount);
    }

    if (!compressSampleToVIO(io, pcmData, sampleCount, 1)) {
        printf("Failed to Opus-encode %d samples\n", sampleCount);
        return false;
    }

    free(pcmData);
    return true;
}

bool decodeOpusSampleXM(VirtualIO* io, const void* data, uint32_t size, bool is16Bit) {
    int error;
    ogg_int64_t bufSize;
    void* buf = opus_read_entire_stream(data, size, &error, &bufSize);
    if (!buf) {
        if (error == OP_ENOTFORMAT) {
            // Not Opus, must be a tiny uncompressed sample
            io->write(io, data, size);
            return true;
        } else {
            printf("Failed to open Opus stream!\n");
            return false;
        }
    }
    const ogg_int64_t sampleCount = bufSize / sizeof(uint16_t);

    // Delta encode in-place
    if (!is16Bit) {
        pcmDeltaEncode16to8(buf, buf, sampleCount);
        bufSize /= 2;
    } else {
        pcmDeltaEncode16(buf, buf, sampleCount);
    }

    io->write(io, buf, bufSize);
    free(buf);
    return true;
}

XMHeader copyHeaderXM(VirtualIO* in, VirtualIO* out) {
    XMHeader header = {0};
    VIO_COPY_VALUE(in, out, header);

    // Copy the order table
    const uint32_t remaining = header.headerSize - (sizeof(header) - offsetof(XMHeader, headerSize));
    uint8_t patterns[256];
    in->read(in, &patterns, remaining);
    out->write(out, &patterns, remaining);

    return header;
}

void copyPatternsXM(VirtualIO* in, VirtualIO* out, uint16_t patternCount) {
    for (uint32_t i = 0; i < patternCount; i++) {
        XMPattern pattern = {0};
        const uint64_t pos = in->tell(in);
        in->read(in, &pattern, sizeof(pattern));
        out->write(out, &pattern, sizeof(pattern));
        if (pattern.patternDataSize == 0) {
            continue;
        }

        const void* buf = in->constRead(in, pattern.patternDataSize);
        if (!buf) {
            printf("Failed to read %d bytes of pattern data!\n", pattern.patternDataSize);
            continue;
        }

        out->write(out, buf, pattern.patternDataSize);
        in->free(in, buf);

        const uint64_t nextOffset = pos + pattern.headerSize + pattern.patternDataSize;
        in->seek(in, nextOffset);
    }

}

typedef bool (*SampleWriter)(VirtualIO* io, const void* data, uint32_t size, bool is16Bit);

void copyInstrumentsXM(VirtualIO* in, VirtualIO* out, uint32_t instrumentCount, SampleWriter writeSample) {
    uint32_t totalOriginalSize = 0;
    uint32_t totalOutSize = 0;
    for (uint32_t i = 0; i < instrumentCount; i++) {
        const uint64_t inPos = in->tell(in);
        const uint64_t outPos = out->tell(out);
        XMInstrument instr = {0};
        in->read(in, &instr, sizeof(instr));
        out->write(out, &instr, sizeof(instr));

        const uint64_t nextOffsetIn = inPos + instr.headerSize;
        const uint64_t nextOffsetOut = outPos + instr.headerSize;
        if (instr.sampleCount == 0) {
            in->seek(in, nextOffsetIn);
            out->seek(out, nextOffsetOut);
            continue; // No other data to copy
        }

        XMSampleSettings ss;
        in->read(in, &ss, sizeof(ss));
        out->write(out, &ss, sizeof(ss));

        in->seek(in, nextOffsetIn);
        out->seek(out, nextOffsetOut);

        XMSampleHeader* samples = malloc(sizeof(*samples) * instr.sampleCount);
        if (!samples) {
            printf("Failed to alloc for %d samples\n", instr.sampleCount);
            continue;
        }
        const uint32_t sampleArrayPos = out->tell(out);
        const uint32_t sampleArraySize = sizeof(*samples) * instr.sampleCount;
        in->read(in, samples, sampleArraySize);
        out->seek(out, sampleArrayPos + sampleArraySize);

        // We don't know the size of the samples until we compress/decompress
        // them, so we need to write all the sample data first.
        for (uint32_t j = 0; j < instr.sampleCount; j++) {
            XMSampleHeader* sample = &samples[j];

            const void* buf = in->constRead(in, sample->length);
            if (!buf) {
                printf("Failed to read %d bytes from sample %d in instrument %d!\n", sample->length, j, i);
                continue;
            }
            const size_t sampleStart = out->tell(out);

            if (!(writeSample)(out, buf, sample->length, XMSampleIs16Bit(*sample))) {
                printf("Failed to write sample %d\n", j);
                continue;
            }
            in->free(in, buf);

            const size_t sampleEnd = out->tell(out);
            const uint32_t outSize = (sampleEnd - sampleStart);
            printf("Copied instrument %d's sample %d data (%d bytes in, %d bytes out)\n", i, j, sample->length, outSize);

            totalOutSize += outSize;
            totalOriginalSize += sample->length;
            sample->length = outSize; // Size should be that of the written data
        }

        // Go back and write the sample headers
        const uint32_t nextPos = out->tell(out);
        out->seek(out, sampleArrayPos);
        out->write(out, samples, sampleArraySize);
        out->seek(out, nextPos); // Return to where we left off
        free(samples);
    }
    printf("Converted %d bytes input audio to %d bytes output audio\n", totalOriginalSize, totalOutSize);
}

bool writeOpusXM(VirtualIO* in, VirtualIO* out) {
    XMHeader header = copyHeaderXM(in, out);
    copyPatternsXM(in, out, header.patternCount);
    copyInstrumentsXM(in, out, header.instrumentCount, writeOpusSampleXM);
    return true;
}

bool decodeOpusXM(VirtualIO* in, VirtualIO* out) {
    XMHeader header = copyHeaderXM(in, out);
    copyPatternsXM(in, out, header.patternCount);
    copyInstrumentsXM(in, out, header.instrumentCount, decodeOpusSampleXM);
    return true;
}
