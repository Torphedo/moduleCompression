#pragma once
#include <stdbool.h>
#include "VirtualIO.h"

// Write a MOD file with all samples compressed with Opus
bool writeOpusMOD(VirtualIO* in, VirtualIO* out);

// Decompress all Opus samples in a MOD file, making it readable by normal players
bool decodeOpusMOD(VirtualIO* in, VirtualIO* out);
