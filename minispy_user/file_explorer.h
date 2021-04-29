#pragma once

#include "nar.h"
#include "platform_io.h"
#include <vector>



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

struct FileRecordHeader {
	uint32_t magic;
	uint16_t updateSequenceOffset;
	uint16_t updateSequenceSize;
	uint64_t logSequence;
	uint16_t sequenceNumber;
	uint16_t hardLinkCount;
	uint16_t firstAttributeOffset;
	uint16_t inUse : 1;
	uint16_t isDirectory : 1;
	uint32_t usedSize;
	uint32_t allocatedSize;
	uint64_t fileReference;
	uint16_t nextAttributeID;
	uint16_t unused;
	uint32_t recordNumber;
};

struct nar_fe_volume_handle{
    HANDLE VolumeHandle;
    nar_file_view Backup;
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

static inline BOOLEAN
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

static inline void
NarExtentFileEntryList(nar_file_entries_list *EList, unsigned int NewCapacity){
    if(EList){
        EList->MaxEntryCount = NewCapacity;
        VirtualAlloc(EList->Entries, NewCapacity*sizeof(nar_file_entry), MEM_COMMIT, PAGE_READWRITE);
        //EList->Entries = (nar_file_entry*)realloc(EList->Entries, NewCapacity*sizeof(nar_file_entry));
    }
}

static inline void
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

inline bool
NarParseIndexAllocationAttribute(void *IndexAttribute, nar_record *OutRegions, uint32_t MaxRegionLen, uint32_t *OutRegionsFound, bool BitmapCompatibleInsert = false);


bool
NarParseDataRun(void* DatarunStart, nar_record *OutRegions, uint32_t MaxRegionLen, uint32_t *OutRegionsFound, bool BitmapCompatibleInsert = false);

inline void
NarParseIndxRegion(void *Data, nar_file_entries_list *EList);

inline void
NarResolveAttributeList(nar_backup_file_explorer_context *Ctx, void *Attribute, size_t OriginalRecordID);

inline void NarGetFileEntriesFromIndxClusters(nar_backup_file_explorer_context *Ctx, nar_record *Clusters, INT32 Count, BYTE *Bitmap, INT32 BitmapLen);


uint32_t
NarGetFileID(void* FileRecord);

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

inline BOOLEAN 
NarInitFEVolumeHandle(nar_fe_volume_handle *FEV, INT32 HandleOptions, char VolumeLetter, const wchar_t *BackupMetadataPath);

inline BOOLEAN 
NarInitFEVolumeHandleFromVolume(nar_fe_volume_handle *FEV, char VolumeLetter, INT32 Version, nar_backup_id ID, wchar_t *RootDir);


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


inline void 
NarFreeMFTRegionsByCommandLine(nar_record *records);


/*
       Caller MUST CALL NarFreeMFTRegionsByCommandLine to free memory allocated by this function, otherwise, it will be leaked
*/
nar_record*
NarGetMFTRegionsByCommandLine(char Letter, unsigned int* OutRecordCount);
