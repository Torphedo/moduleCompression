#include "rewriteS3M.h"
#include "VirtualIO.h"
#include "s3m.h"
#include "pcm.h"
#include "opusenc_stdio.h"
#include "opusfile.h"

// Read a value into a local variable, then write it into another IO stream
#define VIO_COPY_VALUE(in, out, local) (in)->read((in), &(local), sizeof(local)); (out)->write((out), &(local), sizeof(local))

// Seek 2 streams to the same offset
#define VIO_DUAL_SEEK(s1, s2, offset) (s1)->seek((s1), offset); (s2)->seek((s2), offset)

S3MHeader copyNoteDataS3M(VirtualIO* in, VirtualIO* out, const uint16_t** instrPtrs) {
    S3MHeader header;
    VIO_COPY_VALUE(in, out, header);

    // Copy pattern order table
    const void* orderTable = in->constRead(in, header.orderCount);
    out->write(out, orderTable, header.orderCount);
    in->free(in, orderTable);

    // Copy pointer tables
    const uint16_t* instrPointerTable = in->constRead(in, header.instrumentCount * sizeof(uint16_t));
    const uint16_t* patternPointerTable = in->constRead(in, header.patternPtrCount * sizeof(uint16_t));

    out->write(out, instrPointerTable, header.instrumentCount * sizeof(uint16_t));
    out->write(out, patternPointerTable, header.patternPtrCount * sizeof(uint16_t));

    // Copy packed pattern data
    for (uint32_t i = 0; i < header.patternPtrCount; i++) {
        const uint64_t offset = patternPointerTable[i] * 16; // Pointer is in units of 16 bytes
        VIO_DUAL_SEEK(in, out, offset);

        uint16_t length;
        VIO_COPY_VALUE(in, out, length);
        const void* packedData = in->constRead(in, length - sizeof(length));
        out->write(out, packedData, length - sizeof(length));
        in->free(in, packedData);
    }

    in->free(in, patternPointerTable);
    *instrPtrs = instrPointerTable;
    return header;
}

bool writeOpusSampleS3M(VirtualIO* io, const void* data, S3MInstrumentPCM pcm) {
    uint8_t channels = (pcm.flags & S3M_PCM_INSTR_FLAG_STEREO) ? 2 : 1;
    channels = 1; // I think Opus expects interleaved samples

    const uint8_t sampleSize = (pcm.flags & S3M_PCM_INSTR_FLAG_16BIT) ? 2 : 1;
    const uint32_t sampleCount = pcm.lengthBytes / (channels * sampleSize);

    OggOpusComments* comments = ope_comments_create();
    if (!comments) {
        return false;
    }
    OggOpusEnc* enc = ope_encoder_create_callbacks(&opus_vio_impl, io, comments, 48000, channels, 0, NULL);
    if (!enc) {
        ope_comments_destroy(comments);
        return false;
    }

    const void* pcmBuf = data;
    if (sampleSize == 1) {
        void* buf = malloc(pcm.lengthBytes * 2);
        if (!buf) {
            ope_encoder_destroy(enc);
            ope_comments_destroy(comments);
            return false;
        }
        pcm8to16(data, buf, sampleCount);
        pcmBuf = buf;
    }

    ope_encoder_write(enc, pcmBuf, sampleCount / channels);
    ope_encoder_drain(enc);
    ope_encoder_destroy(enc);
    ope_comments_destroy(comments);

    if (sampleSize == 1) {
        free((void*)pcmBuf);
    }
    return true;
}

