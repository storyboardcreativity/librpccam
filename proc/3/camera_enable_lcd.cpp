#include <stdint.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_enable_lcd(int a1)
{
    uint32_t out[1];
    uint32_t in[4];
    in[0] = a1 ? 3 : 4;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 3, in, sizeof(in), out, sizeof(out), 1000))
        return -1;
    
    if (out[0])
        return -1;

    return 0;
}
