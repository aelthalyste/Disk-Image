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

#include <stdio.h>


#define TIMED_BLOCK__(NAME, Number, ...) timed_block timed_##Number(__COUNTER__, __LINE__, __FUNCTION__, NAME);
#define TIMED_BLOCK_(NAME, Number, ...)  TIMED_BLOCK__(NAME, Number,  ## __VA__ARGS__)
#define TIMED_BLOCK(...)                 TIMED_BLOCK_("UNNAMED", __LINE__, ## __VA__ARGS__)
#define TIMED_NAMED_BLOCK(NAME, ...)     TIMED_BLOCK_(NAME, __LINE__, ## __VA__ARGS__)

struct debug_record {
    char* FunctionName;
    char* Description;
    uint64_t Clocks;
    
    uint32_t ThreadIndex;
    uint32_t LineNumber;
    uint32_t HitCount;
};

debug_record GlobalDebugRecordArray[];

struct timed_block {
    
    debug_record* mRecord;
    
    timed_block(int Counter, int LineNumber, char* FunctionName, char* Description) {
        
        mRecord = GlobalDebugRecordArray + Counter;
        mRecord->FunctionName   = FunctionName;
        mRecord->Description = Description;
        mRecord->LineNumber     = LineNumber;
        mRecord->ThreadIndex    = 0;
        mRecord->Clocks        -= __rdtsc();
        mRecord->HitCount++;
        
    }
    
    ~timed_block() {
        mRecord->Clocks += __rdtsc();
    }
    
};


inline void
PrintDebugRecords();


#define NAR_IS_BETWEEN(floor, ceil, test) (((test) >= (floor)) && ((test) <= (ceil)))
#define NAR_OP_ALLOCATE 1
#define NAR_OP_FREE     2
#define NAR_OP_ZERO     3
#define NAR_OP_RESET    4

static inline void*
_NarInternalMemoryOp(int OpCode, size_t Size) {
    
    struct {
        void* Memory;
        size_t Reserve;
        size_t Used;
    }static MemArena = { 0 };
    
    void* Result = 0;
    
    if (!MemArena.Memory) {
        MemArena.Reserve = 1024LL * 1024LL * 1024LL * 64LL; // Reserve 64GB
        MemArena.Used = 0;
        MemArena.Memory = VirtualAlloc(0, MemArena.Reserve, MEM_RESERVE, PAGE_READWRITE);
    }
    if (OpCode == NAR_OP_ALLOCATE) {
        VirtualAlloc(MemArena.Memory, Size + MemArena.Used, MEM_COMMIT, PAGE_READWRITE);
        Result = (void*)((char*)MemArena.Memory + MemArena.Used);
        MemArena.Used += Size;
    }
    if (OpCode == NAR_OP_RESET) {
        memset(MemArena.Memory, 0, MemArena.Used);
        if (VirtualFree(MemArena.Memory, MemArena.Used, MEM_DECOMMIT) == 0) {
            printf("Cant decommit scratch memory\n");
        }
        MemArena.Used = 0;
    }
    if(OpCode == NAR_OP_FREE){
        VirtualFree(MemArena.Memory, 0, MEM_RELEASE);
        MemArena.Reserve = 0;
        MemArena.Used    = 0;
        MemArena.Memory  = 0;
    }
    
    return Result;
}

static inline void*
NarScratchAllocate(size_t Size) {
    return _NarInternalMemoryOp(NAR_OP_ALLOCATE, Size);
}

static inline void
NarScratchReset(){
    _NarInternalMemoryOp(NAR_OP_RESET, 0);
}

static inline void
NarScratchFree() {
    _NarInternalMemoryOp(NAR_OP_FREE, 0);
}



struct nar_log_time{
    BYTE YEAR; // 2000 + YEAR is the actual value
    BYTE MONTH;
    BYTE DAY;
    BYTE HOUR;
    BYTE MIN;
    BYTE SEC;
    // 6 bytes
};

