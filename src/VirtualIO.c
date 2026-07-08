#include "VirtualIO.h"
#include <stdlib.h>
#include <string.h>

bool vioStdioRead(VirtualIO* io, void* buf, uint32_t size) {
    return fread(buf, size, 1, io->ctx) == 1;
}

const void* vioStdioConstRead(VirtualIO* io, uint32_t size) {
    void* buf = calloc(1, size);
    io->read(io, buf, size);
    return buf;
}

bool vioStdioWrite(VirtualIO* io, const void* buf, uint32_t size) {
    return fwrite(buf, size, 1, io->ctx) == 1;
}

uint32_t vioStdioTell(const VirtualIO* io) {
    return ftell(io->ctx);
}

void vioStdioSeek(VirtualIO* io, uint32_t pos) {
    fseek(io->ctx, pos, SEEK_SET);
}

void vioStubClose(VirtualIO* io) {
    return;
}

void vioStdioClose(VirtualIO* io) {
    if (io->ctx) {
        fclose(io->ctx);
    }
}

void vioStdioFree(VirtualIO* io, const void* buf) {
    free((void*)buf);
}

VirtualIO vioOpenPath(const char* path, bool writeMode) {
    const char* mode = writeMode ? "wb" : "rb";
    FILE* f = fopen(path, mode);
    VirtualIO io = vioOpenStdio(f);
    io.close = vioStdioClose; // Actually close the file when done
    return io;
}

VirtualIO vioOpenStdio(FILE* f) {
    VirtualIO io = {
        .ctx = f,
        .close = vioStubClose,
        .write = vioStdioWrite,
        .read = vioStdioRead,
        .seek = vioStdioSeek,
        .tell = vioStdioTell,
        .constRead = vioStdioConstRead,
        .free = vioStdioFree,
    };
    return io;
}

typedef struct {
    void* buf;
    uint32_t size;
    uint32_t pos;
}VirtualIOMemCtx;

uint32_t vioMemSafeSize(const VirtualIOMemCtx* ctx, uint32_t size) {
    if (ctx->pos >= ctx->size) {
        return 0;
    }
    const uint32_t remaining = ctx->size - ctx->pos;
    if (size > remaining) {
        return remaining;
    }
    return size;
}

void vioMemAdvance(VirtualIOMemCtx* ctx, uint32_t size) {
    ctx->pos += size;
    if (ctx->pos > size) {
        ctx->pos = size;
    }
}

bool vioMemRead(VirtualIO* io, void* buf, uint32_t size) {
    VirtualIOMemCtx* ctx = io->ctx;
    const void* pos = (void*)((uintptr_t)ctx->buf + ctx->pos);
    const uint32_t copySize = vioMemSafeSize(ctx, size);
    memcpy(buf, pos, copySize);
    vioMemAdvance(ctx, copySize);
    return copySize > 0;
}

const void* vioMemConstRead(VirtualIO* io, uint32_t size) {
    VirtualIOMemCtx* ctx = io->ctx;
    const void* pos = (void*)((uintptr_t)ctx->buf + ctx->pos);
    vioMemAdvance(ctx, size);
    return pos;
}

bool vioMemWrite(VirtualIO* io, const void* buf, uint32_t size) {
    VirtualIOMemCtx* ctx = io->ctx;
    void* pos = (void*)((uintptr_t)ctx->buf + ctx->pos);
    const uint32_t copySize = vioMemSafeSize(ctx, size);
    memcpy(pos, buf, copySize);
    vioMemAdvance(ctx, copySize);
    return false;
}

uint32_t vioMemTell(const VirtualIO* io) {
    const VirtualIOMemCtx* ctx = io->ctx;
    return ctx->pos;
}

void vioMemSeek(VirtualIO* io, uint32_t pos) {
    VirtualIOMemCtx* ctx = io->ctx;
    ctx->pos = 0;
    vioMemAdvance(ctx, pos); // This does bounds checking for us
}


void vioStubFree(VirtualIO* io, const void* buf) {
}

VirtualIO vioOpenMemory(void* buf, uint32_t size) {
    VirtualIOMemCtx* ctx = malloc(sizeof(*ctx));
    if (!ctx) {
        VirtualIO io = {0};
        return io;
    }
    VirtualIO io = {
        .ctx = ctx,
        .close = vioStubClose,
        .write = vioMemWrite,
        .read = vioMemRead,
        .seek = vioMemSeek,
        .tell = vioMemTell,
        .constRead = vioMemConstRead,
        .free = vioStubFree,
    };
    return io;
}