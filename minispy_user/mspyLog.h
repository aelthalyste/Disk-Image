/*++

Copyright (c) 1989-2002  Microsoft Corporation

Module Name:

    mspyLog.h

Abstract:

    This module contains the structures and prototypes used by the user
    program to retrieve and see the log records recorded by MiniSpy.sys.

Environment:

    User mode

--*/
#ifndef __MSPYLOG_H__
#define __MSPYLOG_H__

#include <stdio.h>
#include <fltUser.h>
#include "minispy.h"
#include <vector>
#include <fltUser.h>

#include <string>
#include <vector>
#include <atlbase.h>

#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <vsmgmt.h>

#define NAR_ERR_TRINITY 3
#define NAR_ERR_REG_OVERFLOW 4
#define NAR_ERR_REG_CANT_FILL 5
#define NAR_ERR_ALIGN 6
#define NAR_ERR_MAX_ITER 7
#define NAR_ERR_OVERFLOW 8


enum rec_or {
    LEFT = 0,
    RIGHT = 1,
    COLLISION = 2,
    OVERRUN = 3,
    SPLIT = 4,
};

typedef struct _record {
    UINT32 StartPos;
    UINT32 Len;
}nar_record, bitmap_region;

template<typename DATA_TYPE>
struct data_array {
    DATA_TYPE* Data;
    UINT Count;
    
    inline void Insert(DATA_TYPE Val) {
        Data = (DATA_TYPE*)realloc(Data, sizeof(Val) * (Count + 1));
        memcpy(&Data[Count], &Val, sizeof(DATA_TYPE));
        Count++;
    }
    
};

inline BOOLEAN
RecordEqual(nar_record* N1, nar_record* N2) {
    return N1->Len == N2->Len && N1->StartPos == N2->StartPos;
}

//#define printf(format,...) LogFile((format),__VA_ARGS__)


#define BUFFER_SIZE     4096


#define MAX(v1,v2) ((v1)>(v2) ? (v1) : (v2))
#define MIN(v1,v2) ((v1)<(v2) ? (v1) : (v2))

#define CLEANHANDLE(handle) if((handle)!=NULL) CloseHandle(handle);
#define CLEANMEMORY(memory) if((memory)!=NULL) free(memory);

#define MINISPY_NAME  L"MiniSpy"

#define FB_FILE_NAME L"FullBackupDisk_"
#define DB_FILE_NAME L"DiffBackupDisk_"

#define FB_METADATA_FILE_NAME L"FMetadata_"
#define DB_METADATA_FILE_NAME L"DMetadata_"

#define MFT_LCN_FILE_NAME L"MFTLCN_"
#define MFT_FILE_NAME L"MFT_BINARY_"

#define MAKE_W_STR(arg) L#arg

#define Assert(expression) if(!(expression)) {*(int*)0 = 0;}
#define ASSERT_VSS(expression) if(FAILED(expression)) {printf("Err @ %d\n",__LINE__);*(int*)0=0; }


inline BOOLEAN
IsSameVolumes(const WCHAR* OpName, const WCHAR VolumeLetter);

struct restore_target_inf;
struct restore_inf;
struct volume_backup_inf;

#if 0
/*
structs for algorithm that minimizes restore operation
that struct generated at run-time, it is safe to std libraries
only valid for diff restore, since fullbackup just copies raw data once
*/
struct region_chain {
    nar_record Rec;
    /*
  Problem about indexing:
  While tearing down region_chain in RemoveDuplicates function, information to
  read correct positions from incremental chunk is lost, but since they are
   in consecutive order, problem may be resolved with reading metadata's lengths,
  and can find which position chain's record falls.
  
  Other than that Index doesnt have any value, and it doesnt carried during insertions,
  appends.
  */
    UINT32 Index;
    region_chain* Next;
    region_chain* Back; /*Fixed root point*/
};

/*Inserts element to given chain*/
void
InsertToList(region_chain* Root, nar_record Rec);

/*Append to end of the list*/
void
AppendToList(region_chain* AnyPoint, nar_record Rec);

void
RemoveFromList(region_chain* R);

void
PrintList(region_chain* Temp);

void
PrintListReverse(region_chain* Temp);
#endif

/*
Used for Getfilename functions, this structure will change completely.
temp structure for mft backups
*/
typedef struct fn_req_inf {
    wchar_t Letter;
    int ID;
}fn_gen_inf;


enum BackupType {
    Diff,
    Inc
};

struct volume_backup_inf {
    //TODO add partition name of the volume to this structure.
    wchar_t Letter;
    BOOLEAN FullBackupExists;
    BackupType BT;
    
