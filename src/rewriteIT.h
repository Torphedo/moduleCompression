#pragma once
#include <stdbool.h>
#include "VirtualIO.h"

bool writeOpusIT(VirtualIO* in, VirtualIO* out);
bool decodeOpusIT(VirtualIO* in, VirtualIO* out);