struct nar_log{
    //char         *FunctionName;
    //unsigned int LineNumber;
    nar_log_time Time;
    char *LogString;
}GlobalLogs[512];
int GlobalLogCount = 0;
HANDLE GlobalLogMutex = 0;


/*

*/
static void
NarLog(const char *str, ...){
    
    static BOOLEAN Init = FALSE;
    if (Init == FALSE) {
        GlobalLogMutex = CreateMutexA(NULL, FALSE, NULL);
        Init = TRUE;
    }
    
    va_list ap;
    
    //
    
#define MAX_BUF_LEN 1024*2
    
    static char buf[MAX_BUF_LEN];
    static SYSTEMTIME Time = { 0 };
    static nar_log_time NarTime = {0};
    
    memset(buf, 0, sizeof(buf));
    GetLocalTime(&Time);
    
    NarTime.YEAR  = (BYTE)(Time.wYear % 100);
    NarTime.MONTH = (BYTE)Time.wMonth;
    NarTime.DAY   = (BYTE)Time.wDay;
    NarTime.HOUR  = (BYTE)Time.wHour;
    NarTime.MIN   = (BYTE)Time.wMinute;
    NarTime.SEC   = (BYTE)Time.wSecond;
	
    
    
#if 1
    va_start(ap, str);
    vsprintf(buf, str, ap);
    va_end(ap);
#endif
    
    // safe cast
    DWORD Len = (DWORD)strlen(buf);
    DWORD H = 0;
    
#if 1    
    if(WaitForSingleObject(GlobalLogMutex, 25) == WAIT_OBJECT_0){
        GlobalLogs[GlobalLogCount].LogString = (char*)NarScratchAllocate((Len + 1)*sizeof(buf[0]));
        memcpy(GlobalLogs[GlobalLogCount].LogString, &buf[0], (Len + 1)*sizeof(buf[0]));
        memcpy(&GlobalLogs[GlobalLogCount].Time, &NarTime, sizeof(NarTime));
        GlobalLogCount++;
        ReleaseMutex(GlobalLogMutex);
    }
#endif
    
#if 1    
	
	static bool fileinit = false;
	static FILE *File = 0;
	if(fileinit == false){
		File = fopen("C:\\ProgramData\\NarDiskBackup\\NAR_APP_LOG_FILE.txt", "a");
		fileinit = true;
	}	
	if(File){
	    static char time_buf[80];
	    snprintf(time_buf, 80, "[%02d/%02d/%04d | %02d:%02d:%02d] : ", NarTime.DAY, NarTime.MONTH, 2000 + NarTime.YEAR, NarTime.HOUR, NarTime.MIN, NarTime.SEC);
        char big_buffer[1024];
        big_buffer[0] = 0;
        strcat(big_buffer, time_buf);
        strcat(big_buffer, buf);
        fwrite(big_buffer, 1, strlen(big_buffer), File);		
		fflush(File);
        printf(buf);
    }
	else{
		OutputDebugStringA(buf);
    }
    
#endif
    
    
#undef MAX_BUF_LEN
    
}


#define printf(fmt, ...) NarLog(fmt, __VA_ARGS__)

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
    UINT ReserveCount = 0;
    
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

template<typename T> inline void
FreeDataArray(data_array<T>* V) {
    if (V != NULL) {
        free(V->Data);
        V->Data = 0;
        V->Count = 0;
    }
}


inline BOOLEAN
RecordEqual(nar_record* N1, nar_record* N2) {
    return N1->Len == N2->Len && N1->StartPos == N2->StartPos;
}

//#define printf(format,...) LogFile((format),__VA_ARGS__)


#define BUFFER_SIZE     4096


#define MAX(v1,v2) ((v1)>(v2) ? (v1) : (v2))
#define MIN(v1,v2) ((v1)<(v2) ? (v1) : (v2))
#define CLAMP(floor,ceil, value) MAX(MIN(ceil, value), 0)

