#pragma once


#define NAR_POSIX                2
#define NAR_ENTRY_SIZE_OFFSET    8
#define NAR_TIME_OFFSET         24
#define NAR_SIZE_OFFSET         64
#define NAR_ATTRIBUTE_OFFSET    72
#define NAR_NAME_LEN_OFFSET     80 
#define NAR_POSIX_OFFSET        81
#define NAR_NAME_OFFSET         82

#define NAR_ROOT_MFT_ID          5

#define NAR_STANDART_FLAG         0x10
#define NAR_ATTRIBUTE_LIST        0x20
#define NAR_FILENAME_FLAG         0x30
#define NAR_DATA_FLAG             0x80
#define NAR_INDEX_ROOT_FLAG       0x90
#define NAR_INDEX_ALLOCATION_FLAG 0xA0
#define NAR_BITMAP_FLAG           0xB0


#define NAR_OFFSET(m, o) ((char*)(m) + (o))


#include <stdint.h>
#include <string> //wcscmp
#include <assert.h>
#include <Windows.h>
#include "performance.hpp"


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


struct nar_fs_region {
    uint32_t StartPos;
    uint32_t Len;
};



// small functions that can be macroed
static inline bool IsValidAttrEntry(attribute_list_entry Entry){
    return (Entry.EntryType != 0);
}

static inline int CompareWCharStrings(const void *v1, const void *v2){
    wchar_t *str1 = *(wchar_t**)v1;
    wchar_t *str2 = *(wchar_t**)v2;
    return wcscmp(str1, str2);
}


/*
CAUTION: This function does NOT lookup attributes from ATTRIBUTE LIST, so if attribute is not resident in original file entry, function wont return it

// NOTE(Batuhan): Function early terminates in attribute iteration loop if it finds attribute with higher ID than given AttributeID parameter

For given MFT FileEntry, returns address AttributeID in given FileRecord. Caller can use return value to directly access given AttributeID
Function returns NULL if attribute is not present
*/
static inline void* NarFindFileAttributeFromFileRecord(void* FileRecord, int32_t AttributeID) {

    if (NULL == FileRecord) return 0;

    TIMED_BLOCK();

    int16_t FirstAttributeOffset = (*(int16_t*)((uint8_t*)FileRecord + 20));
    void* FileAttribute = (char*)FileRecord + FirstAttributeOffset;

    int32_t RemainingLen = *(int32_t*)((uint8_t*)FileRecord + 24); // Real size of the file record
    RemainingLen -= (FirstAttributeOffset + 8); //8 byte for end of record mark, remaining len includes it too.

    while (RemainingLen > 0) {

        if (*(int32_t*)FileAttribute == AttributeID) {
            return FileAttribute;
        }

        // it's guarenteed that attributes are sorted by id's in ascending order
        if (*(int32_t*)FileAttribute > AttributeID) {
            break;
        }

        //Just substract attribute size from remaininglen to determine if we should keep iterating
        int32_t AttrSize = (*(uint16_t*)((uint8_t*)FileAttribute + 4));
        RemainingLen -= AttrSize;
        FileAttribute = (uint8_t*)FileAttribute + AttrSize;
        if (AttrSize == 0) break;

    }

    return NULL;
}

static inline uint64_t NarGetFileSizeFromRecord(void *R){
    if(R == 0) return 0;
    void *D = NarFindFileAttributeFromFileRecord(R, NAR_DATA_FLAG);
    if(D != NULL){
        return *(uint64_t*)NAR_OFFSET(D, 0x30);
    }
    return 0;
}


static inline void* NarNextAttribute(void *Attribute){
    uint32_t LenWithHeader = *(uint32_t*)NAR_OFFSET(Attribute, 4);
    void *Result = NAR_OFFSET(Attribute, LenWithHeader);
    uint32_t NextID = *(uint32_t*)Result;
    if(NextID == 0xFFFFFFFF){
        return 0;
    }
    return Result;
}

static inline uint32_t NarGetFileID(void* FileRecord){
    uint32_t Result = *(uint32_t *)NAR_OFFSET(FileRecord, 44);
    return Result;
}

static inline uint32_t NarGetFileBaseID(void* FileRecord) {
    uint32_t Result = *(uint32_t*)NAR_OFFSET(FileRecord, 32);
    return Result;
}

