/*++

Copyright (c) 1989-2002  Microsoft Corporation

Module Name:

    minispy.h

Abstract:

    Header file which contains the structures, type definitions,
    and constants that are shared between the kernel mode driver,
    minispy.sys, and the user mode executable, minispy.exe.

Environment:

    Kernel and user mode

--*/
#ifndef __MINISPY_H__
#define __MINISPY_H__

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

#define NAR_MEMORYBUFFER_SIZE       (1024*1024*1) // 1k
#define NAR_MAX_VOLUME_COUNT        (8)
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

// force 1 byte aligment
#pragma pack(push ,1)

typedef struct _nar_boot_track_data{
    UINT64 LastBackupOffset;
    char Letter;
    char Version;
    char BackupType;
}nar_boot_track_data;
#pragma pack(pop)

//
//  FltMgr's IRP major codes
//

#define IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION  ((UCHAR)-1)
#define IRP_MJ_RELEASE_FOR_SECTION_SYNCHRONIZATION  ((UCHAR)-2)
#define IRP_MJ_ACQUIRE_FOR_MOD_WRITE                ((UCHAR)-3)
#define IRP_MJ_RELEASE_FOR_MOD_WRITE                ((UCHAR)-4)
#define IRP_MJ_ACQUIRE_FOR_CC_FLUSH                 ((UCHAR)-5)
#define IRP_MJ_RELEASE_FOR_CC_FLUSH                 ((UCHAR)-6)
#define IRP_MJ_NOTIFY_STREAM_FO_CREATION            ((UCHAR)-7)

#define IRP_MJ_FAST_IO_CHECK_IF_POSSIBLE            ((UCHAR)-13)
#define IRP_MJ_NETWORK_QUERY_OPEN                   ((UCHAR)-14)
#define IRP_MJ_MDL_READ                             ((UCHAR)-15)
#define IRP_MJ_MDL_READ_COMPLETE                    ((UCHAR)-16)
#define IRP_MJ_PREPARE_MDL_WRITE                    ((UCHAR)-17)
#define IRP_MJ_MDL_WRITE_COMPLETE                   ((UCHAR)-18)
#define IRP_MJ_VOLUME_MOUNT                         ((UCHAR)-19)
#define IRP_MJ_VOLUME_DISMOUNT                      ((UCHAR)-20)

//
//  My own definition for transaction notify command
//

#define IRP_MJ_TRANSACTION_NOTIFY                   ((UCHAR)-40)


//
//  Version definition
//

#define MINISPY_MAJ_VERSION 2
#define MINISPY_MIN_VERSION 0

typedef struct _MINISPYVER {

    USHORT Major;
    USHORT Minor;

} MINISPYVER, * PMINISPYVER;

//
//  Name of minispy's communication server port
//

#define MINISPY_PORT_NAME                   L"\\MiniSpyPort"

//
//  Local definitions for passing parameters between the filter and user mode
//

typedef ULONG_PTR FILE_ID;
typedef _Return_type_success_(return >= 0) LONG NTSTATUS;

//
//  The maximum size of a record that can be passed from the filter
//

//#define RECORD_SIZE     1024
#define RECORD_SIZE     4096

//
//  This defines the type of record buffer this is along with certain flags.
//

#define RECORD_TYPE_NORMAL                       0x00000000
#define RECORD_TYPE_FILETAG                      0x00000004

#define RECORD_TYPE_FLAG_STATIC                  0x80000000
#define RECORD_TYPE_FLAG_EXCEED_MEMORY_ALLOWANCE 0x20000000
#define RECORD_TYPE_FLAG_OUT_OF_MEMORY           0x10000000
#define RECORD_TYPE_FLAG_MASK                    0xffff0000

//
//  The fixed data received for RECORD_TYPE_NORMAL
//

typedef struct _RECORD_DATA {

    LARGE_INTEGER OriginatingTime;
    LARGE_INTEGER CompletionTime;

    FILE_ID DeviceObject;
    FILE_ID FileObject;
    FILE_ID Transaction;

    FILE_ID ProcessId;
    FILE_ID ThreadId;

    ULONG_PTR Information;

    NTSTATUS Status;

    ULONG IrpFlags;
    ULONG Flags;

    UCHAR CallbackMajorId;
    UCHAR CallbackMinorId;
    UCHAR Reserved[2];      // Alignment on IA64

#pragma warning(push)
#pragma warning(disable:4201) // disable warnings for structures-unions without names


    struct {
        UINT32 S;
        UINT32 L;
    }P[5];
    UINT32 RecCount;
    UINT32 Error;



    ULONG EcpCount;
    ULONG KnownEcpMask;

} RECORD_DATA, * PRECORD_DATA;


