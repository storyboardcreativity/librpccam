#include <stdint.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_playback_forword(int a1)
{
    uint32_t out[18];
    uint32_t in[20];
    in[0] = 5;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 5000))
        return -1;

    if (out[0])
        return -1;
    
    return 0;
}
