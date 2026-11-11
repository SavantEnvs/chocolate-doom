// WAD directory + lump loader (src/w_wad.c, src/w_file*.c), plus MUS lump conversion.
// File-input target: malformed WADs are reported through I_Error() -> exit().
// The file name must end in ".wad", otherwise W_AddFile() treats it as a single raw lump.
// Usage: fuzz_wad <input.wad>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m_argv.h"
#include "memio.h"
#include "mus2mid.h"
#include "w_wad.h"
#include "z_zone.h"

#define MAX_LUMPS_TOUCHED 4096

int main(int argc, char **argv)
{
    static char *fake_argv[] = {"fuzz_wad", "-nogui", NULL};
    unsigned int i;

    if (argc < 2)
    {
        fprintf(stderr, "usage: %s input.wad\n", argv[0]);
        return 2;
    }

    myargc = 2;
    myargv = fake_argv;
    Z_Init();

    if (W_AddFile(argv[1]) == NULL)
    {
        return 0;
    }

    W_GenerateHashTable();
    W_CheckNumForName("PLAYPAL");
    W_CheckNumForName("MAP01");

    for (i = 0; i < numlumps && i < MAX_LUMPS_TOUCHED; ++i)
    {
        int len = W_LumpLength(i);
        unsigned char *lump = W_CacheLumpNum(i, PU_STATIC);

        if (len > 4 && memcmp(lump, "MUS\x1a", 4) == 0)
        {
            MEMFILE *in = mem_fopen_read(lump, len);
            MEMFILE *out = mem_fopen_write();
            mus2mid(in, out);
            mem_fclose(in);
            mem_fclose(out);
        }
    }

    return 0;
}
