#ifndef __MSPYLOG_H__
#define __MSPYLOG_H__

#include <stdio.h>
#include <vector>

#include <string>
#include <vector>

#include <stdio.h>


#if __GNUC__

inline std::string
wstr2str(const std::wstring& s){
	char *c = (char*)malloc(s.size() + 1);
	memset(c, 0, s.size());
	
	size_t sr = wcstombs(c, s.c_str(), s.size() + 1);
	if(sr == 0xffffffffffffffffull - 1)
		memset(c, 0, s.size() + 1);
	std::string Result(c);
	free(c);
	return Result;
}

inline std::wstring
str2wstr(const std::string& s){
	wchar_t *w = (wchar_t*)malloc((s.size() + 1)*sizeof(wchar_t));
	memset(w, 0, (s.size() + 1)*sizeof(wchar_t));
	
	size_t sr = mbstowcs(w, s.c_str(), s.size() + 1);
	if(sr == 0xffffffffffffffffull - 1)
		memset(w, 0, (s.size() + 1)*sizeof(wchar_t));
	std::wstring Result(w);
	free(w);
	return Result;
}

struct nar_backup_id{
    union{
        unsigned long long Q;
        struct{
            uint16_t Year;
            uint8_t Month;
            uint8_t Day;
            uint8_t Hour;
            uint8_t Min;
            uint8_t Letter;
        };
    };
};

#define MAX_PATH 260

#define BOOLEAN 	char
#define BOOL 		int
#define ULONGLONG 	unsigned long long
#define INT64 		int64_t
#define UINT64 		uint64_t
#define UINT32		uint32_t
#define INT16  		int16_t
#define INT8		int8_t
#define TRUE 		1
#define FALSE 		0
#define DWORD 		long
#define HANDLE 		void*
#endif


//#define printf(fmt, ...) NarLog(fmt, __VA_ARGS__)


typedef struct _record {
    UINT32 StartPos;
    UINT32 Len;
}nar_record, bitmap_region;

template<typename DATA_TYPE>
struct data_array {
    DATA_TYPE* Data;
    unsigned int Count;
    unsigned int ReserveCount = 0;
    
    inline void Insert(DATA_TYPE Val) {
        Data = (DATA_TYPE*)realloc(Data, sizeof(Val) * ((unsigned long long)Count + 1));
        memcpy(&Data[Count], &Val, sizeof(DATA_TYPE));
        Count++;
    }
    
};

template<typename T>
inline void Append(data_array<T> *Destination, data_array<T> App) {
    
    if (App.Data == 0) {
        return;
    }
    
    ULONGLONG NewSize = sizeof(T)* ((unsigned long long)App.Count + (unsigned long long)Destination->Count);
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

#define CLEANHANDLE(handle) if((handle)!=NULL) CloseHandle(handle);
#define CLEANMEMORY(memory) if((memory)!=NULL) free(memory);

#define MINISPY_NAME  L"MiniSpy"
#define NAR_BINARY_FILE_NAME L"NAR_BINARY_"

#define MAKE_W_STR(arg) L#arg

#ifdef _DEBUG
#define Assert(expression) if(!(expression)) do{*(int*)0 = 0;}while(0);
#else
#define Assert(expression) (expression)
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
IsSameVolumes(const wchar_t* OpName, const wchar_t VolumeLetter);

struct restore_target_inf;
struct restore_inf;

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
        char Reserved[2048]; // Reserved for future usage
        struct {
            //FOR MBR things
            INT64 GPT_EFIPartitionSize;
            INT64 MBR_SystemPartitionSize;

#if (_MSC_VER)
            SYSTEMTIME BackupDate;
#elif (__GNUC__)
			unsigned char BackupDate[16];
#endif

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
        M = {0};
    }
};


