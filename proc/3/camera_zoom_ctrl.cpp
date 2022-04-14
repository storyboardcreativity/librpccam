#include <stdint.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_zoom_ctrl(int a1, int a2, int a3)
{
    uint32_t out[1];
    uint32_t in[4];
    in[0] = 0;
    in[1] = a1;
    in[2] = a2;
    in[3] = a3;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 3, in, sizeof(in), out, sizeof(out), 1000))
        return -1;
    
    if (out[0])
        return -1;

    return 0;
}
