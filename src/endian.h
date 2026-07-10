#pragma once
/// @file endian.h
/// @brief Utilities for handling endianness
#include <stdbool.h>
#include <stdint.h>
#include <memory.h>

/// @brief Byte-swap any value to the opposite endian
///
/// With most data types on most compilers (@ -O1 or higher optimization), this
/// becomes a single instruction.
#define ENDIAN_FLIP(T, val)                             \
do {                                                    \
    union {                                             \
       T portable;                                      \
       uint8_t bytes[sizeof(T)];                        \
    }_foreign;                                          \
                                                        \
    for (size_t _i = 0; _i < sizeof(T); _i++) {         \
        const size_t _shift = (sizeof(T) - 1 - _i) * 8; \
        _foreign.bytes[_i] = (val >> _shift) & 0xFF;    \
    }                                                   \
    val = _foreign.portable;                            \
} while(0)

/// A wrapper around ENDIAN_FLIP() for floats
#define ENDIAN_FLIP_FLOAT(val) ENDIAN_FLIP(uint32_t, *((u32*)&val))