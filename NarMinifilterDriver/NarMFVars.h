#ifndef _NARMFVARS_
#define _NARMFVARS_

#define BIT_CHECK(var,pos) ((var) & (1<<(pos)))

#define IS_BUFFER_OVERFLOWED(var) BIT_CHECK(var,7)

#define NAR_PORT_NAME L"\\NarMiniFilterPort"
#define MAX_NAR_CLUSTER_MAP_COUNT 128

#define MAX_NAR_SINGLE_LOG_COUNT 128

#define MAX_NAME_CHAR_COUNT 128


enum ERRORS {
  NE_KERNEL_NO_MEMORY,
  NE_OUTPUT_BUFFER_NO_SIZE,
  NE_RETRIEVAL_POINTERS,
  NE_REGION_OVERFLOW,
  NE_BREAK_WITHOUT_LOG,
  NE_ANSI_UNICODE_CONVERSION,
  NE_GETFILENAMEINF_FUNC_FAILED,
  NE_COMPARE_FUNC_FAILED,
  NE_PAGEFILE_FOUND,
  NE_UNDEFINED
};


#ifdef _NAR_KERNEL
#define PushFsRecordToLog(Log) { KIRQL OldIrql; KeAcquireSpinLock(&GlobalNarConnectionData.SpinLock, &OldIrql); \
PushFsRecord((Log)); \
KeReleaseSpinLock(&GlobalNarConnectionData.SpinLock, OldIrql); \
} 

typedef struct _nar_connection_data {
  PFLT_FILTER FilterHandle;
  PFLT_PORT ServerPort;
  PFLT_PORT ClientPort;
  KSPIN_LOCK SpinLock;
}nar_connection_data; nar_connection_data GlobalNarConnectionData;
#endif

enum OpStatus {
  OpSuccess,
  OpFailed,
  Reserved,
  Empty
};
/*
 Prefetch operation info @preoperation callback, then @postoperation check if operation succeeded, if yes 
 mark it or? move it to the _record_to_send_buffer_
 
 I may need to store file_id and it's position at the prefetched array.
*/

enum BufferType {
  NarInf ,
  NarError
};


#pragma warning(push)
#pragma warning(disable:4201) 
typedef struct _nar_record {
  union {//16 byte
    struct {
      ULONGLONG Start; //start of the operation in context of LCN
      ULONGLONG OperationSize; // End of operation, in LCN //TODO change variable name
    };
    struct {
      ULONGLONG Err;
      ULONGLONG Reserved;
    };
  };
  WCHAR Name[MAX_NAME_CHAR_COUNT];
  enum BufferType Type; //Error or information
} nar_record;
#pragma warning(pop)


typedef struct _nar_log { // Align 64byte? 
  UINT32 Count; //leftmost bit is used for overflow flag
  nar_record Record[MAX_NAR_SINGLE_LOG_COUNT];
}nar_log; nar_log GlobalFileLog;


#endif

