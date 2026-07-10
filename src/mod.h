#pragma once
#include <stdint.h>
// All lengths and offsets are given in 2-byte Amiga words, so multiply them by 2 to get the
// length in bytes

enum {
    MOD_MAGIC_OFFSET = 1080,
};

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