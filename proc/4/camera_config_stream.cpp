#include <stdint.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_config_stream(uint32_t* a1)
{
    uint32_t out[1];
    uint32_t in[3];

    if (*a1)
    {
        if (*a1 != 1)
            return -1;

        in[0] = 8;
    }
    else
    {
        in[0] = 9;
    }

    in[1] = a1[1];
    in[2] = a1[2];

    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 4, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    if (out[0])
        return -1;
        
    return 0;
}
