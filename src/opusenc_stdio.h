#pragma once
// Callbacks for ope_encoder_create_callbacks() to write to a VirtualIO context
// handle. Pass the file handle as your user data.

#include <opusenc.h>
#include <stdio.h>
#include <stdlib.h>
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

static const OpusEncCallbacks opus_vio_impl = {
    opus_vio_write, opus_vio_close,
};
