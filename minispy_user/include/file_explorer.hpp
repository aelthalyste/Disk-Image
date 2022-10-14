#pragma once


#ifndef Kilobyte
#define Kilobyte(val) ((val)*1024ll)
#define Megabyte(val) (Kilobyte(val)*1024ll)
#define Gigabyte(val) (Megabyte(val)*1024ll)
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b) ? (a) : (b))
#endif

#include "platform_io.hpp"
#include "nar.hpp"
#include "memory.hpp"
#include "narstring.hpp"
#include "nar_ntfs_utils.hpp"

#define NAR_FE_HAND_OPT_READ_MOUNTED_VOLUME 1
#define NAR_FE_HAND_OPT_READ_BACKUP_VOLUME 2

#define NAR_FEXP_INDX_ENTRY_SIZE(e) *(uint16_t*)((char*)(e) + 8)
#define NAR_FEXP_POSIX -1
#define NAR_FEXP_END_MARK -2
#define NAR_FEXP_SUCCEEDED 1
#define NAR_FE_DIRECTORY_FLAG     0x01


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
