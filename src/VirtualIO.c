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
    // Whether we control the allocation of the buffer, or the caller does. If
    // we don't control allocation, we can't expand it on writes
    bool expandableBuf;
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
    if (ctx->pos > ctx->size) {
        ctx->pos = ctx->size;
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
    if (ctx->expandableBuf) {
        const uint32_t requiredSize = ctx->pos + size;
        if (requiredSize >= ctx->size) {
            const uint32_t newSize = requiredSize * 1.5;
            ctx->buf = realloc(ctx->buf, newSize);
            if (!ctx->buf) {
                printf("Failed to expand buffer to %d bytes!\n");
                return false;
            }
            ctx->size = newSize;
        }
    }

    void* pos = (void*)((uintptr_t)ctx->buf + ctx->pos);
    const uint32_t copySize = vioMemSafeSize(ctx, size);
    memcpy(pos, buf, copySize);
    vioMemAdvance(ctx, copySize);
    return true;
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

void vioMemClose(VirtualIO* io) {
    VirtualIOMemCtx* ctx = io->ctx;
    if (ctx->expandableBuf) {
        free(ctx->buf);
    }
    return;
}

VirtualIO vioOpenMemory(void* buf, uint32_t size) {
    VirtualIOMemCtx* ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        VirtualIO io = {0};
        return io;
    }
    ctx->buf = buf;
    ctx->size = size;
    VirtualIO io = {
        .ctx = ctx,
        .close = vioMemClose,
        .write = vioMemWrite,
        .read = vioMemRead,
        .seek = vioMemSeek,
        .tell = vioMemTell,
        .constRead = vioMemConstRead,
        .free = vioStubFree,
    };
    return io;
}

VirtualIO vioOpenExpandableMemory(uint32_t initialSize) {
    void* buf = calloc(1, initialSize);
    if (!buf) {
        VirtualIO io = {0};
        return io;
    }
    VirtualIO io = vioOpenMemory(buf, initialSize);
    if (!io.ctx) {
        VirtualIO io = {0};
        return io;
    }
    VirtualIOMemCtx* ctx = io.ctx;
    ctx->expandableBuf = true;

    return io;
}

const void* vioMemGetBuffer(VirtualIO io) {
    VirtualIOMemCtx* ctx = io.ctx;
    return ctx->buf;
}

uint32_t vioMemGetBufferSize(VirtualIO io) {
    VirtualIOMemCtx* ctx = io.ctx;
    return ctx->size;
}
