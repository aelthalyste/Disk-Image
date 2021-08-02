#pragma once

#if _MSC_VER
#include <windows.h>
#endif

#include <stdint.h>
#include <string>
#include "memory.h"


#ifdef 	__linux__
#define NAR_LINUX   1
#elif 	_WIN32
#define NAR_WINDOWS 1
#endif

#if NAR_WINDOWS
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


#if _DEBUG || !_MANAGED 

#if NAR_WINDOWS
#define ASSERT(exp) do{if(!(exp)){__debugbreak();}} while(0);
#else
#define ASSERT(exp) do{if(!(exp)){__asm__("int3");}} while(0);
#endif

#else
#define ASSERT(exp)
#endif


#define BOOLEAN char


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


#if NAR_LINUX
#define NAR_BREAK do{__builtin_trap();}while(0);
#elif NAR_WINDOWS
#define NAR_BREAK do{__debugbreak();}while(0);
#endif


#if NAR_WINDOWS
#define NAR_DEBUG(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#define NAR_DBG_ERR(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#else
#define NAR_DEBUG(fmt, ...) printf(fmt, __VA_ARGS__)
#define NAR_DBG_ERR(fmt, ...) printf(fmt, __VA_ARGS__)
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

struct NarUTF8{
    uint8_t *Str;
    uint32_t Len;
    uint32_t Cap;
};
#define NARUTF8(ch) NarUTF8{(uint8_t*)ch, sizeof(ch) - 1, 0}


static inline NarUTF8
NarStringCopy(NarUTF8 Input, nar_arena *Arena){
    NarUTF8 Result = {};
    Result.Str = (uint8_t*)ArenaAllocate(Arena, Input.Len);
    Result.Len = Input.Len;
    Result.Cap = Input.Cap;
    memcpy(Result.Str, Input.Str, Result.Len);
    ASSERT(Result.Str[Result.Len] == 0);
    return Result;
}

static inline bool
NarStringCopy(NarUTF8 *Destination, NarUTF8 Source){
    if(Destination->Cap >= Source.Len){
        memcpy(Destination->Str, Source.Str, Source.Len);
        Destination->Len = Source.Len;
        Destination->Str[Destination->Len] = 0;
        ASSERT(Destination->Str[Destination->Len] == 0); 
        return true;
    }
    return false;
}


static inline bool
NarStringConcatenate(NarUTF8 *Destination, NarUTF8 Append){
    
    // capacity with 0 means its immutable.
    if(Destination->Cap == 0){
        return false;
    }
    
    ASSERT(Destination->Cap);
    
    ASSERT(Destination->Cap >= Destination->Len + Append.Len);
    if(Destination->Cap < Destination->Len + Append.Len){
        return false;
    }
    
    for(size_t i = 0; 
        i<Append.Len; 
        i++)
    {
        Destination->Str[i + Destination->Len] = Append.Str[i];
    }
    Destination->Len += Append.Len;
    Destination->Str[Destination->Len] = 0;// null termination
    return true;
}

static inline NarUTF8
NarUTF8Init(void *Memory, uint32_t Len){
    NarUTF8 Result;
    Result.Str = (uint8_t*)Memory;
    Result.Len = 0;
    Result.Cap = Len;
    return Result;
}

inline NarUTF8
NarGetRootPath(NarUTF8 FileName, nar_arena *Arena);



union nar_record{
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
    UINT Count;
    UINT ReserveCount = 0;
    
