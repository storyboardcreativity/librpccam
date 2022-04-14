#include <stdint.h>
#include <string.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_get_face_detection_result(void* a1)
{
    uint32_t out[41];
    uint32_t in[5];
    in[0] = 12;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 10000))
        return -1;
    
    if (a1 == nullptr)
        return -1;

    memcpy(a1, out, 164);
    return 0;
}
