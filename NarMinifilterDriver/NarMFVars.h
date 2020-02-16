#ifndef _NARMFVARS_
#define _NARMFVARS_

#define BIT_CHECK(var,pos) ((var) & (1<<(pos)))

#define IS_BUFFER_OVERFLOWED(var) BIT_CHECK(var,7)

#define NAR_PORT_NAME L"\\NarMiniFilterPort"
#define MAX_NAR_CLUSTER_MAP_COUNT 128
#define MAX_NAR_BUFFER_SIZE (sizeof(LARGE_INTEGER)*MAX_NAR_CLUSTER_MAP_COUNT*2 + sizeof(ULONG) + sizeof(LARGE_INTEGER))

#define MAX_NAR_SINGLE_LOG_COUNT 64
PVOID GlobalNarBuffer;

enum FLAGS {
  NO_ERR,
  BUFFER_OVERFLOW,
  KERNEL_NO_MEMORY,
  OUTPUT_BUFFER_NO_SIZE
};

typedef struct _nar_connection_data {
  PFLT_FILTER FilterHandle;
  PFLT_PORT ServerPort;
  PFLT_PORT ClientPort;
  KSPIN_LOCK SpinLock;
}nar_connection_data; nar_connection_data GlobalNarConnectionData;

typedef struct _nar_record {
  ULONGLONG Start; //start of the operation in context of LCN
  ULONGLONG OperationSize; // End of operation, in LCN //TODO change variable name
}nar_record; nar_record GlobalNarFsRecord;

typedef struct _nar_log { // Align 64byte? 
  UINT32 Count; //leftmost bit is used for overflow flag
  nar_record Record[MAX_NAR_SINGLE_LOG_COUNT];
}nar_log; nar_log GlobalFileLog;


#endif

