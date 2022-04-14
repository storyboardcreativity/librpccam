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
#include "camera_interface.h"

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