#define CLEANHANDLE(handle) if((handle)!=NULL) CloseHandle(handle);
#define CLEANMEMORY(memory) if((memory)!=NULL) free(memory);

#define MINISPY_NAME  L"MiniSpy"

#define NAR_BINARY_FILE_NAME L"NAR_BINARY_"

#define MAKE_W_STR(arg) L#arg

#ifdef _DEBUG
#define Assert(expression) if(!(expression)) do{*(int*)0 = 0;}while(0);
#define NAR_BREAK_CODE() //__debugbreak();
#else
#define Assert(expression) (expression)
#define NAR_BREAK_CODE()
#endif

#define NAR_INVALID_VOLUME_TRACK_ID -1
#define NAR_INVALID_DISK_ID -1
#define NAR_INVALID_ENTRY -1

typedef char NARDP;
#define NAR_DISKTYPE_GPT 'G'
#define NAR_DISKTYPE_MBR 'M'
#define NAR_DISKTYPE_RAW 'R'

#define NAR_FULLBACKUP_VERSION -1

#define MetadataFileNameDraft L"NAR_M_"
#define BackupFileNameDraft L"NAR_BACKUP_"
#define MetadataExtension L".narmd"
#define BackupExtension   L".narbd"

#define NAR_EFI_PARTITION_LETTER 'S'
#define NAR_RECOVERY_PARTITION_LETTER 'R'

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

inline void
PrintList(region_chain* Temp);

inline void
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


/*
This function silently merges local time with given parameters
*/
inline nar_backup_id
NarGenerateBackupID(char Letter);

inline std::wstring
GenerateMetadataName(nar_backup_id id, int Version);

inline std::wstring
GenerateLogFilePath(char Letter);

inline std::wstring
GenerateBinaryFileName(nar_backup_id id, int Version);



struct volume_backup_inf {
    wchar_t Letter;
    BOOLEAN FullBackupExists;
    BOOLEAN IsOSVolume;
    BOOLEAN INVALIDATEDENTRY; // If this flag is set, this entry is unusable. accessing its element wont give meaningful information to caller.
    BackupType BT = BackupType::Inc;
    
    struct {
        BOOLEAN IsActive;
    }FilterFlags;
    
    // when backing up, we dont want to mess up with log file while setting up file pointers
    // since msging thread has to append, but program itself may read from start, that will
    // eventually cause bugs. This mutex ensures there are no more than one thread using LogHandle
    HANDLE FileWriteMutex;
    
    INT32 Version;
    DWORD ClusterSize;    
    
    nar_backup_id BackupID;
    
    // HANDLE LogHandle; //Handle to file that is logging volume's changes.
    
    ////Incremental change count of the volume, this value will be reseted after every SUCCESSFUL backup operation
    // this value times sizeof(nar_record) indicates how much data appended since last backup, useful when doing incremental backups
    // we dont need that actually, possiblenewbackupregionoffsetmark - lastbackupoffset is equal to that thing
    //UINT32 IncRecordCount;  // IGNORED IF DIFF BACKUP
    
    // Indicates where last backup regions end in local metadata. bytes after that offset is non-backed up parts of the volume.
    // this value + increcordcount*sizeof(nar_record) is PossibleNewBackupRegionOffsetMark
    
    
    INT64 PossibleNewBackupRegionOffsetMark;
    
    union {
        struct{
            INT64 BackupStartOffset;
        }DiffLogMark;
        
        struct{
            INT64 LastBackupRegionOffset;
        }IncLogMark;
    };
    
    DWORD VolumeTotalClusterCount;
    
    
    
    /*
    Valid after diff-incremental setup. Stores all changes occured on disk, starting from latest incremental, or beginning if request was diff
    Diff between this and RecordsMem, RecordsMem is just temporary buffer that stores live changes on the disk, and will be flushed to file after it's available
    
    This structure contains information to track stream head. After every read, ClusterIndex MUST be incremented accordingly and if read operation exceeds that region, RecIndex must be incremented too.
    */
    
