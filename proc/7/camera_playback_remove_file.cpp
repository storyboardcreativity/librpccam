#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "amba_rpc.h"
#include "camera_interface.h"

int camera_playback_remove_file(const char *path)
{
    auto len = strlen(path) + 1;
    if (len > 64)
        return -1;

    uint32_t out[18];
    uint32_t in[20];
    memcpy(&in[1], path, len);
    printf(">> %s %s\n", "camera_playback_remove_file", path);

    in[0] = 11;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    if (out[0])
        return -1;
    
    return 0;
}