static inline uint8_t NarIsFileLinked(void* FileRecord){
    uint32_t Result = NarGetFileBaseID(FileRecord);
    return !(Result == 0);
}







static inline BOOLEAN NarSetFilePointer(HANDLE File, uint64_t V) {
    LARGE_INTEGER MoveTo = { 0 };
    MoveTo.QuadPart = V;
    LARGE_INTEGER NewFilePointer = { 0 };
    SetFilePointerEx(File, MoveTo, &NewFilePointer, FILE_BEGIN);
    return MoveTo.QuadPart == NewFilePointer.QuadPart;
}





static inline void* NarGetBitmapAttributeData(void *BitmapAttributeStart){
    void *Result = 0;
    if(BitmapAttributeStart == NULL) return Result;
    
    uint16_t Aoffset =  *(uint16_t *)NAR_OFFSET(BitmapAttributeStart, 0x14);
    Result = NAR_OFFSET(BitmapAttributeStart, Aoffset);
    
    return Result;
}

static inline int32_t NarGetBitmapAttributeDataLen(void *BitmapAttributeStart){
    int32_t Result = 0;
    if(BitmapAttributeStart == NULL) return Result;
    
    Result = *(int32_t*)NAR_OFFSET(BitmapAttributeStart, 0x10);
    
    return Result;
}


/*
BitmapCompatibleInsert = inserts cluster one by one, so caller can easily zero-out unused ones
*/
static inline bool NarParseDataRun(void* DatarunStart, nar_fs_region *OutRegions, uint32_t MaxRegionLen, uint32_t *OutRegionsFound, bool BitmapCompatibleInsert){
    
    // So it looks like dataruns doesnt actually tells you LCN, to save up space, they kinda use smt like 
    // winapi's deviceiocontrol routine, maybe the reason for fetching VCN-LCN maps from winapi is weird because 
    // thats how its implemented at ntfs at first place. who knows
    // so thats how it looks
    /*
    first one is always absolute LCN in the volume. rest of it is addition to previous one. if file is fragmanted, value might be
    negative. so we dont have to check some edge cases here.
    second data run will be lets say 0x11 04 43
    and first one                    0x11 10 10
    starting lcn is 0x10, but second data run does not start from 0x43, it starts from 0x10 + 0x43
  
    LCN[n] = LCN[n-1] + datarun cluster
    */
    bool Result = true;
    uint32_t InternalRegionsFound = 0;
    void* D = DatarunStart;
    int64_t OldClusterStart = 0;
    
    while (*(uint8_t *)D) {
        
        uint8_t Size = *(uint8_t *)D;
        int32_t ClusterCountSize = 0;
        int32_t FirstClusterSize = 0;
        int32_t ClusterCount     = 0;
        int32_t FirstCluster     = 0;
        if (Size == 0) break;
        
        // extract 4bit nibbles from size
        ClusterCountSize = (Size & 0x0F);
        FirstClusterSize = (Size >> 4);
        
        
        ClusterCount = *(uint32_t*)((uint8_t*)D + 1);
        ClusterCount = ClusterCount & ~(0xffffffffu << (ClusterCountSize * 8));
        
        FirstCluster = 0;
        if(((char*)D + 1 + ClusterCountSize)[FirstClusterSize - 1] & 0x80){
            FirstCluster = -1;
        }
        memcpy(&FirstCluster, (char*)D + 1 + ClusterCountSize, FirstClusterSize);
        
        
        if (ClusterCountSize == 0 || FirstClusterSize == 0){
            printf("ERROR case : case zero len\n");
            break;
        }
        if (ClusterCountSize > 4  || FirstClusterSize > 4){
            printf("ERROR case : 1704  ccs 0x%X fcs 0x%X\n", ClusterCountSize, FirstClusterSize);
            break;
        }
        
        
        if(BitmapCompatibleInsert){
            if((InternalRegionsFound + ClusterCount) < MaxRegionLen){
                int64_t plcholder = (int64_t)FirstCluster + OldClusterStart;
                for(size_t i =0; i<(size_t)ClusterCount; i++){
                    // safe conversion
                    OutRegions[InternalRegionsFound].StartPos = (uint32_t)(plcholder + i);
                    OutRegions[InternalRegionsFound].Len      = 1;
                    InternalRegionsFound++;
                }
            }
            else{
                printf("parser not enough memory %d\n", __LINE__);
                goto NOT_ENOUGH_MEMORY;
            }
        }
        else{
            OutRegions[InternalRegionsFound].StartPos = (uint32_t)(OldClusterStart + (int64_t)FirstCluster);
            OutRegions[InternalRegionsFound].Len = ClusterCount;
            InternalRegionsFound++;
        }
        
        
        if(InternalRegionsFound > MaxRegionLen){
            printf("attribute parser not enough memory[Line : %u]\n", __LINE__);
            goto NOT_ENOUGH_MEMORY;
        }
        
        OldClusterStart = OldClusterStart + (int64_t)FirstCluster;
        D = (uint8_t*)D + (FirstClusterSize + ClusterCountSize + 1);
    }
    
    *OutRegionsFound = InternalRegionsFound;
    return Result;
    
    NOT_ENOUGH_MEMORY:;
    
    Result = FALSE;
    printf("No more memory left to insert index_allocation records to output array\n");
    *OutRegionsFound = 0;
    return Result;
    
}


