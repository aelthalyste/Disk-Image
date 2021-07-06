#pragma once

#include "nar.h"

#define Kilobyte(val) ((val)*1024ll)

#define Megabyte(val) (Kilobyte(val)*1024ll)

#define Gigabyte(val) (Megabyte(val)*1024ll)

#define MIN(a,b) ((a)<(b) ? (a) : (b))


#ifndef ASSERT
#define ASSERT(exp)
#endif

#include "memory.h"
#include "platform_io.h"

#include <stdint.h>
#include <windows.h>

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

#define NAR_STANDART_FLAG         0x10
#define NAR_ATTRIBUTE_LIST        0x20
#define NAR_FILENAME_FLAG         0x30
#define NAR_DATA_FLAG             0x80
#define NAR_INDEX_ROOT_FLAG       0x90
#define NAR_INDEX_ALLOCATION_FLAG 0xA0
#define NAR_BITMAP_FLAG           0xB0

#define NAR_FE_DIRECTORY_FLAG     0x01

#define NAR_OFFSET(m, o) ((char*)(m) + (o))

#if 0
struct nar_record{
    uint32_t StartPos;
    uint32_t Len;
};
#endif

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
    
    nar_record* MFTRecords;
    uint64_t    MFTRecordCount;
    
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
    const void* MFT;
    size_t MFTSize;
    size_t TotalFC;
    
    uint32_t DirectoryID;
    
    wchar_t* DirectoryPath;
    wchar_t* FullPathBuffer;
    uint64_t PathBufferSizes;
    
    file_explorer_file* Files;
    
    // used for fast searching.
    uint32_t *ParentIDs;
    uint64_t FileCount;
    
    file_explorer_memory Memory;
    char VolumeLetter;
    
    uint64_t SearchNeedle;
    uint32_t SearchID;
};


struct attribute_list_entry{
    uint32_t EntryType;
    uint32_t EntryFileID;
};

// Each entry is 8 byte, its ok to store them in stack.
struct attribute_list_contents{
    attribute_list_entry Entries[16];
};



inline BOOLEAN
NarSetFilePointer(HANDLE File, ULONGLONG V);


void*
NarGetBitmapAttributeData(void *BitmapAttributeStart);

int32_t
NarGetBitmapAttributeDataLen(void *BitmapAttributeStart);

inline bool
NarParseIndexAllocationAttribute(void *IndexAttribute, nar_record *OutRegions, uint32_t MaxRegionLen, uint32_t *OutRegionsFound, bool BitmapCompatibleInsert = false);


bool
NarParseDataRun(void* DatarunStart, nar_record *OutRegions, uint32_t MaxRegionLen, uint32_t *OutRegionsFound, bool BitmapCompatibleInsert = false);


uint32_t
NarGetFileID(void* FileRecord);

inline INT32
NarGetVolumeClusterSize(char Letter);


HANDLE
NarOpenVolume(char Letter);



inline void*
NarFindFileAttributeFromFileRecord(void *FileRecord, INT32 AttributeID);



bool
NarGetMFTRegionsFromBootSector(HANDLE Volume, 
                               nar_record* Out, 
                               uint32_t* OutLen, 
                               uint32_t Capacity);



multiple_pid
NarGetFileNameAndParentID(void *FileRecord);

extension_finder_memory
NarSetupExtensionFinderMemory(HANDLE VolumeHandle);

void
NarFreeExtensionFinderMemory(extension_finder_memory *Memory);

extension_search_result
NarFindExtensions(char VolumeLetter, HANDLE VolumeHandle, wchar_t *Extension, extension_finder_memory *Memory);






file_explorer_memory
NarInitFileExplorerMemory(uint32_t TotalFC);

void
NarFreeFileExplorerMemory(file_explorer_memory *Memory);

file_explorer
NarInitFileExplorer(wchar_t *MetadataPath);


file_explorer_file*
FEStartParentSearch(file_explorer *FE, uint32_t ParentID);

file_explorer_file*
FENextParent(file_explorer *FE, file_explorer_file *CurrentFile);

attribute_list_contents
NarGetAttributeListContents(void* AttrListDataStart, uint64_t DataLen);


bool IsValidAttrEntry(attribute_list_entry Entry);