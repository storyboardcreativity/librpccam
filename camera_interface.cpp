#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/eventfd.h>

#include "amba_rpc.h"

struct camera_interface_priv_params_t
{
    void (*dsp_handler)(void *, void *);
    void *dsp_service;
    void (*aud_handler)(void *, void *);
    void *aud_service;
};

// sizeof() == 0x80
typedef struct camera_interface_internal_info
{
    int clnt;
    int device_handle__dsp_buf;
    int device_handle__aud_buf;
    int epoll_handle;
    int event_handle;
    uint32_t __unk_gap;
    epoll_event events[4];
    pthread_t thread;
    uint32_t is_event_dispatcher_processing;
    void* dsp_buf;
    uint32_t dsp_buf_len;
    void* aud_buf;
    uint32_t aud_buf_len;
    camera_interface_priv_params_t params;
} camera_interface_internal_info_t;

camera_interface_internal_info_t g_librpccam_priv;

void* librpccam_event_dispatcher(void*)
{
    auto self = pthread_self();
    auto priority_max = sched_get_priority_max(1);

    if (priority_max == -1)
    {
        printf("get priority failed : %s\n", strerror(errno));
    }
    else
    {
        sched_param param;
        param.sched_priority = priority_max;
        if (pthread_setschedparam(self, 1, &param))
            printf("[ng] set dispatcher thread priority to %d\n", priority_max);
        else
            printf("[ok] set dispatcher thread priority to %d\n", priority_max);
    }

    while (true)
    {
        if (!g_librpccam_priv.is_event_dispatcher_processing)
            return nullptr;
        
        auto event_count = epoll_wait(g_librpccam_priv.epoll_handle, g_librpccam_priv.events, 4, -1);
        if (event_count < 0)
        {
            fprintf(stderr, "data monitor epoll err : %s\n", strerror(errno));
            continue;
        }
        
        for (int i = 0; i < event_count; ++i)
        {
            if (g_librpccam_priv.events[i].data.fd != g_librpccam_priv.event_handle)
            {
                if ((g_librpccam_priv.events[i].events & 1) != 0)
                {
                    if (g_librpccam_priv.events[i].data.fd == g_librpccam_priv.device_handle__dsp_buf)
                    {
                        uint32_t buff[12];
                        if (read(g_librpccam_priv.events[i].data.fd, buff, sizeof(buff)) != sizeof(buff))
                            fprintf(stderr, "read dsp info failed : %s\n", strerror(errno));
                        else
                        {
                            switch(buff[0])
                            {
                                case 0:
                                    buff[4] += (uint32_t)g_librpccam_priv.dsp_buf;
                                    break;

                                case 1:
                                    buff[4] += (uint32_t)g_librpccam_priv.dsp_buf;
                                    buff[5] += (uint32_t)g_librpccam_priv.dsp_buf;
                                    break;

                                case 2:
                                    buff[3] += (uint32_t)g_librpccam_priv.dsp_buf;
                                    break;

                                default:
                                    continue;
                            }

                            if (!g_librpccam_priv.params.dsp_handler)
                                g_librpccam_priv.params.dsp_handler(buff, g_librpccam_priv.params.dsp_service);
                        }
                    }
                    else if (g_librpccam_priv.events[i].data.fd == g_librpccam_priv.device_handle__aud_buf)
                    {
                        uint32_t buff[12];
                        if (read(g_librpccam_priv.events[i].data.fd, buff, sizeof(buff)) != sizeof(buff))
                            fprintf(stderr, "read aud info failed : %s\n", strerror(errno));
                        else
                        {
                            switch(buff[0])
                            {
                                case 3:
                                    buff[4] += (uint32_t)g_librpccam_priv.aud_buf;
                                    break;

                                default:
                                    continue;
                            }
                            if (g_librpccam_priv.params.dsp_handler)
                                g_librpccam_priv.params.dsp_handler(buff, g_librpccam_priv.params.dsp_service);
                        }
                    }
                }
            }

            char buf[8];
            read(g_librpccam_priv.events[i].data.fd, buf, sizeof(buf));
            g_librpccam_priv.is_event_dispatcher_processing = 0;
        }
    }

    return nullptr;
}

