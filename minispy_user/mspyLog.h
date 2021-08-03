#pragma once

#include "precompiled.h"
#include "compression.h"
#include "nar.h"
#include "../inc/minispy.h"
#include "file_explorer.h"

#if 1


#endif


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

#if 0
// some debug stuff
int64_t NarGetPerfCounter();
int64_t NarGetPerfFrequency();

// time elapsed in ms
double NarTimeElapsed(int64_t start);
#endif

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
}static GlobalLogs[512];
static int GlobalLogCount = 0;
static HANDLE GlobalLogMutex = 0;


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


#if 0
template<typename DATA_TYPE>
struct array {
    DATA_TYPE* Data;
    uint32_t   Count;
    uint32_t   ReserveCount;
    nar_arena  Arena;
    
    array(){
        Data         = 0;
        Count        = 0;
        ReserveCount = 0;
        Arena        = {0};
    }
    
    inline void Insert(const DATA_TYPE &Val) {
        if(Count < ReserveCount){
            memcpy(&Data[Count], &Val, sizeof(DATA_TYPE));
            Count++;
        }
        else{
            // TODO
            ASSERT(FALSE);
        }
        ASSERT(FALSE);
    }
    
    inline DATA_TYPE& operator[](uint64_t s){return Data[s];}
    
};

template<typename DT> array<DT>
InitArray(nar_arena *Arena, size_t InitalReserveCount){
    
    array<DT> Result;
    memset(&Result, 0, sizeof(Result));
    Result.Data         = (DT*)ArenaAllocateAligned(Arena, InitalReserveCount*sizeof(DT), sizeof(DT));
    Result.Count        = 0;
    Result.ReserveCount = InitalReserveCount;
    
    return Result;
}

#endif

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
    
    
    BackupStream_Errors Error;
    
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


;

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



inline BOOLEAN
IsNumeric(char val) {
    return val >= '0' && val <= '9';
}

BOOLEAN
NarSetVolumeSize(char Letter, int TargetSizeMB);

BOOLEAN
CreatePartition(int Disk, char Letter, unsigned size);


inline char
NarGetAvailableVolumeLetter();

inline BOOLEAN
NarRemoveLetter(char Letter);

inline void
NarRepairBoot(char Letter, char BootPartitionLetter);

data_array<disk_information>
NarGetDisks();



inline BOOLEAN
NarFormatVolume(char Letter);

inline void
NarGetProductName(char* OutName);


BOOLEAN
CopyData(HANDLE S, HANDLE D, ULONGLONG Len, ULONGLONG FileOffset);


VOID
DisplayError(DWORD Code);


BOOLEAN
WriteStream(volume_backup_inf* Inf, void* Buffer, int Size);

BOOLEAN
SetupStreamHandle(volume_backup_inf* V);

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

backup_metadata
ReadMetadata(nar_backup_id ID, int Version, std::wstring RootDir);


// Used to append recovery partition to metatadata file.
BOOLEAN
AppendRecoveryToFile(HANDLE File, char Letter);


BOOLEAN
NarTruncateFile(HANDLE F, ULONGLONG TargetSize);


inline BOOLEAN
AppendMFTFile(HANDLE File, HANDLE VSSHANDLE, data_array<nar_record> MFTLCN, int ClusterSize);

inline BOOLEAN
IsGPTVolume(char Letter);

void
NarCloseVolume(HANDLE V);

BOOLEAN
SaveMFT(volume_backup_inf* VolInf, HANDLE VSSHandle, data_array<nar_record>* MFTLCN);

BOOLEAN
SaveExtraPartitions(volume_backup_inf* V);

inline BOOLEAN
IsSameVolumes(const WCHAR* OpName, const WCHAR VolumeLetter);

BOOL
CompareNarRecords(const void* v1, const void* v2);

inline bool
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
NarGetBackupsInDirectory(const wchar_t* Directory, backup_metadata* B, int BufferSize, int* FoundCount);


inline point_offset
FindPointOffsetInRecords(nar_record *Records, uint64_t Len, int64_t Offset);



