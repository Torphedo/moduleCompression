#include "rewriteIT.h"
#include <stdlib.h>
#include <string.h>
#include "VirtualIO.h"
#include "it.h"
#include "pcm.h"
#include "opusenc_helpers.h"

bool noopCopySample(VirtualIO* io, const void* data, uint32_t size, ITSample sample) {
    io->write(io, data, size);
    return true;
}

bool writeOpusSampleIT(VirtualIO* io, const void* data, uint32_t size, ITSample sample) {
    if (size < 1024) {
        return noopCopySample(io, data, size, sample);
    }

    assert(!(sample.convertFlags & IT_CONVERT_DELTA_CODED) && "Delta coded samples aren't supported yet!");
    assert(!(sample.convertFlags & IT_CONVERT_BYTE_DELTA_CODED) && "Delta coded byte samples aren't supported yet!");
    assert(!(sample.convertFlags & IT_CONVERT_TXWAVE) && "TXWave 12-bit samples aren't supported yet!");
    assert(!(sample.flags & IT_SAMPLE_COMPRESSED) && "Compressed samples not supported yet!");

    // Allocate buffer for 16-bit samples
    const bool is16Bit = sample.flags & IT_SAMPLE_16BIT;
    const bool isSigned = sample.convertFlags & IT_CONVERT_SIGNED;
    const uint32_t outBufSize = sample.sampleCount * sizeof(uint16_t);
    void* pcmBuf = malloc(outBufSize);
    if (!pcmBuf) {
        printf("Failed to allocate %d bytes for 16-bit sample data!\n", outBufSize);
        return false;
    }

    if (is16Bit) {
        assert(outBufSize == size);
        memcpy(pcmBuf, data, outBufSize);
        if (sample.convertFlags & IT_CONVERT_BIG_ENDIAN) {
            pcmByteSwap16(pcmBuf, sample.sampleCount);
        }
        if (!isSigned) {
            pcmSign16(pcmBuf, sample.sampleCount);
        }
    } else {
        if (!isSigned) {
            // We need a temporary buffer to convert the 8-bit samples to signed values
            void* temp = malloc(sample.sampleCount);
            if (!temp) {
                printf("Failed to allocate %d bytes for temporary 8-bit signing buffer!\n", sample.sampleCount);
                return false;
            }
            // Sign the samples, then convert to 16-bit
            memcpy(temp, data, sample.sampleCount);
            pcmSign8(temp, sample.sampleCount);
            pcmU8to16(temp, pcmBuf, sample.sampleCount);
            free(temp);
        } else {
            // Already signed, just convert to 16-bit
            pcmU8to16(data, pcmBuf, sample.sampleCount);
        }
    }

    bool result = true;
    if (!compressSampleToVIO(io, pcmBuf, sample.sampleCount, 1)) {
        printf("Failed to Opus-encode %d samples\n", sample.sampleCount);
        result = false;
    }
    free(pcmBuf);

    return result;
}

bool decodeOpusSampleIT(VirtualIO* io, const void* data, uint32_t size, ITSample sample) {
    int error;
    ogg_int64_t bufSize;
    void* buf = opus_read_entire_stream(data, size, &error, &bufSize);
    if (!buf) {
        if (error == OP_ENOTFORMAT) {
            // Not Opus, must be a tiny uncompressed sample
            return noopCopySample(io, data, size, sample);
        } else {
            printf("Failed to open Opus stream!\n");
            return false;
        }
    }
    const ogg_int64_t sampleCount = bufSize / sizeof(uint16_t);
    const bool is16Bit = sample.flags & IT_SAMPLE_16BIT;
    const bool isSigned = sample.convertFlags & IT_CONVERT_SIGNED;
    const bool bigEndian = sample.convertFlags & IT_CONVERT_BIG_ENDIAN;
    assert(!(sample.convertFlags & IT_CONVERT_DELTA_CODED) && "Delta coded samples aren't supported yet!");
    assert(!(sample.convertFlags & IT_CONVERT_BYTE_DELTA_CODED) && "Delta coded byte samples aren't supported yet!");
    assert(!(sample.convertFlags & IT_CONVERT_TXWAVE) && "TXWave 12-bit samples aren't supported yet!");
    assert(!(sample.flags & IT_SAMPLE_COMPRESSED) && "Compressed samples not supported yet!");

    // Convert to 8-bit if needed
    if (!is16Bit) {
        pcmU16to8(buf, buf, sampleCount);
        bufSize /= 2;
    }

    // Convert to unsigned values if needed
    if (!isSigned) {
        if (is16Bit) {
            pcmSign16(buf, sampleCount);
        } else {
            pcmSign8(buf, sampleCount);
        }
    }

    if (bigEndian) {
        if (is16Bit) {
            pcmByteSwap16(buf, sampleCount);
        } else {
            printf("Sample is marked as big endian but also 8-bit, which doesn't make sense. Ignoring.\n");
        }
    }

    io->write(io, buf, bufSize);
    free(buf);
    return true;
}