void librpccam_prog_handler(void *ptr, AMBA_IPC_SVC_RESULT_s* svc_result)
{
    if (g_librpccam_priv.params.aud_handler)
        g_librpccam_priv.params.aud_handler(ptr, g_librpccam_priv.params.aud_service);

    svc_result->Status = AMBA_IPC_REPLY_SUCCESS;
    svc_result->Mode = AMBA_IPC_ASYNCHRONOUS;
}

int camera_init(camera_interface_priv_params_t *params)
{
    printf("librpccam %s %s\n", "Dec 5 2016", "16:22:23");
    memset(&g_librpccam_priv, 0x00, sizeof(g_librpccam_priv));

    if (params)
        g_librpccam_priv.params = *params;

    g_librpccam_priv.clnt = ambaipc_clnt_create(1, 0x1F000001, 1);
    if (g_librpccam_priv.clnt <= 0)
    {
        fprintf(stderr, "create rpc client failed\n");
        return -1;
    }

    auto epoll_handle = epoll_create(4);
    if (epoll_handle < 0)
    {
        fprintf(stderr, "create epoll fd failed %s\n", strerror(errno));
        ambaipc_svc_unregister(0x2F000001, 1);
        return -1;
    }

    fcntl(epoll_handle, F_SETFL, fcntl(epoll_handle, F_GETFL, 0) | 0x800);
    g_librpccam_priv.epoll_handle = epoll_handle;

    auto event_handle = eventfd(0, 0x80800);
    if (event_handle < 0)
    {
        fprintf(stderr, "create event fd failed %s\n", strerror(errno));
        close(g_librpccam_priv.epoll_handle);
        ambaipc_svc_unregister(0x2F000001, 1);
        return -1;
    }

    g_librpccam_priv.event_handle = event_handle;

    epoll_event event;
    event.events = 1;
    event.data.fd = g_librpccam_priv.event_handle;
    if (epoll_ctl(g_librpccam_priv.epoll_handle, 1, g_librpccam_priv.event_handle, &event) == -1)
    {
        fprintf(stderr, "failed to add event fd to epoll : %s\n", strerror(errno));
        close(g_librpccam_priv.event_handle);
        close(g_librpccam_priv.epoll_handle);
        ambaipc_svc_unregister(0x2F000001, 1);
        return -1;
    }

    uint32_t in[1];
    uint32_t out[2];

    // Bootstrap DSP
    in[0] = 0;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 1, in, sizeof(in), out, sizeof(out), 1000))
        fprintf(stderr, "bootstrap failed\n");
    else
    {
        g_librpccam_priv.dsp_buf_len = out[1];
        g_librpccam_priv.device_handle__dsp_buf = open64("/dev/dsp_buf", 0x80000);

        if (g_librpccam_priv.device_handle__dsp_buf < 0)
            fprintf(stderr, "open dsp_buf failed : %s\n", strerror(errno));
        else
        {
            g_librpccam_priv.dsp_buf = mmap64(nullptr, g_librpccam_priv.dsp_buf_len, PROT_READ, MAP_SHARED, g_librpccam_priv.device_handle__dsp_buf, 0);
            if (g_librpccam_priv.dsp_buf == (void*)-1)
                fprintf(stderr, "mmap dsp buf failed : %s\n", strerror(errno));
            else
            {
                fcntl(g_librpccam_priv.device_handle__dsp_buf, F_SETFL, fcntl(g_librpccam_priv.device_handle__dsp_buf, F_GETFL, 0) | 0x800);

                epoll_event event0;
                event0.events = 1;
                event0.data.fd = g_librpccam_priv.device_handle__dsp_buf;
                if (epoll_ctl(g_librpccam_priv.epoll_handle, 1, g_librpccam_priv.device_handle__dsp_buf, &event0) == -1)
                {
                    fprintf(stderr, "failed to add dsp buf fd to epoll : %s\n", strerror(errno));
                    munmap(g_librpccam_priv.dsp_buf, g_librpccam_priv.dsp_buf_len);
                    g_librpccam_priv.dsp_buf = nullptr;
                    close(g_librpccam_priv.device_handle__dsp_buf);
                    g_librpccam_priv.device_handle__dsp_buf = -1;
                }
                else
                    printf("dsp buf 0x%x\n", (unsigned int)g_librpccam_priv.dsp_buf);
            }
        }
    }

    // Bootstrap AUD
    in[0] = 1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 1, in, sizeof(in), out, sizeof(out), 1000))
    {
        fprintf(stderr, "bootstrap failed\n");
    }
    else
    {
        g_librpccam_priv.aud_buf_len = out[1];
        g_librpccam_priv.device_handle__aud_buf = open64("/dev/aud_buf", 0x80000);

        if (g_librpccam_priv.device_handle__aud_buf < 0)
            fprintf(stderr, "open aud_buf failed : %s\n", strerror(errno));
        else
        {
            g_librpccam_priv.aud_buf = mmap64(nullptr, g_librpccam_priv.aud_buf_len, PROT_READ, MAP_SHARED, g_librpccam_priv.device_handle__aud_buf, 0);
            if (g_librpccam_priv.aud_buf == (void*)-1)
                fprintf(stderr, "mmap aud buf failed : %s\n", strerror(errno));
            else
            {
                fcntl(g_librpccam_priv.device_handle__aud_buf, F_SETFL, fcntl(g_librpccam_priv.device_handle__aud_buf, F_GETFL, 0) | 0x800);

                epoll_event event0;
                event0.events = 1;
                event0.data.fd = g_librpccam_priv.device_handle__aud_buf;
                if (epoll_ctl(g_librpccam_priv.epoll_handle, 1, g_librpccam_priv.device_handle__aud_buf, &event0) == -1)
                {
                    fprintf(stderr, "failed to add aud buf fd to epoll : %s\n", strerror(errno));
                    munmap(g_librpccam_priv.aud_buf, g_librpccam_priv.aud_buf_len);
                    g_librpccam_priv.aud_buf = nullptr;
                    close(g_librpccam_priv.device_handle__aud_buf);
                    g_librpccam_priv.device_handle__aud_buf = -1;
                }
                else
                    printf("aud buf 0x%x\n", (unsigned int)g_librpccam_priv.aud_buf);
            }
        }
    }

    g_librpccam_priv.is_event_dispatcher_processing = 1;
    pthread_create(&g_librpccam_priv.thread, nullptr, librpccam_event_dispatcher, nullptr);

    AMBA_IPC_PROC_s proc;
    proc.Mode = AMBA_IPC_ASYNCHRONOUS;
    proc.Proc = librpccam_prog_handler;

    AMBA_IPC_PROG_INFO_s prog_info;
    prog_info.ProcNum = 1;
    prog_info.pProcInfo = &proc;
    ambaipc_svc_register(0x2F000001, 1, "rpc_lnx_svc", &prog_info, 1);

    uint32_t in_2[5];
    uint32_t out_2[1];
    in_2[0] = 1;
    ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in_2, sizeof(in_2), out_2, sizeof(out_2), 1000);

    return 0;
}

