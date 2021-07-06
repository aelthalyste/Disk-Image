#pragma once

#include <stdio.h>
#include "minispy.h"

#if 1
#include <string>
#include <vector>
#include <atlbase.h>

#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <vsmgmt.h>

#include <stdio.h>
#endif

#if 0
#define TIMED_BLOCK__(NAME, Number, ...) timed_block timed_##Number(__COUNTER__, __LINE__, __FUNCTION__, NAME);
#define TIMED_BLOCK_(NAME, Number, ...)  TIMED_BLOCK__(NAME, Number,  ## __VA__ARGS__)
#define TIMED_BLOCK(...)                 TIMED_BLOCK_("UNNAMED", __LINE__, ## __VA__ARGS__)
#define TIMED_NAMED_BLOCK(NAME, ...)     TIMED_BLOCK_(NAME, __LINE__, ## __VA__ARGS__)
#else

#define TIMED_BLOCK__(NAME, Number, ...)
#define TIMED_BLOCK_(NAME, Number, ...)  
#define TIMED_BLOCK(...)                 
#define TIMED_NAMED_BLOCK(NAME, ...)     
#endif

struct debug_record {
    char* FunctionName;
    char* Description;
    uint64_t Clocks;
    
    uint32_t ThreadIndex;
    uint32_t LineNumber;
    uint32_t HitCount;
};

debug_record GlobalDebugRecordArray[1000];

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

// some debug stuff
int64_t NarGetPerfCounter();
int64_t NarGetPerfFrequency();

// time elapsed in ms
double NarTimeElapsed(int64_t start);

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
		//File = fopen("NAR_APP_LOG_FILE.txt", "a");
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
        printf(buf);
    }
    
#endif
    
    
#undef MAX_BUF_LEN
    
}

//#define printf(fmt, ...) NarLog(fmt, __VA_ARGS__)

enum rec_or {
    LEFT = 0,
    RIGHT = 1,
    COLLISION = 2,
    OVERRUN = 3,
    SPLIT = 4,
};

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


typedef char NARDP;

inline BOOLEAN
IsSameVolumes(const WCHAR* OpName, const WCHAR VolumeLetter);

struct volume_backup_inf;



#if _MANAGED
public
#endif

enum class BackupStream_Errors: int{
    Error_NoError,
    Error_Read,
    Error_SetFP,
    Error_SizeOvershoot,
    Error_Compression,
    Error_Count
};


struct stream {
    
    data_array<nar_record> Records;
    INT32 RecIndex;
    INT32 ClusterIndex;
    HANDLE Handle; //Used for streaming data to C#
    
    
    // BackupStream_Errors Error;
    
#if 1    
    BackupStream_Errors Error;
#endif
    
    const char* GetErrorDescription(){
        
        static const struct {
            BackupStream_Errors val;
            const char* desc;
        }
        Table[] = {
            {
                BackupStream_Errors::Error_NoError,
                "No error occured during stream\n"
            },
            {
                BackupStream_Errors::Error_Read,
                "Error occured while reading shadow copy\n"
            },
            {
                BackupStream_Errors::Error_SetFP,
                "Error occured while setting volume file pointer\n"
            },
            {
                BackupStream_Errors::Error_SizeOvershoot,
                "Logical cluster exceeds volume size\n"
            },
            {
                BackupStream_Errors::Error_Compression,
                "Internal compression error occured\n"
            },
            {
                BackupStream_Errors::Error_Count,
                "Error_Count is not an error. This is placeholder\n"
            }
        };
        
        const int TableElCount  =sizeof(Table)/sizeof(Table[0]);
        static_assert(TableElCount - 1 == (int)BackupStream_Errors::Error_Count, "There must be same number of descriptions as error count\n");
        
        for(size_t i =0; i<TableElCount; i++){
            if(Table[i].val == this->Error){
                return Table[i].desc;
            }
        }
        
        return "Couldn't find error code in table, you must not be able to see this message\n";
    }
    
    
    // If compression enabled, this value is equal to uncompressed size of the resultant compression job
    // otherwise it's equal to readstream's return value
    uint32_t BytesProcessed;
    