    struct {
        BOOLEAN SaveToFile; //If false, records will be saved to memory, RecordsMem
        BOOLEAN FlushToFile; //If this is set, all memory will be flushed to file
        BOOLEAN IsActive;
    }FilterFlags;
    
    int CurrentLogIndex;
    
    DWORD ClusterSize;
    
    std::vector<nar_record> RecordsMem;
    
    //these are going to be fully backed up,
    //since driver does not support them
    wchar_t *DOSName;
    data_array<int> ExtraPartitions; //TODO this is placeholder
    
    HANDLE LogHandle; //Handle to file that is logging volume's changes.
    UINT32 IncRecordCount; //Incremental change count of the volume, this value is will be reseted after every SUCCESSFUL backup operation
    
    
    /*
    Valid after diff-incremental setup. Stores all changes occured on disk, starting from latest incremental, or beginning if request was diff
    Diff between this and RecordsMem, RecordsMem is just temporary buffer that stores live changes on the disk, and will be flushed to file after it's available
  
    This structure contains information to track stream head. After every read, ClusterIndex MUST be incremented accordingly and if read operation exceeds that region, RecIndex must be incremented too.
    */
    
    struct {
        data_array<nar_record> Records;
        INT32 RecIndex;
        INT32 ClusterIndex;
        HANDLE Handle; //Used for streaming data to C#
    }Stream;
    
    //TODO
    
    CComPtr<IVssBackupComponents> VSSPTR;
    
};

struct restore_inf {
    wchar_t TargetLetter;
    wchar_t SrcLetter;
    UINT32 ClusterSize;
    BOOLEAN ToFull;
    BOOLEAN Version;
    BackupType Type;
    
    struct{
        data_array<nar_record> Records;
        INT32 RecIndex;
        INT32 ClusterIndex;
        HANDLE Handle;
    }Stream;
    
};

struct StreamInf{
  INT32 ClusterSize; //Size of clusters, requester has to call readstream with multiples of this size
  INT32 ClusterCount; //In clusters
  std::wstring FileName;
  std::wstring MetadataFileName;
};


//
//  Structure for managing current state.
//
struct LOG_CONTEXT {
    
    HANDLE Port;
    HANDLE Thread;
    BOOLEAN LogToScreen;
    
    data_array<volume_backup_inf> Volumes;
    
    //
    // For synchronizing shutting down of both threads
    //
    
    BOOLEAN CleaningUp;
    HANDLE  ShutDown;
};
typedef LOG_CONTEXT* PLOG_CONTEXT;

//
//  Function prototypes
//



/*
Function declerations
*/


#define Kilobyte(val) (val)*1024LL
#define Megabyte(val) Kilobyte(val)*1024LL

void
MergeRegions(data_array<nar_record>* R);

inline BOOLEAN
InitNewLogFile(volume_backup_inf* V);

/*Returns negative ID if not found*/
INT
GetVolumeID(PLOG_CONTEXT C, wchar_t Letter);

BOOLEAN
CopyData(HANDLE S, HANDLE D, ULONGLONG Len);

inline std::wstring
GenerateMFTFileName(wchar_t Letter, int ID);

inline std::wstring
GenerateMFTMetadataFileName(wchar_t Letter, int ID);

/*Used for diff*/
inline std::wstring
GenerateDBMetadataFileName(wchar_t Letter, int ID);

inline std::wstring
GenerateDBFileName(wchar_t Letter, int ID);


inline std::wstring
GenerateFBFileName(wchar_t Letter);

inline std::wstring
GenerateFBMetadataFileName(wchar_t Letter);

data_array<nar_record>
ReadFBMetadata(HANDLE F);

data_array<nar_record>
ReadMetadata(HANDLE F);

VOID
DisplayError(DWORD Code);


BOOLEAN
ReadStream(volume_backup_inf* VolInf, void* Buffer, int Size);


BOOLEAN
SetupStream(PLOG_CONTEXT Context, wchar_t Letter, StreamInf *SI = NULL);

BOOLEAN
SetupStreamHandle(volume_backup_inf* V);

BOOLEAN
SetFullRecords(volume_backup_inf* V);

BOOLEAN
SetIncRecords(volume_backup_inf* VolInf);

BOOLEAN
SetDiffRecords(volume_backup_inf* VolInf);

BOOLEAN
TerminateBackup(volume_backup_inf* V, BOOLEAN Succeeded);

BOOLEAN
OfflineRestore(restore_inf *Inf);

BOOLEAN
OfflineIncRestore(restore_inf *Inf, HANDLE V);