int camera_deinit()
{
    puts(">> camera deinit");
    ambaipc_svc_unregister(0x2F000001, 1);

    if (g_librpccam_priv.clnt > 0)
        ambaipc_clnt_destroy(g_librpccam_priv.clnt);

    uint64_t ev_data = 1;
    if (g_librpccam_priv.event_handle > 0 && write(g_librpccam_priv.event_handle, &ev_data, sizeof(ev_data)) != 8)
        fprintf(stderr, "write event fd error : %s\n", strerror(errno));

    if (g_librpccam_priv.thread)
        pthread_join(g_librpccam_priv.thread, nullptr);

    if (g_librpccam_priv.dsp_buf != nullptr)
        munmap(g_librpccam_priv.dsp_buf, g_librpccam_priv.dsp_buf_len);

    if (g_librpccam_priv.device_handle__dsp_buf > 0)
        close(g_librpccam_priv.device_handle__dsp_buf);

    if ( g_librpccam_priv.epoll_handle > 0 )
        close(g_librpccam_priv.epoll_handle);

    if ( g_librpccam_priv.event_handle > 0 )
        close(g_librpccam_priv.event_handle);
    
    puts("<< camera deinit");
    return 0;
}

// =================== PROC 3 ===================

int camera_enable_cvbs(int a1)
{
    uint32_t out[1];
    uint32_t in[4];
    in[0] = a1 ? 1 : 2;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 3, in, sizeof(in), out, sizeof(out), 1000))
        return -1;
    
    if (out[0])
        return -1;

    return 0;
}

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