    stream Stream;
    
    
    //data_array<nar_record> MFTandINDXRegions;
    
    //
    nar_record*  MFTLCN;
    unsigned int MFTLCNCount;
    
    CComPtr<IVssBackupComponents> VSSPTR;
    VSS_ID SnapshotID;
    
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
    
    
#define NAR_MAX_TASK_NAME_LEN        128
#define NAR_MAX_TASK_DESCRIPTION_LEN 512
#define NAR_MAX_PRODUCT_NAME         50
#define NAR_MAX_COMPUTERNAME_LENGTH 15
    
    union {
        CHAR Reserved[2048]; // Reserved for future usage
        struct {
            //FOR MBR things
            union{
            	INT64 GPT_EFIPartitionSize;
            	INT64 MBR_SystemPartitionSize;
            };
            SYSTEMTIME BackupDate;
            char ProductName[NAR_MAX_PRODUCT_NAME];
            char ComputerName[NAR_MAX_COMPUTERNAME_LENGTH  + 1];
            wchar_t TaskName[NAR_MAX_TASK_NAME_LEN];
            wchar_t TaskDescription[NAR_MAX_TASK_DESCRIPTION_LEN];
            nar_backup_id ID;
        };
    };
    
    ULONGLONG VolumeTotalSize;
    ULONGLONG VolumeUsedSize;
    
    // NOTE(Batuhan): Last volume offset must be written to disk that if this specific version were to be restored, version itself can be 5 gb big, but last offset it indicates that changes were made can be 100gb'th offset.
    ULONGLONG VersionMaxWriteOffset; 
    
    int Version; // -1 for full backup
    int ClusterSize; // 4096 default
    
