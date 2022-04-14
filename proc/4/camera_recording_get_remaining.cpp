#include <stdint.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_recording_get_remaining()
{
    uint32_t out[17];
    uint32_t in[3];
    in[0] = 6;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 4, in, sizeof(in), out, sizeof(out), 15000))
        return -1;
        
    return out[1];
}
