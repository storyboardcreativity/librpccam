#include <stdint.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_take_still_capture(int a1, int timeout)
{
    uint32_t out[17];
    uint32_t in[2];
    in[0] = 0;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 5, in, sizeof(in), out, sizeof(out), timeout))
        return -1;
    
    return out[0];
}
