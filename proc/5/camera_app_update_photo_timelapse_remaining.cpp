#include <stdint.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_app_update_photo_timelapse_remaining(int a1)
{
    uint32_t out[17];
    uint32_t in[2];
    in[0] = 5;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 5, in, sizeof(in), out, sizeof(out), 10000))
        return -1;
    
    return 0;
}
