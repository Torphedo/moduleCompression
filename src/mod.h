#pragma once
#include <stdint.h>
// Structures for the Ultimate Sound Tracker / ProTracker MOD music format
// https://github.com/lclevy/unmo3/blob/master/spec/mod.txt
// https://pollak.thebe.de/b/the-mod-format/

// All lengths and offsets are given in 2-byte Amiga words, so multiply them by
// 2 to get the length in bytes

enum {
    MOD_MAGIC_OFFSET = 1080,
};

typedef enum {
    MOD_STD_15SAMPLE,
    MOD_STD_31SAMPLE,
}MODType;

static MODType getMODType(const char magic[4], uint8_t* channelsOut) {
    const bool mk31 = strncmp(magic, "M.K.", 4) == 0 || strncmp(magic, "M&K!", 4) == 0;
    const bool mk64 = strncmp(magic, "M!K!", 4) == 0;
    const bool xCHN = strncmp(&magic[1], "CHN", 3) == 0;
    const bool xxCH = strncmp(&magic[2], "CH", 2) == 0;
    const bool TDZx = strncmp(magic, "TDZ", 3) == 0;
    const bool FLTx = strncmp(magic, "FLT", 3) == 0;
    const bool is8chan = strncmp(magic, "CD81", 4) == 0 || strncmp(magic, "OCTA", 4) == 0 || strncmp(magic, "OKTA", 4) == 0;

    uint8_t channels = 4;
    MODType result = MOD_STD_31SAMPLE;
    if (mk31 || mk64) {
    } else if (xCHN) {
        channels = magic[0] - '0';
    } else if (xxCH) {
        channels = 10 * (magic[0] - '0') + (magic[1] - '0');
    } else if (TDZx || FLTx) {
        channels = (magic[3] - '0');
    } else if (is8chan) {
        channels = 8;
    } else {
        result = MOD_STD_15SAMPLE;
    }

    *channelsOut = channels;
    return result;
}

typedef struct {
    char name[22];
    uint16_t length;
    int8_t finetune;
    uint8_t volume;
    uint16_t repeatOffset;
    uint16_t repeatLength;
}MODSampleHeader;

typedef struct {
    uint8_t patternCount;
    uint8_t songEnd;
    uint8_t patternOrder[128];
    char magic[4];
}MODHeader;