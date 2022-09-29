#ifndef __NAR_COMMON_H__
#define __NAR_COMMON_H__

#if _MSC_VER
// Custom NAR error codes

#define NAR_ERR_TRINITY         0x00000001
#define NAR_ERR_REG_OVERFLOW    0x00000002 
#define NAR_ERR_REG_CANT_FILL   0x00000004
#define NAR_ERR_REG_BELOW_ZERO  0x00000008
#define NAR_ERR_ALIGN 6
#define NAR_ERR_MAX_ITER 7

// all neccecary kernel information will be stored here, every time system boots
// driver will lookup for this file, if not present, it assumes fresh startup on system, if present
// it will parse it accordingly and resume tracking neccecary volumes

#define NARKERNEL_MAIN_FILENAME "NARKERNEL_METADATA"
#define NARKERNEL_RF_PREFIX "NARKERNEL_RF_" // Region file

#define NAR_GUID_STR_SIZE 96

#define NAR_MEMORYBUFFER_SIZE       (1024*64) //
#define NAR_MAX_VOLUME_COUNT        (12)
#define NAR_REGIONBUFFER_SIZE       (sizeof(*NarData.VolumeRegionBuffer)) //struct itself
#define NAR_VOLUMEREGIONBUFFERSIZE  (NAR_MAX_VOLUME_COUNT)*(NAR_REGIONBUFFER_SIZE)


#define NAR_INIT_MEMORYBUFFER(Buffer)           *(INT32*)(Buffer) = 2*sizeof(INT32)
#define NAR_MB_USED(Buffer)                     *(INT32*)(Buffer)
#define NAR_MB_PUSH(Buffer, Src, Size)          memcpy((char*)(Buffer) + NAR_MB_USED(Buffer), (Src), (Size)); *(INT32*)(Buffer) = (*(INT32*)(Buffer) + (Size));
#define NAR_MB_MARK_NOT_ENOUGH_SPACE(Buffer)    *(INT32*)((char*)(Buffer) + sizeof(INT32)) = TRUE;
#define NAR_MB_CLEAR_FLAGS(Buffer)              *(INT32*)((char*)(Buffer) + sizeof(INT32)) = 0;
#define NAR_MB_ERROR_FLAGS(Buffer)              *(INT32*)((char*)(Buffer) + sizeof(INT32))
#define NAR_MB_ERROR_OCCURED(Buffer)            NAR_MB_ERROR_FLAGS(Buffer)
#define NAR_MB_DATA(Buffer)                     (PVOID)((char*)(Buffer) + 2*sizeof(INT32))
#define NAR_MB_DATA_USED(Buffer)                (NAR_MB_USED(Buffer) - 2*sizeof(INT32))

#define NAR_BOOTFILE_NAME  "NARBOOTFILE"
#define NAR_BOOTFILE_W_NAME L"NARBOOTFILE"

#define NAR_KERNEL_INVALID_FILE_ID 0
#define NAR_KERNEL_MAX_FILE_ID 30
#define NAR_KERNEL_GEN_FILE_ID(c) ((char)(c) - 'A' + 1)

#endif // msvc, kernel build detection

// force 1 byte aligment
#pragma pack(push ,1)

struct nar_backup_id{
    union{
        unsigned long long Q;
        struct{
            unsigned short Year;
            unsigned char Month;
            unsigned char Day;
            unsigned char Hour;
            unsigned char Min;
            char Letter;
        };
    };
};


struct nar_log_thread_params {
    void* Data;
    size_t FileSize;
    long InternalError; // NTSTATUS = LONG
    int DataLen;
    int FileID;
    int ShouldFlush;
    int ShouldQueryFileSize;
    int ShouldDelete; // set's file size to 0
};

#ifndef __cplusplus
typedef struct nar_log_thread_params nar_log_thread_params;
#endif


#if NAR_KERNEL

#include <ntstrsafe.h>

inline void
NarLogThread(PVOID param);

inline NTSTATUS
NarWriteLogsToFile(nar_log_thread_params* tp, PETHREAD* OutTObject);

#else


#endif



typedef struct nar_backup_id nar_backup_id;

typedef struct _nar_boot_track_data{
    unsigned long long LastBackupOffset;
    char Letter;
    char Version;
    char BackupType;
    nar_backup_id BackupID;
}nar_boot_track_data;
#pragma pack(pop)


//
//  Name of minispy's communication server port
//

#define MINISPY_PORT_NAME                   L"\\MiniSpyPort"




#pragma warning(push)
#pragma warning(disable:4200) // disable warnings for structures with zero length arrays.
typedef enum NAR_COMMAND_TYPE {
    NarCommandType_GetVolumeLog,
    NarCommandType_DeleteVolume,
    NarCommandType_AddVolume,
    NarCommandType_FlushLog // Flushes all logs and returns current file size in bytes
}NAR_COMMAND_TYPE;

typedef struct NAR_COMMAND {
    
    NAR_COMMAND_TYPE Type;
    struct {
        wchar_t VolumeGUIDStr[49];     // Null terminated VolumeGUID string
    };
    char Letter; // neccecary for driver to calculate it's fileid
    
}NAR_COMMAND;

typedef struct NAR_LOG_INF{
    long long CurrentSize;
    int ErrorOccured;
}NAR_LOG_INF;

#pragma warning(pop)

typedef struct _NAR_CONNECTION_CONTEXT {
    unsigned long PID;           // PID of user mode application to prevent deadlock in write operations on same volumes. This PID will be filtered out in PREOPERATION callback
    int  OsDeviceID;    // parse QueryDeviceName
    int UserNameSize;  // In bytes, not characters.
    wchar_t* UserName;      // Null terminated USERname that is currently active
}NAR_CONNECTION_CONTEXT;


#endif /* __NAR_COMMON_H__ */

