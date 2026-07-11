#include "rewriteMOD.h"
#include <string.h>
#include <stdlib.h>
#include "VirtualIO.h"
#include "mod.h"
#include "pcm.h"
#include "endian.h"
#include "opusenc_helpers.h"

uint8_t getPatternCount(const uint8_t* patternTable) {
    // Find largest pattern index
    uint8_t result = 0;
    for (int i = 0; i < 128; i++) {
        if (patternTable[i] > result) {
            result = patternTable[i];
        }
    }
    return result + 1;
}

// Copy pattern data (each pattern is 256 bytes per channel)
void copyPatternsMOD(VirtualIO* in, VirtualIO* out, uint8_t channels, uint8_t numPatterns) {
    for (uint8_t i = 0; i < numPatterns; i++) {
        for (uint8_t j = 0; j < channels; j++) {
            uint8_t patternBuf[256] = {0};
            in->read(in, patternBuf, sizeof(patternBuf));
            out->write(out, patternBuf, sizeof(patternBuf));
        }
    }
}

bool writeOpusSampleMOD(VirtualIO* io, const void* data, uint32_t size) {
    // The OGG container seems to have ~1KiB minimum overhead
    if (size < 1024) {
        io->write(io, data, size); // Just keep raw samples
        return true;
    }

    const uint32_t sampleCount = size;
    void* pcmData = malloc(sampleCount * sizeof(int16_t));
    if (!pcmData) {
        printf("Failed to allocate %d bytes for decoded PCM data\n", size);
        return false;
    }

    // Convert to 16-bit PCM for Opus
    pcmU8to16(data, pcmData, sampleCount);

    if (!compressSampleToVIO(io, pcmData, sampleCount, 1)) {
        printf("Failed to Opus-encode %d samples\n", sampleCount);
        return false;
    }

    free(pcmData);
    return true;
}

bool decodeOpusSampleMOD(VirtualIO* io, const void* data, uint32_t size) {
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

    // Convert to 8-bit PCM in-place
    pcmU16to8(buf, buf, sampleCount);
    bufSize /= 2;

    io->write(io, buf, bufSize);
    free(buf);
    return true;
}


typedef bool (*SampleWriter)(VirtualIO* io, const void* data, uint32_t size);
bool copyMOD(VirtualIO* in, VirtualIO* out, SampleWriter writeSample) {
    char title[20];
    VIO_COPY_VALUE(in, out, title);

    char magic[4];
    VIO_DUAL_SEEK(in, out, MOD_MAGIC_OFFSET);
    in->read(in, magic, sizeof(magic));

    uint8_t channels;
    MODType type = getMODType(magic, &channels);
    const uint8_t sampleCount = (type == MOD_STD_15SAMPLE) ? 15 : 31;
    const uint32_t headerOffset = sizeof(title) + sampleCount * sizeof(MODSampleHeader);

    MODHeader header;
    VIO_DUAL_SEEK(in, out, headerOffset);
    VIO_COPY_VALUE(in, out, header);
    copyPatternsMOD(in, out, channels, getPatternCount(header.patternOrder));

    const uint32_t pcmOffset = in->tell(in);
    uint32_t nextOutSampleOffset = pcmOffset;
    uint32_t nextInSampleOffset = pcmOffset;
    uint32_t nextSampleHeaderPos = sizeof(title);

    for (uint32_t i = 0; i < sampleCount; i++) {
        MODSampleHeader sample;
        in->seek(in, nextSampleHeaderPos);
        in->read(in, &sample, sizeof(sample));

        uint16_t length = sample.length;
        ENDIAN_FLIP(uint16_t, length);
        length *= 2; // This is originally in 2-byte Amiga words

        in->seek(in, nextInSampleOffset);
        out->seek(out, nextOutSampleOffset);
        const void* sampleBuf = in->constRead(in, length);
        (writeSample)(out, sampleBuf, length);
        in->free(in, sampleBuf);

        // Force size to be even
        if (out->tell(out) % 2 != 0) {
            uint8_t blank = 0;
            out->write(out, &blank, sizeof(blank));
        }
        const uint32_t sampleEndOffset = out->tell(out);
        const uint32_t outSize = sampleEndOffset - nextOutSampleOffset;
        printf("Input: %d bytes @ 0x%X. Output: %d bytes @ 0x%X\n", length, nextInSampleOffset, outSize, nextOutSampleOffset);
        nextInSampleOffset += length;
        nextOutSampleOffset += outSize;


        // Update sample size
        sample.length = outSize / 2;
        ENDIAN_FLIP(uint16_t, sample.length);

        out->seek(out, nextSampleHeaderPos);
        out->write(out, &sample, sizeof(sample));
        nextSampleHeaderPos += sizeof(sample);
    }

    // Write samples
    return true;
}

bool writeOpusMOD(VirtualIO* in, VirtualIO* out) {
    return copyMOD(in, out, writeOpusSampleMOD);
}

bool decodeOpusMOD(VirtualIO* in, VirtualIO* out) {
    return copyMOD(in, out, decodeOpusSampleMOD);
}