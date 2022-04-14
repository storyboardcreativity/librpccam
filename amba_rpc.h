#pragma once

// Based on: https://github.com/qdxavier/amba_rpc

typedef enum _AMBA_IPC_COMMUNICATION_MODE_e {
  
  AMBA_IPC_SYNCHRONOUS = 0,
  AMBA_IPC_ASYNCHRONOUS,
  AMBA_IPC_MODE_MAX = 0xFFFFFFFF
  
} AMBA_IPC_COMMUNICATION_MODE_e;

typedef enum _AMBA_IPC_REPLY_STATUS_e_ {
	AMBA_IPC_REPLY_SUCCESS = 0,
	AMBA_IPC_REPLY_PROG_UNAVAIL,
	AMBA_IPC_REPLY_PARA_INVALID,
	AMBA_IPC_REPLY_SYSTEM_ERROR,
	AMBA_IPC_REPLY_TIMEOUT,
	AMBA_IPC_INVALID_CLIENT_ID,
	AMBA_IPC_UNREGISTERED_SERVER,
	AMBA_IPC_REREGISTERED_SERVER,
	AMBA_IPC_SERVER_MEM_ALLOC_FAILED,
	AMBA_IPC_IS_NOT_READY,
	AMBA_IPC_CRC_ERROR,
	AMBA_IPC_NETLINK_ERROR,
	AMBA_IPC_STATUS_NUM,
	AMBA_IPC_REPLY_MAX = 0xFFFFFFFF
} AMBA_IPC_REPLY_STATUS_e;

typedef struct _AMBA_IPC_SVC_RESULT_s {

  int Length;
  void *pResult;
  AMBA_IPC_COMMUNICATION_MODE_e Mode;
  AMBA_IPC_REPLY_STATUS_e Status;

} AMBA_IPC_SVC_RESULT_s;

typedef enum _AMBA_IPC_HOST_e {
  AMBA_IPC_HOST_LINUX = 0,
  AMBA_IPC_HOST_THREADX,
  AMBA_IPC_HOST_MAX
} AMBA_IPC_HOST_e;

/* function pointer prototype for svc procedure*/
typedef void (*AMBA_IPC_PROC_f)(void *, AMBA_IPC_SVC_RESULT_s*);

typedef struct _AMBA_IPC_PROC_s {

  AMBA_IPC_PROC_f Proc;
  AMBA_IPC_COMMUNICATION_MODE_e Mode;
  
} AMBA_IPC_PROC_s;

typedef struct _AMBA_IPC_PROG_INFO_s_ {
  
  int ProcNum;
  AMBA_IPC_PROC_s *pProcInfo;
  
} AMBA_IPC_PROG_INFO_s;

// ================================== API Definition ==================================

int ambaipc_svc_register(int prog, int vers, const char *name, AMBA_IPC_PROG_INFO_s *prog_info, int new_thread);
int ambaipc_svc_unregister(int program, int version);
int ambaipc_clnt_create(int host, int program, int version);   // host can be set by AMBA_IPC_HOST_LINUX, AMBA_IPC_HOST_THREADX, it shows the server is on the linux or threadx side.
int ambaipc_clnt_destroy(int clnt);
AMBA_IPC_REPLY_STATUS_e ambaipc_clnt_call(int clnt, int proc, void *in, int in_len, void *out, int out_len, unsigned int timeout);