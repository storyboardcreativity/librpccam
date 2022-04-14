#include <stdint.h>
#include <stdio.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_rel_move(int a1, int a2, int a3)
{
    printf("pt rel %d, %d, %d\n", a1, a2, a3);

    uint32_t out[1];
    uint32_t in[5];
    in[0] = 20;
    in[1] = a1;
    in[2] = a2;
    in[3] = a3;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 1000))
        return -1;

    return out[0];
}
