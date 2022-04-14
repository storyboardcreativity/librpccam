#include <stdint.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_get_mode()
{
    uint32_t out[1];
    uint32_t in[5];
    in[0] = 3;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 1000))
        return 6;

    return out[0];
}
