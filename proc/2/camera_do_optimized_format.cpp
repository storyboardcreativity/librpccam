#include <stdint.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_do_optimized_format()
{
    uint32_t out[1];
    uint32_t in[5];
    in[0] = 9;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    return out[0];
}