    inline void Insert(DATA_TYPE Val) {
        Data = (DATA_TYPE*)realloc(Data, sizeof(Val) * ((ULONGLONG)Count + 1));
        memcpy(&Data[Count], &Val, sizeof(DATA_TYPE));
        Count++;
    }
    
};


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
        uint64_t RegionsMetadata;
        uint64_t Regions;
        
        uint64_t MFTMetadata;
        uint64_t MFT;
        
        uint64_t Recovery;
    }Size; //In bytes!
    
    struct {
        uint64_t RegionsMetadata;
        uint64_t AlignmentReserved;
        
        uint64_t MFTMetadata;
        uint64_t MFT;
        
        uint64_t Recovery;
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
        uint8_t Reserved[2048]; // Reserved for future usage
        struct {
            //FOR MBR things
            union{
            	int64_t GPT_EFIPartitionSize;
            	int64_t MBR_SystemPartitionSize;
            };
            
#if NAR_LINUX
            uint16_t BackupDate[8];
#else // _MSC_VER
            SYSTEMTIME BackupDate;
#endif
            char ProductName[NAR_MAX_PRODUCT_NAME];
            char ComputerName[NAR_MAX_COMPUTERNAME_LENGTH  + 1];
            
#if NAR_LINUX
            uint16_t TaskName[NAR_MAX_TASK_NAME_LEN];
            uint16_t TaskDescription[NAR_MAX_TASK_DESCRIPTION_LEN];
#else //_MSC_VER
            wchar_t TaskName[NAR_MAX_TASK_NAME_LEN];
            wchar_t TaskDescription[NAR_MAX_TASK_DESCRIPTION_LEN];
#endif
            
            nar_backup_id ID;
        	
        	unsigned char IsCompressed;
        	unsigned int FrameSize;
            
            size_t   CompressionInfoOffset;
            uint32_t CompressionInfoCount;
            
        };
    };
    
    uint64_t VolumeTotalSize;
    uint64_t VolumeUsedSize;
    
    // NOTE(Batuhan): Last volume offset must be written to disk that if this specific version were to be restored, version itself can be 5 gb big, but last offset it indicates that changes were made can be 100gb'th offset.
    uint64_t VersionMaxWriteOffset; 
    
    int Version; // -1 for full backup
    int ClusterSize; // 4096 default
    
    char Letter;
    uint8_t DiskType;
    unsigned char IsOSVolume;
    
    BackupType BT; // diff or inc
};

#pragma pack(pop)

static inline void
NarBackupIDToStr(nar_backup_id ID, std::wstring &Res);

static inline void
NarBackupIDToStr(nar_backup_id ID, std::string &Res);


template<typename StrType>
static inline void
GenerateMetadataName(nar_backup_id ID, int Version, StrType &Res);

template<typename StrType>
static inline void
GenerateBinaryFileName(nar_backup_id ID, int Version, StrType &Res);

// If function is called with Result argument as NULL, it returns maximum bytes
// needed to generate the name. Otherwise it returns how many bytes written to
// Result.
// Function silently overwrites Result string's length.
static inline uint32_t
GenerateMetadataNameUTF8(nar_backup_id ID, int32_t Version, NarUTF8 *Out){
    
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
    int ChWritten = snprintf(Bf, sizeof(Bf), "NB_M_%s-%c%02d%02d%02d%02d.nbfsm", VersionBf, ID.Letter, ID.Month, ID.Day, ID.Hour, ID.Min);
    ASSERT(ChWritten <= Out->Cap);
    if(ChWritten <= Out->Cap){
        memset(Out->Str, 0, Out->Cap);
        memcpy(Out->Str, Bf, ChWritten);
        Out->Len = ChWritten;
        Result = ChWritten;
    }
    
    return Result;
}

// If function is called with Result argument as NULL, it returns maximum bytes
// needed to generate the name. Otherwise it returns how many bytes written to
// Result.
// Function silently overwrites Result string's length.
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
    ASSERT(ChWritten <= Out->Cap);
    if(ChWritten <= Out->Cap){
        memset(Out->Str, 0, Out->Cap);
        memcpy(Out->Str, Bf, ChWritten);
        Out->Len = ChWritten;
        Result = Out->Len;
    }
    
    return Result;
}


// FUNDAMENTAL FILE NAME DRAFTS
/////////////////////////////////////////////////////
static inline void
NarGetMetadataDraft(std::string &Res){
	Res = std::string("NB_M_");
}

static inline void
NarGetMetadataDraft(std::wstring &Res){
	Res = std::wstring(L"NB_M_");
}
/////////////////////////////////////////////////////

/////////////////////////////////////////////////////
static inline void
NarGetBinaryDraft(std::string &Res){
	Res = std::string("NB_");
}

static inline void
NarGetBinaryDraft(std::wstring &Res){
	Res = std::wstring(L"NB_");
}
/////////////////////////////////////////////////////


/////////////////////////////////////////////////////
static inline void
NarGetMetadataExtension(std::string &Res){
	Res = std::string(".nbfsm");
}

static inline void
NarGetMetadataExtension(std::wstring &Res){
	Res = std::wstring(L".nbfsm");
}
/////////////////////////////////////////////////////


/////////////////////////////////////////////////////
static inline void
NarGetBinaryExtension(std::string &Res){
	Res = std::string(".nbfsf");
}

static inline void
NarGetBinaryExtension(std::wstring &Res){
	Res = std::wstring(L".nbfsf");
}
/////////////////////////////////////////////////////



// VERSION MIDFIXES
/////////////////////////////////////////////////////
static inline void
NarGetVersionMidFix(int Version, std::string &Res){
    if(Version == NAR_FULLBACKUP_VERSION){
    	Res = "FULL";
    }
	else{
		Res = std::to_string(Version);
	}
}

