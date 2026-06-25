#include "rewriteXM.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include <common/util.h>

#include <opus/opus.h>

enum {
    OPUS_MIN_SAMPLES = 120,
};

typedef struct {
    char magic[17]; // "Extended Module: "
    char name[20];
    char unknown; // Always 0x1A
    char trackerName[20];
    uint8_t versionMajor; // Latest is 1.4
    uint8_t versionMinor;
    uint32_t headerSize;

    uint16_t songLength;
    uint16_t songRestartPos;
    uint16_t channelCount;
    uint16_t patternCount; // Max = 256
    uint16_t instrumentCount; // Max = 128
    uint16_t flags;
    uint16_t defaultTempo;
    uint16_t defaultBPM;
    uint8_t patternOrderTable[256];
}XMHeader;
static_assert(sizeof(XMHeader) == 336, "Wrong XM header size!");


// Disable struct padding, otherwise our structs won't match the file and
// loading will break.
#pragma pack(push, r1, 1)
typedef struct {
    uint32_t headerSize;
    uint8_t packingType; // Always 0
    uint16_t rowCount; // Max = 256
    uint16_t patternDataSize;
    uint8_t patternData[];
}XMPattern;
static_assert(sizeof(XMPattern) == 9, "Wrong XM pattern size!");

typedef struct {
    uint32_t headerSize;
    char name[22];
    uint8_t type; // Supposed to be zero, but isn't in practice
    uint16_t sampleCount; // If this isn't 0, XMSampleSettings comes next
}XMInstrument;
static_assert(sizeof(XMInstrument) == 29, "Wrong XM instrument size!");

typedef struct {
    uint16_t frameNum;
    uint16_t value;
}XMEnvelopePoint;

typedef struct {
    uint32_t headerSize;
    uint8_t samplesPerNote[96];
    uint8_t volumeEnvelopePoints[48];
    uint8_t panningEnvelopePoints[48];
    uint8_t volumeEnvelopePointCount;
    uint8_t panningEnvelopePointCount;
    uint8_t volumeSustainPoint;
    uint8_t volumeLoopStart;
    uint8_t volumeLoopEnd;
    uint8_t panningSustainPoint;
    uint8_t panningLoopStart;
    uint8_t panningLoopEnd;
    uint8_t volumeType;
    uint8_t panningType;
    uint8_t vibratoType;
    uint8_t vibratoSweep;
    uint8_t vibratoDepth;
    uint8_t vibratoRate;
    uint16_t volumeFade;
    uint16_t reserved;
    XMEnvelopePoint volumeEnvelope[12];
    XMEnvelopePoint panningEnvelope[12];
}XMSampleSettings;
static_assert(sizeof(XMSampleSettings) == 310, "Wrong XM instrument sample settings size!");
#pragma pack(pop, r1)

typedef struct {
    uint32_t length;
    uint32_t loopStart;
    uint32_t loopLength;
    uint8_t volume;
    uint8_t fineTune;
    uint8_t sampleType; // e.g. loop, ping-pong
    uint8_t panning;
    uint8_t relativeNoteNum;
    uint8_t reserved;
    char sampleName[22];
}XMSampleHeader;
static_assert(sizeof(XMSampleHeader) == 40, "Wrong XM sample header size!");

