#pragma once

//#include "nar.h"

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

#define NAR_DATA_FLAG 0x80
#define NAR_INDEX_ALLOCATION_FLAG 0xA0
#define NAR_INDEX_ROOT_FLAG 0x90
#define NAR_BITMAP_FLAG     0xB0
#define NAR_ATTRIBUTE_LIST 0x20

#define NAR_FE_DIRECTORY_FLAG 0x01

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

struct name_and_parent_id{
    uint32_t FileID;
    uint32_t ParentFileID;
    uint8_t  NameLen; // not including whitespace
    wchar_t *Name;
};


struct extension_search_result {
    wchar_t **Files;
    uint64_t Len;
};


inline BOOLEAN
NarSetFilePointer(HANDLE File, ULONGLONG V);


inline void*
NarGetBitmapAttributeData(void *BitmapAttributeStart);

inline INT32
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

// input MUST be sorted
// Finds point Offset in relative to Records structure, useful when converting absolue volume offsets to our binary backup data offsets.
// returns NAR_POINT_OFFSET_FAILED if fails to find given offset, 
#define NAR_POINT_OFFSET_FAILED -1
inline INT64 
FindPointOffsetInRecords(nar_record *Records, uint32_t Len, uint64_t Offset);



bool
NarGetMFTRegionsFromBootSector(HANDLE Volume, 
                               nar_record* Out, 
                               uint32_t* OutLen, 
                               uint32_t Capacity);



name_and_parent_id
NarGetFileNameAndParentID(void *FileRecord);

extension_search_result
NarFindExtensions(char VolumeLetter, HANDLE VolumeHandle, wchar_t *Extension, nar_arena *Arena);
