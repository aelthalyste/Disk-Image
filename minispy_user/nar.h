#pragma once

#include "package.h"

#ifdef  __linux__
#define NAR_LINUX   1
#elif   _WIN32
#define NAR_WINDOWS 1
#endif

#if NAR_WINDOWS
#include <windows.h>
#include "../inc/minispy.h"
#else


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
#endif

#if _DEBUG && !_MANAGED 
#if NAR_WINDOWS

#define ASSERT(exp) do{if(!(exp)){printf("### !ASSERT! ###\nFILE : %s\nFUNCTION & LINE : %s %d\nDATE : [%s] : [%s]\n", __FILE__, __FUNCTION__, __LINE__, __DATE__, __TIME__); __debugbreak();}} while(0);
#define NAR_BREAK do{__debugbreak();}while(0);

#else // IF NAR WINDOWS

#define ASSERT(exp) do{if(!(exp)){__asm__("int3");}} while(0);
#define NAR_BREAK do{__builtin_trap();}while(0);

#endif // IF NAR WINDWOS END

#else

#define ASSERT(exp)
#define NAR_BREAK
#endif





#define NAR_METADATA_EXTENSION  ".nbfsm"
#define NAR_BINARY_EXTENSION    ".nbfsf"

#define Kilobyte(val) ((val)*1024ll)
#define Megabyte(val) (Kilobyte(val)*1024ll)
#define Gigabyte(val) (Megabyte(val)*1024ll)


#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define MAX(a,b) ((a)>(b) ? (a) : (b))

#define NAR_COMPRESSION_FRAME_SIZE (1024ull*1024ull*16ull)

#define NAR_INVALID_VOLUME_TRACK_ID (-1)
#define NAR_INVALID_DISK_ID (-1)
#define NAR_INVALID_ENTRY (-1)

#define NAR_DISKTYPE_GPT 'G'
#define NAR_DISKTYPE_MBR 'M'
#define NAR_DISKTYPE_RAW 'R'

#define NAR_FULLBACKUP_VERSION     (-1)
#define NAR_INVALID_BACKUP_VERSION (-2)

#define NAR_EFI_PARTITION_LETTER 'S'
#define NAR_RECOVERY_PARTITION_LETTER 'R'


#include "precompiled.h"
#include "memory.h"
#include "narstring.h"
#include "memory.h"



#if NAR_WINDOWS
#define NAR_DEBUG(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#define NAR_DBG_ERR(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#else
#define NAR_DEBUG(fmt, ...) printf(fmt, __VA_ARGS__)
#define NAR_DBG_ERR(fmt, ...) printf(fmt, __VA_ARGS__)
#endif


struct nar_time {
    uint16_t YEAR; // 2000 + YEAR is the actual value
    uint8_t MONTH;
    uint8_t DAY;
    uint8_t HOUR;
    uint8_t MIN;
    uint8_t SEC;
    // 7 bytes
};


// time-date for windows
#if NAR_WINDOWS
static inline nar_time SystemTimeToNarTime(SYSTEMTIME *w_time) {
    nar_time result;
    result.YEAR     = (uint16_t)w_time->wYear;
    result.MONTH    = (uint8_t)w_time->wMonth;
    result.DAY      = (uint8_t)w_time->wDay;
    result.HOUR     = (uint8_t)w_time->wHour;
    result.MIN      = (uint8_t)w_time->wMinute;
    result.SEC      = (uint8_t)w_time->wSecond;
    return result;
}

static inline nar_time NarGetLocalTime() {
    SYSTEMTIME w_time;
    GetLocalTime(&w_time);
    return SystemTimeToNarTime(&w_time);
}
#endif



// time-date for linux
#if NAR_LINUX

#include <time.h>
static inline nar_time TmTimeToNarTime(struct tm * tm) {
    Bg_Date result;
    result.YEAR   = tm->tm_year + 1900;
    result.MONTH  = tm->tm_mon;
    result.DAY    = tm->tm_mday;
    result.HOUR   = tm->tm_hour;
    result.MIN    = tm->tm_min;
    result.SEC    = tm->tm_sec;
    return result;
}

static inline nar_time NarGetLocalTime() {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    return TmTimeToNarTime(&tm); 
}

#endif




