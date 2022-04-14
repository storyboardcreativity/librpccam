#include <stdint.h>
#include <string.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_playback_start(const char *path, int a2, int a3, int a4)
{
    auto len = strlen(path) + 1;
    if (len > 64)
        return -1;

    uint32_t out[18];
    uint32_t in[20];
    memcpy(&in[1], path, len);

    in[0] = 3;
    in[17] = a2;
    in[18] = a3;
    in[19] = a4;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 5000))
        return -1;

    if (out[0])
        return -1;
    
    return 0;
}
