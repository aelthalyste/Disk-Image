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
    Data = (DATA_TYPE*)realloc(Data, sizeof(Val) * ((ULONGLONG)Count + 1));
    memcpy(&Data[Count], &Val, sizeof(DATA_TYPE));
    Count++;
  }


};

template<typename T>
inline void Append(data_array<T> *Destination, data_array<T> App) {
  
  if (App.Data == 0) {
    return;
  }
  
  ULONGLONG NewSize = sizeof(T)* ((ULONGLONG)App.Count + (ULONGLONG)Destination->Count);
  Destination->Data = (T*)realloc(Destination->Data, NewSize);
  memcpy(&Destination->Data[Destination->Count], App.Data, App.Count * sizeof(T));
  Destination->Count += App.Count;

}


template<typename T> void
FreeDataArray(data_array<T>* V) {
  if (V) {
    free(V->Data);
    free(V);
  }
}

template<typename T> void
FreeDataArray(data_array<T> V) {
  free(V.Data);
}


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

#define NAR_BINARY_FILE_NAME L"NAR_BINARY_"

#define MAKE_W_STR(arg) L#arg

#ifdef _DEBUG
#define Assert(expression, msg) if(!(expression)) do{printf(msg); *(int*)0 = 0;}while(0);
#define Assert(expression) if(!(expression)) do{*(int*)0 = 0;}while(0);
#else
#define Assert(expression, msg) do{(expression);} while(0);
#define Assert(expression) do{ (expression); }while(0);
#endif

#define NAR_INVALID_DISK_ID -1
#define NAR_INVALID_ENTRY -1
#define NAR_INVALID_VOLUME_LETTER 63

typedef char NARDP;
#define NAR_DISKTYPE_GPT 'G'
#define NAR_DISKTYPE_MBR 'M'
#define NAR_DISKTYPE_RAW 'R'

#define MetadataFileNameDraft "NAR_M_"
#define BackupFileNameDraft "NAR_BACKUP_"
#define NAR_FULLBACKUP_VERSION -1

#define WideMetadataFileNameDraft L"NAR_M_"
#define WideBackupFileNameDraft L"NAR_BACKUP_"



inline BOOLEAN
IsSameVolumes(const WCHAR* OpName, const WCHAR VolumeLetter);

struct restore_target_inf;
struct restore_inf;
struct volume_backup_inf;

#if 1
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


enum class BackupType : short {
  Diff,
  Inc
};

struct stream {
  data_array<nar_record> Records;
  INT32 RecIndex;
  INT32 ClusterIndex;
  HANDLE Handle; //Used for streaming data to C#
};

struct volume_backup_inf {
  wchar_t Letter;
  BOOLEAN FullBackupExists;
  BOOLEAN IsOSVolume;
  BOOLEAN INVALIDATEDENTRY; // If this flag is set, this entry is unusable. accessing its element wont give meaningful information to caller.
  BackupType BT = BackupType::Inc;

  struct {
    BOOLEAN SaveToFile; //If false, records will be saved to memory, RecordsMem
    BOOLEAN FlushToFile; //If this is set, all memory will be flushed to file
    BOOLEAN IsActive;
  }FilterFlags;

  int CurrentLogIndex;

  DWORD ClusterSize;

  std::vector<nar_record> RecordsMem;

  wchar_t DOSName[32];

  HANDLE LogHandle; //Handle to file that is logging volume's changes.
  UINT32 IncRecordCount; //Incremental change count of the volume, this value will be reseted after every SUCCESSFUL backup operation


  /*
  Valid after diff-incremental setup. Stores all changes occured on disk, starting from latest incremental, or beginning if request was diff
  Diff between this and RecordsMem, RecordsMem is just temporary buffer that stores live changes on the disk, and will be flushed to file after it's available

  This structure contains information to track stream head. After every read, ClusterIndex MUST be incremented accordingly and if read operation exceeds that region, RecIndex must be incremented too.
  */

  stream Stream;

  CComPtr<IVssBackupComponents> VSSPTR;

};


#pragma pack(push ,1) // force 1 byte alignment
/*
// NOTE(Batuhan): file that contains this struct contains:
-- RegionMetadata:
-- MFTMetadata: (optional)
-- MFT: (optional)
-- Recovery: (optional)

If any metadata error occurs, it's related binary data will be marked as corrupt too. If i cant copy mft metadata
to file, mft itself will be marked as corrupt because i wont have anything to map it to volume  at restore state.
*/

// NOTE(Batuhan): nar binary file contains backup data, mft, and recovery
static const int GlobalBackupMetadataVersion = 1;
struct backup_metadata {

  struct {
    int Size = sizeof(backup_metadata); // Size of this struct
    // NOTE(Batuhan): structure may change over time(hope it wont), this value hold which version it is so i can identify and cast accordingly
    int Version;
  }MetadataInf;

