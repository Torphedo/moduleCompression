#pragma once
#include <stdlib.h>
#include <opusenc.h>
#include <opusfile.h>
#include "VirtualIO.h"

static int opus_vio_write(void *user_data, const unsigned char *ptr, opus_int32 len) {
    VirtualIO* io = user_data;
    if (io->write(io, ptr, len)) {
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

static int opus_vio_close(void *user_data) {
    return EXIT_SUCCESS;
}

// Callbacks for ope_encoder_create_callbacks() to write to a VirtualIO context
// handle. Pass the file handle as your user data.
static const OpusEncCallbacks opus_vio_impl = {
    opus_vio_write, opus_vio_close,
};

static bool compressSampleToVIO(VirtualIO* io, const void* pcm, uint32_t sampleCount, uint8_t channels) {
    OggOpusComments* comments = ope_comments_create();
    if (!comments) {
        return false;
    }
    OggOpusEnc* enc = ope_encoder_create_callbacks(&opus_vio_impl, io, comments, 48000, channels, 0, NULL);
    if (!enc) {
        ope_comments_destroy(comments);
        return false;
    }

    ope_encoder_write(enc, pcm, sampleCount);
    ope_encoder_drain(enc);
    ope_encoder_destroy(enc);
    ope_comments_destroy(comments);
    return true;
}

static void* opus_read_entire_stream(const void* data, uint32_t size, int* errorOut, ogg_int64_t* bufSizeOut) {
    OggOpusFile* file = op_open_memory(data, size, errorOut);
    if (!file) {
        return NULL;
    }

    const int sampleCount = op_pcm_total(file, -1);
    if (sampleCount < 0) {
        printf("Failed to get sample count of Opus stream!\n");
        op_free(file);
        return NULL;
    }
    *bufSizeOut = sampleCount * sizeof(uint16_t);
    void* buf = calloc(1, *bufSizeOut);
    if (!buf) {
        printf("Failed to allocate %ld bytes for uncompressed audio!\n", *bufSizeOut);
        op_free(file);
        return NULL;
    }

    int16_t* bufpos = buf;
    int totalSamplesRead = 0;
    while (totalSamplesRead < sampleCount) {
        const int samplesRead = op_read(file, bufpos, sampleCount, NULL);
        if (samplesRead < 0) {
            printf("Failed to decode Opus stream!\n");
            free(buf);
            op_free(file);
            return NULL;
        }
        totalSamplesRead += samplesRead;
        bufpos += samplesRead;
    }

    return buf;
}