BOOLEAN
OfflineDiffRestore(restore_inf *Inf, HANDLE V);





BOOLEAN
SetupVSS();


BOOLEAN
SaveMFT(volume_backup_inf* VolInf, HANDLE VSSHandle, data_array<nar_record>* MFTLCN);

BOOLEAN
RestoreMFT(restore_inf* R, HANDLE VolumeHandle);

inline BOOLEAN
InitRestoreTargetInf(restore_inf* Inf, wchar_t Letter);

inline BOOLEAN
InitVolumeInf(volume_backup_inf* VolInf, wchar_t Letter, BackupType Type);

inline BOOLEAN
IsSameVolumes(const WCHAR* OpName, const WCHAR VolumeLetter);

BOOL
CompareNarRecords(const void* v1, const void* v2);

wchar_t*
GetShadowPath(std::wstring Drive, CComPtr<IVssBackupComponents>& ptr);

inline BOOL
IsRegionsCollide(nar_record* R1, nar_record* R2);


inline VOID
NarCloseThreadCom(PLOG_CONTEXT Context);

inline BOOL
NarCreateThreadCom(PLOG_CONTEXT Context);

std::string
NarExecuteCommand(const char* cmd, std::string FileName);

/*Make these function generated from safe template*/
inline std::vector<std::string>
Split(std::string str, std::string delimiter);

inline std::vector<std::wstring>
Split(std::wstring str, std::wstring delimiter);

data_array<nar_record>
GetMFTLCN(char VolumeLetter);

BOOLEAN
AddVolumeToTrack(PLOG_CONTEXT Context, wchar_t Letter, BackupType Type);

inline BOOLEAN
DetachVolume(volume_backup_inf* VolInf);

inline BOOLEAN
AttachVolume(volume_backup_inf* VolInf, BOOLEAN SetActive = TRUE);

DWORD WINAPI
RetrieveLogRecords(
  _In_ LPVOID lpParameter
);

BOOL
FileDump(
  _In_ PRECORD_DATA RecordData,
  _In_ HANDLE File
);

VOID
ScreenDump(
  _In_ ULONG SequenceNumber,
  _In_ WCHAR CONST* Name,
  _In_ PRECORD_DATA RecordData
);

BOOLEAN
ConnectDriver(PLOG_CONTEXT Ctx);

#if 0
inline rec_or
GetOrientation(nar_record* M, nar_record* S);

void
RemoveDuplicates(region_chain** Metadatas,
                 region_chain* MDShadow, int ID);
#endif


//
//  Values set for the Flags field in a RECORD_DATA structure.
//  These flags come from the FLT_CALLBACK_DATA structure.
//

#define FLT_CALLBACK_DATA_IRP_OPERATION         0x00000001  //  Set for Irp operations
#define FLT_CALLBACK_DATA_FAST_IO_OPERATION     0x00000002  //  Set for Fast Io operations
#define FLT_CALLBACK_DATA_FS_FILTER_OPERATION   0x00000004  //  Set for FsFilter operations

//
// standard IRP_MJ string definitions
//

