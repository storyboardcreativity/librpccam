#include <stdint.h>
#include <sys/epoll.h>

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

extern camera_interface_internal_info_t g_librpccam_priv;