int camera_zoom_ctrl(int a1, int a2, int a3)
{
    uint32_t out[1];
    uint32_t in[4];
    in[0] = 0;
    in[1] = a1;
    in[2] = a2;
    in[3] = a3;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 3, in, sizeof(in), out, sizeof(out), 1000))
        return -1;
    
    if (out[0])
        return -1;

    return 0;
}

// =================== PROC 5 ===================

int camera_app_update_photo_timelapse_remaining(int a1)
{
    uint32_t out[17];
    uint32_t in[2];
    in[0] = 5;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 5, in, sizeof(in), out, sizeof(out), 10000))
        return -1;
    
    return 0;
}

int camera_still_get_remaining()
{
    uint32_t out[17];
    uint32_t in[2];
    in[0] = 1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 5, in, sizeof(in), out, sizeof(out), 10000))
        return -1;
    
    return out[1];
}

int camera_still_is_doing_timelapse()
{
    uint32_t out[17];
    uint32_t in[2];
    in[0] = 4;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 5, in, sizeof(in), out, sizeof(out), 10000))
        return -1;
    
    return out[1];
}

int camera_still_multiple_master_stage0()
{
    uint32_t out[17];
    uint32_t in[2];
    in[0] = 7;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 5, in, sizeof(in), out, sizeof(out), 10000))
        return -1;
    
    return out[1];
}

int camera_still_multiple_master_stage1()
{
    uint32_t out[17];
    uint32_t in[2];
    in[0] = 8;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 5, in, sizeof(in), out, sizeof(out), 10000))
        return -1;
    
    return out[1];
}

int camera_still_start_timelapse(int a1)
{
    uint32_t out[17];
    uint32_t in[2];
    in[0] = 8;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 5, in, sizeof(in), out, sizeof(out), 10000))
        return -1;
    
    return 0;
}

int camera_still_stop_burst()
{
    uint32_t out[17];
    uint32_t in[2];
    in[0] = 6;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 5, in, sizeof(in), out, sizeof(out), 5000))
        return -1;
    
    return out[0];
}

int camera_still_stop_timelapse()
{
    uint32_t out[17];
    uint32_t in[2];
    in[0] = 3;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 5, in, sizeof(in), out, sizeof(out), 10000))
        return -1;
    
    return 0;
}

int camera_take_still_capture(int a1, int timeout)
{
    uint32_t out[17];
    uint32_t in[2];
    in[0] = 0;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 5, in, sizeof(in), out, sizeof(out), timeout))
        return -1;
    
    return out[0];
}

// =================== PROC 6 ===================

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

int camera_set_config(void *src)
{
    uint32_t out[1];
    uint32_t in[21];
    
    memcpy(&in[1], src, 80);

    in[0] = 2;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 6, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    if (out[0])
        return -1;

    return 0;
}

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

// =================== PROC 7 ===================

int camera_is_paused()
{
    uint32_t out[18];
    uint32_t in[3];
    in[0] = 9;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 5000) || out[0])
        return -1;
    
    return out[2];
}

int camera_is_playing()
{
    uint32_t out[18];
    uint32_t in[3];
    in[0] = 8;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 5000) || out[0])
        return -1;
    
    return out[2];
}

int camera_playback_backword(int a1)
{
    uint32_t out[18];
    uint32_t in[20];
    in[0] = 6;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 5000))
        return -1;

    if (out[0])
        return -1;
    
    return 0;
}

int camera_playback_forword(int a1)
{
    uint32_t out[18];
    uint32_t in[20];
    in[0] = 5;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 5000))
        return -1;

    if (out[0])
        return -1;
    
    return 0;
}

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

