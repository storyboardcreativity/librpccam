#include <stdint.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_is_paused()
{
    uint32_t out[18];
    uint32_t in[3];
    in[0] = 9;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 5000) || out[0])
        return -1;
    
    return out[2];
}
