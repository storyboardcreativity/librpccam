#include <stdint.h>
#include <stdio.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_start_video_recording(int a1, int timeout)
{
    puts("camera_start_video_recording");

    uint32_t out[17];
    uint32_t in[3];
    in[0] = 0;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 4, in, sizeof(in), out, sizeof(out), timeout))
        return -1;
        
    return out[0];
}
