#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "amba_rpc.h"
#include "camera_interface.h"

struct camera_playback_file_info_t
{
    uint32_t dword0;
    uint32_t dword4;
    uint32_t dword8;
    uint32_t dwordC;
};

int camera_playback_get_file_info(const char *path, camera_playback_file_info_t* a2)
{
    auto len = strlen(path) + 1;
    if (len > 64)
        return -1;

    uint32_t out[18];
    uint32_t in[20];
    memcpy(&in[1], path, len);
    printf(">> %s %s\n", "camera_playback_get_file_info", path);

    in[0] = 10;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    if (out[0])
        return -1;

    if (a2)
    {
        auto src = (camera_playback_file_info_t*)&out[2];
        for (int i = 0; i < 4; ++i)
        {
            a2[i].dword0 = src->dword0;
            a2[i].dword4 = src->dword4;
            a2[i].dword8 = src->dword8;
            a2[i].dwordC = src->dwordC;
        }
    }
    
    return 0;
}
