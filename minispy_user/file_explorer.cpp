#include "precompiled.h"
#include "file_explorer.h"


#define TIMED_BLOCK__(NAME, Number, ...) 
#define TIMED_BLOCK_(NAME, Number, ...)  
#define TIMED_BLOCK(...)                 
#define TIMED_NAMED_BLOCK(NAME, ...)     

double AllocatorTimeElapsed = 0.0f;
uint64_t AllocationCount = 0;


#if 0
void*
ProfileAllocator(linear_allocator *a, size_t n){
    AllocationCount++;
    int64_t start = NarGetPerfCounter();
    void* Result = LinearAllocate(a, n);
    AllocatorTimeElapsed += NarTimeElapsed(start);
    return Result;
}

void*
ProfileAllocatorAligned(linear_allocator *a, size_t n, size_t al){
    AllocationCount++;
    int64_t start = NarGetPerfCounter();
    void *Result = LinearAllocateAligned(a, n, al);
    AllocatorTimeElapsed += NarTimeElapsed(start);
    return Result;
}

#define LinearAllocate(Allocator, Size) ProfileAllocator(Allocator, Size)
#define LinearAllocateAligned(Allocator, Size, Align) ProfileAllocatorAligned(Allocator, Size, Align)
#endif

inline BOOL
CompareWCharStrings(const void *v1, const void *v2){
    wchar_t *str1 = *(wchar_t**)v1;
    wchar_t *str2 = *(wchar_t**)v2;
    return wcscmp(str1, str2);
}

/**/
inline uint64_t
NarGetFileSizeFromRecord(void *R){
    if(R == 0) return 0;
    void *D = NarFindFileAttributeFromFileRecord(R, NAR_DATA_FLAG);
    if(D != NULL){
        return *(UINT64*)NAR_OFFSET(D, 0x30);
    }
    return 0;
}


inline BOOLEAN
NarSetFilePointer(HANDLE File, ULONGLONG V) {
    LARGE_INTEGER MoveTo = { 0 };
    MoveTo.QuadPart = V;
    LARGE_INTEGER NewFilePointer = { 0 };
    SetFilePointerEx(File, MoveTo, &NewFilePointer, FILE_BEGIN);
    return MoveTo.QuadPart == NewFilePointer.QuadPart;
}


