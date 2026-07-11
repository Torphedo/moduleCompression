#pragma once
#include <stdbool.h>
#include "VirtualIO.h"

bool writeOpusS3M(VirtualIO* in, VirtualIO* out);

bool decodeOpusS3M(VirtualIO* in, VirtualIO* out);