#define IRP_MJ_CREATE_STRING                   "IRP_MJ_CREATE"
#define IRP_MJ_CREATE_NAMED_PIPE_STRING        "IRP_MJ_CREATE_NAMED_PIPE"
#define IRP_MJ_CLOSE_STRING                    "IRP_MJ_CLOSE"
#define IRP_MJ_READ_STRING                     "IRP_MJ_READ"
#define IRP_MJ_WRITE_STRING                    "IRP_MJ_WRITE"
#define IRP_MJ_QUERY_INFORMATION_STRING        "IRP_MJ_QUERY_INFORMATION"
#define IRP_MJ_SET_INFORMATION_STRING          "IRP_MJ_SET_INFORMATION"
#define IRP_MJ_QUERY_EA_STRING                 "IRP_MJ_QUERY_EA"
#define IRP_MJ_SET_EA_STRING                   "IRP_MJ_SET_EA"
#define IRP_MJ_FLUSH_BUFFERS_STRING            "IRP_MJ_FLUSH_BUFFERS"
#define IRP_MJ_QUERY_VOLUME_INFORMATION_STRING "IRP_MJ_QUERY_VOLUME_INFORMATION"
#define IRP_MJ_SET_VOLUME_INFORMATION_STRING   "IRP_MJ_SET_VOLUME_INFORMATION"
#define IRP_MJ_DIRECTORY_CONTROL_STRING        "IRP_MJ_DIRECTORY_CONTROL"
#define IRP_MJ_FILE_SYSTEM_CONTROL_STRING      "IRP_MJ_FILE_SYSTEM_CONTROL"
#define IRP_MJ_DEVICE_CONTROL_STRING           "IRP_MJ_DEVICE_CONTROL"
#define IRP_MJ_INTERNAL_DEVICE_CONTROL_STRING  "IRP_MJ_INTERNAL_DEVICE_CONTROL"
#define IRP_MJ_SHUTDOWN_STRING                 "IRP_MJ_SHUTDOWN"
#define IRP_MJ_LOCK_CONTROL_STRING             "IRP_MJ_LOCK_CONTROL"
#define IRP_MJ_CLEANUP_STRING                  "IRP_MJ_CLEANUP"
#define IRP_MJ_CREATE_MAILSLOT_STRING          "IRP_MJ_CREATE_MAILSLOT"
#define IRP_MJ_QUERY_SECURITY_STRING           "IRP_MJ_QUERY_SECURITY"
#define IRP_MJ_SET_SECURITY_STRING             "IRP_MJ_SET_SECURITY"
#define IRP_MJ_POWER_STRING                    "IRP_MJ_POWER"
#define IRP_MJ_SYSTEM_CONTROL_STRING           "IRP_MJ_SYSTEM_CONTROL"
#define IRP_MJ_DEVICE_CHANGE_STRING            "IRP_MJ_DEVICE_CHANGE"
#define IRP_MJ_QUERY_QUOTA_STRING              "IRP_MJ_QUERY_QUOTA"
#define IRP_MJ_SET_QUOTA_STRING                "IRP_MJ_SET_QUOTA"
#define IRP_MJ_PNP_STRING                      "IRP_MJ_PNP"
#define IRP_MJ_MAXIMUM_FUNCTION_STRING         "IRP_MJ_MAXIMUM_FUNCTION"

//
//  FSFilter string definitions
//

#define IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION_STRING   "IRP_MJ_ACQUIRE_FOR_SECTION_SYNC"
#define IRP_MJ_RELEASE_FOR_SECTION_SYNCHRONIZATION_STRING   "IRP_MJ_RELEASE_FOR_SECTION_SYNC"
#define IRP_MJ_ACQUIRE_FOR_MOD_WRITE_STRING   "IRP_MJ_ACQUIRE_FOR_MOD_WRITE"
#define IRP_MJ_RELEASE_FOR_MOD_WRITE_STRING   "IRP_MJ_RELEASE_FOR_MOD_WRITE"
#define IRP_MJ_ACQUIRE_FOR_CC_FLUSH_STRING    "IRP_MJ_ACQUIRE_FOR_CC_FLUSH"
#define IRP_MJ_RELEASE_FOR_CC_FLUSH_STRING    "IRP_MJ_RELEASE_FOR_CC_FLUSH"
#define IRP_MJ_NOTIFY_STREAM_FO_CREATION_STRING "IRP_MJ_NOTIFY_STREAM_FO_CREATION"

//
//  FAST_IO and other string definitions
//

#define IRP_MJ_FAST_IO_CHECK_IF_POSSIBLE_STRING "IRP_MJ_FAST_IO_CHECK_IF_POSSIBLE"
#define IRP_MJ_DETACH_DEVICE_STRING           "IRP_MJ_DETACH_DEVICE"
#define IRP_MJ_NETWORK_QUERY_OPEN_STRING      "IRP_MJ_NETWORK_QUERY_OPEN"
#define IRP_MJ_MDL_READ_STRING                "IRP_MJ_MDL_READ"
#define IRP_MJ_MDL_READ_COMPLETE_STRING       "IRP_MJ_MDL_READ_COMPLETE"
#define IRP_MJ_PREPARE_MDL_WRITE_STRING       "IRP_MJ_PREPARE_MDL_WRITE"
#define IRP_MJ_MDL_WRITE_COMPLETE_STRING      "IRP_MJ_MDL_WRITE_COMPLETE"
#define IRP_MJ_VOLUME_MOUNT_STRING            "IRP_MJ_VOLUME_MOUNT"
#define IRP_MJ_VOLUME_DISMOUNT_STRING         "IRP_MJ_VOLUME_DISMOUNT"

//
// Strings for the Irp minor codes
//

