#pragma once
#include <stdbool.h>

// Write a MOD file with all samples compressed with Opus
bool writeOpusMOD(const char* inpath, const char* outpath);

// Decompress all Opus samples in a MOD file, making it readable by normal players
bool decodeOpusMOD(const char* inpath, const char* outpath);