void writeOpusSampleXM(FILE* f, void* data, uint32_t size) {
    void* opusBuf = malloc(size);
    if (!opusBuf) {
        printf("Failed to allocate Opus output buffer!\n");
        return;
    }

    int error;
    OpusEncoder* enc = opus_encoder_create(48000, 1, OPUS_APPLICATION_AUDIO, &error);
    if (!enc) {
        printf("Failed to create encoder!\n");
        free(opusBuf);
        return;
    }

    uint32_t totalFrames = ALIGN_UP(size / sizeof(uint16_t), OPUS_MIN_SAMPLES);
    printf("Encoding %d frames...\n", totalFrames);

    opus_int32 encodedLen = 0;
    while (totalFrames > 480) {
        printf("Encoding 480 frames (%d remaining, %d total)\n", totalFrames, encodedLen);
        opus_int32 len = opus_encode(enc, data, 480, opusBuf, size);
        if (len > 0) {
            encodedLen += len;
            totalFrames -= 480;
        } else {
            break;
        }
    }

    while (totalFrames > 0) {
        printf("Encoding 120 frames (%d remaining, %d total)\n", totalFrames, encodedLen);
        opus_int32 len = opus_encode(enc, data, 120, opusBuf, size);
        if (len > 0) {
            encodedLen += len;
            totalFrames -= 120;
        } else {
            break;
        }
    }

    if (encodedLen <= 0) {
        printf("Failed to encode Opus audio!\n");
    } else {
    }

    printf("Encoded %d bytes PCM to %d bytes Opus data\n", size, encodedLen);
    fwrite(opusBuf, encodedLen, 1, f);

    if (size > 60000) {
        FILE* debugOut = fopen("sample.opus", "wb");
        fwrite(opusBuf, encodedLen, 1, debugOut);
        fclose(debugOut);
    }

    opus_encoder_destroy(enc);
    free(opusBuf);
}

bool rewriteXM(const char* inpath, const char* outpath) {
    FILE* in = fopen(inpath, "rb");
    FILE* out = fopen(outpath, "wb");
    if (!in || !out) {
        fclose(in);
        fclose(out);
        printf("Failed to open one of the files!\n");
        return false;
    }

    XMHeader header = {0};
    fread(&header, sizeof(header), 1, in);
    fwrite(&header, sizeof(header), 1, out);

    for (uint32_t i = 0; i < header.patternCount; i++) {
        XMPattern pattern = {0};
        const uint64_t pos = ftell(in);
        fread(&pattern, sizeof(pattern), 1, in);
        fwrite(&pattern, sizeof(pattern), 1, out);
        if (pattern.patternDataSize == 0) {
            continue;
        }

        void* buf = malloc(pattern.patternDataSize);
        if (!buf) {
            printf("Failed to alloc %d bytes for pattern data!\n", pattern.patternDataSize);
            continue;
        }
        fread(buf, pattern.patternDataSize, 1, in);
        fwrite(buf, pattern.patternDataSize, 1, out);
        free(buf);

        const uint64_t nextOffset = pos + pattern.headerSize + pattern.patternDataSize;
        fseek(in, nextOffset, SEEK_SET);
    }

    for (uint32_t i = 0; i < header.instrumentCount; i++) {
        const uint64_t pos = ftell(in);
        XMInstrument instr = {0};
        fread(&instr, sizeof(instr), 1, in);
        fwrite(&instr, sizeof(instr), 1, out);

        const uint64_t nextOffset = pos + instr.headerSize;
        if (instr.sampleCount == 0) {
            fseek(in, nextOffset, SEEK_SET);
            fseek(out, nextOffset, SEEK_SET);
            continue; // No other data to copy
        }

        XMSampleSettings ss;
        fread(&ss, sizeof(ss), 1, in);
        fwrite(&ss, sizeof(ss), 1, out);

        fseek(in, nextOffset, SEEK_SET);
        fseek(out, nextOffset, SEEK_SET);

        XMSampleHeader* samples = malloc(sizeof(*samples) * instr.sampleCount);
        if (!samples) {
            printf("Failed to alloc for %d samples\n", instr.sampleCount);
            continue;
        }
        fread(samples, sizeof(*samples), instr.sampleCount, in);
        fwrite(samples, sizeof(*samples), instr.sampleCount, out);

        for (uint32_t j = 0; j < instr.sampleCount; j++) {
            XMSampleHeader sample = samples[j];

            void* buf = malloc(ALIGN_UP(sample.length, OPUS_MIN_SAMPLES));
            if (!buf) {
                printf("Failed to alloc %d bytes for sample %d in instrument %d!\n", sample.length, j, i);
                continue;
            }
            fread(buf, sample.length, 1, in);
            fwrite(buf, sample.length, 1, out);
            // writeOpusSampleXM(out, buf, sample.length);

            printf("Copied instrument %d's sample %d data (%d bytes)\n", i, j, sample.length);
            free(buf);
        }
        free(samples);
    }

    fclose(in);
    fclose(out);
    return true;
}