/*
This functions inserts clusters one-by-one, so region with 4 cluster length will be added 4 times as if they were representing 4 different consequent regions. Increases memory usage, but excluding regions via bitmap becomes so easy i think it's worth
*/
static inline bool NarParseIndexAllocationAttribute(void *IndexAttribute, nar_fs_region *OutRegions, uint32_t MaxRegionLen, uint32_t *OutRegionsFound, bool BitmapCompatibleInsert){
    
    if(IndexAttribute == NULL 
       || OutRegions == NULL 
       || OutRegionsFound == NULL) 
        return FALSE;
    
    TIMED_NAMED_BLOCK("Index singular");
    
    int32_t DataRunsOffset = *(int32_t*)NAR_OFFSET(IndexAttribute, 32);
    void* D = NAR_OFFSET(IndexAttribute, DataRunsOffset);
    
    return NarParseDataRun(D, OutRegions, MaxRegionLen, OutRegionsFound, BitmapCompatibleInsert);
}


static inline bool NarGetMFTRegionsFromBootSector(HANDLE Volume, 
                               nar_fs_region* Out, 
                               uint32_t* OutLen, 
                               uint32_t Capacity){
    
    BOOLEAN Result = false;
    char bf[4096];
    
    if(NarSetFilePointer(Volume, 0)){
        DWORD br = 0;
        static_assert(sizeof(bf) == 4096);
        ReadFile(Volume, bf, 4096, &br, 0);
        if(br == sizeof(bf)){
            
            uint32_t MftClusterOffset = *(uint32_t*)NAR_OFFSET(bf, 48);
            uint64_t MFTVolOffset = MftClusterOffset*4096ull;
            if(NarSetFilePointer(Volume, MFTVolOffset)){
                static_assert(sizeof(bf) >= 1024);
                memset(bf, 0, 1024);
                ReadFile(Volume, bf, 1024, &br, 0);
                if(br == 1024){
                    
                    uint8_t *FileRecord = (uint8_t*)&bf[0];
                    // lsn, lsa swap to not confuse further parsing stages.
                    FileRecord[510] = *(uint8_t*)NAR_OFFSET(FileRecord, 50);
                    FileRecord[511] = *(uint8_t*)NAR_OFFSET(FileRecord, 51);
                    void* Indx = NarFindFileAttributeFromFileRecord(FileRecord, NAR_DATA_FLAG);
                    if(Indx != NULL){
                        NarParseIndexAllocationAttribute(Indx, Out, Capacity, OutLen, false);
                        Result = true;
                    }
                    
                }
                
            }
            
            
        }
    }
    
    return Result;
}



