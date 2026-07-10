#include "rewriteS3M.h"
#include "VirtualIO.h"
#include "s3m.h"
#include "pcm.h"
#include "opusenc_helpers.h"
#include "opusfile.h"

S3MHeader copyNoteDataS3M(VirtualIO* in, VirtualIO* out, const uint16_t** instrPtrs) {
    S3MHeader header;
    VIO_COPY_VALUE(in, out, header);

    // Copy pattern order table
    const void* orderTable = in->constRead(in, header.orderCount);
    if (!orderTable) {
        printf("Failed to read S3M order table!\n");
        return header;
    }
    out->write(out, orderTable, header.orderCount);
    in->free(in, orderTable);

    // Copy pointer tables
    const uint16_t* instrPointerTable = in->constRead(in, header.instrumentCount * sizeof(uint16_t));
    const uint16_t* patternPointerTable = in->constRead(in, header.patternPtrCount * sizeof(uint16_t));
    if (!instrPointerTable || !patternPointerTable) {
        printf("Failed to read S3M pattern and/or instrument pointer table!\n");
        return header;
    }

    out->write(out, instrPointerTable, header.instrumentCount * sizeof(uint16_t));
    out->write(out, patternPointerTable, header.patternPtrCount * sizeof(uint16_t));

    // Copy packed pattern data
    for (uint32_t i = 0; i < header.patternPtrCount; i++) {
        const uint64_t offset = patternPointerTable[i] * 16; // Pointer is in units of 16 bytes
        VIO_DUAL_SEEK(in, out, offset);

        uint16_t length;
        VIO_COPY_VALUE(in, out, length);
        const void* packedData = in->constRead(in, length - sizeof(length));
        if (!packedData) {
            printf("Failed to read S3M pattern data!\n");
            continue;
        }
        out->write(out, packedData, length - sizeof(length));
        in->free(in, packedData);
    }

    in->free(in, patternPointerTable);
    *instrPtrs = instrPointerTable;

    return header;
}

bool writeOpusSampleS3M(VirtualIO* io, void* data, S3MInstrumentPCM pcm, bool signedSamples) {
    if (pcm.lengthBytes <= 1024 || pcm.pack == 1) {
        // Don't bother compressing tiny samples or ADPCM, the OGG container
        // seems to have ~1KiB minimum overhead
        io->write(io, data, pcm.lengthBytes);
        return true;
    }

    uint8_t channels = (pcm.flags & S3M_PCM_INSTR_FLAG_STEREO) ? 2 : 1;
    channels = 1; // I think Opus expects interleaved samples

    const uint8_t sampleSize = (pcm.flags & S3M_PCM_INSTR_FLAG_16BIT) ? 2 : 1;
    const uint32_t sampleCount = pcm.lengthBytes / (channels * sampleSize);

    // Convert to signed values if needed
    if (!signedSamples) {
        if (sampleSize == 1) {
            pcmSign8(data, sampleCount);
        } else {
            pcmSign16(data, sampleCount);
        }
    }

    void* pcmBuf = data;
    if (sampleSize == 1) {
        pcmBuf = calloc(1, pcm.lengthBytes * 2);
        if (!pcmBuf) {
            return false;
        }

        pcmU8to16(data, pcmBuf, sampleCount);
    }

    bool result = true;
    if (!compressSampleToVIO(io, pcmBuf, sampleCount / channels, channels)) {
        printf("Failed to Opus-encode %d samples\n", sampleCount);
        result = false;
    }

    if (sampleSize == 1) {
        free(pcmBuf);
    }
    return result;
}

bool decodeOpusSampleS3M(VirtualIO* io, void* data, S3MInstrumentPCM pcm, bool signedSamples) {
    const uint8_t sampleSize = (pcm.flags & S3M_PCM_INSTR_FLAG_16BIT) ? 2 : 1;

    int error;
    ogg_int64_t bufSize;
    void* buf = opus_read_entire_stream(data, pcm.lengthBytes, &error, &bufSize);
    if (!buf) {
        if (error == OP_ENOTFORMAT) {
            // Not Opus, must be a tiny uncompressed sample
            io->write(io, data, pcm.lengthBytes);
            return true;
        } else {
            printf("Failed to open Opus stream!\n");
            return false;
        }
    }
    const ogg_int64_t sampleCount = bufSize / sizeof(uint16_t);

    // Convert to 8-bit if needed
    if (sampleSize == 1) {
        pcmU16to8(buf, buf, sampleCount);
        bufSize /= 2;
    }

    // Convert to unsigned values if needed
    if (!signedSamples) {
        if (sampleSize == 1) {
            pcmSign8(buf, sampleCount);
        } else {
            pcmSign16(buf, sampleCount);
        }
    }

    io->write(io, buf, bufSize);
    free(buf);
    return true;
}