typedef bool (*SampleWriter)(VirtualIO* io, const void* data, uint32_t size, ITSample sample);
bool copyIT(VirtualIO* in, VirtualIO* out, SampleWriter writeSample) {
    ITHeader header;
    VIO_COPY_VALUE(in, out, header);
    if (header.special & IT_SPECIAL_SONG_MESSAGE) {
        VIO_DUAL_SEEK(in, out, header.embeddedMessageOffset);
        const void* msg = in->constRead(in, header.embeddedMessageLength);
        if (!msg) {
            printf("Unable to read song message!\n");
            return false;
        }
        out->write(out, msg, header.embeddedMessageLength);
        in->free(in, msg);

        VIO_DUAL_SEEK(in, out, sizeof(header)); // Back to where we were
    }

    const void* orderTable = in->constRead(in, header.orderCount);
    if (!orderTable) {
        printf("Unable to read order table!\n");
        return false;
    }
    out->write(out, orderTable, header.orderCount);
    in->free(in, orderTable);

    const uint32_t* instrPtrs = in->constRead(in, header.instrumentCount * sizeof(uint32_t));
    const uint32_t* samplePtrs = in->constRead(in, header.sampleCount * sizeof(uint32_t));
    const uint32_t* patternPtrs = in->constRead(in, header.patternCount * sizeof(uint32_t));
    if (!instrPtrs || !samplePtrs || !patternPtrs) {
        printf("Unable to read instrument or sample or pattern pointers!\n");
        return false;
    }

    out->write(out, instrPtrs, header.instrumentCount * sizeof(uint32_t));
    out->write(out, samplePtrs, header.sampleCount * sizeof(uint32_t));
    out->write(out, patternPtrs, header.patternCount * sizeof(uint32_t));

    // Copy edit history data
    if (header.special & IT_SPECIAL_EMBEDDED_EDIT_HISTORY) {
        uint16_t historyCount;
        VIO_COPY_VALUE(in, out, historyCount);
        for (uint16_t i = 0; i < historyCount; i++) {
            uint64_t entry;
            VIO_COPY_VALUE(in, out, entry);
        }
    }

    for (uint32_t i = 0; i < header.instrumentCount; i++) {
        VIO_DUAL_SEEK(in, out, instrPtrs[i]);
        ITInstrument instr;
        VIO_COPY_VALUE(in, out, instr);
    }

    uint32_t nextSampleOutOffset = 0;
    for (uint32_t i = 0; i < header.sampleCount; i++) {
        VIO_DUAL_SEEK(in, out, samplePtrs[i]);
        ITSample sample;
        in->read(in, &sample, sizeof(sample));
        assert(!(sample.flags & IT_SAMPLE_COMPRESSED) && "Compressed samples not supported yet!");

        uint32_t sampleSize = sample.sampleCount;
        if (sample.flags & IT_SAMPLE_16BIT) {
            sampleSize *= 2;
        }

        in->seek(in, sample.dataPtr);
        if (nextSampleOutOffset > 0) {
            out->seek(out, nextSampleOutOffset);
        } else {
            out->seek(out, sample.dataPtr);
        }
        const uint32_t sampleOutStart = out->tell(out);

        const void* data = in->constRead(in, sampleSize);
        if (!data) {
            printf("Unable to read sample data!\n");
            continue;
        }

        (writeSample)(out, data, sampleSize, sample);
        const uint32_t sampleEnd = out->tell(out);
        const uint32_t outSize = sampleEnd - sampleOutStart;
        in->free(in, data);
        nextSampleOutOffset = sampleOutStart + outSize;

        sample.dataPtr = sampleOutStart;
        sample.sampleCount = outSize;
        if (sample.flags & IT_SAMPLE_16BIT) {
            if (sample.sampleCount % 2 != 0) {
                sample.sampleCount++;
            }
            sample.sampleCount /= 2;
        }
        out->seek(out, samplePtrs[i]);
        out->write(out, &sample, sizeof(sample));
    }

    for (uint32_t i = 0; i < header.patternCount; i++) {
        VIO_DUAL_SEEK(in, out, patternPtrs[i]);
        ITPattern pat;
        VIO_COPY_VALUE(in, out, pat);

        const uint32_t* data = in->constRead(in, pat.patternLen);
        if (!data) {
            printf("Unable to read packed pattern data!\n");
            continue;
        }
        out->write(out, data, pat.patternLen);
        in->free(in, data);
    }


    in->free(in, instrPtrs);
    in->free(in, samplePtrs);
    in->free(in, patternPtrs);

    return true;
}

bool writeOpusIT(const char* inpath, const char* outpath) {
    VirtualIO in = vioOpenPath(inpath, false);
    VirtualIO out = vioOpenPath(outpath, true);
    if (!in.ctx || !out.ctx) {
        in.close(&in);
        out.close(&out);
        printf("Failed to open one of the files!\n");
        return false;
    }

    return copyIT(&in, &out, writeOpusSampleIT);
}

bool decodeOpusIT(const char* inpath, const char* outpath) {
    VirtualIO in = vioOpenPath(inpath, false);
    VirtualIO out = vioOpenPath(outpath, true);
    if (!in.ctx || !out.ctx) {
        in.close(&in);
        out.close(&out);
        printf("Failed to open one of the files!\n");
        return false;
    }

    return copyIT(&in, &out, decodeOpusSampleIT);
}