  struct {
    ULONGLONG RegionsMetadata;
    ULONGLONG Regions;

    ULONGLONG MFTMetadata;
    ULONGLONG MFT;

    ULONGLONG Recovery;
  }Size; //In bytes!

  struct {
    ULONGLONG RegionsMetadata;
    ULONGLONG AlignmentReserved;


    ULONGLONG MFTMetadata;
    ULONGLONG MFT;

    ULONGLONG Recovery;
  }Offset; // offsets from beginning of the file

  // NOTE(Batuhan): error flags to indicate corrupted data, indicates file
  // may not contain particular metadata or binary data.
  struct {
    BOOLEAN RegionsMetadata;
    BOOLEAN Regions;

    BOOLEAN MFTMetadata;
    BOOLEAN MFT;

    BOOLEAN Recovery;
  }Errors;

  union {
    CHAR Reserved[128]; // Reserved for future usage
  };

  ULONGLONG VolumeSize;
  ULONGLONG LastUsedByteOffset; //DO NOT USE IT, NOT IMPLEMENTED

  int Version; // -1 for full backup
  int ClusterSize; // 4096 default

  char Letter;
  BOOLEAN DiskType;
  union {
    BOOLEAN IsOSVolume;
    BOOLEAN Recovery; // true if contains restore partition
  }; // 4byte
  BackupType BT; // diff or inc


};

#pragma pack(pop)

/*
işletim sistemi durumu hakkında, eğer ilk seçenek verilmişse, sadece datayı geri yükler, eğer ikinci seçenek var ise
boot aşamalarını yapar. kullanıcı sadece içerideki veriyi almak da isteyebilir

input: letter,version,rootdir,targetletter var olan bir volume'a restore yapılmalı
input: letter,version,rootdir,diskid,targetletter belirtilen diskte yeni volume oluşturularak restore yapılır
bu seçenekte, işletim sistemi geri yükleniliyorsa, disk ona göre hazırlanır
*/

struct backup_metadata_ex {
  backup_metadata M;
  std::wstring FilePath;
  data_array<nar_record> RegionsMetadata;
  backup_metadata_ex() {
    RegionsMetadata = { 0, 0 };
    FilePath = L" ";
    memset(&M, 0, sizeof(M));
  }
};



struct restore_inf {
  wchar_t TargetLetter;
  wchar_t SrcLetter;
  int Version;
  std::wstring RootDir;
};

struct DotNetStreamInf {
  INT32 ClusterSize; //Size of clusters, requester has to call readstream with multiples of this size
  INT32 ClusterCount; //In clusters
  std::wstring FileName;
  std::wstring MetadataFileName;
};



struct disk_information {
  ULONGLONG Size; //In bytes!
  ULONGLONG UnallocatedGB; // IN GB!
  char Type; // first character of {RAW,GPT,MBR}
  int ID;
};


/*
example output of diskpart list volume command
Volume ###  Ltr  Label        Fs     Type        Size     Status     Info
  ----------  ---  -----------  -----  ----------  -------  ---------  --------
  Volume 0     N                NTFS   Simple      5000 MB  Healthy
  Volume 1     D   HDD          NTFS   Simple       926 GB  Healthy    Pagefile
  Volume 2     C                NTFS   Partition    230 GB  Healthy    Boot
  Volume 3                      FAT32  Partition    100 MB  Healthy    System
*/
struct volume_information {
  ULONGLONG Size; //in bytes!
  BOOLEAN Bootable; // Healthy && NTFS && !Boot
  char Letter;
  INT8 DiskID;
  char DiskType;
};

// Up to 2GB
struct file_read {
  void* Data;
  int Len;
};

inline BOOLEAN
IsNumeric(char val) {
  return val >= '0' && val <= '9';
}

file_read
NarReadFile(const char* FileName);

BOOLEAN
NarDumpToFile(const char* FileName, void* Data, int Size);

BOOLEAN
NarSetVolumeSize(char Letter, int TargetSizeMB);

BOOLEAN
CreatePartition(int Disk, char Letter, unsigned size);

BOOLEAN
NarCreateCleanGPTBootablePartition(int DiskID, int VolumeSizeMB, int EFISizeMB, int RecoverySizeMB, char Letter);

BOOLEAN
NarCreateCleanGPTPartition(int DiskID, int VolumeSizeMB, char Letter);

BOOLEAN
NarCreateCleanMBRPartition();

BOOLEAN
NarCreateCleanMBRBootPartition();

void
NarRepairBoot(char Letter);

data_array<disk_information>
NarGetDisks();

data_array<volume_information>
NarGetVolumes();

ULONGLONG
NarGetVolumeSize(char Letter);

inline int 
NarGetVolumeDiskType(char Letter);

inline int 
NarGetVolumeDiskID(char Letter);