int camera_playback_pause()
{
    uint32_t out[18];
    uint32_t in[20];
    in[0] = 2;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 5000))
        return -1;

    if (out[0])
        return -1;
    
    return 0;
}

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

int camera_playback_resume()
{
    uint32_t out[18];
    uint32_t in[20];
    in[0] = 3;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 5000))
        return -1;

    if (out[0])
        return -1;
    
    return 0;
}

int camera_playback_show_singalview(const char *path)
{
    auto len = strlen(path) + 1;
    if (len > 64)
        return -1;

    uint32_t out[18];
    uint32_t in[20];
    memcpy(&in[1], path, len);
    printf(">> %s %s\n", "camera_playback_show_singalview", path);

    in[0] = 12;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    if (out[0])
        return -1;
    
    return 0;
}

int camera_playback_start(const char *path, int a2, int a3, int a4)
{
    auto len = strlen(path) + 1;
    if (len > 64)
        return -1;

    uint32_t out[18];
    uint32_t in[20];
    memcpy(&in[1], path, len);

    in[0] = 3;
    in[17] = a2;
    in[18] = a3;
    in[19] = a4;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 5000))
        return -1;

    if (out[0])
        return -1;
    
    return 0;
}

int camera_playback_step()
{
    uint32_t out[18];
    uint32_t in[20];

    in[0] = 4;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 5000))
        return -1;

    if (out[0])
        return -1;
    
    return 0;
}

int camera_playback_stop(int a1)
{
    uint32_t out[18];
    uint32_t in[20];
    
    in[0] = 1;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 5000))
        return -1;

    if (out[0])
        return -1;
    
    return 0;
}

int camera_playback_time_search(int a1)
{
    uint32_t out[18];
    uint32_t in[20];
    
    in[0] = 7;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 5000))
        return -1;

    if (out[0])
        return -1;
    
    return 0;
}

int camera_playback_zoom_ratio(int a1, int a2, int a3, int a4)
{
    uint32_t out[18];
    uint32_t in[20];
    
    in[0] = 13;
    in[1] = a1;
    in[2] = a2;
    in[3] = a3;
    in[4] = a4;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 7, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    if (out[0])
        return -1;
    
    return 0;
}

// =================== PROC 2 ===================

int camera_is_optimized_format()
{
    uint32_t out[1];

    uint32_t in[5];
    in[0] = 8;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 5000))
        return -1;
    
    return out[2];
}

int camera_abs_move(int a1, int a2, int a3)
{
    uint32_t out[1];
    uint32_t in[5];
    in[0] = 21;
    in[1] = a1;
    in[2] = a2;
    in[3] = a3;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 1000))
        return -1;
    
    return out[0];
}

int camera_delete_firmware()
{
    uint32_t out[1];
    uint32_t in[5];
    in[0] = 24;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    if (out[0])
        return -1;
    
    return 0;
}

int camera_disable_stream_data()
{
    uint32_t out[1];
    uint32_t in[5];
    in[0] = 5;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 2000))
        return -1;

    return out[0];
}

int camera_do_optimized_format()
{
    uint32_t out[1];
    uint32_t in[5];
    in[0] = 9;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    return out[0];
}

int camera_enable_stream_data()
{
    uint32_t out[1];
    uint32_t in[5];
    in[0] = 4;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 2000))
        return -1;

    return out[0];
}

int camera_format_card_exfat()
{
    uint32_t out[1];
    uint32_t in[5];
    in[0] = 22;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    return out[0];
}

int camera_format_card_fat32()
{
    uint32_t out[1];
    uint32_t in[5];
    in[0] = 23;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    return out[0];
}

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

int camera_get_mode()
{
    uint32_t out[1];
    uint32_t in[5];
    in[0] = 3;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 1000))
        return 6;

    return out[0];
}

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

int camera_powerdown()
{
    uint32_t out[1];
    uint32_t in[5];

    in[0] = 11;
    in[1] = 0;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    return out[0];
}

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

int camera_pt_home()
{
    printf("pt home");

    uint32_t out[1];
    uint32_t in[5];
    in[0] = 18;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 1000))
        return -1;

    return out[0];
}

