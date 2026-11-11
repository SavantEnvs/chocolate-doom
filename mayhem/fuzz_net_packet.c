// Fuzzes the network packet readers and structure decoders (src/net_packet.c, src/net_structrw.c)
// that process datagrams from remote clients, servers and LAN query responses.
// The first input byte selects which decoder runs on the rest of the packet.
#include <stdint.h>
#include <string.h>

#include "net_defs.h"
#include "net_packet.h"
#include "net_structrw.h"
#include "z_zone.h"

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    Z_Init();
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    net_packet_t *packet;

    if (size < 1)
    {
        return -1;
    }

    packet = NET_NewPacket(size);
    memcpy(packet->data, data + 1, size - 1);
    packet->len = size - 1;

    switch (data[0] % 14)
    {
        case 0:
        {
            net_connect_data_t v;
            memset(&v, 0, sizeof(v));
            NET_ReadConnectData(packet, &v);
            break;
        }
        case 1:
        {
            net_gamesettings_t v;
            memset(&v, 0, sizeof(v));
            NET_ReadSettings(packet, &v);
            break;
        }
        case 2:
        {
            net_querydata_t v;
            memset(&v, 0, sizeof(v));
            NET_ReadQueryData(packet, &v);
            break;
        }
        case 3:
        {
            net_ticdiff_t v;
            memset(&v, 0, sizeof(v));
            NET_ReadTiccmdDiff(packet, &v, data[0] & 0x80);
            break;
        }
        case 4:
        {
            net_full_ticcmd_t v;
            memset(&v, 0, sizeof(v));
            NET_ReadFullTiccmd(packet, &v, data[0] & 0x80);
            break;
        }
        case 5:
        {
            net_waitdata_t v;
            memset(&v, 0, sizeof(v));
            NET_ReadWaitData(packet, &v);
            break;
        }
        case 6:
        {
            sha1_digest_t v;
            NET_ReadSHA1Sum(packet, v);
            break;
        }
        case 7:
        {
            prng_seed_t v;
            NET_ReadPRNGSeed(packet, v);
            break;
        }
        case 8:
            NET_ReadProtocol(packet);
            break;
        case 9:
            NET_ReadProtocolList(packet);
            break;
        case 10:
            NET_ReadString(packet);
            break;
        case 11:
            NET_ReadSafeString(packet);
            break;
        default:
        {
            unsigned int u;
            signed int s;
            NET_ReadInt8(packet, &u);
            NET_ReadInt16(packet, &u);
            NET_ReadInt32(packet, &u);
            NET_ReadSInt8(packet, &s);
            NET_ReadSInt16(packet, &s);
            NET_ReadSInt32(packet, &s);
            break;
        }
    }

    NET_FreePacket(packet);
    return 0;
}