static inline multiple_pid NarGetFileNameAndParentID(void *FileRecord) {
    multiple_pid Result = {0};
    
    uint32_t FileID = NarGetFileID(FileRecord);
    
    // doesnt matter if it's posix or normal file name attribute
    void* FNAttribute = NarFindFileAttributeFromFileRecord(FileRecord, 0x30);
    
    if(FNAttribute){
        
        uint32_t AttributeID = *(uint32_t*)FNAttribute;
        
        while(AttributeID == 0x30){
            
            AttributeID = *(uint32_t*)FNAttribute;
            
            if(AttributeID == 0x30){
                
                // if DOS file name, skip.
                if(*(uint8_t*)NAR_OFFSET(FNAttribute, 89) != 2){
                    Result.PIDS[Result.Len].FileID       = FileID;
                    Result.PIDS[Result.Len].Name         = (wchar_t*)NAR_OFFSET(FNAttribute, 90);
                    Result.PIDS[Result.Len].NameLen      = *(uint8_t*)NAR_OFFSET(FNAttribute, 88);
                    Result.PIDS[Result.Len].ParentFileID = *(uint32_t*)NAR_OFFSET(FNAttribute, 24);
                    Result.Len++;
                    
                    assert(Result.Len < 16);
                }
                else{
                    //skip
                }
                
            }
            if(AttributeID > 0x30){
                break;
            }
            
            uint16_t AttrSize = *(uint16_t*)NAR_OFFSET(FNAttribute, 4);
            FNAttribute       = NAR_OFFSET(FNAttribute, AttrSize);
            
        }
        
    }
    else{
        
    }
    
    return Result;
}


/*
	AttrListDataStart MUST be first entry in the attribute list, not the 
	attribute start itself. NTFS sucks, you might not able to reach attribute
	data from normal file record, it might be stored somewhere else in the disk.
	It's better we isolate this layer so both codepaths can call this.
*/
static inline attribute_list_contents GetAttributeListContents(void* AttrListDataStart, uint64_t DataLen){
    
    attribute_list_contents Result = {};
    
    // skip first 24 bytes, header.
    uint64_t LenRemaining      = DataLen;
    uint8_t* CurrentAttrRecord = (uint8_t*)AttrListDataStart;
    uint64_t Indice            = 0;
    
    // first run to determine the memory needed.
    while(LenRemaining){
        uint16_t RecordLen      = *(uint16_t*)NAR_OFFSET(CurrentAttrRecord, 4);
        uint32_t Type           = *(uint32_t*)CurrentAttrRecord;
        uint32_t AttrListFileID = *(uint32_t*)NAR_OFFSET(CurrentAttrRecord, 16);
        
        Result.Entries[Indice].EntryType     = Type;
        Result.Entries[Indice].EntryFileID   = AttrListFileID;
        Indice++;
        
        assert(Indice < sizeof(Result.Entries)/sizeof(Result.Entries[0]));
        
        CurrentAttrRecord += RecordLen;
        LenRemaining      -= RecordLen;
    }
    
    return Result;
}


static inline int32_t NarGetVolumeClusterSize(char Letter){
    
    char V[] = "!:\\";
    V[0] = Letter;
    
    int32_t Result = 0;
    DWORD SectorsPerCluster = 0;
    DWORD BytesPerSector = 0;
    
    if (GetDiskFreeSpaceA(V, &SectorsPerCluster, &BytesPerSector, 0, 0)){
        Result = SectorsPerCluster * BytesPerSector;
    }
    else{
        printf("Couldnt get disk free space for volume %c\n", Letter);
    }
    
    return Result;
}


static inline HANDLE NarOpenVolume(char Letter) {
    char VolumePath[64];
    snprintf(VolumePath, 64, "\\\\.\\%c:", Letter);
    
    HANDLE Volume = CreateFileA(VolumePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
    if (Volume != INVALID_HANDLE_VALUE) {
        
        
#if 0        
        if (DeviceIoControl(Volume, FSCTL_LOCK_VOLUME, 0, 0, 0, 0, 0, 0)) {
            
        }
        else {
            // NOTE(Batuhan): this isnt an error, tho prohibiting volume access for other processes would be great.
            printf("Couldn't lock volume %c\n", Letter);
        }
        
        
        if (DeviceIoControl(Volume, FSCTL_DISMOUNT_VOLUME, 0, 0, 0, 0, 0, 0)) {
            
        }
        else {
            // printf("Couldnt dismount volume\n");
        }
        
#endif
        
        
    }
    else {
        printf("Couldn't open volume %c\n", Letter);
    }
    
    return Volume;
}