    bool ShouldCompress;
    void *CompressionBuffer;
    size_t BufferSize;
    ZSTD_CCtx* CCtx;
    
    
};




/*
This function silently merges local time with given parameters
*/
inline nar_backup_id
NarGenerateBackupID(char Letter);

inline std::wstring
GenerateLogFilePath(char Letter);



struct volume_backup_inf {
    
    wchar_t Letter;
    BOOLEAN IsOSVolume;
    BOOLEAN INVALIDATEDENTRY; // If this flag is set, this entry is unusable. accessing its element wont give meaningful information to caller.
    
    BackupType BT = BackupType::Inc;
    
    
    int32_t Version;
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
    
    /*
    Valid after diff-incremental setup. Stores all changes occured on disk, starting from latest incremental, or beginning if request was diff
    Diff between this and RecordsMem, RecordsMem is just temporary buffer that stores live changes on the disk, and will be flushed to file after it's available
    
    This structure contains information to track stream head. After every read, ClusterIndex MUST be incremented accordingly and if read operation exceeds that region, RecIndex must be incremented too.
    */
    
    union {
        struct{
            INT64 BackupStartOffset;
        }DiffLogMark;
        
        struct{
            INT64 LastBackupRegionOffset;
        }IncLogMark;
    };
    
    DWORD VolumeTotalClusterCount;
    
    stream Stream;
    
    CComPtr<IVssBackupComponents> VSSPTR;
    VSS_ID SnapshotID;
    
};

struct LOG_CONTEXT {
    HANDLE Port;
    data_array<volume_backup_inf> Volumes;
};
typedef LOG_CONTEXT* PLOG_CONTEXT;


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


struct point_offset{
    int64_t  Offset;
    uint64_t  Readable; // remaining region length
    uint64_t Indice;        // region indice we just found
};


inline BOOLEAN
IsNumeric(char val) {
    return val >= '0' && val <= '9';
}

bool
CheckStreamCompletedSuccessfully(volume_backup_inf *V){
    if(V){
        return (V->Stream.Error == BackupStream_Errors::Error_NoError);
    }
}

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


BOOLEAN
WriteStream(volume_backup_inf* Inf, void* Buffer, int Size);

BOOLEAN
SetupStream(PLOG_CONTEXT C, wchar_t L, BackupType Type, DotNetStreamInf* SI = NULL, bool ShouldCompress = false);

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

backup_metadata
ReadMetadata(nar_backup_id ID, int Version, std::wstring RootDir);

BOOLEAN
SaveMetadata(char Letter, int Version, int ClusterSize, BackupType BT, data_array<nar_record> BackupRegions, nar_backup_id BackupID, bool IsCompressed);

// Used to append recovery partition to metatadata file.
BOOLEAN
AppendRecoveryToFile(HANDLE File, char Letter);


BOOLEAN
NarTruncateFile(HANDLE F, ULONGLONG TargetSize);

inline ULONGLONG
NarGetFilePointer(HANDLE F);

inline BOOLEAN
AppendMFTFile(HANDLE File, HANDLE VSSHANDLE, data_array<nar_record> MFTLCN, int ClusterSize);

inline BOOLEAN
IsGPTVolume(char Letter);

void
NarCloseVolume(HANDLE V);

BOOLEAN
NarGetVolumeGUIDKernelCompatible(wchar_t Letter, wchar_t* VolumeGUID);

BOOLEAN
SaveMFT(volume_backup_inf* VolInf, HANDLE VSSHandle, data_array<nar_record>* MFTLCN);

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


/*
    Gets the intersection of the r1 and r2 arrays, writes new array to r3
*/
inline void
NarGetRegionIntersection(nar_record* r1, nar_record* r2, nar_record** intersections, INT32 len1, INT32 len2, INT32* IntersectionLen);

inline void
NarFreeRegionIntersection(nar_record* intersections);




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

BOOLEAN
ConnectDriver(PLOG_CONTEXT Ctx);


inline point_offset
FindPointOffsetInRecords(nar_record *Records, uint64_t Len, int64_t Offset);