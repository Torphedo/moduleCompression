#pragma once
#include <stdbool.h>
#include "VirtualIO.h"

bool writeOpusXM(VirtualIO* in, VirtualIO* out);

bool decodeOpusXM(VirtualIO* in, VirtualIO* out);