#pragma warning(pop)


//
//  What information we actually log.
//

#pragma warning(push)
#pragma warning(disable:4200) // disable warnings for structures with zero length arrays.

typedef struct _LOG_RECORD {


    ULONG Length;           // Length of log record.  This Does not include
    ULONG SequenceNumber;   // space used by other members of RECORD_LIST

    ULONG RecordType;       // The type of log record this is.
    ULONG Reserved;         // For alignment on IA64

    RECORD_DATA Data;
    WCHAR Name[];           //  This is a null terminated string

} LOG_RECORD, * PLOG_RECORD;

#pragma warning(pop)

//
//  How the mini-filter manages the log records.
//

typedef struct _RECORD_LIST {

    LIST_ENTRY List;

    //
    // Must always be last item.  See MAX_LOG_RECORD_LENGTH macro below.
    // Must be aligned on PVOID boundary in this structure. This is because the
    // log records are going to be packed one after another & accessed directly
    // Size of log record must also be multiple of PVOID size to avoid alignment
    // faults while accessing the log records on IA64
    //

    LOG_RECORD LogRecord;

} RECORD_LIST, * PRECORD_LIST;

//
//  Defines the commands between the utility and the filter
//


//
//  Defines the command structure between the utility and the filter.
//

#pragma warning(push)
#pragma warning(disable:4200) // disable warnings for structures with zero length arrays.

typedef enum NAR_COMMAND_TYPE {
    NarCommandType_GetVolumeLog,
    NarCommandType_DeleteVolume,
    NarCommandType_AddVolume
}NAR_COMMAND_TYPE;

typedef struct NAR_COMMAND {

    NAR_COMMAND_TYPE Type;
    struct {
        WCHAR VolumeGUIDStr[49];     // Null terminated VolumeGUID string
    };

}NAR_COMMAND;

#pragma warning(pop)

typedef struct _NAR_CONNECTION_CONTEXT {
    ULONG  PID;           // PID of user mode application to prevent deadlock in write operations on same volumes. This PID will be filtered out in PREOPERATION callback
    INT32  OsDeviceID;    // parse QueryDeviceName
    INT32  UserNameSize;  // In bytes, not characters.
    WCHAR* UserName;      // Null terminated USERname that is currently active
}NAR_CONNECTION_CONTEXT;



//
//  The maximum number of BYTES that can be used to store the file name in the
//  RECORD_LIST structure
//

#define MAX_NAME_SPACE ROUND_TO_SIZE( (RECORD_SIZE - sizeof(RECORD_LIST)), sizeof( PVOID ))

//
//  The maximum space, in bytes and WCHARs, available for the name (and ECP
//  if present) string, not including the space that must be reserved for a NULL
//

#define MAX_NAME_SPACE_LESS_NULL (MAX_NAME_SPACE - sizeof(UNICODE_NULL))
#define MAX_NAME_WCHARS_LESS_NULL MAX_NAME_SPACE_LESS_NULL / sizeof(WCHAR)

//
//  Returns the number of BYTES unused in the RECORD_LIST structure.  Note that
//  LogRecord->Length already contains the size of LOG_RECORD which is why we
//  have to remove it.
//

#define REMAINING_NAME_SPACE(LogRecord) \
(FLT_ASSERT((LogRecord)->Length >= sizeof(LOG_RECORD)), \
 (USHORT)(MAX_NAME_SPACE - ((LogRecord)->Length - sizeof(LOG_RECORD))))

#define MAX_LOG_RECORD_LENGTH  (RECORD_SIZE - FIELD_OFFSET( RECORD_LIST, LogRecord ))


//
//  Macros available in kernel mode which are not available in user mode
//

#ifndef Add2Ptr
#define Add2Ptr(P,I) ((PVOID)((PUCHAR)(P) + (I)))
#endif

#ifndef ROUND_TO_SIZE
#define ROUND_TO_SIZE(_length, _alignment)    \
(((_length) + ((_alignment)-1)) & ~((_alignment) - 1))
#endif

#ifndef FlagOn
#define FlagOn(_F,_SF)        ((_F) & (_SF))
#endif

#endif /* __MINISPY_H__ */

