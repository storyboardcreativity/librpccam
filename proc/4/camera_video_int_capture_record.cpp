#include <stdint.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_video_int_capture_record()
{
    uint32_t out[17];
    uint32_t in[3];
    in[0] = 7;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 4, in, sizeof(in), out, sizeof(out), 10000))
        return -1;
    
    return out[0];
}
