#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <miniaudio.h>
#include "miniaudio_ibxm.h"
#include "miniaudio_it2play.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s [audio file]\n", argv[0]);
        return 1;
    }

    const char* path = argv[1];

    ma_decoding_backend_vtable* customDecoders[] = {
        ma_decoding_backend_ibxm,
        ma_decoding_backend_it2,
    };

    ma_decoder_config cfg = ma_decoder_config_init(ma_format_s16, 2, 48000);
    cfg.ppCustomBackendVTables = customDecoders;
    cfg.customBackendCount = sizeof(customDecoders) / sizeof(*customDecoders);
    cfg.pCustomBackendUserData = NULL;

    ma_decoder decoder = {};
    ma_result ma_res = ma_decoder_init_file(path, &cfg, &decoder);
    if (ma_res != MA_SUCCESS) {
        printf("Failed to setup audio decoder!\n");
        return false;
    }

}