#define IRP_MN_QUERY_DIRECTORY_STRING          "IRP_MN_QUERY_DIRECTORY"
#define IRP_MN_NOTIFY_CHANGE_DIRECTORY_STRING  "IRP_MN_NOTIFY_CHANGE_DIRECTORY"
#define IRP_MN_USER_FS_REQUEST_STRING          "IRP_MN_USER_FS_REQUEST"
#define IRP_MN_MOUNT_VOLUME_STRING             "IRP_MN_MOUNT_VOLUME"
#define IRP_MN_VERIFY_VOLUME_STRING            "IRP_MN_VERIFY_VOLUME"
#define IRP_MN_LOAD_FILE_SYSTEM_STRING         "IRP_MN_LOAD_FILE_SYSTEM"
#define IRP_MN_TRACK_LINK_STRING               "IRP_MN_TRACK_LINK"
#define IRP_MN_LOCK_STRING                     "IRP_MN_LOCK"
#define IRP_MN_UNLOCK_SINGLE_STRING            "IRP_MN_UNLOCK_SINGLE"
#define IRP_MN_UNLOCK_ALL_STRING               "IRP_MN_UNLOCK_ALL"
#define IRP_MN_UNLOCK_ALL_BY_KEY_STRING        "IRP_MN_UNLOCK_ALL_BY_KEY"
#define IRP_MN_NORMAL_STRING                   "IRP_MN_NORMAL"
#define IRP_MN_DPC_STRING                      "IRP_MN_DPC"
#define IRP_MN_MDL_STRING                      "IRP_MN_MDL"
#define IRP_MN_COMPLETE_STRING                 "IRP_MN_COMPLETE"
#define IRP_MN_COMPRESSED_STRING               "IRP_MN_COMPRESSED"
#define IRP_MN_MDL_DPC_STRING                  "IRP_MN_MDL_DPC"
#define IRP_MN_COMPLETE_MDL_STRING             "IRP_MN_COMPLETE_MDL"
#define IRP_MN_COMPLETE_MDL_DPC_STRING         "IRP_MN_COMPLETE_MDL_DPC"
#define IRP_MN_SCSI_CLASS_STRING               "IRP_MN_SCSI_CLASS"
#define IRP_MN_START_DEVICE_STRING                 "IRP_MN_START_DEVICE"
#define IRP_MN_QUERY_REMOVE_DEVICE_STRING          "IRP_MN_QUERY_REMOVE_DEVICE"
#define IRP_MN_REMOVE_DEVICE_STRING                "IRP_MN_REMOVE_DEVICE"
#define IRP_MN_CANCEL_REMOVE_DEVICE_STRING         "IRP_MN_CANCEL_REMOVE_DEVICE"
#define IRP_MN_STOP_DEVICE_STRING                  "IRP_MN_STOP_DEVICE"
#define IRP_MN_QUERY_STOP_DEVICE_STRING            "IRP_MN_QUERY_STOP_DEVICE"
#define IRP_MN_CANCEL_STOP_DEVICE_STRING           "IRP_MN_CANCEL_STOP_DEVICE"
#define IRP_MN_QUERY_DEVICE_RELATIONS_STRING       "IRP_MN_QUERY_DEVICE_RELATIONS"
#define IRP_MN_QUERY_INTERFACE_STRING              "IRP_MN_QUERY_INTERFACE"
#define IRP_MN_QUERY_CAPABILITIES_STRING           "IRP_MN_QUERY_CAPABILITIES"
#define IRP_MN_QUERY_RESOURCES_STRING              "IRP_MN_QUERY_RESOURCES"
#define IRP_MN_QUERY_RESOURCE_REQUIREMENTS_STRING  "IRP_MN_QUERY_RESOURCE_REQUIREMENTS"
#define IRP_MN_QUERY_DEVICE_TEXT_STRING            "IRP_MN_QUERY_DEVICE_TEXT"
#define IRP_MN_FILTER_RESOURCE_REQUIREMENTS_STRING "IRP_MN_FILTER_RESOURCE_REQUIREMENTS"
#define IRP_MN_READ_CONFIG_STRING                  "IRP_MN_READ_CONFIG"
#define IRP_MN_WRITE_CONFIG_STRING                 "IRP_MN_WRITE_CONFIG"
#define IRP_MN_EJECT_STRING                        "IRP_MN_EJECT"
#define IRP_MN_SET_LOCK_STRING                     "IRP_MN_SET_LOCK"
#define IRP_MN_QUERY_ID_STRING                     "IRP_MN_QUERY_ID"
#define IRP_MN_QUERY_PNP_DEVICE_STATE_STRING       "IRP_MN_QUERY_PNP_DEVICE_STATE"
#define IRP_MN_QUERY_BUS_INFORMATION_STRING        "IRP_MN_QUERY_BUS_INFORMATION"
#define IRP_MN_DEVICE_USAGE_NOTIFICATION_STRING    "IRP_MN_DEVICE_USAGE_NOTIFICATION"
#define IRP_MN_SURPRISE_REMOVAL_STRING             "IRP_MN_SURPRISE_REMOVAL"
#define IRP_MN_QUERY_LEGACY_BUS_INFORMATION_STRING "IRP_MN_QUERY_LEGACY_BUS_INFORMATION"
#define IRP_MN_WAIT_WAKE_STRING                    "IRP_MN_WAIT_WAKE"
#define IRP_MN_POWER_SEQUENCE_STRING               "IRP_MN_POWER_SEQUENCE"
#define IRP_MN_SET_POWER_STRING                    "IRP_MN_SET_POWER"
#define IRP_MN_QUERY_POWER_STRING                  "IRP_MN_QUERY_POWER"
#define IRP_MN_QUERY_ALL_DATA_STRING               "IRP_MN_QUERY_ALL_DATA"
#define IRP_MN_QUERY_SINGLE_INSTANCE_STRING        "IRP_MN_QUERY_SINGLE_INSTANCE"
#define IRP_MN_CHANGE_SINGLE_INSTANCE_STRING       "IRP_MN_CHANGE_SINGLE_INSTANCE"
#define IRP_MN_CHANGE_SINGLE_ITEM_STRING           "IRP_MN_CHANGE_SINGLE_ITEM"
#define IRP_MN_ENABLE_EVENTS_STRING                "IRP_MN_ENABLE_EVENTS"
#define IRP_MN_DISABLE_EVENTS_STRING               "IRP_MN_DISABLE_EVENTS"
#define IRP_MN_ENABLE_COLLECTION_STRING            "IRP_MN_ENABLE_COLLECTION"
#define IRP_MN_DISABLE_COLLECTION_STRING           "IRP_MN_DISABLE_COLLECTION"
#define IRP_MN_REGINFO_STRING                      "IRP_MN_REGINFO"
#define IRP_MN_EXECUTE_METHOD_STRING               "IRP_MN_EXECUTE_METHOD"