static inline void
NarGetVersionMidFix(int Version, std::wstring &Res){
    if(Version == NAR_FULLBACKUP_VERSION){
		Res = L"FULL";
    }
    else{
		Res = std::to_wstring(Version);
    }
}
/////////////////////////////////////////////////////



/////////////////////////////////////////////////////
static inline void
NarBackupIDToStr(nar_backup_id ID, std::wstring &Res){
    wchar_t bf[128];
    memset(bf, 0, sizeof(bf));
    swprintf(bf, 128, L"-%c%02d%02d%02d%02d", ID.Letter, ID.Month, ID.Day, ID.Hour, ID.Min);
    Res = std::wstring(bf);
}

static inline void
NarBackupIDToStr(nar_backup_id ID, std::string &Res){
    char bf[64];
    memset(bf, 0, sizeof(bf));
    snprintf(bf, sizeof(bf), "-%c%02d%02d%02d%02d", ID.Letter, ID.Month, ID.Day, ID.Hour, ID.Min);
    Res = std::string(bf);
}
/////////////////////////////////////////////////////


template<typename StrType>
static inline void
GenerateMetadataName(nar_backup_id ID, int Version, StrType &Res){
	NarGetMetadataDraft(Res);
	
	// VERSION NUMBER EMBEDDED AS STRING
	{
    	StrType garbage;
		NarGetVersionMidFix(Version, garbage);
		Res += garbage;
	}
    
    
	// BACKUP ID
	{
		// LETTER IS BEING SILENTLY APPENDED HERE
        StrType garbage;
		NarBackupIDToStr(ID, garbage);
		Res += garbage;
	}
    
	// EXTENSION
	{
		StrType garbage;
		NarGetMetadataExtension(garbage);
		Res += garbage;
	}
	// done;
}

template<typename StrType>
static inline void
GenerateBinaryFileName(nar_backup_id ID, int Version, StrType &Res){
	NarGetBinaryDraft(Res);
	
	// VERSION NUMBER EMBEDDED AS STRING
	{
    	StrType garbage;
		NarGetVersionMidFix(Version, garbage);
		Res += garbage;
	}
    
	// BACKUP ID
	{
		// LETTER IS BEING SILENTLY APPENDED HERE
        StrType garbage;
		NarBackupIDToStr(ID, garbage);
		Res += garbage;
	}
    
	// EXTENSION
	{
		StrType garbage;
		NarGetBinaryExtension(garbage);
		Res += garbage;
	}
}

//mmap(0, length, PROT_READ|PROT_WRITE, MAP_ANON, 0, 0)


inline bool
NarIsRegionIterValid(RegionCoupleIter Iter);


inline bool
__NarIsRegionIterExpired(RegionCoupleIter Iter);



inline RegionCoupleIter
NarInitRegionCoupleIter(const nar_record *Base, const nar_record *Ex, size_t BaseN, size_t ExN);

inline bool
NarIterateRegionCoupleUntilCollision(RegionCoupleIter *Iter);


inline void
NarNextExcludeIter(RegionCoupleIter *Iter);

inline RegionCoupleIter
NarStartExcludeIter(const nar_record *Base, const nar_record *Ex, size_t BaseN, size_t ExN);


inline RegionCoupleIter
NarStartIntersectionIter(const  nar_record *R1, const nar_record *R2, size_t R1Len, size_t R2Len);

inline void
NarNextIntersectionIter(RegionCoupleIter *Iter);


inline void
NarGetPreviousBackupInfo(int32_t Version, BackupType Type, int32_t *OutVersion);

inline void
NarConvertBackupMetadataToUncompressed(NarUTF8 Metadata);


inline size_t
NarLCNToVCN(nar_record *LCN, size_t LCNCount, size_t Offset);


/*
    ASSUMES RECORDS ARE SORTED
THIS FUNCTION REALLOCATES MEMORY VIA realloc(), DO NOT PUT MEMORY OTHER THAN ALLOCATED BY MALLOC, OTHERWISE IT WILL CRASH THE PROGRAM
*/
inline void
MergeRegions(data_array<nar_record>* R);

inline void
MergeRegionsWithoutRealloc(data_array<nar_record>* R);


inline bool
IsRegionsCollide(nar_record R1, nar_record R2);


// input MUST be sorted
// Finds point Offset in relative to Records structure, useful when converting absolue volume offsets to our binary backup data offsets.
// returns NAR_POINT_OFFSET_FAILED if fails to find given offset, 
inline point_offset 
FindPointOffsetInRecords(nar_record *Records, uint64_t Len, int64_t Offset);