typedef bool (*SampleWriterS3M)(VirtualIO* io, void* data, S3MInstrumentPCM pcm, bool signedSamples);

void rewriteS3M(VirtualIO* in, VirtualIO* out, SampleWriterS3M writeSample) {
    const uint16_t* instrTable;
    S3MHeader header = copyNoteDataS3M(in, out, &instrTable);
    if (header.sampleType == 1) {
        printf("Samples are signed!\n");
    } else {
        printf("Samples are unsigned!\n");
    }

    int64_t outputSampleOffset = -1;
    for (uint32_t i = 0; i < header.instrumentCount; i++) {
        const uint64_t offset = instrTable[i] * 16; // Pointer is in units of 16 bytes
        VIO_DUAL_SEEK(in, out, offset);

        // Copy instrument metadata
        S3MInstrumentHeader instr;
        in->read(in, &instr, sizeof(instr));

        if (instr.type == S3M_INSTR_EMPTY) {
            // Move on
            continue;
        } else if (instr.type == S3M_INSTR_PCM) {
            const uint32_t size = instr.pcm.lengthBytes;
            uint32_t sampleOffset = ((uint32_t)instr.pcm.samplePtrHigh << 16) | instr.pcm.samplePtrLow;
            sampleOffset *= 16;
            if (outputSampleOffset > 0) {
                // Override original offset to pack samples tighter
                instr.pcm.samplePtrLow = (outputSampleOffset / 16) & UINT16_MAX;
                instr.pcm.samplePtrHigh = (outputSampleOffset / 16) >> 16;

                uint32_t test = ((uint32_t)instr.pcm.samplePtrHigh << 16) | instr.pcm.samplePtrLow;
                assert(test * 16 == outputSampleOffset);
            } else {
                outputSampleOffset = sampleOffset;
            }

            // Copy sample data
            in->seek(in, sampleOffset);
            out->seek(out, outputSampleOffset);
            void* sample = malloc(size);
            if (!sample) {
                printf("Failed to allocate %d bytes to read sample %d!\n", size, i);
                continue;
            }
            in->read(in, sample, size);
            (writeSample)(out, sample, instr.pcm, header.sampleType == 1);
            free(sample);

            const uint64_t sampleEndOffset = out->tell(out);
            instr.pcm.lengthBytes = sampleEndOffset - outputSampleOffset;

            // Start the next sample immediately after this one, aligned up by 16
            outputSampleOffset = (sampleEndOffset & ~((uint64_t)0xF)) + 16;

            printf("Input audio %d bytes, output audio %d bytes\n", size, instr.pcm.lengthBytes);

        }

        out->seek(out, offset);
        out->write(out, &instr, sizeof(instr));
    }
    in->free(in, instrTable);
}

bool writeOpusS3M(const char* inpath, const char* outpath) {
    VirtualIO in = vioOpenPath(inpath, false);
    VirtualIO out = vioOpenPath(outpath, true);
    if (!in.ctx || !out.ctx) {
        in.close(&in);
        out.close(&out);
        printf("Failed to open one of the files!\n");
        return false;
    }

    rewriteS3M(&in, &out, writeOpusSampleS3M);
    in.close(&in);
    out.close(&out);
    return true;
}

bool decodeOpusS3M(const char* inpath, const char* outpath) {
    VirtualIO in = vioOpenPath(inpath, false);
    VirtualIO out = vioOpenPath(outpath, true);
    if (!in.ctx || !out.ctx) {
        in.close(&in);
        out.close(&out);
        printf("Failed to open one of the files!\n");
        return false;
    }

    rewriteS3M(&in, &out, decodeOpusSampleS3M);
    in.close(&in);
    out.close(&out);
    return true;
}
