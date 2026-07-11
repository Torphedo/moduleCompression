#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct VirtualIO VirtualIO;
typedef bool (*VirtualIORead)(VirtualIO* io, void* buf, uint32_t size);
typedef const void* (*VirtualIOConstRead)(VirtualIO* io, uint32_t size);
typedef bool (*VirtualIOWrite)(VirtualIO* io, const void* buf, uint32_t size);
typedef uint32_t (*VirtualIOTell)(const VirtualIO* io);
typedef void (*VirtualIOSeek)(VirtualIO* io, uint32_t pos);
typedef void (*VirtualIOClose)(VirtualIO* io);
typedef void (*VirtualIOFree)(VirtualIO* io, const void* buf);

struct VirtualIO {
    void* ctx;
    VirtualIORead read; // Read data into a buffer, like fread()
    // Get an immutable buffer of data from the file. If the data is already in
    // memory, this allows for zero-copy reading of dynamically sized data.
    VirtualIOConstRead constRead;
    VirtualIOWrite write; // Write data into a buffer, like fwrite()
    VirtualIOTell tell;   // Get the current position, like ftell()
    VirtualIOSeek seek;   // Set the current position, like fseek() in SEEK_SET mode
    VirtualIOClose close; // Close the IO context
    // Free a buffer returned by constRead()
    // (may be a no-op if the implementation was able to avoid a copy)
    VirtualIOFree free;
};

// Read a value into a local variable, then write it into another IO stream
#define VIO_COPY_VALUE(in, out, local) (in)->read((in), &(local), sizeof(local)); (out)->write((out), &(local), sizeof(local))

// Seek 2 streams to the same offset
#define VIO_DUAL_SEEK(s1, s2, offset) (s1)->seek((s1), offset); (s2)->seek((s2), offset)


VirtualIO vioOpenPath(const char* path, bool writeMode);
VirtualIO vioOpenStdio(FILE* f);
VirtualIO vioOpenMemory(void* buf, uint32_t size);
VirtualIO vioOpenExpandableMemory(uint32_t initialSize);

const void* vioMemGetBuffer(VirtualIO io);
uint32_t vioMemGetBufferSize(VirtualIO io);