//
//  Transaction notification string definitions.
//

#define IRP_MJ_TRANSACTION_NOTIFY_STRING       "IRP_MJ_TRANSACTION_NOTIFY"

#define TRANSACTION_BEGIN                               "BEGIN_TRANSACTION"
#define TRANSACTION_NOTIFY_PREPREPARE_STRING            "TRANSACTION_NOTIFY_PREPREPARE"
#define TRANSACTION_NOTIFY_PREPARE_STRING               "TRANSACTION_NOTIFY_PREPARE"
#define TRANSACTION_NOTIFY_COMMIT_STRING                "TRANSACTION_NOTIFY_COMMIT"
#define TRANSACTION_NOTIFY_ROLLBACK_STRING              "TRANSACTION_NOTIFY_ROLLBACK"
#define TRANSACTION_NOTIFY_PREPREPARE_COMPLETE_STRING   "TRANSACTION_NOTIFY_PREPREPARE_COMPLETE"
#define TRANSACTION_NOTIFY_PREPARE_COMPLETE_STRING      "TRANSACTION_NOTIFY_PREPARE_COMPLETE"
#define TRANSACTION_NOTIFY_COMMIT_COMPLETE_STRING       "TRANSACTION_NOTIFY_COMMIT_COMPLETE"
#define TRANSACTION_NOTIFY_COMMIT_FINALIZE_STRING       "TRANSACTION_NOTIFY_COMMIT_FINALIZE"
#define TRANSACTION_NOTIFY_ROLLBACK_COMPLETE_STRING     "TRANSACTION_NOTIFY_ROLLBACK_COMPLETE"
#define TRANSACTION_NOTIFY_RECOVER_STRING               "TRANSACTION_NOTIFY_RECOVER"
#define TRANSACTION_NOTIFY_SINGLE_PHASE_COMMIT_STRING   "TRANSACTION_NOTIFY_SINGLE_PHASE_COMMIT"
#define TRANSACTION_NOTIFY_DELEGATE_COMMIT_STRING       "TRANSACTION_NOTIFY_DELEGATE_COMMIT"
#define TRANSACTION_NOTIFY_RECOVER_QUERY_STRING         "TRANSACTION_NOTIFY_RECOVER_QUERY"
#define TRANSACTION_NOTIFY_ENLIST_PREPREPARE_STRING     "TRANSACTION_NOTIFY_ENLIST_PREPREPARE"
#define TRANSACTION_NOTIFY_LAST_RECOVER_STRING          "TRANSACTION_NOTIFY_LAST_RECOVER"
#define TRANSACTION_NOTIFY_INDOUBT_STRING               "TRANSACTION_NOTIFY_INDOUBT"
#define TRANSACTION_NOTIFY_PROPAGATE_PULL_STRING        "TRANSACTION_NOTIFY_PROPAGATE_PULL"
#define TRANSACTION_NOTIFY_PROPAGATE_PUSH_STRING        "TRANSACTION_NOTIFY_PROPAGATE_PUSH"
#define TRANSACTION_NOTIFY_MARSHAL_STRING               "TRANSACTION_NOTIFY_MARSHAL"
#define TRANSACTION_NOTIFY_ENLIST_MASK_STRING           "TRANSACTION_NOTIFY_ENLIST_MASK"


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


