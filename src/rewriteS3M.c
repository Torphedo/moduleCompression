#include "rewriteS3M.h"
#include "VirtualIO.h"
#include "s3m.h"

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
        VIO_COPY_VALUE(&in, &out, instr);

        if (instr.type == S3M_INSTR_EMPTY) {
            // Move on
        } else if (instr.type == S3M_INSTR_PCM) {
            const uint32_t size = instr.pcm.lengthBytes;
            uint32_t sampleOffset = ((uint32_t)instr.pcm.samplePtrHigh << 16) | instr.pcm.samplePtrLow;
            sampleOffset *= 16;

            // Copy PCM data
            VIO_DUAL_SEEK(&in, &out, sampleOffset);
            const void* sample = in.constRead(&in, size);
            out.write(&out, sample, size);

            in.free(&in, sample);
        }
    }
    in.free(&in, instrTable);

    in.close(&in);
    out.close(&out);
    return true;
}

bool decodeOpusS3M(const char* inpath, const char* outpath) {

}
