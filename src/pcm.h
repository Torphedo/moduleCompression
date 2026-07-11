#pragma once
#include <stdint.h>

// Decode 8-bit delta-coded samples, to normal 8-bit PCM
void pcmDeltaDecode8(const int8_t* sample, int8_t* pcmOut, uint32_t sampleCount);

// Decode 8-bit delta-coded samples to normal 16-bit PCM
void pcmDelta8to16(const int8_t* sample, int16_t* pcmOut, uint32_t sampleCount);

// Decode 16-bit delta-coded samples, to normal 16-bit PCM
void pcmDeltaDecode16(const int16_t* sample, int16_t* pcmOut, uint32_t sampleCount);

// Encode normal 16-bit PCM to 8-bit delta-coded samples
void pcmDeltaEncode16to8(const int16_t* pcmIn, int8_t* out, uint32_t sampleCount);

// Encode normal 8-bit PCM to 8-bit delta-coded samples
void pcmDeltaEncode8(const int8_t* pcmIn, int8_t* out, uint32_t sampleCount);

// Encode normal 16-bit PCM to 16-bit delta-coded samples
void pcmDeltaEncode16(const int16_t* pcmIn, int16_t* out, uint32_t sampleCount);

// Convert 8-bit PCM to 16-bit PCM
void pcmU8to16(const uint8_t* in, uint16_t* out, uint32_t sampleCount);

// Convert 16-bit PCM to 8-bit PCM
void pcmU16to8(const uint16_t* in, uint8_t* out, uint32_t sampleCount);

// Convert 16-bit samples from big to little endian, or vice versa
void pcmByteSwap16(uint16_t* buf, uint32_t sampleCount);

// Convert signed 16-bit samples to unsigned ones, or vice versa
void pcmSign16(uint16_t* buf, uint32_t sampleCount);

// Convert signed 8-bit samples to unsigned ones, or vice versa
void pcmSign8(uint8_t* buf, uint32_t sampleCount);