bool decodeOpusSampleS3M(VirtualIO* io, const void* data, S3MInstrumentPCM pcm) {
    const uint8_t sampleSize = (pcm.flags & S3M_PCM_INSTR_FLAG_16BIT) ? 2 : 1;
    int error;
    OggOpusFile* file = op_open_memory(data, pcm.lengthBytes, &error);
    if (!file && error == OP_ENOTFORMAT) {
        // Not Opus, must be a tiny uncompressed sample
        io->write(io, data, pcm.lengthBytes);
        return true;
    } else if (!file) {
        printf("Failed to open Opus stream!\n");
        return false;
    }

    const int sampleCount = op_pcm_total(file, -1);
    if (sampleCount < 0) {
        printf("Failed to get sample count of Opus stream!\n");
        op_free(file);
        return false;
    }
    ogg_int64_t bufSize = sampleCount * sizeof(uint16_t);
    void* buf = malloc(bufSize);
    if (!buf) {
        printf("Failed to allocate %d bytes for uncompressed audio!\n", bufSize);
        op_free(file);
        return false;
    }

    int16_t* bufpos = buf;
    int totalSamplesRead = 0;
    while (totalSamplesRead < sampleCount) {
        const int samplesRead = op_read(file, bufpos, sampleCount, NULL);
        if (samplesRead < 0) {
            printf("Failed to decode Opus stream!\n");
            free(buf);
            op_free(file);
            return false;
        }
        totalSamplesRead += samplesRead;
        bufpos += samplesRead;
    }

    // Convert to 8-bit if needed
    if (sampleSize == 1) {
        pcm16to8(buf, buf, sampleCount);
        bufSize /= 2;
    }

    io->write(io, buf, bufSize);
    free(buf);
    op_free(file);
    return true;
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

    const uint16_t* instrTable;
    S3MHeader header = copyNoteDataS3M(&in, &out, &instrTable);

    for (uint32_t i = 0; i < header.instrumentCount; i++) {
        const uint64_t offset = instrTable[i] * 16; // Pointer is in units of 16 bytes
        VIO_DUAL_SEEK(&in, &out, offset);

        // Copy instrument metadata
        S3MInstrumentHeader instr;
        in.read(&in, &instr, sizeof(instr));

        if (instr.type == S3M_INSTR_EMPTY) {
            // Move on
            continue;
        } else if (instr.type == S3M_INSTR_PCM) {
            const uint32_t size = instr.pcm.lengthBytes;
            uint32_t sampleOffset = ((uint32_t)instr.pcm.samplePtrHigh << 16) | instr.pcm.samplePtrLow;
            sampleOffset *= 16;

            // Copy sample data
            VIO_DUAL_SEEK(&in, &out, sampleOffset);
            const void* sample = in.constRead(&in, size);
            if (size > 1024) {
                writeOpusSampleS3M(&out, sample, instr.pcm);
            } else {
                // Don't bother compressing tiny samples
                out.write(&out, sample, size);
            }

            const uint64_t sampleEndOffset = out.tell(&out);
            instr.pcm.lengthBytes = sampleEndOffset - sampleOffset;
            printf("Compressed %d bytes raw audio to %d bytes\n", size, instr.pcm.lengthBytes);

            in.free(&in, sample);
        }

        out.seek(&out, offset);
        out.write(&out, &instr, sizeof(instr));
    }
    in.free(&in, instrTable);

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

    const uint16_t* instrTable;
    S3MHeader header = copyNoteDataS3M(&in, &out, &instrTable);

    for (uint32_t i = 0; i < header.instrumentCount; i++) {
        const uint64_t offset = instrTable[i] * 16; // Pointer is in units of 16 bytes
        VIO_DUAL_SEEK(&in, &out, offset);

        // Copy instrument metadata
        S3MInstrumentHeader instr;
        in.read(&in, &instr, sizeof(instr));

        if (instr.type == S3M_INSTR_EMPTY) {
            // Move on
            continue;
        } else if (instr.type == S3M_INSTR_PCM) {
            const uint32_t size = instr.pcm.lengthBytes;
            uint32_t sampleOffset = ((uint32_t)instr.pcm.samplePtrHigh << 16) | instr.pcm.samplePtrLow;
            sampleOffset *= 16;

            // Copy sample data
            VIO_DUAL_SEEK(&in, &out, sampleOffset);
            const void* sample = in.constRead(&in, size);
            decodeOpusSampleS3M(&out, sample, instr.pcm);

            const uint64_t sampleEndOffset = out.tell(&out);
            instr.pcm.lengthBytes = sampleEndOffset - sampleOffset;
            printf("Compressed %d bytes raw audio to %d bytes\n", size, instr.pcm.lengthBytes);

            in.free(&in, sample);
        }

        out.seek(&out, offset);
        out.write(&out, &instr, sizeof(instr));
    }
    in.free(&in, instrTable);

    in.close(&in);
    out.close(&out);
    return true;
}
