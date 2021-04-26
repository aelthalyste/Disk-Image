#pragma once

#if _MSC_VER
#include <windows.h>
#endif

#include <stdint.h>
#include <string>

#include "../inc/minispy.h"


#ifdef 	__linux__
#define NAR_LINUX   1
#elif 	_WIN32
#define NAR_WINDOWS 1
#endif


#if _DEBUG || !_MANAGED 
#define ASSERT(exp) do{if(!(exp)){*(volatile int*)0 = 42;}} while(0);
#else
#define ASSERT(exp)
#endif


#define BOOLEAN char


#define Kilobyte(val) ((val)*1024ll)
#define Megabyte(val) (Kilobyte(val)*1024ll)
#define Gigabyte(val) (Megabyte(val)*1024ll)


#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define MAX(a,b) ((a)>(b) ? (a) : (b))

#define NAR_COMPRESSION_FRAME_SIZE (1024ull*1024ull*4ull)

#define NAR_INVALID_VOLUME_TRACK_ID (-1)
#define NAR_INVALID_DISK_ID (-1)
#define NAR_INVALID_ENTRY (-1)

#define NAR_DISKTYPE_GPT 'G'
#define NAR_DISKTYPE_MBR 'M'
#define NAR_DISKTYPE_RAW 'R'

#define NAR_FULLBACKUP_VERSION -1

#define NAR_EFI_PARTITION_LETTER 'S'
#define NAR_RECOVERY_PARTITION_LETTER 'R'


#define ZSTD_DLL_IMPORT 1
#include "zstd.h"


#if NAR_LINUX
#define NAR_BREAK do{__builtin_trap();}while(0);
#elif NAR_WINDOWS
#define NAR_BREAK do{__debugbreak();}while(0);
#endif

#define NAR_DEBUG(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)


#define NAR_DBG_ERR(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)


enum class BackupType : short {
    Diff,
    Inc
};


struct nar_record{
    unsigned int StartPos;
    unsigned int Len;
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
    wchar_t bf[64];
    memset(bf, 0, sizeof(bf));
    wsprintfW(bf, L"-%c%02d%02d%02d%02d", ID.Letter, ID.Month, ID.Day, ID.Hour, ID.Min);
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