    char Letter;
    unsigned char DiskType;
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
    nar_backup_id BackupID;
    int Version;
    std::wstring RootDir;
    bool OverrideDiskType;
    bool RepairBoot;
    char DiskType;
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
    UINT64 TotalSize;
    UINT64 FreeSize;
    BOOLEAN Bootable; // Healthy && NTFS && !Boot
    char Letter;
    unsigned char DiskID;
    char DiskType;
    wchar_t VolumeName[MAX_PATH + 1];
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

inline BOOLEAN
NarDumpToFile(const char* FileName, void* Data, unsigned int Size);

BOOLEAN
NarSetVolumeSize(char Letter, int TargetSizeMB);

BOOLEAN
CreatePartition(int Disk, char Letter, unsigned size);

inline BOOLEAN
NarCreateCleanGPTBootablePartition(int DiskID, int VolumeSizeMB, int EFISizeMB, int RecoverySizeMB, char Letter, char BootPartitionLetter);

inline BOOLEAN
NarCreateCleanGPTPartition(int DiskID, int VolumeSizeMB, char Letter);

BOOLEAN
NarCreateCleanMBRPartition(int DiskID, char VolumeLetter, int VolumeSize);


inline BOOLEAN
NarCreateCleanMBRBootPartition(int DiskID, char VolumeLetter, int VolumeSizeMB, int SystemPartitionSizeMB, int RecoveryPartitionSizeMB,
                               char BootPartitionLetter);

inline char
NarGetAvailableVolumeLetter();

inline BOOLEAN
NarRemoveLetter(char Letter);

inline void
NarRepairBoot(char Letter, char BootPartitionLetter);

data_array<disk_information>
NarGetDisks();

data_array<volume_information>
NarGetVolumes();

ULONGLONG
NarGetVolumeTotalSize(char Letter);

inline int
NarGetVolumeDiskType(char Letter);

inline unsigned char
NarGetVolumeDiskID(char Letter);

inline BOOLEAN
NarFormatVolume(char Letter);

inline void
NarGetProductName(char* OutName);

//
//  Structure for managing current state.
//
struct LOG_CONTEXT {
    HANDLE Port;
    data_array<volume_backup_inf> Volumes;
};
typedef LOG_CONTEXT* PLOG_CONTEXT;

//
//  Function prototypes
//



/*
Function declerations
*/


#define Kilobyte(val) ((val)*1024ll)
#define Megabyte(val) (Kilobyte(val)*1024ll)
#define Gigabyte(val) (Megabyte(val)*1024ll)

void
MergeRegions(data_array<nar_record>* R);


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


UINT32
ReadStream(volume_backup_inf* VolInf, void* Buffer, unsigned int Size);

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

ULONGLONG
NarGetLogFileSizeFromKernel(HANDLE CommPort, char Letter);

BOOLEAN
SetIncRecords(HANDLE CommPort, volume_backup_inf* VolInf);

BOOLEAN
SetDiffRecords(HANDLE CommPort, volume_backup_inf* VolInf);

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



backup_metadata_ex*
InitBackupMetadataEx(nar_backup_id, int Version, std::wstring RootDir);

backup_metadata
ReadMetadata(nar_backup_id ID, int Version, std::wstring RootDir);

BOOLEAN
OfflineRestoreCleanDisk(restore_inf* R, int DiskID);

BOOLEAN
OfflineRestoreToVolume(restore_inf* R, BOOLEAN ShouldFormat);

BOOLEAN
SaveMetadata(char Letter, int Version, int ClusterSize, BackupType BT, data_array<nar_record> BackupRegions, HANDLE VSSHandle, nar_record* MFTLCN, unsigned int MFTLCNCount, nar_backup_id BackupID);

BOOLEAN
RestoreIncVersion(restore_inf R, HANDLE Volume); // Volume optional, might pass INVALID_HANDLE_VALUE

BOOLEAN
RestoreDiffVersion(restore_inf R, HANDLE Volume); // Volume optional, might pass INVALID_HANDLE_VALUE

BOOLEAN
NarTruncateFile(HANDLE F, ULONGLONG TargetSize);

inline ULONGLONG
NarGetFilePointer(HANDLE F);

inline BOOLEAN
AppendMFTFile(HANDLE File, HANDLE VSSHANDLE, data_array<nar_record> MFTLCN, int ClusterSize);

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

inline BOOLEAN
NarSetFilePointer(HANDLE File, ULONGLONG V);

BOOLEAN
NarGetVolumeGUIDKernelCompatible(wchar_t Letter, wchar_t* VolumeGUID);

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

inline BOOLEAN
IsRegionsCollide(nar_record R1, nar_record R2);


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
RemoveVolumeFromTrack(LOG_CONTEXT *Context, wchar_t Letter);

inline BOOLEAN
DetachVolume(volume_backup_inf* VolInf);

inline BOOLEAN
AttachVolume(volume_backup_inf* VolInf, BOOLEAN SetActive = TRUE);


BOOLEAN
NarGetBackupsInDirectory(const wchar_t* Directory, backup_metadata* B, int BufferSize, int* FoundCount);

inline void
FreeFileRead(file_read FR);



#define NAR_POSIX                2
#define NAR_ENTRY_SIZE_OFFSET    8
#define NAR_TIME_OFFSET         24
#define NAR_SIZE_OFFSET         64
#define NAR_ATTRIBUTE_OFFSET    72
#define NAR_NAME_LEN_OFFSET     80 
#define NAR_POSIX_OFFSET        81
#define NAR_NAME_OFFSET         82

#define NAR_ROOT_MFT_ID          5

#define NAR_FE_HAND_OPT_READ_MOUNTED_VOLUME 1
#define NAR_FE_HAND_OPT_READ_BACKUP_VOLUME 2

#define NAR_FEXP_INDX_ENTRY_SIZE(e) *(UINT16*)((char*)(e) + 8)
#define NAR_FEXP_POSIX -1
#define NAR_FEXP_END_MARK -2
#define NAR_FEXP_SUCCEEDED 1

#define NAR_DATA_FLAG 0x80
#define NAR_INDEX_ALLOCATION_FLAG 0xA0
#define NAR_INDEX_ROOT_FLAG 0x90
#define NAR_BITMAP_FLAG     0xB0
#define NAR_ATTRIBUTE_LIST 0x20

#define NAR_FE_DIRECTORY_FLAG 0x01

#define NAR_OFFSET(m, o) ((char*)(m) + (o))


struct nar_fe_volume_handle{
    HANDLE VolumeHandle;
    backup_metadata_ex *BMEX;
};

// could be either file or dir
struct nar_file_entry {
    
