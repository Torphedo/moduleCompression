#include "rewriteXM.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include <common/util.h>

#include <opus/opus.h>
#include "xm.h"

enum {
    OPUS_MIN_SAMPLES = 120,
};

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