typedef enum {
    TRANSACTION_NOTIFY_PREPREPARE_CODE = 1,
    TRANSACTION_NOTIFY_PREPARE_CODE,
    TRANSACTION_NOTIFY_COMMIT_CODE,
    TRANSACTION_NOTIFY_ROLLBACK_CODE,
    TRANSACTION_NOTIFY_PREPREPARE_COMPLETE_CODE,
    TRANSACTION_NOTIFY_PREPARE_COMPLETE_CODE,
    TRANSACTION_NOTIFY_COMMIT_COMPLETE_CODE,
    TRANSACTION_NOTIFY_ROLLBACK_COMPLETE_CODE,
    TRANSACTION_NOTIFY_RECOVER_CODE,
    TRANSACTION_NOTIFY_SINGLE_PHASE_COMMIT_CODE,
    TRANSACTION_NOTIFY_DELEGATE_COMMIT_CODE,
    TRANSACTION_NOTIFY_RECOVER_QUERY_CODE,
    TRANSACTION_NOTIFY_ENLIST_PREPREPARE_CODE,
    TRANSACTION_NOTIFY_LAST_RECOVER_CODE,
    TRANSACTION_NOTIFY_INDOUBT_CODE,
    TRANSACTION_NOTIFY_PROPAGATE_PULL_CODE,
    TRANSACTION_NOTIFY_PROPAGATE_PUSH_CODE,
    TRANSACTION_NOTIFY_MARSHAL_CODE,
    TRANSACTION_NOTIFY_ENLIST_MASK_CODE,
    TRANSACTION_NOTIFY_COMMIT_FINALIZE_CODE = 31
} TRANSACTION_NOTIFICATION_CODES;

//
//  Standard IRP Major codes
//

#define IRP_MJ_CREATE                       0x00
#define IRP_MJ_CREATE_NAMED_PIPE            0x01
#define IRP_MJ_CLOSE                        0x02
#define IRP_MJ_READ                         0x03
#define IRP_MJ_WRITE                        0x04
#define IRP_MJ_QUERY_INFORMATION            0x05
#define IRP_MJ_SET_INFORMATION              0x06
#define IRP_MJ_QUERY_EA                     0x07
#define IRP_MJ_SET_EA                       0x08
#define IRP_MJ_FLUSH_BUFFERS                0x09
#define IRP_MJ_QUERY_VOLUME_INFORMATION     0x0a
#define IRP_MJ_SET_VOLUME_INFORMATION       0x0b
#define IRP_MJ_DIRECTORY_CONTROL            0x0c
#define IRP_MJ_FILE_SYSTEM_CONTROL          0x0d
#define IRP_MJ_DEVICE_CONTROL               0x0e
#define IRP_MJ_INTERNAL_DEVICE_CONTROL      0x0f
#define IRP_MJ_SHUTDOWN                     0x10
#define IRP_MJ_LOCK_CONTROL                 0x11
#define IRP_MJ_CLEANUP                      0x12
#define IRP_MJ_CREATE_MAILSLOT              0x13
#define IRP_MJ_QUERY_SECURITY               0x14
#define IRP_MJ_SET_SECURITY                 0x15
#define IRP_MJ_POWER                        0x16
#define IRP_MJ_SYSTEM_CONTROL               0x17
#define IRP_MJ_DEVICE_CHANGE                0x18
#define IRP_MJ_QUERY_QUOTA                  0x19
#define IRP_MJ_SET_QUOTA                    0x1a
#define IRP_MJ_PNP                          0x1b
#define IRP_MJ_MAXIMUM_FUNCTION             0x1b

//
//  IRP minor codes
//

