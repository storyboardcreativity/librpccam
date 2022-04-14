#include <stdint.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_playback_zoom_ratio(int a1, int a2, int a3, int a4)
{
    uint32_t out[18];
    uint32_t in[20];
    
    in[0] = 13;
    in[1] = a1;
    in[2] = a2;
    in[3] = a3;
    in[4] = a4;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    if (out[0])
        return -1;
    
    return 0;
}