int camera_pt_left(int a1)
{
    printf("pt left %d\n", a1);

    uint32_t out[1];
    uint32_t in[5];
    in[0] = 16;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 1000))
        return -1;

    return out[0];
}

int camera_pt_right(int a1)
{
    printf("pt reset");

    uint32_t out[1];
    uint32_t in[5];
    in[0] = 15;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 1000))
        return -1;

    return out[0];
}

int camera_pt_stop(int a1)
{
    printf("pt stop %d\n", a1);

    uint32_t out[1];
    uint32_t in[5];
    in[0] = 17;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 1000))
        return -1;

    return out[0];
}

int camera_pt_up(int a1)
{
    printf("pt up %d\n", a1);

    uint32_t out[1];
    uint32_t in[5];
    in[0] = 13;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 1000))
        return -1;

    return out[0];
}

int camera_reboot()
{
    uint32_t out[1];
    uint32_t in[5];
    in[0] = 11;
    in[1] = 1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    return out[0];
}

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

int camera_reset_settings()
{
    uint32_t out[1];
    uint32_t in[5];
    in[0] = 10;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    return out[0];
}

int camera_set_virtual_key(int a1)
{
    uint32_t out[1];
    uint32_t in[5];
    in[0] = 25;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    return out[0];
}

int camera_switch_mode(int a1)
{
    uint32_t out[1];
    uint32_t in[5];
    in[0] = 25;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 2, in, sizeof(in), out, sizeof(out), 8000))
        return -1;

    if (out[0])
        return -1;

    return 0;
}

// =================== PROC 4 ===================

int camera_is_record_paused(int a1)
{
    uint32_t out[17];
    uint32_t in[3];
    in[0] = 5;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 4, in, sizeof(in), out, sizeof(out), 5000))
        return -1;
    
    return out[0];
}

int camera_is_recording(int a1)
{
    uint32_t out[17];
    uint32_t in[3];
    in[0] = 4;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 4, in, sizeof(in), out, sizeof(out), 15000))
        return -1;
    
    return out[1];
}

int camera_video_int_capture_record()
{
    uint32_t out[17];
    uint32_t in[3];
    in[0] = 7;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 4, in, sizeof(in), out, sizeof(out), 10000))
        return -1;
    
    return out[0];
}

int camera_stop_video_recording(int a1)
{
    uint32_t out[17];
    uint32_t in[3];
    in[0] = 1;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 4, in, sizeof(in), out, sizeof(out), 15000))
        return -1;
    if (out[0])
        return -1;

    return out[0];
}

int camera_start_video_recording(int a1, int timeout)
{
    puts("camera_start_video_recording");

    uint32_t out[17];
    uint32_t in[3];
    in[0] = 0;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 4, in, sizeof(in), out, sizeof(out), timeout))
        return -1;
        
    return out[0];
}

int camera_resume_video_recording(int a1, int timeout)
{
    uint32_t out[1];
    uint32_t in[3];
    in[0] = 3;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 4, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    if (out[0])
        return -1;
        
    return 0;
}

int camera_recording_get_remaining()
{
    uint32_t out[17];
    uint32_t in[3];
    in[0] = 6;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 4, in, sizeof(in), out, sizeof(out), 15000))
        return -1;
        
    return out[1];
}

int camera_pause_video_recording(int a1)
{
    uint32_t out[1];
    uint32_t in[3];
    in[0] = 2;
    in[1] = a1;
    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 4, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    if (out[0])
        return -1;
        
    return 0;
}

int camera_config_stream(uint32_t* a1)
{
    uint32_t out[1];
    uint32_t in[3];

    if (*a1)
    {
        if (*a1 != 1)
            return -1;

        in[0] = 8;
    }
    else
    {
        in[0] = 9;
    }

    in[1] = a1[1];
    in[2] = a1[2];

    if (ambaipc_clnt_call(g_librpccam_priv.clnt, 4, in, sizeof(in), out, sizeof(out), 10000))
        return -1;

    if (out[0])
        return -1;
        
    return 0;
}
