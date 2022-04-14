#include <stdint.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_fetch_config_key(int *a1, int* a2)
{
    uint32_t in[21];

    if (a1 && !a2)
    {
        uint32_t out[1];

        in[0] = 0;
        out[0] = 0;
        auto res = ambaipc_clnt_call(g_librpccam_priv.clnt, 6, in, sizeof(in), out, sizeof(out), 5000);
        *a1 = out[0];
        if (res)
            return -1;
    }
    else if (a1 && a2)
    {
        in[0] = 1;
        if (ambaipc_clnt_call(g_librpccam_priv.clnt, 6, in, sizeof(in), a2, 4 * *a1, 5000))
            return -1;
        
        return 0;
    }

    return -1;
}
