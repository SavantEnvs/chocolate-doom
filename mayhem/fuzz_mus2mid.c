// Fuzzes the MUS -> MIDI converter (src/mus2mid.c) on arbitrary byte strings.
#include <stddef.h>
#include <stdint.h>

#include "memio.h"
#include "mus2mid.h"
#include "z_zone.h"

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    Z_Init();
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    MEMFILE *in = mem_fopen_read((void *) data, size);
    MEMFILE *out = mem_fopen_write();
    boolean failed = mus2mid(in, out);

    mem_fclose(in);
    mem_fclose(out);

    // mus2mid() returns true on error: reject so such inputs are not kept.
    return failed ? -1 : 0;
}