    BOOLEAN IsDirectory;
    
    UINT64 MFTFileIndex;
    UINT64 Size; // file size in bytes
    
    UINT64 CreationTime;
    UINT64 LastModifiedTime;
    
    wchar_t Name[MAX_PATH + 1]; // max path + 1 for null termination
};

// represents files in directory
struct nar_file_entries_list {
    UINT64 MFTIndex;
    UINT32 EntryCount;
    UINT32 MaxEntryCount;
    nar_file_entry* Entries;
};

inline BOOLEAN
NarInitFileEntryList(nar_file_entries_list *EList, unsigned int  MaxEntryCount){
    
    if(EList == NULL) FALSE;
    EList->MFTIndex = 0;
    EList->EntryCount = 0;
    EList->MaxEntryCount = MaxEntryCount;
    
    UINT64 ReserveSize = (0x7FFFFFFF) * sizeof(nar_file_entry);
    
    // EList->Entries = (nar_file_entry*)malloc(MaxEntryCount*sizeof(nar_file_entry));
    
    EList->Entries = (nar_file_entry*)VirtualAlloc(0, ReserveSize, MEM_RESERVE, PAGE_READWRITE);
    VirtualAlloc(EList->Entries, MaxEntryCount*sizeof(nar_file_entry), MEM_COMMIT, PAGE_READWRITE);
    
    return (EList->Entries != NULL);
}

inline void
NarExtentFileEntryList(nar_file_entries_list *EList, unsigned int NewCapacity){
    if(EList){
        EList->MaxEntryCount = NewCapacity;
        VirtualAlloc(EList->Entries, NewCapacity*sizeof(nar_file_entry), MEM_COMMIT, PAGE_READWRITE);
        //EList->Entries = (nar_file_entry*)realloc(EList->Entries, NewCapacity*sizeof(nar_file_entry));
    }
}

inline void
NarFreeFileEntryList(nar_file_entries_list *EList){
    if(EList == NULL) return;
    VirtualFree(EList->Entries, 0, MEM_RELEASE);
    memset(EList, 0, sizeof(*EList));
}


struct nar_backup_file_explorer_context {
    
    char Letter;
    nar_fe_volume_handle FEHandle;
    INT32 ClusterSize;
    wchar_t RootDir[256];
    
    BYTE HandleOption;
    UINT32 LastIndx;
    
    unsigned int MFTRecordsCount;
    
    struct {
        INT16 I;
        UINT64 S[512];
    }HistoryStack;
    
    nar_record *MFTRecords;
    nar_file_entries_list EList;
    
    wchar_t CurrentDirectory[512];
};


struct nar_file_version_stack {
    
    /*
        Indicates how many version we must go back to start restore operation from given version.
        If let's say user requested from version 9, restore that file, we search 8-7-6-5th versions, and couldnt find file
        in 4th one. we must set this parameter as 5(we are not including latest version).
    */
    INT32 VersionsFound;
    
    /*
        Indicates which version we must start iterating when actually restoring file
    */
    INT32 StartingVersion;
    
    /*
        Offset to the file's mft record in backup file. Backful file begin + this value will lead
        file pointer to mft record
    */
    
    // 0th element is first backup that contains file, goes up to  (VersionsFound)
    UINT64 FileAbsoluteMFTOffset[64];
};

struct nar_fe_search_result {
    UINT64 FileMFTID;
    UINT64 AbsoluteMFTRecordOffset; // in bytes
    BOOLEAN Found;
};

struct lcn_from_mft_query_result {
    
