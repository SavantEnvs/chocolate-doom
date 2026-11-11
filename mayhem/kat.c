// Known-answer probe built with the project's NORMAL flags from upstream's own sources.
// mayhem/test.sh feeds it fixed inputs from mayhem/kat/ and compares its stdout to exact
// expected values. Subcommands: mus <f> | midi <f> | wad <f> | deh <f> | net
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "deh_main.h"
#include "doom/info.h"
#include "m_argv.h"
#include "memio.h"
#include "midifile.h"
#include "mus2mid.h"
#include "net_defs.h"
#include "net_packet.h"
#include "w_wad.h"
#include "z_zone.h"

extern int maxammo[];

static int cmd_mus(const char *path)
{
    FILE *f = fopen(path, "rb");
    static unsigned char buf[65536];
    size_t n, i, outlen;
    void *outbuf;
    MEMFILE *in, *out;

    if (f == NULL) return 1;
    n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    in = mem_fopen_read(buf, n);
    out = mem_fopen_write();
    if (mus2mid(in, out)) { printf("mus2mid: error\n"); return 1; }
    mem_get_buf(out, &outbuf, &outlen);
    printf("mid %zu:", outlen);
    for (i = 0; i < outlen; ++i) printf(" %02x", ((unsigned char *) outbuf)[i]);
    printf("\n");
    return 0;
}

static int cmd_midi(const char *path)
{
    midi_file_t *file = MIDI_LoadFile((char *) path);
    unsigned int t;

    if (file == NULL) { printf("midi: load failed\n"); return 1; }
    printf("tracks=%u division=%u\n", MIDI_NumTracks(file), MIDI_GetFileTimeDivision(file));
    for (t = 0; t < MIDI_NumTracks(file); ++t)
    {
        midi_track_iter_t *it = MIDI_IterateTrack(file, t);
        midi_event_t *ev;
        unsigned int dt = MIDI_GetDeltaTime(it);
        printf("track %u:", t);
        while (MIDI_GetNextEvent(it, &ev))
        {
            if (ev->event_type == MIDI_EVENT_META)
                printf(" [dt=%u meta=%02x len=%u]", dt, ev->data.meta.type, ev->data.meta.length);
            else
                printf(" [dt=%u ev=%02x ch=%u p1=%u p2=%u]", dt, ev->event_type,
                       ev->data.channel.channel, ev->data.channel.param1, ev->data.channel.param2);
            dt = MIDI_GetDeltaTime(it);
        }
        printf("\n");
        MIDI_FreeIterator(it);
    }
    MIDI_FreeFile(file);
    return 0;
}

static int cmd_wad(const char *path)
{
    unsigned int i;
    char name[9];
    const char *foo;

    if (W_AddFile(path) == NULL) { printf("wad: open failed\n"); return 1; }
    printf("numlumps=%u\n", numlumps);
    for (i = 0; i < numlumps; ++i)
    {
        memset(name, 0, sizeof(name));
        memcpy(name, lumpinfo[i]->name, 8);
        printf("lump %u %s size=%d\n", i, name, W_LumpLength(i));
    }
    foo = W_CacheLumpName("FOO", PU_STATIC);
    printf("FOO=%.5s BAR_index=%d NOPE_index=%d\n", foo, W_CheckNumForName("BAR"), W_CheckNumForName("NOPE"));
    return 0;
}

static int cmd_deh(const char *path)
{
    DEH_LoadFile(path);
    printf("player_health=%d frame3_tics=%d maxammo0=%d\n",
           mobjinfo[MT_PLAYER].spawnhealth, (int) states[3].tics, maxammo[0]);
    return 0;
}

static int cmd_net(void)
{
    static const unsigned char raw[] = {0x12, 0x34, 'd', 'o', 'o', 'm', 0, 0xff, 0xfe};
    net_packet_t *p = NET_NewPacket(sizeof(raw));
    unsigned int u16, u8;
    signed int s8;
    char *s;

    memcpy(p->data, raw, sizeof(raw));
    p->len = sizeof(raw);
    if (!NET_ReadInt16(p, &u16)) return 1;
    s = NET_ReadString(p);
    if (s == NULL) return 1;
    printf("int16=%u str=%s", u16, s);
    if (!NET_ReadSInt8(p, &s8)) return 1;
    printf(" sint8=%d", s8);
    if (!NET_ReadInt8(p, &u8)) return 1;
    printf(" int8=%u", u8);
    printf(" more=%d\n", NET_ReadInt8(p, &u8));
    NET_FreePacket(p);
    return 0;
}

int main(int argc, char **argv)
{
    static char *fake_argv[] = {"kat", "-nogui", NULL};

    myargc = 2;
    myargv = fake_argv;
    Z_Init();
    if (argc < 2) return 2;
    if (!strcmp(argv[1], "mus") && argc > 2) return cmd_mus(argv[2]);
    if (!strcmp(argv[1], "midi") && argc > 2) return cmd_midi(argv[2]);
    if (!strcmp(argv[1], "wad") && argc > 2) return cmd_wad(argv[2]);
    if (!strcmp(argv[1], "deh") && argc > 2) return cmd_deh(argv[2]);
    if (!strcmp(argv[1], "net")) return cmd_net();
    return 2;
}