#if 1
static HANDLE GlobalLogMutex = 0;
static void
NarLog(const char *str, ...){
    
    char buf[1024];
    
    memset(buf, 0, sizeof(buf));
    
    
    va_list args;
    va_start(args, str);
    vsnprintf(buf, sizeof(buf), str, args);
    va_end(args);
    
    
    char time_buf[128];
    // time
    {
        SYSTEMTIME Time = { 0 };
        GetLocalTime(&Time);
        snprintf(time_buf, sizeof(time_buf), "[%02d/%02d/%04d | %02d:%02d:%02d] : ", Time.wDay, Time.wMonth, Time.wYear, Time.wHour, Time.wMinute, Time.wSecond);
    }
    
    char big_buffer[2048];
    memset(big_buffer, 0, sizeof(big_buffer));
    int WriteSize = snprintf(big_buffer, sizeof(big_buffer), "%s %s", time_buf, buf);
    
    static bool fileinit = false;
    static HANDLE File = INVALID_HANDLE_VALUE;
    if(fileinit == false){
        File = CreateFileA("C:\\ProgramData\\NarDiskBackup\\NAR_APP_LOG_FILE.txt", GENERIC_WRITE|GENERIC_READ, FILE_SHARE_READ, 0, OPEN_ALWAYS, 0, 0);
        
        LARGE_INTEGER Start  = {};
        LARGE_INTEGER Result = {};
        SetFilePointerEx(File, Start, &Result, FILE_END);

        fileinit = true;
    }
    
    if(File != INVALID_HANDLE_VALUE){
        DWORD br = 0;
        WriteFile(File, big_buffer, WriteSize, &br, 0);		
        FlushFileBuffers(File);
        //OutputDebugStringA(buf);
    }
    else{
        printf(buf);
    }
    
    
    
}


#define printf(fmt, ...) NarLog(fmt, __VA_ARGS__)
#endif





template <typename F>
struct privDefer {
    F f;
    privDefer(F f) : f(f) {}
    ~privDefer() { f(); }
};

