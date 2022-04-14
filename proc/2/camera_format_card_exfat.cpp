#include <stdint.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_format_card_exfat()
{
    uint32_t out[1];
    uint32_t in[5];
    in[0] = 22;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    return out[0];
}