    enum {
        FAIL = 0x0,
        SUCCESS = 0x1,
        HAS_DATA_IN_MFT = 0x2
    };
    
    INT8 Flags;
    
    // valid if record contains file data in it
    INT16 DataOffset;
    INT16 DataLen;
    
    /* 
I dont want to use dynamically allocated array then free it. 
Since these tasks are disk IO bounded, I can totally neglect cache behavior(thats a big sin) and preallocate big stack array and never have to deal with freeing it
     probably %95 of it will be empty most of the time
*/
    
    INT32 RecordCount;
    nar_record Records[1024];
    
};

struct nar_ext_query{
    std::vector<std::wstring> Files;
    
#if 0    
    wchar_t **Files; // list of zero terminated strings
    void *Mem;
    size_t FileCount;
    size_t Used;
    size_t Capacity;
#endif
    
    wchar_t Extension[32];
    wchar_t bf[512];
};

inline lcn_from_mft_query_result
ReadLCNFromMFTRecord(void* RecordStart);

/*
    Gets the intersection of the r1 and r2 arrays, writes new array to r3
*/
inline void
NarGetRegionIntersection(nar_record* r1, nar_record* r2, nar_record** intersections, INT32 len1, INT32 len2, INT32* IntersectionLen);

inline void
NarFreeRegionIntersection(nar_record* intersections);


// inf
// stores file names, instead of MFTID's. because in early version of volumes, user might have same file with different ID, like then later user might have delete it and paste another copy of the file with same name
// there, accepts new file as new copied one, totally normal thing for user but at backend of the NTFS, MFTID will be changed, because file will be deleted and new copied file will be assigned to new
// ID. Intuitively user expects it's file to be restored as what it has been appeared in history, not exacty copy of file that related with ID.
// Thats like business decision, storing MFTID's and restoring is much easier, but not intuitive for user, there is a slight line what file means to user and to us at backend. 



inline void
NarPushDirectoryStack(nar_backup_file_explorer_context *Ctx, UINT64 ID);

inline void
NarPopDirectoryStack(nar_backup_file_explorer_context* Ctx);


inline void
NarPopDirectoryStack(nar_backup_file_explorer_context* Ctx);

inline void
NarReleaseFileExplorerContext(nar_backup_file_explorer_context* Ctx);

inline void
NarGetFileListFromMFTID(nar_backup_file_explorer_context *Ctx, size_t TargetMFTID);

inline void 
NarFileExplorerPushDirectory(nar_backup_file_explorer_context* Ctx, UINT32 SelectedID);

inline void
NarFileExplorerPopDirectory(nar_backup_file_explorer_context* Ctx);

inline nar_fe_search_result
NarSearchFileInVolume(const wchar_t* RootDir, const wchar_t* FileName, nar_backup_id ID, INT32 Version);

inline BOOLEAN
NarFileExplorerSetFilePointer(nar_fe_volume_handle FEV, UINT64 NewFilePointer);

inline BOOLEAN
NarFileExplorerReadVolume(nar_fe_volume_handle FEV, void* Buffer, DWORD ReadSize, DWORD* OutBytesRead);

nar_file_version_stack
NarSearchFileInVersions(const wchar_t* RootDir, nar_backup_id ID, INT32 CeilVersion, const wchar_t* FileName);


inline void*
NarGetBitmapAttributeData(void *BitmapAttributeStart);

inline INT32
NarGetBitmapAttributeDataLen(void *BitmapAttributeStart);

inline BOOLEAN
NarParseIndexAllocationAttribute(void *IndexAttribute, nar_record *OutRegions, INT32 MaxRegionLen, INT32 *OutRegionsFound);

inline BOOLEAN
NarParseIndexAllocationAttributeSingular(void *IndexAttribute, nar_record *OutRegions, INT32 MaxRegionLen, INT32 *OutRegionsFound);

inline void
NarParseIndxRegion(void *Data, nar_file_entries_list *EList);

inline void
NarResolveAttributeList(nar_backup_file_explorer_context *Ctx, void *Attribute, size_t OriginalRecordID);

inline void
NarGetFileEntriesFromIndxClusters(nar_backup_file_explorer_context *Ctx, nar_record *Clusters, INT32 Count, BYTE *Bitmap, INT32 BitmapLen);


/*
    Ctx = output
    HandleOptions
        NAR_READ_MOUNTED_VOLUME = Reads mounted local disk VolumeLetter and ignores rest of the parameters, and makes FEV->BMEX NULL to. this will lead FEV to become normal volume handle
        NAR_READ_BACKUP_VOLUME 2 = Tries to find backup files in RootDir, if one not given, searches current running directory.
                                    Then according to backup region information, handles read-seak operations in wrapped function

*/
inline BOOLEAN
NarInitFileExplorerContext(nar_backup_file_explorer_context* Ctx, const wchar_t *RootDir, const wchar_t *MetadataPath);

inline INT32
NarGetVolumeClusterSize(char Letter);

// asserts Buffer is large enough to hold all data needed, since caller has information about metadata this isnt problem at all
inline BOOLEAN
ReadMFTLCNFromMetadata(HANDLE FHandle, backup_metadata Metadata, void *Buffer);


inline BOOLEAN
NarFileExplorerSetFilePointer(nar_fe_volume_handle FEV, UINT64 NewFilePointer);

inline BOOLEAN
NarFileExplorerReadVolume(nar_fe_volume_handle FEV, void* Buffer, DWORD ReadSize, DWORD* OutBytesRead);

inline void*
NarFindFileAttributeFromFileRecord(void *FileRecord, INT32 AttributeID);

/*
    args:
    FEV = output, simple
    HandleOptions
        NAR_READ_MOUNTED_VOLUME = Reads mounted local disk VolumeLetter and ignores rest of the parameters, and makes FEV->BMEX NULL to. this will lead FEV to become normal volume handle
        NAR_READ_BACKUP_VOLUME 2 = Tries to find backup files in RootDir, if one not given, searches current running directory.
                                    Then according to backup region information, handles read-seak operations in wrapped function
*/
#if 1
inline BOOLEAN 
NarInitFEVolumeHandle(nar_fe_volume_handle *FEV, INT32 HandleOptions, char VolumeLetter, const wchar_t *BackupMetadataPath);
#endif

inline BOOLEAN NarInitFEVolumeHandleFromBackup(nar_fe_volume_handle *FEV, const wchar_t* RootDir, const wchar_t* MetadataFilePath);
inline BOOLEAN NarInitFEVolumeHandleFromVolume(nar_fe_volume_handle *FEV, char VolumeLetter, INT32 Version, nar_backup_id ID, wchar_t *RootDir);


inline void 
NarFreeFEVolumeHandle(nar_fe_volume_handle FEV);

// input MUST be sorted
// Finds point Offset in relative to Records structure, useful when converting absolue volume offsets to our binary backup data offsets.
// returns NAR_POINT_OFFSET_FAILED if fails to find given offset, 
#define NAR_POINT_OFFSET_FAILED -1
inline INT64 
FindPointOffsetInRecords(nar_record *Records, INT32 Len, INT64 Offset);

/*
    Be careful about return value, its not a fail-pass thing
*/
inline INT
NarFileExplorerGetFileEntryFromData(void* IndxCluster, nar_file_entry* OutFileEntry);

inline void
NarFreeFileVersionStack(nar_file_version_stack Stack);


BOOLEAN
ConnectDriver(PLOG_CONTEXT Ctx);

#if 0
inline rec_or
GetOrientation(nar_record* M, nar_record* S);

void
RemoveDuplicates(region_chain** Metadatas,
                 region_chain* MDShadow, int ID);
#endif

#endif //__MSPYLOG_H__