inline INT32
NarGetVolumeClusterSize(char Letter){
    
    char V[] = "!:\\";
    V[0] = Letter;
    
    INT32 Result = 0;
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


HANDLE
NarOpenVolume(char Letter) {
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


/*
CAUTION: This function does NOT lookup attributes from ATTRIBUTE LIST, so if attribute is not resident in original file entry, function wont return it

// NOTE(Batuhan): Function early terminates in attribute iteration loop if it finds attribute with higher ID than given AttributeID parameter

For given MFT FileEntry, returns address AttributeID in given FileRecord. Caller can use return value to directly access given AttributeID 
Function returns NULL if attribute is not present 
*/
inline void*
NarFindFileAttributeFromFileRecord(void *FileRecord, INT32 AttributeID){
    
    if(NULL == FileRecord) return 0;
    
    TIMED_BLOCK();
    
    void *Start = FileRecord;
    
    INT16 FirstAttributeOffset = (*(int16_t*)((BYTE*)FileRecord + 20));
    void* FileAttribute = (char*)FileRecord + FirstAttributeOffset;
    
    INT32 RemainingLen = *(int32_t*)((BYTE*)FileRecord + 24); // Real size of the file record
    RemainingLen      -= (FirstAttributeOffset + 8); //8 byte for end of record mark, remaining len includes it too.
    
    while(RemainingLen > 0){
        
        if(*(int32_t*)FileAttribute == AttributeID){
            return FileAttribute;
        }
        
        // it's guarenteed that attributes are sorted by id's in ascending order
        if(*(int32_t*)FileAttribute > AttributeID){
            break;
        }
        
        //Just substract attribute size from remaininglen to determine if we should keep iterating
        int32_t AttrSize = (*(unsigned short*)((BYTE*)FileAttribute + 4));
        RemainingLen -= AttrSize;
        FileAttribute = (BYTE*)FileAttribute + AttrSize;
        if (AttrSize == 0) break;
        
    }
    
    return NULL;
}

void*
NarGetBitmapAttributeData(void *BitmapAttributeStart){
    void *Result = 0;
    if(BitmapAttributeStart == NULL) return Result;
    
    UINT16 Aoffset =  *(UINT16*)NAR_OFFSET(BitmapAttributeStart, 0x14);
    Result = NAR_OFFSET(BitmapAttributeStart, Aoffset);
    
    return Result;
}

int32_t
NarGetBitmapAttributeDataLen(void *BitmapAttributeStart){
    INT32 Result = 0;
    if(BitmapAttributeStart == NULL) return Result;
    
    Result = *(INT32*)NAR_OFFSET(BitmapAttributeStart, 0x10);
    
    return Result;
}




/*
BitmapCompatibleInsert = inserts cluster one by one, so caller can easily zero-out unused ones
*/
bool
NarParseDataRun(void* DatarunStart, nar_record *OutRegions, uint32_t MaxRegionLen, uint32_t *OutRegionsFound, bool BitmapCompatibleInsert){
    
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
    
    while (*(BYTE*)D) {
        
        BYTE Size = *(BYTE*)D;
        int32_t ClusterCountSize = 0;
        int32_t FirstClusterSize = 0;
        int32_t ClusterCount     = 0;
        int32_t FirstCluster     = 0;
        if (Size == 0) break;
        
        // extract 4bit nibbles from size
        ClusterCountSize = (Size & 0x0F);
        FirstClusterSize = (Size >> 4);
        
        
        ClusterCount = *(uint32_t*)((BYTE*)D + 1);
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
        D = (BYTE*)D + (FirstClusterSize + ClusterCountSize + 1);
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
bool
NarParseIndexAllocationAttribute(void *IndexAttribute, nar_record *OutRegions, uint32_t MaxRegionLen, uint32_t *OutRegionsFound, bool BitmapCompatibleInsert){
    
    if(IndexAttribute == NULL 
       || OutRegions == NULL 
       || OutRegionsFound == NULL) 
        return FALSE;
    
    TIMED_NAMED_BLOCK("Index singular");
    
    int32_t DataRunsOffset = *(INT32*)NAR_OFFSET(IndexAttribute, 32);
    void* D = NAR_OFFSET(IndexAttribute, DataRunsOffset);
    
    return NarParseDataRun(D, OutRegions, MaxRegionLen, OutRegionsFound, BitmapCompatibleInsert);
}







bool
NarGetMFTRegionsFromBootSector(HANDLE Volume, 
                               nar_record* Out, 
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

void
NarFreeExtensionFinderMemory(extension_finder_memory *Memory){
    VirtualFree(Memory->Arena.Memory, 0, MEM_RELEASE);
    NarFreeLinearAllocator(&Memory->StringAllocator);
    memset(Memory, 0, sizeof(*Memory));
}

extension_finder_memory
NarSetupExtensionFinderMemory(HANDLE VolumeHandle){
    extension_finder_memory Result = {0};
    
    uint32_t MFTRecordsCapacity = 256;
    uint64_t TotalFC            = 0;
    nar_record* MFTRecords      = (nar_record*)calloc(1024, sizeof(nar_record));
    
    uint64_t FileBufferSize        = Megabyte(128);
    uint64_t ArenaSize             = 0;
    uint64_t StringAllocatorSize   = 0;
    
    
    void* ArenaMemory = 0;
    
    if(MFTRecords){
        uint32_t MFTRecordsCount = 0;
        bool tres = NarGetMFTRegionsFromBootSector(VolumeHandle, 
                                                   MFTRecords, 
                                                   &MFTRecordsCount, 
                                                   MFTRecordsCapacity);
        
        if(tres){
            for(unsigned int MFTOffsetIndex = 0; MFTOffsetIndex < MFTRecordsCount; MFTOffsetIndex++){
                TotalFC += MFTRecords[MFTOffsetIndex].Len;
            }
            TotalFC*=4;
        }
        else{
            
        }
        
        ArenaSize  = TotalFC*2*sizeof(name_pid) + FileBufferSize + Megabyte(50);
        
        // At worst, all of the files might be extension we might looking for,
        // so we better allocate 1gig extra for them + stack
        StringAllocatorSize = 260*sizeof(wchar_t)*TotalFC + Megabyte(500);
        
        ArenaMemory = VirtualAlloc(0, ArenaSize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
        if(NULL ==ArenaMemory){
            goto bail;
        }
        Result.Arena = ArenaInit(ArenaMemory, ArenaSize);
        
        Result.StringAllocator = NarCreateLinearAllocator(StringAllocatorSize, Megabyte(50));
        if(Result.StringAllocator.Memory == NULL){
            goto bail;
        }
        
        Result.TotalFC  = TotalFC;
        
        Result.FileBuffer     = ArenaAllocate(&Result.Arena, FileBufferSize);
        Result.FileBufferSize = FileBufferSize;
        
        Result.MFTRecords     = (nar_record*)ArenaAllocate(&Result.Arena, MFTRecordsCount*sizeof(nar_record));
        Result.MFTRecordCount = MFTRecordsCount;
        memcpy(Result.MFTRecords, MFTRecords, sizeof(nar_record)*MFTRecordsCount);
        
        Result.DirMappingMemory = ArenaAllocate(&Result.Arena, TotalFC*sizeof(name_pid));
        Result.PIDArrMemory = ArenaAllocate(&Result.Arena, TotalFC*sizeof(name_pid));
        
        ASSERT(Result.PIDArrMemory);
        ASSERT(Result.DirMappingMemory);
        ASSERT(Result.MFTRecords);
        ASSERT(Result.FileBuffer);
        
    }
    
    
    if(MFTRecords != 0){
        free(MFTRecords);
    }
    
    return Result;
    
    bail:
    if(0 != MFTRecords){
        free(MFTRecords);
    }
    if(0 != ArenaMemory){
        VirtualFree(ArenaMemory, 0, MEM_RELEASE);
    }
    if(0 != Result.StringAllocator.Memory){
        NarFreeLinearAllocator(&Result.StringAllocator);
    }
    
    return {0};
}

extension_search_result
NarFindExtensions(char VolumeLetter, HANDLE VolumeHandle, wchar_t *Extension, extension_finder_memory *Memory) {
    
    extension_search_result Result = {0};
    
    
    name_pid *DirectoryMapping  = (name_pid*)Memory->DirMappingMemory; 
    name_pid *PIDResultArr      = (name_pid*)Memory->PIDArrMemory; 
    uint64_t ArrLen     	    = 0;
    uint64_t FileRecordSize     = 1024; 
    size_t ExtensionLen = wcslen(Extension);
    double ParserTotalTime    = 0;
    double TraverserTotalTime = 0;
    int64_t ParserLoopCount    = 0;
    
    DWORD BR = 0;
    
    uint64_t ClusterSize    = NarGetVolumeClusterSize(VolumeLetter);
    
    uint32_t BufferStartFileID = 0;
    uint64_t VCNOffset = 0;
    
    uint64_t DEBUG_I = 0;
    
    
    for (uint64_t MFTOffsetIndex = 0; 
         MFTOffsetIndex < Memory->MFTRecordCount; 
         MFTOffsetIndex++) 
    {
        
        
        uint64_t FilePerCluster = ClusterSize / 1024ull;
        uint64_t Offset = (uint64_t)Memory->MFTRecords[MFTOffsetIndex].StartPos * (uint64_t)ClusterSize;
        
        // set file pointer to actual records
        if (NarSetFilePointer(VolumeHandle, Offset)) {
            
            uint64_t FileRemaining   = (uint64_t)Memory->MFTRecords[MFTOffsetIndex].Len * (uint64_t)FilePerCluster;
            while(FileRemaining){
                
                uint64_t FBCount       = Memory->FileBufferSize/1024ull;
                size_t TargetFileCount = MIN(FileRemaining, FBCount);
                FileRemaining         -= TargetFileCount;
                
                ReadFile(VolumeHandle, 
                         Memory->FileBuffer, 
                         TargetFileCount*1024ull, 
                         &BR, 0);
                ASSERT(BR == TargetFileCount*1024);
                
                if(BR == TargetFileCount*1024ull){
                    
                    int64_t start = NarGetPerfCounter();
                    ParserLoopCount += TargetFileCount;
                    for (uint64_t FileRecordIndex = 0; FileRecordIndex < TargetFileCount; FileRecordIndex++) {
                        
                        void* FileRecord = (uint8_t*)Memory->FileBuffer + (uint64_t)FileRecordSize * (uint64_t)FileRecordIndex;
                        
                        // file flags are at 22th offset in the record
                        if (*(int32_t*)FileRecord != 'ELIF') {
                            // block doesnt start with 'FILE0', skip
                            continue;
                        }
                        
                        
                        // lsn, lsa swap to not confuse further parsing stages.
                        ((uint8_t*)FileRecord)[510] = *(uint8_t*)NAR_OFFSET(FileRecord, 50);
                        ((uint8_t*)FileRecord)[511] = *(uint8_t*)NAR_OFFSET(FileRecord, 51);
                        
                        FileRecordHeader *r = (FileRecordHeader*)FileRecord;
                        if(r->inUse == 0){
                            continue;
                        }
                        
                        
                        uint32_t FileID      = NarGetFileID(FileRecord);;
                        uint32_t IsDirectory = r->isDirectory;
                        uint32_t ParentID = 0;
                        
                        
                        multiple_pid MultPIDs = NarGetFileNameAndParentID(FileRecord);
                        
                        void *AttributeList = NarFindFileAttributeFromFileRecord(FileRecord, NAR_ATTRIBUTE_LIST);
                        
                        
                        for(size_t _pidi = 0; _pidi < MultPIDs.Len; _pidi++){
                            name_pid NamePID = MultPIDs.PIDS[_pidi];
                            
                            if(NamePID.Name != 0){
                                
                                // 
                                if(IsDirectory){
                                    DirectoryMapping[FileID] = NamePID;
                                    uint32_t NameSize = (NamePID.NameLen + 1) * 2;
                                    DirectoryMapping[FileID].Name = (wchar_t*)LinearAllocate(&Memory->StringAllocator, NameSize);
                                    memcpy(DirectoryMapping[FileID].Name, NamePID.Name, NameSize - 2);
                                    DirectoryMapping[FileID].NameLen = NamePID.NameLen + 1;
                                }
                                else{
                                    /*
    If it's not a directory, we don't need to save it for possible
    future references, since we know no file has ever referenced to another file, but they rather reference their roots to  directories.
    So we can skip at this phase, and check it at extension comparision stage. If it matches, then we add.
                                   */
                                }
                                
                                
                                uint64_t NameSize = (NamePID.NameLen + 1)* 2;
                                
                                
                                // NOTE(Batuhan): if _pidi > 0, we know that file name matches, if it wouldn't be we wouldn't even see _pidi =1, check below for break condition. 
                                if(ExtensionLen <= NamePID.NameLen){
                                    
                                    wchar_t *LastChars = &NamePID.Name[NamePID.NameLen - ExtensionLen];
                                    bool Add = true;
                                    for(uint64_t wi = 0; wi < ExtensionLen; wi++){
                                        if(towlower(LastChars[wi]) != towlower(Extension[wi])){
                                            Add = false;
                                            break;
                                        }
                                    }
                                    
                                    if(Add){
                                        PIDResultArr[ArrLen] = NamePID;
                                        uint64_t NameSize = (NamePID.NameLen + 1) * 2;
                                        PIDResultArr[ArrLen].Name = (wchar_t*)LinearAllocate(&Memory->StringAllocator, NameSize);
                                        memcpy(PIDResultArr[ArrLen].Name, NamePID.Name, NameSize - 2);
                                        PIDResultArr[ArrLen].NameLen = NamePID.NameLen + 1;
                                        ArrLen++;
                                    }
                                    
                                    // NOTE(Batuhan): bail out if file name doesn't match, its same entry all the way down.
                                    else{
                                        break;
                                    }
                                    
                                }
                                
                            }
                            else{
                                
                            }
                            
                        }
                        
#if 0                        
                        if(AttributeList != NULL){
                            
                            uint32_t DiffFileIDCount = 0;
                            
                            // skip first 24 bytes, header.
                            uint32_t AttrListLen       = *(uint32_t*)NAR_OFFSET(AttributeList, 16);
                            uint32_t LenRemaining      = AttrListLen;
                            uint8_t* CurrentAttrRecord = (uint8_t*)NAR_OFFSET(AttributeList, 24);
                            
                            for(uint64_t i =0; i<16; i++){
                                attribute_list_entry Entry = Contents.Entries[i];
                                if(false == IsValidAttrEntry(Entry)){
                                    break;
                                }
                                if(Entry.EntryType == NAR_FILENAME_FLAG){
                                    if(Entry.EntryFileID != FileID){
                                        DiffFileIDCount++;
                                    }
                                }
                                if(DiffFileIDCount > 1){
                                    //NAR_BREAK;
                                }
                                
                            }
                        }
#endif
                        
                        if(MultPIDs.Len == 0 
                           && NULL != AttributeList
                           && !!IsDirectory){
                            // resolve the attribute list and
                            // redirect this entry.
                            
                            // skip first 24 bytes, header.
                            uint32_t AttrListLen       = *(uint32_t*)NAR_OFFSET(AttributeList, 16);
                            uint32_t LenRemaining      = AttrListLen;
                            uint8_t* CurrentAttrRecord = (uint8_t*)NAR_OFFSET(AttributeList, 24);
                            
                            while(LenRemaining){
                                uint16_t RecordLen = *(uint16_t*)NAR_OFFSET(CurrentAttrRecord, 4);
                                uint32_t Type      = *(uint32_t*)CurrentAttrRecord;
                                
                                
                                if(Type == NAR_FILENAME_FLAG){
                                    // found the filename attribute, but it might be referencing itself, if so scan for other filename entries.
                                    uint32_t AttrListFileID = *(uint32_t*)NAR_OFFSET(CurrentAttrRecord, 16);
                                    if(AttrListFileID != FileID){
                                        
                                        // found it, rereference the map to this id.
                                        ASSERT(!!IsDirectory);
                                        
                                        DirectoryMapping[FileID] = {0};
                                        DirectoryMapping[FileID].ParentFileID = AttrListFileID;
                                        break;
                                    }
                                    else{
                                        
                                    }
                                    
                                }
                                
                                CurrentAttrRecord += RecordLen;
                                LenRemaining      -= RecordLen;
                            }
                            
                        }
                        
                        
                    }
                    
                    ParserTotalTime += NarTimeElapsed(start);
                    
                }
                else{
                    //failed!
                }
                
                
            }
            
            
        }
    }
    
    
    int64_t TraverserStart = NarGetPerfCounter();
    
    uint64_t SkippedFileCount = 0;
    wchar_t** FilenamesExtended = (wchar_t**)LinearAllocateAligned(&Memory->StringAllocator, ArrLen*8, 8);
    uint32_t *stack = (uint32_t*)ArenaAllocateAligned(&Memory->Arena, 1024*4, 4);
    uint32_t DuplicateStart = 0;
    
    for(uint64_t s = 0; s<ArrLen; s++){
        
        uint32_t si = 0;
        uint16_t TotalFileNameSize = 0;
        memset(&stack[0], 0, sizeof(stack));
        
        name_pid* TargetPID = &PIDResultArr[s];
        
        
        for(uint32_t ParentID = TargetPID->ParentFileID;
            ParentID != 5;
            ParentID = DirectoryMapping[ParentID].ParentFileID)
        {
            if(DirectoryMapping[ParentID].Name == 0){
                ASSERT(DirectoryMapping[ParentID].ParentFileID != 0);
                continue;
            }
            stack[si++]    = ParentID;
            TotalFileNameSize += DirectoryMapping[ParentID].NameLen;
            ASSERT(DirectoryMapping[ParentID].FileID != 0);
            _mm_prefetch((const char*)&DirectoryMapping[ParentID], _MM_HINT_T0);
        }
        
        // additional memory for trailing backlashes
        FilenamesExtended[s] = (wchar_t*)LinearAllocate(&Memory->StringAllocator, TotalFileNameSize*2 + 200);
        uint64_t WriteIndex = 0;
        
        {
            wchar_t tmp[] = L"C:\\";
            tmp[0] = (wchar_t)VolumeLetter;
            memcpy(&FilenamesExtended[s][WriteIndex], &tmp[0], 6);
            WriteIndex += 3;        
        }
        
        
        for(int64_t i = si - 1; i>=0; i--){
            if(stack[i] != 5 
               && stack[i] != 0)
            {
                uint32_t FLen = DirectoryMapping[stack[i]].NameLen - 1;
                memcpy(&FilenamesExtended[s][WriteIndex], 
                       DirectoryMapping[stack[i]].Name, 
                       FLen*2);
                WriteIndex += FLen;
                
                memcpy(&FilenamesExtended[s][WriteIndex], L"\\", 2);
                WriteIndex += 1;
            }
        }
        
        uint64_t FSize = TargetPID->NameLen;
        memcpy(&FilenamesExtended[s][WriteIndex], TargetPID->Name, FSize*2);
        WriteIndex += FSize;
        FilenamesExtended[s][WriteIndex] = 0;        
        
    }
    
    TraverserTotalTime = NarTimeElapsed(TraverserStart);
    
    //qsort(FilenamesExtended, ArrLen, 8, CompareWCharStrings);
    
#if 0
    for(size_t i =0; i<ArrLen; i++){
        //printf("%S\n", FilenamesExtended[i]);
    }
#endif
    
    printf("Parser, %9u files in %.5f sec, file per ms %.5f\n", Memory->TotalFC, ParserTotalTime, (double)Memory->TotalFC/ParserTotalTime/1000.0);
    printf("Traverser %8u files in %.5f sec, file per ms %.5f\n", ArrLen, TraverserTotalTime, (double)ArrLen/TraverserTotalTime/1000.0);
    //printf("Allocating count %8u in  %.5f sec, allocation per ms %.5f\n", AllocationCount, AllocatorTimeElapsed, (double)AllocationCount/AllocatorTimeElapsed/(1000.0));
    
    printf("Match count %I64u\n", ArrLen);
    
    
    Result.Files = FilenamesExtended;
    Result.Len   = ArrLen;
    
    return Result;
}


uint32_t
NarGetFileID(void* FileRecord){
    uint32_t Result = *(uint32_t*)NAR_OFFSET(FileRecord, 44);
    return Result;
}


multiple_pid
NarGetFileNameAndParentID(void *FileRecord){
    multiple_pid Result = {0};
    
    uint32_t FileID = NarGetFileID(FileRecord);
    
    // doesnt matter if it's posix or normal file name attribute
    void* FNAttribute = NarFindFileAttributeFromFileRecord(FileRecord, 0x30);
    uint32_t RemainingLen = 0;
    
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
                    
                    ASSERT(Result.Len < 16);
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
    
    bail:;
    
    return Result;
}


file_explorer_memory
NarInitFileExplorerMemory(uint32_t TotalFC){
    file_explorer_memory Result = {0};
    
    uint64_t StringAllocatorSize     = TotalFC*520 + Megabyte(500);
    linear_allocator StringAllocator = NarCreateLinearAllocator(StringAllocatorSize, Megabyte(16));
    
    if(NULL != StringAllocator.Memory){
        size_t ArenaSize   = TotalFC*sizeof(file_explorer_file) + Megabyte(50);
        void* ArenaMemory = VirtualAlloc(0, ArenaSize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
        
        if(NULL != ArenaMemory){
            Result.Arena = ArenaInit(ArenaMemory, ArenaSize);
        }
        else{
            goto bail;
        }
        
    }
    else{
        goto bail;
    }
    return Result;
    
    bail:
    NarFreeFileExplorerMemory(&Result);
    return {0};
}

void
NarFreeFileExplorerMemory(file_explorer_memory *Memory){
    if(Memory){
        if(Memory->StringAllocator.Memory){
            NarFreeLinearAllocator(&Memory->StringAllocator);
        }
        if(Memory->Arena.Memory){
            VirtualFree(Memory->Arena.Memory, 0, MEM_RELEASE);
        }
        memset(Memory, 0, sizeof(*Memory));
    }
}

file_explorer
NarInitFileExplorer(wchar_t *MetadataPath){
    
    file_explorer Result = {0};
    Result.MetadataView = NarOpenFileView(MetadataPath);
    if(NULL != Result.MetadataView.Data){
        backup_metadata* BM = (backup_metadata*)Result.MetadataView.Data;
        
        Result.MFT     = (uint8_t*)Result.MetadataView.Data + BM->Offset.MFT;
        Result.MFTSize = BM->Size.MFT;
        Result.TotalFC = Result.MFTSize / (1024);
        
        Result.Memory  = NarInitFileExplorerMemory(Result.TotalFC);
        
        Result.DirectoryID   = 5;
        Result.DirectoryPath = (wchar_t*)ArenaAllocate(&Result.Memory.Arena, Kilobyte(32));
        Result.ParentIDs     = (uint32_t*)ArenaAllocate(&Result.Memory.Arena, Result.TotalFC*sizeof(uint32_t)*2);
        
        ASSERT(Result.ParentIDs);
        ASSERT(Result.DirectoryPath);
        
        for(uint64_t i =0; i<Result.TotalFC; i++){
            uint8_t *FileRecord = (uint8_t*)Result.MFT + 1024*i;
            if(*(uint32_t*)FileRecord != 'ELIF'){
                continue;
            }
            
            // ntfs madness, swap lsn and lsa
            ((uint8_t*)FileRecord)[510] = *(uint8_t*)NAR_OFFSET(FileRecord, 50);
            ((uint8_t*)FileRecord)[511] = *(uint8_t*)NAR_OFFSET(FileRecord, 51);
            
            FileRecordHeader *Header = (FileRecordHeader*)FileRecord;
            
            if(Header->inUse == false){
                continue;
            }
            multiple_pid MultPIDs = NarGetFileNameAndParentID(FileRecord);
            uint32_t FileID       = NarGetFileID(FileRecord);
            
            // Extract file creation and modification time.
            SYSTEMTIME WinFileCreated  = {0};
            SYSTEMTIME WinFileModified = {0};
            void *StdAttr = NarFindFileAttributeFromFileRecord(FileRecord, NAR_STANDART_FLAG);
            if(NULL != StdAttr){
                NAR_BREAK;
                void* AttrData = NAR_OFFSET(StdAttr, 24);
                
                uint64_t FileCreated  = *(uint64_t*)NAR_OFFSET(AttrData, 0);
                uint64_t FileModified = *(uint64_t*)NAR_OFFSET(AttrData, 8);
                
                if(0 == FileTimeToSystemTime((const FILETIME*)(void*)&FileCreated, &WinFileCreated)){
                    memset(&WinFileCreated, 0, sizeof(SYSTEMTIME));
                }
                if(0 == FileTimeToSystemTime((const FILETIME*)(void*)&FileModified, &WinFileModified)){
                    memset(&WinFileModified, 0, sizeof(SYSTEMTIME));
                }
                
            }// if stdattr
            
            //Extract file size if not directory
            uint64_t FileSize = 0;
            if(!Header->isDirectory){
                void* DataAttr = NarFindFileAttributeFromFileRecord(FileRecord, NAR_DATA_FLAG);
                if(NULL != DataAttr){
                    FileSize = *(uint64_t*)NAR_OFFSET(DataAttr, 48);
                }
            }
            
            for(size_t _pidi = 0; 
                _pidi < MultPIDs.Len; 
                _pidi++)
            {
                name_pid NamePID = MultPIDs.PIDS[_pidi];
                if(NamePID.Name == NULL){
                    continue;
                }
                
                file_explorer_file *File = &Result.Files[Result.FileCount];
                
                uint64_t NameSize = (NamePID.NameLen+1)*2; 
                File->Name = (wchar_t*)LinearAllocate(&Result.Memory.StringAllocator, NameSize);
                
                // +1 for null termination
                memcpy(File->Name, NamePID.Name, FileSize - 2);
                File->NameLen      = NamePID.NameLen + 1;
                File->Size         = FileSize;
                File->FileID       = FileID;
                File->ParentFileID = NamePID.ParentFileID;
                File->IsDirectory  = Header->isDirectory;
                
                
                memcpy(&File->CreationTime, &WinFileCreated, sizeof(SYSTEMTIME));
                memcpy(&File->LastModifiedTime, &WinFileModified, sizeof(SYSTEMTIME));
                
                Result.ParentIDs[Result.FileCount] = NamePID.ParentFileID;
                Result.FileCount++;
            }
            
            void* AttributeList = NarFindFileAttributeFromFileRecord(FileRecord, NAR_ATTRIBUTE_LIST);
            if(MultPIDs.Len == 0 && 
               NULL != AttributeList){
                // resolve the attribute list and
                // redirect this entry.
                
                // skip first 24 bytes, header.
                uint32_t AttrListLen       = *(uint32_t*)NAR_OFFSET(AttributeList, 16);
                uint32_t LenRemaining      = AttrListLen;
                uint8_t* CurrentAttrRecord = (uint8_t*)NAR_OFFSET(AttributeList, 24);
                
                file_explorer_file *File = &Result.Files[Result.FileCount];
                memset(File, 0, sizeof(*File));
                Result.FileCount++;
                
                while(LenRemaining){
                    uint16_t RecordLen = *(uint16_t*)NAR_OFFSET(CurrentAttrRecord, 4);
                    uint32_t Type      = *(uint32_t*)CurrentAttrRecord;
                    
                    
                    if(Type == NAR_FILENAME_FLAG){
                        // found the filename attribute, but it might be referencing itself, if so scan for other filename entries.
                        uint32_t AttrListFileID = *(uint32_t*)NAR_OFFSET(CurrentAttrRecord, 16);
                        if(AttrListFileID != FileID){
                            
                            NAR_BREAK;
                            
                            // found it, rereference the map to this id.
                            ASSERT(!!Header->isDirectory);
                            
                            File->IsDirectory  = Header->isDirectory;
                            File->Size         = FileSize;
                            File->FileID       = FileID;
                            File->ParentFileID = AttrListFileID;
                            File->IsDirectory  = Header->isDirectory;
                            
                            break;
                        }
                        else{
                            
                        }
                        
                    }
                    
                    // If we haven't extracted file size from entry, try to find it
                    // from attribute list. 
                    if(FileSize == 0 && 
                       Type == NAR_DATA_FLAG){
                        NAR_BREAK;
                        // TODO(Batuhan): 
                    }
                    
                    CurrentAttrRecord += RecordLen;
                    LenRemaining      -= RecordLen;
                }
                
            }
            
        }
        
        
    }
    else{
        return {0};
    }
    
    return Result;
}


void
NarFreeFileExplorer(file_explorer* FileExplorer){
    if(FileExplorer){
        NarFreeFileView(FileExplorer->MetadataView);
        NarFreeFileExplorerMemory(&FileExplorer->Memory);
    }
}



// BELOW MUST GO TO FILE EXPLORER.H
file_explorer_file*
FEStartParentSearch(file_explorer *FE, uint32_t ParentID){
    for(uint64_t i = 0; i<FE->FileCount; i++){
        if(FE->ParentIDs[i] == ParentID){
            return &FE->Files[i];
        }
    }
    return 0;
}

file_explorer_file*
FENextFileInDir(file_explorer *FE, file_explorer_file *CurrentFile){
    uint64_t StartIndice = (CurrentFile - &FE->Files[0])/sizeof(file_explorer_file);
    uint64_t ParentID    = CurrentFile->ParentFileID;
    for(uint64_t i = StartIndice; i<FE->FileCount; i++){
        if(FE->ParentIDs[i] == ParentID){
            return &FE->Files[i];
        }
    }
    return 0;
}

wchar_t*
FEGetFileFullPath(file_explorer* FE, file_explorer_file* File){
    return L"NOT IMPLEMENTED";
}


bool IsValidAttrEntry(attribute_list_entry Entry){
    return (Entry.EntryType != 0);
}


/*
Return value might be either fall in file record or in backup data. If last one,
it will be duplicate of the binary data since we don't know if it's compressed or not, we better extract it from backup and copy it to new buffer.
*/
void*
GetAttributeListData(file_explorer* FE, void* AttributeStart){
    ASSERT(FALSE);
    NAR_BREAK;
    return 0;
}


/*
AttrListDataStart MUST be first entry in the attribute list, not the 
attribute start itself. NTFS sucks, you might not able to reach attribute
data from normal file record, it might be stored somewhere else in the disk.
It's better we isolate this layer so both codepaths can call this.
*/
attribute_list_contents
GetAttributeListContents(void* AttrListDataStart, uint64_t DataLen){
    
    attribute_list_contents Result = {0};
    // skip first 24 bytes, header.
    uint32_t LenRemaining      = DataLen;
    uint8_t* CurrentAttrRecord = (uint8_t*)AttrListDataStart;
    
    
    uint32_t NameAttributeFileID = 0;
    uint32_t DataAttributeFileID = 0;
    
    uint64_t Indice = 0;
    
    while(LenRemaining){
        uint16_t RecordLen      = *(uint16_t*)NAR_OFFSET(CurrentAttrRecord, 4);
        uint32_t Type           = *(uint32_t*)CurrentAttrRecord;
        uint32_t AttrListFileID = *(uint32_t*)NAR_OFFSET(CurrentAttrRecord, 16);
        
        Result.Entries[Indice].EntryType     = Type;
        Result.Entries[Indice].EntryFileID   = AttrListFileID;
        Indice++;
        
        ASSERT(Indice < sizeof(Result.Entries)/sizeof(Result.Entries[0]));
        
        CurrentAttrRecord += RecordLen;
        LenRemaining      -= RecordLen;
    }
    
    return Result;
}


/*
CAUTION : Assumes we can insert as much as we can into Files array.

Args:
MFTStart, start of MFT, assumes it's sequential all they way in the memory.
BaseFileRecord : Record that attribute array belongs to.
Files: Array to append found file(s). Since there might be hard links we may
append more than one file to array.

Return: Returns how many files appended to Files. 0 is unexpected.
*/
uint64_t
SolveAttributeListReferences(const void* MFTStart,
                             void* BaseFileRecord,
                             attribute_list_contents Contents, file_explorer_file* Files,
                             linear_allocator* StringAllocator
                             ){
    
    // Those common fields applies to all hard links if any exist.
    // at the end of the function those are applied to each file.
    SYSTEMTIME WinFileCreated  = {0};
    SYSTEMTIME WinFileModified = {0};
    uint64_t   FileSize = 0;
    
    FileRecordHeader *Header = (FileRecordHeader*)BaseFileRecord;
    
    uint32_t BaseFileID = NarGetFileID(BaseFileRecord);
    uint64_t FileCount = 0;
    
    for(uint64_t i =0; i < 16; i++){
        attribute_list_entry Entry = Contents.Entries[i];
        if(false == IsValidAttrEntry(Entry)){
            break;
        }
        
        void *EntryFileRecord = (uint8_t*)MFTStart + Entry.EntryFileID * 1024;
        void *Attr            = NarFindFileAttributeFromFileRecord(EntryFileRecord, Entry.EntryType);
        ASSERT(Attr != NULL);
        
        switch(Entry.EntryType){
            
            case NAR_FILENAME_FLAG:{
                //DOS FILE NAME
                if(*(uint8_t*)NAR_OFFSET(Attr, 89) != 2){
                    uint32_t NameLen  = *(uint8_t*)NAR_OFFSET(Attr, 88);
                    wchar_t *Name     = (wchar_t*)NAR_OFFSET(Attr, 90);
                    uint64_t NameSize = (NameLen+1)*2; 
                    
                    Files[FileCount].Name    = (wchar_t*)LinearAllocate(StringAllocator, NameSize);
                    Files[FileCount].NameLen = NameLen;
                    
                    memcpy(Files[FileCount].Name, Name, NameSize - 2);
                    FileCount++;
                }
                
                break;
            }
            case NAR_DATA_FLAG:{
                if(FileSize != 0){
                    break;
                }
                FileSize = *(uint64_t*)NAR_OFFSET(Attr, 48);
                break;
            }
            case NAR_STANDART_FLAG:{
                NAR_BREAK;
                break;
            }
            default:{
                ASSERT(FALSE);
                break;
            }
        }
    }
    
    
    for(uint64_t i = 0; i<FileCount; i++){
        Files[i].Size         = FileSize;
        memcpy(&Files[i].CreationTime, &WinFileCreated, sizeof(SYSTEMTIME));
        memcpy(&Files[i].LastModifiedTime, &WinFileModified, sizeof(SYSTEMTIME));
    }
    
    return FileCount;
}


/*
Doesn't fail if reads less than ReadSiz, instead returns how many bytes it read.
ARGS:
Backup, Metadata: File views of according backup.
AbsoluteClusterOffset: (in clusters) Absolute volume offset we are trying to read.
ReadSizeInCluster    : (in clusters) How many clusters we should be reading.
Output               : Output buffer we should fill.
OutputMaxSize        : Max size of the output buffer.
ZSTDBuffer           : If backup is compressed, this buffer will be used to temporarily store decompressed data. This value might be NULL, if so, function allocates its own buffer. 
If buffer doesn't have enought space to contain decompressed data, function fails.
ZSTDBufferSize       : Size of given buffer, if no buffer is given this value is ignored.
*/
uint64_t
FEReadBackup(nar_file_view *Backup, nar_file_view *Metadata, 
             uint64_t AbsoluteClusterOffset, uint64_t ReadSizeInCluster, 
             void *Output, uint64_t OutputMaxSize,
             void *ZSTDBuffer, size_t ZSTDBufferSize)
{
    
    
    if(NULL == Backup){
        return 0;
    }
    if(NULL == Metadata){
        return 0;
    }
    
    uint64_t Result = 0;
    backup_metadata *BM = (backup_metadata*)Metadata->Data;
    
    nar_record* Records       = (nar_record*)((uint8_t*)Metadata->Data + BM->Offset.RegionsMetadata);
    uint64_t RecordCount      = BM->Size.RegionsMetadata/sizeof(nar_record);
    
    point_offset OffsetResult = FindPointOffsetInRecords(Records, RecordCount, AbsoluteClusterOffset);
    
    uint64_t BackupOffsetInBytes = 0;
    uint64_t AvailableBytes      = 0;
    uint64_t ReadSizeInBytes     = 0;
    
    {
        uint64_t BackupClusterOffset = OffsetResult.Offset;
        uint64_t AvailableClusters   = OffsetResult.Readable;
        if(AvailableClusters < ReadSizeInCluster){
            printf("Requested bytes exceeds available region size in the backup! Requested %I64u, available %I64u\n", ReadSizeInCluster, AvailableClusters);
            ReadSizeInCluster = AvailableClusters;
        }
        ReadSizeInBytes     = (uint64_t)BM->ClusterSize * ReadSizeInCluster;
        BackupOffsetInBytes = (uint64_t)BM->ClusterSize * BackupClusterOffset;
    }
    
    if(BM->IsCompressed){
        
        uint64_t CompressedSize      = 0;
        uint64_t Remaining           = 0;
        uint64_t DecompSize          = 0;
        uint64_t DecompAdvancedSoFar = 0;
        uint64_t BackupRemainingLen  = Backup->Len;
        uint64_t RemainingReadSize   = ReadSizeInBytes;
        uint8_t* DataNeedle          = Backup->Data;
        
        
        for(;BackupRemainingLen>0 && RemainingReadSize > 0 ;)
        {
            
            uint64_t DecompSize    = ZSTD_getFrameContentSize(DataNeedle, BackupRemainingLen);
            size_t CompressedSize  = ZSTD_findFrameCompressedSize(DataNeedle, BackupRemainingLen);       
            if(ZSTD_isError(DecompSize)){
                ZSTD_ErrorCode ErrorCode = ZSTD_getErrorCode(DecompSize);
                const char* ErrorString  = ZSTD_getErrorString(ErrorCode);
                printf("ZSTD error when determining frame content size, error : %s\n", ErrorString); 
                return 0;
            }
            if(ZSTD_isError(CompressedSize)){
                ZSTD_ErrorCode ErrorCode = ZSTD_getErrorCode(DecompSize);
                const char* ErrorString  = ZSTD_getErrorString(ErrorCode);
                printf("ZSTD error when determining frame compressed size, error : %s\n", ErrorString); 
                return 0;
            }
            
            // found!
            if(BackupOffsetInBytes < DecompAdvancedSoFar + DecompSize){
                
                bool ZSTDBufferLocal = (ZSTDBuffer == NULL);
                if(ZSTDBufferLocal){
                    ZSTDBuffer     = malloc(DecompSize);
                    ZSTDBufferSize = DecompSize;
                }
                ASSERT(ZSTDBufferSize >= DecompSize);
                ASSERT(NULL != ZSTDBuffer);
                
                
                size_t ZSTD_RetCode = ZSTD_decompress(ZSTDBuffer, ZSTDBufferSize, DataNeedle, CompressedSize);
                
                ASSERT(!ZSTD_isError(ZSTD_RetCode));
                
                uint64_t BufferOffset        = BackupOffsetInBytes - DecompAdvancedSoFar;
                uint64_t BufferReadableBytes = DecompSize - BufferOffset;
                uint64_t CopySize            = MIN(RemainingReadSize, BufferReadableBytes);
                
                memcpy(Output, (uint8_t*)ZSTDBuffer + BufferOffset, CopySize);
                Output = (uint8_t*)Output + CopySize;
                
                
                BackupOffsetInBytes += CopySize;
                RemainingReadSize   -= CopySize;
                
                if(ZSTDBufferLocal){
                    free(ZSTDBuffer);
                    ZSTDBuffer = 0;
                }
                
            }
            
            DecompAdvancedSoFar += DecompSize;
            
            BackupRemainingLen -= CompressedSize;
            DataNeedle          = DataNeedle + (CompressedSize);
        }
        
        
        
        // decompress
        
    }
    else{
        memcpy(Output, (uint8_t*)Backup->Data + BackupOffsetInBytes, ReadSizeInBytes);
    }
    
    
    return ReadSizeInBytes;
}

