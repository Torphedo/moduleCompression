#pragma once
#include <stdint.h>
#include <assert.h>

typedef struct {
    char magic[17]; // "Extended Module: "
    char name[20];
    char unknown; // Always 0x1A
    char trackerName[20];
    uint8_t versionMajor; // Latest is 1.4
    uint8_t versionMinor;
    uint32_t headerSize;

    uint16_t songLength;
    uint16_t songRestartPos;
    uint16_t channelCount;
    uint16_t patternCount; // Max = 256
    uint16_t instrumentCount; // Max = 128
    uint16_t flags;
    uint16_t defaultTempo;
    uint16_t defaultBPM;
}XMHeader;
static_assert(sizeof(XMHeader) == 80, "Wrong XM header size!");


// Disable struct padding, otherwise our structs won't match the file and
// loading will break.
#pragma pack(push, r1, 1)
typedef struct {
    uint32_t headerSize;
    uint8_t packingType; // Always 0
    uint16_t rowCount; // Max = 256
    uint16_t patternDataSize;
    uint8_t patternData[];
}XMPattern;
static_assert(sizeof(XMPattern) == 9, "Wrong XM pattern size!");

typedef struct {
    uint32_t headerSize;
    char name[22];
    uint8_t type; // Supposed to be zero, but isn't in practice
    uint16_t sampleCount; // If this isn't 0, XMSampleSettings comes next
}XMInstrument;
static_assert(sizeof(XMInstrument) == 29, "Wrong XM instrument size!");

typedef struct {
    uint16_t frameNum;
    uint16_t value;
}XMEnvelopePoint;

typedef struct {
    uint32_t headerSize;
    uint8_t samplesPerNote[96];
    uint8_t volumeEnvelopePoints[48];
    uint8_t panningEnvelopePoints[48];
    uint8_t volumeEnvelopePointCount;
    uint8_t panningEnvelopePointCount;
    uint8_t volumeSustainPoint;
    uint8_t volumeLoopStart;
    uint8_t volumeLoopEnd;
    uint8_t panningSustainPoint;
    uint8_t panningLoopStart;
    uint8_t panningLoopEnd;
    uint8_t volumeType;
    uint8_t panningType;
    uint8_t vibratoType;
    uint8_t vibratoSweep;
    uint8_t vibratoDepth;
    uint8_t vibratoRate;
    uint16_t volumeFade;
    uint16_t reserved;
    XMEnvelopePoint volumeEnvelope[12];
    XMEnvelopePoint panningEnvelope[12];
}XMSampleSettings;
static_assert(sizeof(XMSampleSettings) == 310, "Wrong XM instrument sample settings size!");
#pragma pack(pop, r1)

typedef struct {
    uint32_t length;
    uint32_t loopStart;
    uint32_t loopLength;
    uint8_t volume;
    uint8_t fineTune;
    uint8_t sampleType; // e.g. loop, ping-pong
    uint8_t panning;
    int8_t relativeNoteNum;
    uint8_t reserved;
    char sampleName[22];
}XMSampleHeader;
static_assert(sizeof(XMSampleHeader) == 40, "Wrong XM sample header size!");

static bool XMSampleIs16Bit(XMSampleHeader header) {
    return header.sampleType & (1 << 4);
}

