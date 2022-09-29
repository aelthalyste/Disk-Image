#pragma once


#ifndef Kilobyte
#define Kilobyte(val) ((val)*1024ll)
#define Megabyte(val) (Kilobyte(val)*1024ll)
#define Gigabyte(val) (Megabyte(val)*1024ll)
#endif


#define MIN(a,b) ((a)<(b) ? (a) : (b))

#include "platform_io.hpp"
#include "nar.hpp"
#include "memory.hpp"
#include "narstring.hpp"

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

#define NAR_FEXP_INDX_ENTRY_SIZE(e) *(uint16_t*)((char*)(e) + 8)
#define NAR_FEXP_POSIX -1
#define NAR_FEXP_END_MARK -2
#define NAR_FEXP_SUCCEEDED 1

#define NAR_STANDART_FLAG         0x10
#define NAR_ATTRIBUTE_LIST        0x20
#define NAR_FILENAME_FLAG         0x30
#define NAR_DATA_FLAG             0x80
#define NAR_INDEX_ROOT_FLAG       0x90
#define NAR_INDEX_ALLOCATION_FLAG 0xA0
#define NAR_BITMAP_FLAG           0xB0

#define NAR_FE_DIRECTORY_FLAG     0x01

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

#pragma pack(push ,1)
struct data_attr_header{
    uint32_t Type;
    uint32_t LenWithHeader;
    uint8_t  NonResidentFlag;
    uint8_t  NameLen;
    uint16_t NameOffset;
    uint16_t Compressed : 1;  //0th byte
    uint16_t Empty      : 13; //
    uint16_t Encrypted : 1;   //14th byte
    uint16_t Sparse    : 1;   //15th byte
};
#pragma pack(pop)

struct name_pid{
    wchar_t *Name;
    uint32_t FileID;
    uint32_t ParentFileID;
    uint8_t  NameLen; // 
};


struct multiple_pid{
    name_pid PIDS[16];
    uint8_t Len = 0;
};


struct extension_search_result {
    wchar_t **Files;
    uint64_t Len;
};


struct extension_finder_memory{
    nar_arena Arena;
    linear_allocator StringAllocator;
    
    void*       FileBuffer;
    uint64_t    FileBufferSize;
    
    nar_kernel_record* MFTRecords;
    uint64_t           MFTRecordCount;
    
    void*       DirMappingMemory;
    void*       PIDArrMemory;
    
    uint64_t    TotalFC;
};


struct file_explorer_file{
    size_t   Size;
    
    wchar_t* Name; // Null terminated
    uint8_t  NameLen;
    
    uint32_t FileID;
    uint32_t ParentFileID;
    
    SYSTEMTIME LastModifiedTime;
    SYSTEMTIME CreationTime;
    
    uint8_t  IsDirectory;
};


struct file_explorer_memory{
    linear_allocator StringAllocator;
    nar_arena        Arena;
};

struct file_explorer{
    nar_file_view MetadataView;
    nar_file_view FullbackupView;
    
    const void* MFT;
    size_t MFTSize;
    size_t TotalFC;
    size_t ClusterSize;
    
    uint32_t DirectoryID;
    
    wchar_t* DirectoryPath;
    wchar_t* FullPathBuffer;
    uint64_t PathBufferSizes;
    
    file_explorer_file* Files;
    
    // used for fast searching.
    uint32_t *ParentIDs;
    uint32_t *FileIDs;
    uint64_t FileCount;
    
    file_explorer_memory Memory;
    char VolumeLetter;
    
    UTF8  *RootDir;
    
    int Version;
    nar_backup_id ID;
    
    uint64_t SearchNeedle;
    uint32_t SearchID;
};


struct attribute_list_data{
    void*    Data;
    uint32_t Len;
};

struct attribute_list_entry{
    uint32_t EntryType;
    uint32_t EntryFileID;
};

// Each entry is 8 byte, its ok to store them in stack.
struct attribute_list_contents{
    attribute_list_entry Entries[32];
};


#define IS_FILE_LAYOUT_RESIDENT(fdl) (fdl.Data != 0)
struct file_disk_layout{
    uint64_t     TotalSize;
    
    uint32_t     MaxCount;
    
    void        *ResidentData;
    
    nar_kernel_record  *LCN;
    nar_kernel_record  *SortedLCN;
    uint32_t     LCNCount;
};

struct file_restore_source{
    nar_file_view Backup;
    nar_file_view Metadata;
    
    const nar_kernel_record *BackupLCN;
    uint64_t   LCNCount;
    
    BackupType    Type;
    int           Version;
    nar_backup_id ID;
};


#ifdef  _MANAGED
public
#endif
enum class FileRestore_Errors : int {
    Error_NoError,
    Error_EndOfBackups,
    Error_InsufficentBufferSize,
    Error_LCNToVCNMap, 
#if 0    
    Error_DecompressionUnknownContentSize,
    Error_DecompressionErrorContentsize,
    Error_DecompressionCompressedSize,
#endif
    
