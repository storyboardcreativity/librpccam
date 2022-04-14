#include <stdint.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_still_stop_timelapse()
{
    uint32_t out[17];
    uint32_t in[2];
    in[0] = 3;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 5, in, sizeof(in), out, sizeof(out), 10000))
        return -1;
    
    return 0;
}
