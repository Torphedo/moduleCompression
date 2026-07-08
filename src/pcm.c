#include "pcm.h"
#include <assert.h>

void pcmDeltaDecode8(const int8_t* sample, int8_t* pcmOut, uint32_t sampleCount) {
    int16_t amp;
    for (uint32_t i = 0; i < sampleCount; i++) {
        amp += sample[i];
        // The sample may go beyond the 8-bit boundary, just grab the signed 8-bit portion
        pcmOut[i] = (int8_t)amp;
    }
}

void pcmDeltaDecode16(const int16_t* sample, int16_t* pcmOut, uint32_t sampleCount) {
    int32_t amp;
    for (uint32_t i = 0; i < sampleCount; i++) {
        amp += sample[i];
        // The sample may go beyond the 16-bit boundary, just grab the signed 16-bit portion
        pcmOut[i] = (int16_t)amp;
    }
}

// Decode 8-bit delta-coded samples, and convert to 16-bit PCM
void pcmDelta8to16(const int8_t* sample, int16_t* pcmOut, uint32_t sampleCount) {
    int16_t amp = 0;
    for( uint32_t i = 0; i < sampleCount; i++ ) {
        amp += sample[i]; // Calculate next sample value

        // The sample may go beyond the 8-bit boundary, just grab the signed
        // 8-bit portion but at 16-bit magnitude.
        pcmOut[i] = amp << 8;
    }
}

void pcmDeltaEncode16to8(const int16_t* pcmIn, int8_t* out, uint32_t sampleCount) {
    int8_t old = 0, new;
    for (uint32_t i = 0; i < sampleCount; i++) {
        new = pcmIn[i] >> 8; // Convert to 8-bit magnitude
        const int8_t diff = new - old;

        const int8_t result = old + diff;
        // Despite the diff value overflowing, the truncated 8-bit diff will end
        // up reaching the target sample value via over/underflow
        assert(result == new);

        out[i] = diff;
        old = new;
    }
}


void pcmDeltaEncode8(const int8_t* pcmIn, int8_t* out, uint32_t sampleCount) {
    int8_t old = 0, new;
    for (uint32_t i = 0; i < sampleCount; i++) {
        new = pcmIn[i];
        int8_t diff = new - old;
        const int8_t result = old + diff;
        // Despite the diff value overflowing, the truncated 8-bit diff will end
        // up reaching the target sample value via over/underflow
        assert(result == new);
        out[i] = diff;
        old = new;
    }
}


void pcmDeltaEncode16(const int16_t* pcmIn, int16_t* out, uint32_t sampleCount) {
    int16_t old = 0, new;
    for (uint32_t i = 0; i < sampleCount; i++) {
        new = pcmIn[i];
        const int16_t diff = new - old;
        out[i] = diff;
        old = new;
    }
}

void pcmU8to16(const uint8_t* in, uint16_t* out, uint32_t sampleCount) {
    for (uint32_t i = 0; i < sampleCount; i++) {
        const uint16_t sample = in[i];
        uint16_t sampleOut = (sample << 8) | (uint8_t)sample;
        out[i] = sampleOut;
    }
}

void pcmS8to16(const int8_t* in, int16_t* out, uint32_t sampleCount) {
    for (uint32_t i = 0; i < sampleCount; i++) {
        const int16_t sample = in[i];
        int16_t sampleOut = (sample << 8) | (int8_t)sample;
        out[i] = sampleOut;
    }
}

void pcmU16to8(const uint16_t* in, uint8_t* out, uint32_t sampleCount) {
    for (uint32_t i = 0; i < sampleCount; i++) {
        const uint16_t sample = in[i];
        const uint16_t sampleOut = sample >> 8;
        out[i] = (uint8_t)sampleOut;
    }
}


void pcmS16to8(const int16_t* in, int8_t* out, uint32_t sampleCount) {
    for (uint32_t i = 0; i < sampleCount; i++) {
        const int16_t sample = in[i];
        const int16_t sampleOut = sample >> 8;
        out[i] = (int8_t)sampleOut;
    }
}

void pcmSign16(uint16_t* buf, uint32_t sampleCount) {
    for (uint32_t i = 0; i < sampleCount; i++) {
        const uint16_t sample = ((uint16_t*)buf)[i];
        // Invert the sign bit
        const uint16_t sampleOut = (sample & INT16_MAX) | (~sample & 0x8000);
        buf[i] = sampleOut;
    }
}

void pcmSign8(uint8_t* buf, uint32_t sampleCount) {
    for (uint32_t i = 0; i < sampleCount; i++) {
        const uint8_t sample = ((uint8_t*)buf)[i];
        // Invert the sign bit
        const uint8_t sampleOut = (sample & INT8_MAX) | (~sample & 0x80);
        buf[i] = sampleOut;
    }
}