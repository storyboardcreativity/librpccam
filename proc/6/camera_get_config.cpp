#include <stdint.h>
#include <string.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_get_config(int *a1)
{
    uint32_t out[20];
    uint32_t in[21];
    in[0] = 3;
    in[1] = *a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 6, in, sizeof(in), out, sizeof(out), 2000))
        return -1;
    
    memcpy(a1, out, 80);

    if (*a1 = 255)
        return -1;

    return 0;
}