BOOLEAN
NarFormatVolume(char Letter);


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
  wchar_t RootDir[512];
  BOOLEAN CleaningUp;
  BOOLEAN DriverErrorOccured;
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
CopyData(HANDLE S, HANDLE D, ULONGLONG Len, ULONGLONG FileOffset);

BOOLEAN
CopyData(HANDLE S, HANDLE D, ULONGLONG Len);

inline void
StrToGUID(const char* guid, GUID* G);



VOID
DisplayError(DWORD Code);


BOOLEAN
ReadStream(volume_backup_inf* VolInf, void* Buffer, int Size);

//BOOLEAN
//SetupRestoreStream(PLOG_CONTEXT C, wchar_t Letter, void *Metadata, int MSize);

BOOLEAN
WriteStream(volume_backup_inf* Inf, void* Buffer, int Size);

void
FreeBackupMetadataEx(backup_metadata_ex* BMEX);

BOOLEAN
SetupStream(PLOG_CONTEXT C, wchar_t L, BackupType Type, DotNetStreamInf* SI = NULL);

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
RestoreSystemPartitions(restore_inf* Inf);

BOOLEAN
InitGPTPartition(int DiskID);

BOOLEAN
CreateAndMountSystemPartition(int DiskID, char Letter, unsigned SizeMB);

BOOLEAN
CreateAndMountRecoveryPartition(int DiskID, char Letter, unsigned SizeMB);

BOOLEAN
CreateAndMountMSRPartition(int DiskID, unsigned SizeMB);

BOOLEAN
RemoveLetter(int DiskID, unsigned PartitionID, char Letter);

// NOTE(Batuhan): create partition with given size
BOOLEAN
NarCreatePrimaryPartition(int DiskID, char Letter, unsigned SizeMB);
// NOTE(Batuhan): new partition will use all unallocated space left
BOOLEAN
NarCreatePrimaryPartition(int DiskID, char Letter);

BOOLEAN
SetupVSS();

std::wstring
GenerateLogFileName(wchar_t Letter, int Version);

std::wstring
GenerateBinaryFileName(wchar_t Letter, int Version);

backup_metadata_ex*
InitBackupMetadataEx(wchar_t Letter, int Version, std::wstring RootDir);

backup_metadata
ReadMetadata(wchar_t Letter, int Version, std::wstring RootDir);

BOOLEAN
OfflineRestoreCleanDisk(restore_inf* R, int DiskID);

BOOLEAN
OfflineRestoreToVolume(restore_inf* R, BOOLEAN ShouldFormat);

BOOLEAN
SaveMetadata(char Letter, int Version, int ClusterSize, BackupType BT, data_array<nar_record> BackupRegions, HANDLE VSSHandle);

BOOLEAN
RestoreIncVersion(restore_inf R, HANDLE Volume); // Volume optional, might pass INVALID_HANDLE_VALUE

BOOLEAN
RestoreDiffVersion(restore_inf R, HANDLE Volume); // Volume optional, might pass INVALID_HANDLE_VALUE

BOOLEAN
NarTruncateFile(HANDLE F, ULONGLONG TargetSize);

ULONGLONG
NarGetFilePointer(HANDLE F);

BOOLEAN
AppendMFTFile(HANDLE File, HANDLE VSSHANDLE, char Letter, int ClusterSize);

BOOLEAN
AppendRecoveryToFile(HANDLE File, char Letter);

BOOLEAN
RestoreRecoveryFile(restore_inf R);

BOOLEAN
RestoreVersionWithoutLoop(restore_inf R, BOOLEAN RestoreMFT, HANDLE Volume); // Volume optional, might pass INVALID_HANDLE_VALUE

BOOLEAN
NarRestoreMFT(backup_metadata_ex* BMEX, HANDLE Volume);

data_array<nar_record>
ReadMFTLCN(backup_metadata_ex* BMEX);

inline BOOLEAN
IsGPTVolume(char Letter);

HANDLE
NarOpenVolume(char Letter);

void
NarCloseVolume(HANDLE V);

BOOLEAN
SaveMFT(volume_backup_inf* VolInf, HANDLE VSSHandle, data_array<nar_record>* MFTLCN);

BOOLEAN
RestoreMFT(restore_inf* R, HANDLE VolumeHandle);

inline BOOLEAN
InitRestoreTargetInf(restore_inf* Inf, wchar_t Letter);

inline BOOLEAN
InitVolumeInf(volume_backup_inf* VolInf, wchar_t Letter, BackupType Type);

BOOLEAN
SaveExtraPartitions(volume_backup_inf* V);

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


BOOLEAN
AddVolumeToTrack(PLOG_CONTEXT Context, wchar_t Letter, BackupType Type);

BOOLEAN
RemoveVolumeFromTrack(PLOG_CONTEXT Context, wchar_t Letter);

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

