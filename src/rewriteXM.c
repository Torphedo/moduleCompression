#include "rewriteXM.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <common/util.h>

#include <opus/opus.h>
#include "opusenc_stdio.h"
#include "xm.h"
#include "VirtualIO.h"

enum {
    OPUS_MIN_SAMPLES = 120,
};

bool writeOpusSampleXM(VirtualIO* io, const void* data, uint32_t size) {
    void* opusBuf = malloc(size);
    if (!opusBuf) {
        printf("Failed to allocate Opus output buffer!\n");
        return false;
    }

    int error;
    OggOpusComments* comments = ope_comments_create();
    OggOpusEnc* enc = ope_encoder_create_callbacks(&opus_vio_impl, io, comments, 48000, 1, 0, &error);
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

bool writeOpusXM(const char* inpath, const char* outpath) {
    VirtualIO in = vioOpenPath(inpath, false);
    VirtualIO out = vioOpenPath(outpath, true);
    if (!in.ctx || !out.ctx) {
        in.close(&in);
        out.close(&out);
        printf("Failed to open one of the files!\n");
        return false;
    }

    XMHeader header = {0};
    in.read(&in, &header, sizeof(header));
    out.write(&out, &header, sizeof(header));

    for (uint32_t i = 0; i < header.patternCount; i++) {
        XMPattern pattern = {0};
        const uint64_t pos = in.tell(&in);
        in.read(&in, &pattern, sizeof(pattern));
        out.write(&out, &pattern, sizeof(pattern));
        if (pattern.patternDataSize == 0) {
            continue;
        }

        const void* buf = in.constRead(&in, pattern.patternDataSize);
        if (!buf) {
            printf("Failed to read %d bytes of pattern data!\n", pattern.patternDataSize);
            continue;
        }

        out.write(&out, buf, pattern.patternDataSize);
        in.free(&in, buf);

        const uint64_t nextOffset = pos + pattern.headerSize + pattern.patternDataSize;
        in.seek(&in, nextOffset);
    }

    uint32_t totalRawAudioSize = 0;
    uint32_t totalOpusSize = 0;
    for (uint32_t i = 0; i < header.instrumentCount; i++) {
        const uint64_t inPos = in.tell(&in);
        const uint64_t outPos = out.tell(&out);
        XMInstrument instr = {0};
        in.read(&in, &instr, sizeof(instr));
        out.write(&out, &instr, sizeof(instr));

        const uint64_t nextOffsetIn = inPos + instr.headerSize;
        const uint64_t nextOffsetOut = outPos + instr.headerSize;
        if (instr.sampleCount == 0) {
            in.seek(&in, nextOffsetIn);
            out.seek(&out, nextOffsetOut);
            continue; // No other data to copy
        }

        XMSampleSettings ss;
        in.read(&in, &ss, sizeof(ss));
        out.write(&out, &ss, sizeof(ss));

        in.seek(&in, nextOffsetIn);
        out.seek(&out, nextOffsetOut);

        XMSampleHeader* samples = malloc(sizeof(*samples) * instr.sampleCount);
        if (!samples) {
            printf("Failed to alloc for %d samples\n", instr.sampleCount);
            continue;
        }
        const uint32_t sampleArrayPos = out.tell(&out);
        const uint32_t sampleArraySize = sizeof(*samples) * instr.sampleCount;
        in.read(&in, samples, sampleArraySize);
        out.seek(&out, sampleArrayPos + sampleArraySize);

        // We don't know the size of the samples until we compress them, so we
        // need to compress and write all the sample data first.
        for (uint32_t j = 0; j < instr.sampleCount; j++) {
            XMSampleHeader* sample = &samples[j];

            const void* buf = in.constRead(&in, sample->length);
            if (!buf) {
                printf("Failed to read %d bytes from sample %d in instrument %d!\n", sample->length, j, i);
                continue;
            }
            const size_t sampleStart = out.tell(&out);

            if (!writeOpusSampleXM(&out, buf, sample->length)) {
                printf("Failed to encode sample %d\n", j);
                continue;
            } else {
                printf("Wrote OGG sample @ 0x%lX\n", sampleStart);
            }

            in.free(&in, buf);

            const size_t sampleEnd = out.tell(&out);
            const uint32_t opusSize = (sampleEnd - sampleStart);
            printf("Copied instrument %d's sample %d data (%d bytes raw, %d bytes compressed)\n", i, j, sample->length, opusSize);

            totalOpusSize += opusSize;
            totalRawAudioSize += sample->length;
            sample->length = opusSize; // Size should be that of the compressed data
        }

        // Go back and write the sample headers
        const uint32_t nextPos = out.tell(&out);
        out.seek(&out, sampleArrayPos);
        out.write(&out, samples, sampleArraySize);
        out.seek(&out, nextPos); // Return to where we left off
        free(samples);
    }
    printf("Converted %d bytes raw audio to %d bytes Opus audio\n", totalRawAudioSize, totalOpusSize);

    in.close(&in);
    out.close(&out);
    return true;
}

bool decodeOpusXM(const char* inpath, const char* outpath) {
    return false;
}
