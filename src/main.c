#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <miniaudio.h>
#include <miniaudio_libvorbis.h>
#include <miniaudio_libopus.h>
#include "miniaudio_ibxm.h"
#include "miniaudio_it2play.h"

#include <common/path.h>
#include "rewriteXM.h"

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    ma_decoder* dec = pDevice->pUserData;

    ma_uint64 framesRead = 0;
    ma_decoder_read_pcm_frames(dec, pOutput, frameCount, &framesRead);
}

void playFile(const char* path) {
    ma_decoding_backend_vtable* customDecoders[] = {
        ma_decoding_backend_libvorbis,
        ma_decoding_backend_libopus,
        ma_decoding_backend_ibxm,
        ma_decoding_backend_it2,
    };

    const uint32_t sampleRate = 44100;

    ma_decoder_config cfg = ma_decoder_config_init(ma_format_s16, 2, sampleRate);
    cfg.ppCustomBackendVTables = customDecoders;
    cfg.customBackendCount = sizeof(customDecoders) / sizeof(*customDecoders);
    cfg.pCustomBackendUserData = NULL;

    ma_decoder decoder = {};
    ma_result ma_res = ma_decoder_init_file(path, &cfg, &decoder);
    if (ma_res != MA_SUCCESS) {
        printf("Failed to setup audio decoder!\n");
        return;
    }

    ma_device device;
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_s16;  // Set to ma_format_unknown to use the device's native format.
    config.playback.channels = 2;              // Set to 0 to use the device's native channel count.
    config.sampleRate        = sampleRate;
    config.dataCallback      = data_callback; // This function will be called when miniaudio needs more data.
    config.pUserData         = &decoder;        // Can be accessed from the device object (device.pUserData).

    ma_device_init(NULL, &config, &device);

    ma_device_start(&device);
    while (1) {
    }

    ma_decoder_uninit(&decoder);
}

bool rewriteModule(const char* inpath, const char* outpath) {
    if (path_has_extension(inpath, "xm") || path_has_extension(inpath, "XM")) {
        return rewriteXM(inpath, outpath);
    }

}

int main(int argc, char** argv) {
    if (argc == 2) {
        const char* path = argv[1];
        playFile(path);
    } else if (argc == 3) {
        const char* inpath = argv[1];
        const char* outpath = argv[2];
        rewriteModule(inpath, outpath);
    } else {
        printf("Usage: %s [audio file]\n", argv[0]);
        return 1;
    }

}
