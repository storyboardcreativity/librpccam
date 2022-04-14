#include <stdint.h>
#include <string.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_set_config(void *src)
{
    uint32_t out[1];
    uint32_t in[21];
    
    memcpy(&in[1], src, 80);

    in[0] = 2;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 6, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    if (out[0])
        return -1;

    return 0;
}
