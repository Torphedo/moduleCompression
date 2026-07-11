#pragma once
// Structures for Impulse Tracker's .it music format
// https://github.com/jthlim/impulse-tracker/blob/main/ReleaseDocumentation/ITTECH.TXT
// https://fileformats.fandom.com/wiki/Impulse_tracker
#include <stdint.h>
#include <assert.h>

enum {
    IT_VERSION_200 = 0x0200, // Indicates version 2.00

    IT_FLAG_STEREO =               1 << 0,
    IT_FLAG_MIXING =               1 << 1, // Obsolete in v1.04
    IT_FLAG_USE_INSTRUMENTS =      1 << 2, // Otherwise, use samples only
    IT_FLAG_LINEAR_SLIDES =        1 << 3, // Otherwise, use Amiga slides
    IT_FLAG_OLD_EFFECTS =          1 << 4,
    IT_FLAG_EFFECT_G_LINKED =      1 << 5,
    IT_FLAG_MIDI_PITCH_CONTROL =   1 << 6,
    IT_FLAG_EMBEDDED_MIDI_MACROS = 1 << 7,

    IT_SPECIAL_SONG_MESSAGE =          1 << 0,
    IT_SPECIAL_EMBEDDED_EDIT_HISTORY = 1 << 1,
    IT_SPECIAL_EMBEDDED_HIGHLIGHT =    1 << 2,
    IT_SPECIAL_EMBEDDED_MIDI_MACROS =  1 << 3,

    IT_SAMPLE_HEADER =           1 << 0,
    IT_SAMPLE_16BIT =            1 << 1,
    IT_SAMPLE_STEREO =           1 << 2,
    IT_SAMPLE_COMPRESSED =       1 << 3,
    IT_SAMPLE_LOOP =             1 << 4,
    IT_SAMPLE_SUSTAIN_LOOP =     1 << 5,
    IT_SAMPLE_PINGPONG =         1 << 6,
    IT_SAMPLE_PINGPONG_SUSTAIN = 1 << 7,

    // Fandom soruce claims bits 1, 3, 4, and 5 can generally be ignored because
    // they were only used for intermediate instrument files
    IT_CONVERT_SIGNED =           1 << 0,
    IT_CONVERT_BIG_ENDIAN =       1 << 1,
    IT_CONVERT_DELTA_CODED =      1 << 2,
    IT_CONVERT_BYTE_DELTA_CODED = 1 << 3,
    IT_CONVERT_TXWAVE =           1 << 4, // 12-bit samples
    IT_CONVERT_STEREO_PROMPT =    1 << 5, // Show left/right/all prompt
};

typedef struct {
    char magic[4]; // "IMPM" (IMPulse Module)
    char name[26];
    uint16_t patternHighlight; // Only used in editing
    uint16_t orderCount; // Size of order table
    uint16_t instrumentCount;
    uint16_t sampleCount;
    uint16_t patternCount;
    uint16_t trackerVersion;
    uint16_t trackerCompatibleVersion; // aka. format version
    uint16_t flags;
    uint16_t special;
    uint8_t globalVolume;
    uint8_t mixVolume;
    uint8_t initialSpeed;
    uint8_t initialTempo;
    uint8_t channelPanSeparation;
    uint8_t midiPitchWheelDepth;
    uint16_t embeddedMessageLength;
    uint32_t embeddedMessageOffset;
    uint32_t reserved; // OpenMPT uses this to store the signature "OMPT"
    int8_t initialChannelPan[64]; // 0-64 (0 == left, 64 == right, 100 = surround sound, negative = disabled)
    uint8_t initialChannelVolume[64]; // 0-64
    uint8_t orderTable[]; // Size == order count
}ITHeader;

typedef struct {
    uint8_t yPos;
    uint8_t tickPos[2]; // This is a 16-bit value, I'm making it an array to prevent any padding - torf
}ITEnvelopeNode;

typedef struct {
    uint8_t flags;
    uint8_t nodeCount;
    uint8_t loopStart;
    uint8_t loopEnd;
    uint8_t sustainLoopStart;
    uint8_t sustainLoopEnd;
    ITEnvelopeNode nodes[25];
}ITEnvelope;

typedef struct {
    char magic[4]; // "IMPI" (IMPulse Instrument)
    char filename[12]; // 8.3 filename format
    uint8_t reserved;
    uint8_t newNoteAction;
    uint8_t duplicateCheckType;
    uint8_t duplicateCheckAction;
    int16_t fadeOut;
    int8_t pitchPanSeparation;
    uint8_t pitchPanCenter;
    uint8_t globalVolume;
    uint8_t defaultPan;
    uint8_t randomVolumeVariation;
    uint8_t randomPanVariation;
    uint16_t trackerVersion; // Instrument files only
    uint8_t sampleCount; // Instrument files only
    uint8_t reserved2;
    char instrumentName[26];
    int8_t initialFilterCutoff;
    int8_t initialFilterResonance;
    int8_t midiChannel;
    uint8_t midiProgram;
    uint16_t midiBank;
    uint16_t keyboardTable[120];
    ITEnvelope volumeEnvelope;
    ITEnvelope panEnvelope;
    ITEnvelope pitchAndFilterEnvelope;
    uint8_t padding[7];
}ITInstrument;
static_assert(sizeof(ITInstrument) == 554, "Wrong IT instrument size!");

// Pre-2.00 format
typedef struct {
    char magic[4]; // "IMPI" (IMPulse Instrument)
    char filename[12]; // 8.3 filename format
    uint8_t reserved;
    uint8_t flags;
    uint8_t volumeLoopStart;
    uint8_t volumeLoopEnd;
    uint8_t sustainLoopStart;
    uint8_t sustainLoopEnd;
    uint16_t reserved2;
    int16_t fadeOut;
    uint8_t newNoteAction;
    uint8_t duplicateNoteCheck;
    uint16_t trackerVersion; // Instrument files only
    uint8_t sampleCount; // Instrument files only
    uint8_t reserved3;
    char instrumentName[26];
    uint8_t reserved4[6];

    uint16_t keyboardTable[120];
    uint8_t volumeEnvelope[200];
    uint8_t volumeEnvelopeNodes[50];
}ITOldInstrument;
static_assert(sizeof(ITOldInstrument) == 554, "Wrong IT old instrument size!");

typedef struct {
    char magic[4]; // "IMPS" (IMPulse Sample)
    char filename[12]; // 8.3 filename format
    uint8_t reserved;
    uint8_t globalVolume; // 0-64
    uint8_t flags; // IT_SAMPLE_ bitmask
    uint8_t defaultVolume; // 0-64
    char name[26];
    uint8_t convertFlags;
    uint8_t defaultPan;
    uint32_t sampleCount; // # of PCM sample values, not # of bytes
    uint32_t sampleLoopStart;
    uint32_t sampleLoopEnd;
    uint32_t middleCRate;
    uint32_t sampleSustainLoopStart;
    uint32_t sampleSustainLoopEnd;
    uint32_t dataPtr;
    uint8_t vibratoSpeed;
    uint8_t vibratoDepth;
    uint8_t vibratoSweep;
    uint8_t vibratoWaveform;
}ITSample;

// Total pattern struct can be up to 64KiB (0xFFFF)
typedef struct {
    uint16_t patternLen;
    int16_t rowCount; // IT allows 32-200, OpenMPT allows more
    uint32_t reserved;
    uint8_t packedData[];
}ITPattern;
static_assert(sizeof(ITPattern) == 8, "Wrong IT pattern header size!");