#include <stdint.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_delete_firmware()
{
    uint32_t out[1];
    uint32_t in[5];
    in[0] = 24;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    if (out[0])
        return -1;
    
    return 0;
}