    Error_Decompression,
    Error_NullFileViews,
    Error_Count
};

struct file_restore_ctx{
    file_disk_layout    Layout;
    file_restore_source Source;
    
    /* 
    Intersection iter that keeps where we should fetch next region from
    backup
        */
    RegionCoupleIter     IIter;
    
    size_t AdvancedSoFar;
    size_t ClustersLeftInRegion;
    
    nar_memory_pool LCNPool;
    void*           DecompBuffer;
    size_t          DecompBufferSize;
    nar_arena       StringAllocator;// less than 1mb 
    UTF8            *RootDir; // directory to look for backups
    
    nar_kernel_record *ActiveLCN;
    size_t            ActiveLCNCount;
    
    FileRestore_Errors Error;
    
    size_t ClusterSize;
};


struct file_restore_advance_result{
    size_t Offset;
    size_t Len;
};


#if 1

inline BOOLEAN
NarSetFilePointer(HANDLE File, ULONGLONG V);


void*
NarGetBitmapAttributeData(void *BitmapAttributeStart);

int32_t
NarGetBitmapAttributeDataLen(void *BitmapAttributeStart);

inline bool
NarParseIndexAllocationAttribute(void *IndexAttribute, nar_kernel_record *OutRegions, int64_t MaxRegionLen, int64_t *OutRegionsFound, bool BitmapCompatibleInsert = false);


bool
NarParseDataRun(void* DatarunStart, nar_kernel_record *OutRegions, int64_t MaxRegionLen, int64_t *OutRegionsFound, bool BitmapCompatibleInsert = false);


inline uint32_t
NarGetFileID(void* FileRecord);

inline uint8_t
NarIsFileLinked(void* FileRecord);

inline uint32_t
NarGetFileBaseID(void* FileRecord);

inline int32_t
NarGetVolumeClusterSize(char Letter);


HANDLE
NarOpenVolume(char Letter);



inline void*
NarFindFileAttributeFromFileRecord(void *FileRecord, int32_t AttributeID);



bool
NarGetMFTRegionsFromBootSector(HANDLE Volume, 
                               nar_kernel_record* Out, 
                               uint32_t* OutLen, 
                               uint32_t Capacity);



multiple_pid
NarGetFileNameAndParentID(void *FileRecord);

extension_finder_memory
NarSetupExtensionFinderMemory(HANDLE VolumeHandle);

void
NarFreeExtensionFinderMemory(extension_finder_memory *Memory);

extension_search_result
NarFindExtensions(char VolumeLetter, HANDLE VolumeHandle, wchar_t **ExtensionList, size_t ExtensionListCount, extension_finder_memory *Memory);






file_explorer_memory
NarInitFileExplorerMemory(uint32_t TotalFC);

void
NarFreeFileExplorerMemory(file_explorer_memory *Memory);

file_explorer
NarInitFileExplorer(const UTF8 *MetadataPath);


file_explorer_file*
FEStartParentSearch(file_explorer *FE, uint32_t ParentID);

file_explorer_file*
FENextParent(file_explorer *FE, file_explorer_file *CurrentFile);

attribute_list_contents
NarGetAttributeListContents(void* AttrListDataStart, uint64_t DataLen);


bool IsValidAttrEntry(attribute_list_entry Entry);

attribute_list_contents
GetAttributeListContents(void* AttrListDataStart, uint64_t DataLen);


uint64_t
SolveAttributeListReferences(const void* MFTStart,
                             void* BaseFileRecord,
                             attribute_list_contents Contents, file_explorer_file* Files,
                             linear_allocator* StringAllocator
                             );



file_restore_ctx
NarInitFileRestoreCtx(file_explorer *FE, file_explorer_file *Target, const UTF8 *RootDir, nar_backup_id ID, int Version, nar_arena *Arena);

file_restore_ctx
NarInitFileRestoreCtx(file_explorer *FE, file_explorer_file* Target, nar_arena *Arena);


void
NarFreeFileRestoreCtx(file_restore_ctx *Ctx);



file_restore_advance_result
NarAdvanceFileRestore(file_restore_ctx *ctx, void* Out, size_t OutSize);


file_explorer_file*
FEFindFileWithID(file_explorer* FE, uint32_t ID);

file_explorer_file*
FENextFileInDir(file_explorer *FE, file_explorer_file *CurrentFile);


void
NarFreeFileExplorer(file_explorer* FileExplorer);

wchar_t*
FEGetFileFullPath(file_explorer* FE, file_explorer_file* BaseFile);


file_restore_source
NarInitFileRestoreSource(const UTF8 *MetadataName, UTF8 *BinaryName);


file_restore_source 
NarInitFileRestoreSource(const UTF8 *RootDir, nar_backup_id ID, int32_t Version, nar_arena *StringArena);

#endif