#define IRP_MN_QUERY_DIRECTORY              0x01
#define IRP_MN_NOTIFY_CHANGE_DIRECTORY      0x02
#define IRP_MN_USER_FS_REQUEST              0x00
#define IRP_MN_MOUNT_VOLUME                 0x01
#define IRP_MN_VERIFY_VOLUME                0x02
#define IRP_MN_LOAD_FILE_SYSTEM             0x03
#define IRP_MN_TRACK_LINK                   0x04
#define IRP_MN_LOCK                         0x01
#define IRP_MN_UNLOCK_SINGLE                0x02
#define IRP_MN_UNLOCK_ALL                   0x03
#define IRP_MN_UNLOCK_ALL_BY_KEY            0x04
#define IRP_MN_NORMAL                       0x00
#define IRP_MN_DPC                          0x01
#define IRP_MN_MDL                          0x02
#define IRP_MN_COMPLETE                     0x04
#define IRP_MN_COMPRESSED                   0x08
#define IRP_MN_MDL_DPC                      (IRP_MN_MDL | IRP_MN_DPC)
#define IRP_MN_COMPLETE_MDL                 (IRP_MN_COMPLETE | IRP_MN_MDL)
#define IRP_MN_COMPLETE_MDL_DPC             (IRP_MN_COMPLETE_MDL | IRP_MN_DPC)
#define IRP_MN_SCSI_CLASS                   0x01
#define IRP_MN_START_DEVICE                 0x00
#define IRP_MN_QUERY_REMOVE_DEVICE          0x01
#define IRP_MN_REMOVE_DEVICE                0x02
#define IRP_MN_CANCEL_REMOVE_DEVICE         0x03
#define IRP_MN_STOP_DEVICE                  0x04
#define IRP_MN_QUERY_STOP_DEVICE            0x05
#define IRP_MN_CANCEL_STOP_DEVICE           0x06
#define IRP_MN_QUERY_DEVICE_RELATIONS       0x07
#define IRP_MN_QUERY_INTERFACE              0x08
#define IRP_MN_QUERY_CAPABILITIES           0x09
#define IRP_MN_QUERY_RESOURCES              0x0A
#define IRP_MN_QUERY_RESOURCE_REQUIREMENTS  0x0B
#define IRP_MN_QUERY_DEVICE_TEXT            0x0C
#define IRP_MN_FILTER_RESOURCE_REQUIREMENTS 0x0D
#define IRP_MN_READ_CONFIG                  0x0F
#define IRP_MN_WRITE_CONFIG                 0x10
#define IRP_MN_EJECT                        0x11
#define IRP_MN_SET_LOCK                     0x12
#define IRP_MN_QUERY_ID                     0x13
#define IRP_MN_QUERY_PNP_DEVICE_STATE       0x14
#define IRP_MN_QUERY_BUS_INFORMATION        0x15
#define IRP_MN_DEVICE_USAGE_NOTIFICATION    0x16
#define IRP_MN_SURPRISE_REMOVAL             0x17
#define IRP_MN_QUERY_LEGACY_BUS_INFORMATION 0x18
#define IRP_MN_WAIT_WAKE                    0x00
#define IRP_MN_POWER_SEQUENCE               0x01
#define IRP_MN_SET_POWER                    0x02
#define IRP_MN_QUERY_POWER                  0x03
#define IRP_MN_QUERY_ALL_DATA               0x00
#define IRP_MN_QUERY_SINGLE_INSTANCE        0x01
#define IRP_MN_CHANGE_SINGLE_INSTANCE       0x02
#define IRP_MN_CHANGE_SINGLE_ITEM           0x03
#define IRP_MN_ENABLE_EVENTS                0x04
#define IRP_MN_DISABLE_EVENTS               0x05
#define IRP_MN_ENABLE_COLLECTION            0x06
#define IRP_MN_DISABLE_COLLECTION           0x07
#define IRP_MN_REGINFO                      0x08
#define IRP_MN_EXECUTE_METHOD               0x09

//
//  IRP Flags
//

#define IRP_NOCACHE                     0x00000001
#define IRP_PAGING_IO                   0x00000002
#define IRP_SYNCHRONOUS_API             0x00000004
#define IRP_SYNCHRONOUS_PAGING_IO       0x00000040

//
//  Define the FLT_TAG_DATA structure so that we can display it.
//

#pragma warning(push)
#pragma warning(disable:4201) // nonstandard extension used : nameless struct/union

typedef struct _FLT_TAG_DATA_BUFFER {
    ULONG FileTag;
    USHORT TagDataLength;
    USHORT UnparsedNameLength;
    union {
        GUID TagGuid;
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            ULONG  Flags;
            WCHAR PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            WCHAR PathBuffer[1];
        } MountPointReparseBuffer;
        
        struct {
            UCHAR  DataBuffer[1];
        } GenericReparseBuffer;
    };
} FLT_TAG_DATA_BUFFER, * PFLT_TAG_DATA_BUFFER;

#pragma warning(pop)

#endif //__MSPYLOG_H__

