#include "rewriteXM.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include <common/util.h>

#include <opus/opus.h>
#include "opusenc_stdio.h"
#include "xm.h"

enum {
    OPUS_MIN_SAMPLES = 120,
};

bool writeOpusSampleXM(FILE* f, void* data, uint32_t size) {
    void* opusBuf = malloc(size);
    if (!opusBuf) {
        printf("Failed to allocate Opus output buffer!\n");
        return false;
    }

    int error;
    OggOpusComments* comments = ope_comments_create();
    OggOpusEnc* enc = ope_encoder_create_callbacks(&opus_stdio_impl, f, comments, 48000, 1, 0, &error);
    if (!enc) {
        printf("Failed to create encoder!\n");
        free(opusBuf);
        return false;
    }

    uint32_t totalFrames = ALIGN_UP(size / sizeof(uint16_t), OPUS_MIN_SAMPLES);
    printf("Encoding %d frames...\n", totalFrames);

    if (ope_encoder_write(enc, data, totalFrames) != 0) {
        printf("Failed to encode Opus audio!\n");
    }
    ope_encoder_drain(enc);
    ope_encoder_destroy(enc);
    ope_comments_destroy(comments);
    free(opusBuf);
    return true;
}

bool rewriteXM(const char* inpath, const char* outpath) {
    FILE* in = fopen(inpath, "rb");
    FILE* out = fopen(outpath, "wb");
    if (!in || !out) {
        if (in) {
            fclose(in);
        }
        if (out) {
            fclose(out);
        }
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

    uint32_t totalRawAudioSize = 0;
    uint32_t totalOpusSize = 0;
    for (uint32_t i = 0; i < header.instrumentCount; i++) {
        const uint64_t inPos = ftell(in);
        const uint64_t outPos = ftell(out);
        XMInstrument instr = {0};
        fread(&instr, sizeof(instr), 1, in);
        fwrite(&instr, sizeof(instr), 1, out);

        const uint64_t nextOffsetIn = inPos + instr.headerSize;
        const uint64_t nextOffsetOut = outPos + instr.headerSize;
        if (instr.sampleCount == 0) {
            fseek(in, nextOffsetIn, SEEK_SET);
            fseek(out, nextOffsetOut, SEEK_SET);
            continue; // No other data to copy
        }

        XMSampleSettings ss;
        fread(&ss, sizeof(ss), 1, in);
        fwrite(&ss, sizeof(ss), 1, out);

        fseek(in, nextOffsetIn, SEEK_SET);
        fseek(out, nextOffsetOut, SEEK_SET);

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
            const size_t sampleStart = ftell(out);

            // fwrite(buf, sample.length, 1, out);

            if (!writeOpusSampleXM(out, buf, sample.length)) {
                printf("Failed to encode sample %d\n", j);
                continue;
            } else {
                printf("Wrote OGG sample @ 0x%lX\n", sampleStart);
            }
            const size_t sampleEnd = ftell(out);
            const uint32_t opusSize = (sampleEnd - sampleStart);
            totalOpusSize += opusSize;
            totalRawAudioSize += sample.length;

            printf("Copied instrument %d's sample %d data (%d bytes raw, %d bytes compressed)\n", i, j, sample.length, opusSize);
            free(buf);
        }
        free(samples);
    }
    printf("Converted %d bytes raw audio to %d bytes Opus audio\n", totalRawAudioSize, totalOpusSize);

    fclose(in);
    fclose(out);
    return true;
}
