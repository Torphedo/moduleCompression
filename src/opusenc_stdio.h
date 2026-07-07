#pragma once
// Callbacks for ope_encoder_create_callbacks() to write to an existing FILE*
// handle. Pass the file handle as your user data.

#include <opusenc.h>
#include <stdio.h>
#include <stdlib.h>

static int opus_stdio_write(void *user_data, const unsigned char *ptr, opus_int32 len) {
    if (fwrite(ptr, len, 1, user_data) != 1) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int opus_stdio_close(void *user_data) {
    return EXIT_SUCCESS;
}

static const OpusEncCallbacks opus_stdio_impl = {
    opus_stdio_write, opus_stdio_close,
};
