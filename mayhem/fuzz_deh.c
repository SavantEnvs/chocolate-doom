// DeHackEd patch parser (src/deh_*.c, src/doom/deh_*.c). File-input target: the parser reports
// errors through I_Error() -> exit(), so each input must be its own process.
// Usage: fuzz_deh <patch.deh>
#include <stdio.h>
#include <stdlib.h>

#include "deh_main.h"
#include "m_argv.h"
#include "z_zone.h"

int main(int argc, char **argv)
{
    static char *fake_argv[] = {"fuzz_deh", "-nogui", NULL};

    if (argc < 2)
    {
        fprintf(stderr, "usage: %s patch.deh\n", argv[0]);
        return 2;
    }

    myargc = 2;
    myargv = fake_argv;
    Z_Init();
    DEH_LoadFile(argv[1]);
    return 0;
}