struct restore_inf {
    std::string TargetPartition;
    nar_backup_id BackupID;
    int Version;
    std::wstring RootDir;
    std::string BootPartition;
    std::string RecoveryPartition;
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

inline BOOLEAN
NarCreateCleanGPTBootablePartition(int DiskID, int VolumeSizeMB, int EFISizeMB, int RecoverySizeMB, char Letter);

inline BOOLEAN
NarCreateCleanGPTPartition(int DiskID, int VolumeSizeMB, char Letter);

BOOLEAN
NarCreateCleanMBRPartition(int DiskID, char VolumeLetter, int VolumeSize);


inline BOOLEAN
NarCreateCleanMBRBootPartition(int DiskID, char VolumeLetter, int VolumeSizeMB, int SystemPartitionSizeMB, int RecoveryPartitionSizeMB);

inline BOOLEAN
NarRemoveLetter(char Letter);

inline void
NarRepairBoot(char Letter);



/*
Function declerations
*/


#define Kilobyte(val) ((val)*1024ll)
#define Megabyte(val) (Kilobyte(val)*1024ll)
#define Gigabyte(val) (Megabyte(val)*1024ll)

void
MergeRegions(data_array<nar_record>* R);


BOOLEAN
CopyData(HANDLE S, HANDLE D, ULONGLONG Len, ULONGLONG FileOffset);

BOOLEAN
CopyData(HANDLE S, HANDLE D, ULONGLONG Len);


void
DisplayError(DWORD Code);


void
FreeBackupMetadataEx(backup_metadata_ex* BMEX);

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


backup_metadata_ex*
InitBackupMetadataEx(nar_backup_id, int Version, std::wstring RootDir);

backup_metadata
ReadMetadata(nar_backup_id ID, int Version, std::wstring RootDir);

BOOLEAN
OfflineRestore(restore_inf* R);

BOOLEAN
SaveMetadata(char Letter, int Version, int ClusterSize, BackupType BT, data_array<nar_record> BackupRegions, HANDLE VSSHandle, nar_record* MFTLCN, unsigned int MFTLCNCount, nar_backup_id BackupID);

BOOLEAN
RestoreIncVersion(restore_inf R, FILE* Volume); // Volume optional, might pass INVALID_HANDLE_VALUE

BOOLEAN
RestoreDiffVersion(restore_inf R, FILE* Volume); // Volume optional, might pass INVALID_HANDLE_VALUE

BOOLEAN
NarTruncateFile(HANDLE F, ULONGLONG TargetSize);

BOOLEAN
AppendRecoveryToFile(HANDLE File, char Letter);

BOOLEAN
RestoreRecoveryFile(restore_inf R);

BOOLEAN
RestoreVersionWithoutLoop(restore_inf R, BOOLEAN RestoreMFT, FILE* Volume); // Volume optional, might pass INVALID_HANDLE_VALUE

data_array<nar_record>
ReadMFTLCN(backup_metadata_ex* BMEX);

inline BOOLEAN
IsGPTVolume(char Letter);

inline BOOLEAN
IsSameVolumes(const wchar_t* OpName, const wchar_t VolumeLetter);

BOOL
CompareNarRecords(const void* v1, const void* v2);

inline BOOLEAN
IsRegionsCollide(nar_record R1, nar_record R2);

std::string
NarExecuteCommand(const char* cmd, std::string FileName);

/*Make these function generated from safe template*/
inline std::vector<std::string>
Split(std::string str, std::string delimiter);

inline std::vector<std::wstring>
Split(std::wstring str, std::wstring delimiter);

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


// asserts Buffer is large enough to hold all data needed, since caller has information about metadata this isnt problem at all
inline BOOLEAN
ReadMFTLCNFromMetadata(FILE* FHandle, backup_metadata Metadata, void *Buffer);


// input MUST be sorted
// Finds point Offset in relative to Records structure, useful when converting absolue volume offsets to our binary backup data offsets.
// returns NAR_POINT_OFFSET_FAILED if fails to find given offset, 
#define NAR_POINT_OFFSET_FAILED -1
inline INT64 
FindPointOffsetInRecords(nar_record *Records, int32_t Len, uint64_t Offset);


#if 0
inline rec_or
GetOrientation(nar_record* M, nar_record* S);

void
RemoveDuplicates(region_chain** Metadatas,
                 region_chain* MDShadow, int ID);
#endif

#endif //__MSPYLOG_H__
