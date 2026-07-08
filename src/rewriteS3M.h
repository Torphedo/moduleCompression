#pragma once
#include <stdbool.h>

bool writeOpusS3M(const char* inpath, const char* outpath);

bool decodeOpusS3M(const char* inpath, const char* outpath);
