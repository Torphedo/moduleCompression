#pragma once
#include <stdint.h>
#include <assert.h>
// Offsets are always given in units of 16 bytes.

typedef struct {
    char title[28];
    uint8_t signature; // Signature byte, always 0x1A
    uint8_t songType; // Always 0x10 for S3M
    uint16_t reserved; // Always 0
    uint16_t orderCount; // Should be even
    uint16_t instrumentCount;
    uint16_t patternPtrCount;
    uint16_t flags; // See below
    uint16_t trackerVersion;
    uint16_t sampleType; // 1=signed samples [deprecated], 2=unsigned samples
    char magic[4]; // "SCRM"
    uint8_t globalVolume;
    uint8_t initialSpeed;
    uint8_t initialTempo;
    uint8_t masterVolume; // bit 7: 1=stereo, 0=mono, bits 6-0: volume
    uint8_t ultraClickRemoval;
    uint8_t defaultPan;
    uint8_t reserved2[8]; // Unused, some trackers store data here
    uint16_t ptrSpecial; // Parapointer to additional data, if <tt>flags</tt> has bit 7 set
    uint8_t channelSettings[32];
    uint8_t orderList[];
    // uint16_t ptrInstruments[instrumentCount]; // List of parapointers to each instrument's data
    // uint16_t ptrPatterns[patternPtrCount]; // List of parapointers to each pattern's data
}S3MHeader;

enum {
    S3M_INSTR_EMPTY = 0,
    S3M_INSTR_PCM = 1,
    // All other instrument types are synths

    S3M_PCM_INSTR_FLAG_LOOP = 1,
    S3M_PCM_INSTR_FLAG_STEREO = 2,
    S3M_PCM_INSTR_FLAG_16BIT = 4,
};

// Disable struct padding, otherwise our structs won't match the file and
// loading will break.
#pragma pack(push, r1, 1)
typedef struct {
    uint8_t samplePtrHigh; // This is a 24-bit pointer split into 2 values
    uint16_t samplePtrLow;
    uint32_t lengthBytes;
    uint32_t loopStartBytes;
    uint32_t loopEndBytes;
    uint8_t volume;
    uint8_t reserved;
    uint8_t pack; // Value of 1 indicates DP30ADPCM packing
    uint8_t flags;
    uint32_t middleCRate; // Sample rate @ middle C
    uint8_t internal[12];
    char name[28];
    char magic[4]; // "SCRS" (probably SCReam tracker Sample)
}S3MInstrumentPCM;
static_assert(sizeof(S3MInstrumentPCM) == 67, "Wrong S3M PCM instrument size!");

typedef struct {
    uint8_t reserved[3];
    uint8_t oplValues[12];
    uint8_t volume;
    uint8_t unknown;
    uint8_t reserved2[2];
    uint32_t middleCRate;
    uint8_t unused[12];
    char title[28];
    char magic[4];
}S3MInstrumentSynth;
static_assert(sizeof(S3MInstrumentSynth) == 67, "Wrong S3M synth instrument size!");
static_assert(sizeof(S3MInstrumentPCM) == sizeof(S3MInstrumentSynth), "S3M Synth & PCM instruments should be the same size!");

#pragma pack(pop, r1)

typedef struct {
    uint8_t type; // 1 == PCM instrument
    char filename[12]; // 8.3 filename format
    union {
        S3MInstrumentSynth synth;
        S3MInstrumentPCM pcm;
    };
}S3MInstrumentHeader;

typedef struct {
    uint16_t packedLenBytes; // Includes the 2 bytes used for this value
    uint8_t packedData[];
}S3MPattern;