template <typename F>
privDefer<F> defer_func(F f) {
    return privDefer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = defer_func([&](){code;})


enum class BackupType : short {
    Diff,
    Inc
};


const int32_t NAR_COMPRESSION_NONE = 0;
const int32_t NAR_COMPRESSION_LZ4  = 1;
const int32_t NAR_COMPRESSION_ZSTD = 2;


#pragma pack(push ,1) // force 1 byte alignment
struct backup_information {
    uint64_t SizeOfBinaryData;
    uint64_t VersionMaxWriteOffset;

    uint64_t OriginalSizeOfVolume;

    uint64_t EFIPartitionSize;       // gpt + os volume 
    uint64_t SystemPartitionSize;    // mbr + os volume
    uint64_t RecoveryPartitionSize;  // gpt + os volume? (probably, gotta look spec)    

    uint32_t ClusterSize;

    int32_t Version;
    BackupType BT;

    int8_t Letter;
    int8_t DiskType;

    uint8_t  IsOSVolume;

    uint8_t IsBinaryDataCompressed;
    uint8_t CompressionType;

    nar_backup_id BackupID;
    nar_time MetadataTimeStamp;
};

#pragma pack(pop)


struct backup_package {
    backup_information BackupInformation;
    Package_Reader     Package;
};


struct nar_fs_table_header {
    uint64_t data_size;
    int32_t  DataCompressionType;
};

// 2 bytes for the size of the actual name, then the name itself, 
// WITHOUT null termination
struct nar_name_table : nar_fs_table_header {
    uint8_t *names;
};

struct nar_layout_table : nar_fs_table_header {
    uint8_t *layouts;
};

struct nar_file_table : nar_fs_table_header {
    uint32_t *ids;
};

union nar_record {
    struct{
        uint32_t StartPos;
        uint32_t Len;
    };
    struct{
        uint32_t CompressedSize;
        uint32_t DecompressedSize;
    };
};


template<typename DATA_TYPE>
struct data_array {
    DATA_TYPE* Data;
    uint32_t Count;
    uint32_t ReserveCount = 0;
    
    inline void Insert(DATA_TYPE Val) {
        Data = (DATA_TYPE*)realloc(Data, sizeof(Val) * ((size_t)Count + 1));
        memcpy(&Data[Count], &Val, sizeof(DATA_TYPE));
        Count++;
    }
    
};

template<typename T> inline void
FreeDataArray(data_array<T>* V) {
    if (V != NULL) {
        free(V->Data);
        V->Data = 0;
        V->Count = 0;
    }
}

template<typename T>
inline void Append(data_array<T> *Destination, data_array<T> App) {
    
    if (App.Data == 0) {
        return;
    }
    
    uint64_t NewSize = sizeof(T)* ((uint64_t)App.Count + (uint64_t)Destination->Count);
    Destination->Data = (T*)realloc(Destination->Data, NewSize);
    memcpy(&Destination->Data[Destination->Count], App.Data, App.Count * sizeof(T));
    Destination->Count += App.Count;
    
}


struct RegionCoupleIter{
    const nar_record *R1;
    const nar_record *R2;
    
    size_t     R1Len;
    size_t     R2Len;
    
    const nar_record *R1End;
    const nar_record *R2End;
    
    const nar_record *R1Iter;
    const nar_record *R2Iter;
    
    nar_record __CompRegion;
    nar_record It;
};


struct point_offset{
    int64_t  Offset;
    uint64_t  Readable; // remaining region length
    uint64_t Indice;        // region indice we just found
};




static inline void
NarBackupIDToStr(nar_backup_id ID, char *Res, int ResMax);


#if 0
static inline uint32_t
GenerateBinaryFileNameUTF8(nar_backup_id ID, int32_t Version, NarUTF8 *Out){
    
    if(Out == NULL){
        return 27;
    }
    uint32_t Result = 0;
    
    char Bf[1024];
    char VersionBf[32];
    if(Version == NAR_FULLBACKUP_VERSION){
        snprintf(VersionBf, sizeof(VersionBf), "%s", "FULL");
    }
    else{
        snprintf(VersionBf, sizeof(VersionBf), "%d", Version);
    }
    
    // 27
    int ChWritten = snprintf(Bf, sizeof(Bf), "NB_%s-%c%02d%02d%02d%02d.nbfsf", VersionBf, ID.Letter, ID.Month, ID.Day, ID.Hour, ID.Min);
    if(ChWritten <= (int)Out->Cap){
        memset(Out->Str, 0, Out->Cap);
        memcpy(Out->Str, Bf, ChWritten);
        Out->Len = ChWritten;
        Result = Out->Len;
    }
    
    return Result;
}
#endif




static inline void 
NarBackupIDToStr(nar_backup_id ID, char *Res, int MaxRes){
    snprintf(Res, MaxRes, "-%c%02d%02d%02d%02d", ID.Letter, ID.Month, ID.Day, ID.Hour, ID.Min);
}




bool
NarIsRegionIterValid(RegionCoupleIter Iter);


bool
__NarIsRegionIterExpired(RegionCoupleIter Iter);



inline RegionCoupleIter
NarInitRegionCoupleIter(const nar_record *Base, const nar_record *Ex, size_t BaseN, size_t ExN);

inline bool
NarIterateRegionCoupleUntilCollision(RegionCoupleIter *Iter);


inline void
NarNextExcludeIter(RegionCoupleIter *Iter);

RegionCoupleIter
NarStartExcludeIter(const nar_record *Base, const nar_record *Ex, size_t BaseN, size_t ExN);


RegionCoupleIter
NarStartIntersectionIter(const  nar_record *R1, const nar_record *R2, size_t R1Len, size_t R2Len);

void
NarNextIntersectionIter(RegionCoupleIter *Iter);

void
NarGetPreviousBackupInfo(int32_t Version, BackupType Type, int32_t *OutVersion);

size_t
NarLCNToVCN(nar_record *LCN, size_t LCNCount, size_t Offset);


/*
    ASSUMES RECORDS ARE SORTED
THIS FUNCTION REALLOCATES MEMORY VIA realloc(), DO NOT PUT MEMORY OTHER THAN ALLOCATED BY MALLOC, OTHERWISE IT WILL CRASH THE PROGRAM
*/
void
MergeRegions(data_array<nar_record>* R);

void
MergeRegionsWithoutRealloc(data_array<nar_record>* R);


bool
IsRegionsCollide(nar_record R1, nar_record R2);


// input MUST be sorted
// Finds point Offset in relative to Records structure, useful when converting absolue volume offsets to our binary backup data offsets.
// returns NAR_POINT_OFFSET_FAILED if fails to find given offset, 
point_offset 
FindPointOffsetInRecords(nar_record *Records, uint64_t Len, int64_t Offset);


int
CompareNarRecords(const void* v1, const void* v2);


static inline int8_t
IsNumeric(char val) {
    return val >= '0' && val <= '9';
}


#pragma warning(push)
#pragma warning(disable:4244)
#pragma warning(disable:4146)

// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

typedef struct { uint64_t state;  uint64_t inc; } pcg32_random_t;

static inline uint32_t 
pcg32_random_r(pcg32_random_t* rng)
{
    uint64_t oldstate = rng->state;
    // Advance internal state
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc|1);
    // Calculate output function (XSH RR), uses old state for max ILP
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

#pragma warning(pop)


static inline bool
NarIsFullOnlyBackup(nar_backup_id ID){
    return (ID.Q & (1<<15));
}


static inline nar_backup_id
NarSetAsFullOnlyBackup(nar_backup_id ID){
    nar_backup_id Result = ID;
    Result.Q |= (1<<15);
    return Result;
}

UTF8 **GetFilesInDirectoryWithExtension(const UTF8 *DirectoryAsUTF8, uint64_t *OutCount, UTF8 *Extension);
UTF8 **GetFilesInDirectory(const UTF8 *Directory, uint64_t *OutCount);
void FreeDirectoryList(UTF8 **List, uint64_t Count);


#if 0

nar_time {
    uint8_t year   : 5;
    uint8_t month  : 4;
    uint8_t day    : 5;
    uint8_t hour   : 5;
    uint8_t minute : 6;
    uint8_t second : 6;
};

typedef uint32_t layout_id;
typedef uint32_t file_id;

struct file_record {
    uint64_t FileSize;
    uint32_t CreationTime;
    uint32_t ModifiedTime;
    uint32_t FileNameSlot;
    file_id ID;
    file_id ParentID;
    layout_id Layout;
};

struct file_table {

};

300
n_of_files * ( bytes) + 6mb per million file;
#endif