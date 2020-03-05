#ifndef _NARMFVARS_
#define _NARMFVARS_

#define BIT_CHECK(var,pos) ((var) & (1<<(pos)))
#define BIT_SET(var,pos) ((var) | (1<<(pos)))
#define IS_BUFFER_OVERFLOWED(var) BIT_CHECK(var,7)


#define NAR_PORT_NAME L"\\NarMiniFilterPort"

#define MAX_NAR_RECORD_COUNT 32 // this parameter hardcoded in user-mode, care when changing it 
#define MAX_NAME_CHAR_COUNT 128

#define MAX_BUFFER_SIZE (sizeof(nar_record)*MAX_NAR_RECORD_COUNT) // this parameter hardcoded in user-mode, care when changing it 

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
  NE_MAX_ITER_EXCEEDED,
  NE_MFT_ENTRY,
  NE_UNDEFINED
};

enum REQUEST_TYPE {
  START_FILTERING,
  STOP_FILTERING,
  GET_RECORDS,
  RESET_BUFFER
};

#ifdef _NAR_KERNEL
typedef struct _nar_global_conf_data {
  PFLT_FILTER FilterHandle;
  PFLT_PORT ServerPort;
  PFLT_PORT ClientPort;
  KSPIN_LOCK SpinLock;
  BOOLEAN ShouldFilter;
}nar_global_conf_data; nar_global_conf_data GlobalNarConfiguration;

#define PushFsRecordToLog(Log) { KIRQL OldIrql; KeAcquireSpinLock(&GlobalNarConfiguration.SpinLock, &OldIrql); \
PushFsRecord((Log)); \
KeReleaseSpinLock(&GlobalNarConfiguration.SpinLock, OldIrql); \
} 
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
  ULONGLONG Temp[4];
  enum BufferType Type; //Error or information
} nar_record;
#pragma warning(pop)


typedef struct _nar_log { // Align 64byte? 
  UINT32 Count; //leftmost bit is used for overflow flag
  BOOLEAN Overflowed;
  nar_record Record[MAX_NAR_RECORD_COUNT];
}nar_log; nar_log GlobalFileLog;


#endif

