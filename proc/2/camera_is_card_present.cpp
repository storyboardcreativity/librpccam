#include <stdint.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_is_card_present(int a1)
{
    uint32_t out[1];
    uint32_t in[5];

    if (a1 != 0 && a1 != 1)
        return -1;

    in[0] = a1 == 0 ? 6 : 7;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 5000))
        return -1;

    return out[0];
}
