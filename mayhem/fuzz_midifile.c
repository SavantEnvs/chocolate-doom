// Fuzzes the Standard MIDI File loader and event iterator (src/midifile.c).
// MIDI_LoadFile() only takes a path, so each input is staged in a private temp file.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "doomtype.h"
#include "midifile.h"
#include "z_zone.h"

static char template_path[4096];

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    const char *tmp = getenv("TMPDIR");
    int fd;

    Z_Init();
    snprintf(template_path, sizeof(template_path), "%s/fuzz_midifile.XXXXXX",
             tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    fd = mkstemp(template_path);
    if (fd < 0)
    {
        return 1;
    }
    close(fd);
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    midi_file_t *file;
    unsigned int track;
    FILE *f = fopen(template_path, "wb");

    if (f == NULL)
    {
        abort();
    }
    fwrite(data, 1, size, f);
    fclose(f);

    file = MIDI_LoadFile(template_path);
    if (file == NULL)
    {
        return -1;
    }

    for (track = 0; track < MIDI_NumTracks(file); ++track)
    {
        midi_track_iter_t *iter = MIDI_IterateTrack(file, track);
        midi_event_t *event;

        MIDI_GetDeltaTime(iter);
        while (MIDI_GetNextEvent(iter, &event))
        {
            MIDI_GetDeltaTime(iter);
        }
        MIDI_FreeIterator(iter);
    }

    MIDI_FreeFile(file);
    return 0;
}
