#include <stdint.h>
#include <stdio.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_pt_down(int a1)
{
    printf("pt down %d\n", a1);

    uint32_t out[1];
    uint32_t in[5];
    in[0] = 14;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 1000))
        return -1;

    return out[0];
}
