//#include "precompiled.h"

#include "memory.h"
#include "file_explorer.h"
#include "platform_io.h"
#include "performance.h"
#include "nar_win32.h"

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


inline void*
NarNextAttribute(void *Attribute){
    uint32_t LenWithHeader = *(uint32_t*)NAR_OFFSET(Attribute, 4);
    void *Result = NAR_OFFSET(Attribute, LenWithHeader);
    uint32_t NextID = *(uint32_t*)Result;
    if(NextID == 0xFFFFFFFF){
        return 0;
    }
    return Result;
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
    uint64_t ParserLoopCount  = 0;
    DWORD BR = 0;
    
    uint64_t ClusterSize    = NarGetVolumeClusterSize(VolumeLetter);
    
    
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
                        
                        multiple_pid MultPIDs = NarGetFileNameAndParentID(FileRecord);
                        
                        void *AttributeList = NarFindFileAttributeFromFileRecord(FileRecord, NAR_ATTRIBUTE_LIST);
                        
                        if(AttributeList){
                            
                            void*   ATLData = 0;
                            uint32_t ATLLen = 0;
                            uint8_t ATLNonResident = *(uint8_t*)NAR_OFFSET(AttributeList, 8);
                            
                            if(!ATLNonResident){
                                ATLData = NAR_OFFSET(AttributeList, 24);
                                ATLLen  = *(uint32_t*)NAR_OFFSET(AttributeList, 16);
                                ASSERT(ATLLen < 512);
                            }
                            
                        }
                        if(!NarIsFileLinked(FileRecord)){
                            ASSERT(NarFindFileAttributeFromFileRecord(FileRecord, NAR_STANDART_FLAG));
                        }
                        
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
                            
                            for(uint64_t i =0; i<32; i++){
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
    
    wchar_t** FilenamesExtended = (wchar_t**)LinearAllocateAligned(&Memory->StringAllocator, ArrLen*8, 8);
    uint32_t *stack = (uint32_t*)ArenaAllocateAligned(&Memory->Arena, 1024*4, 4);
    
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
    
    fprintf(stdout, "Parser, %9llu files in %.5f sec, file per ms %.5f\n", Memory->TotalFC, ParserTotalTime, (double)Memory->TotalFC/ParserTotalTime/1000.0);
    fprintf(stdout, "Traverser %9llu files in %.5f sec, file per ms %.5f\n", ArrLen, TraverserTotalTime, (double)ArrLen/TraverserTotalTime/1000.0);
    //fprintf(stdout, "Allocating count %8u in  %.5f sec, allocation per ms %.5f\n", AllocationCount, AllocatorTimeElapsed, (double)AllocationCount/AllocatorTimeElapsed/(1000.0));
    
    //printf("Match count %I64u\n", ArrLen);
    
    
    Result.Files = FilenamesExtended;
    Result.Len   = ArrLen;
    
    return Result;
}


inline uint32_t
NarGetFileID(void* FileRecord){
    uint32_t Result = *(uint64_t*)NAR_OFFSET(FileRecord, 44);
    return Result;
}

inline uint8_t
NarIsFileLinked(void* FileRecord){
    uint32_t Result = NarGetFileBaseID(FileRecord);
    return !(Result == 0);
}

inline uint32_t
NarGetFileBaseID(void* FileRecord){
    uint32_t Result = *(uint32_t*)NAR_OFFSET(FileRecord, 32);
    return Result;
}


multiple_pid
NarGetFileNameAndParentID(void *FileRecord){
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
    
    return Result;
}


file_explorer_memory
NarInitFileExplorerMemory(uint32_t TotalFC){
    file_explorer_memory Result = {0};
    
    uint64_t StringAllocatorSize     = TotalFC*520 + Megabyte(500);
    Result.StringAllocator = NarCreateLinearAllocator(StringAllocatorSize, Megabyte(50));
    
    if(NULL != Result.StringAllocator.Memory){
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
NarInitFileExplorer(NarUTF8 MetadataPath){
    
    file_explorer Result = {0};
    
    Result.MetadataView   = NarOpenFileView(MetadataPath);
    
    
    if(NULL != Result.MetadataView.Data){
        backup_metadata* BM = (backup_metadata*)Result.MetadataView.Data;
        
        Result.MFT     = (uint8_t*)Result.MetadataView.Data + BM->Offset.MFT;
        Result.MFTSize = BM->Size.MFT;
        Result.TotalFC = (Result.MFTSize / (1024))*3/2;
        
        Result.Memory  = NarInitFileExplorerMemory(Result.TotalFC);
        Result.VolumeLetter = BM->Letter;
        Result.Version      = BM->Version;
        Result.ID           = BM->ID;
        
        
        Result.RootDir = NarGetRootPath(MetadataPath, &Result.Memory.Arena);
        
        {
            auto restore = ArenaGetRestorePoint(&Result.Memory.Arena);
            defer({ArenaRestoreToPoint(&Result.Memory.Arena, restore);});
            
            uint32_t SpaceNeeded = Result.RootDir.Len + GenerateBinaryFileNameUTF8(BM->ID, BM->Version, 0);
            
            NarUTF8 FullBinaryPath = NarUTF8Init(ArenaAllocateZero(&Result.Memory.Arena, SpaceNeeded), SpaceNeeded);
            
            NarUTF8 Name = NarUTF8Init(ArenaAllocateZero(&Result.Memory.Arena, SpaceNeeded), SpaceNeeded);
            
            
            GenerateBinaryFileNameUTF8(BM->ID, BM->Version, &Name);
            
            NarStringConcatenate(&FullBinaryPath, Result.RootDir);
            NarStringConcatenate(&FullBinaryPath, Name);
            
            Result.FullbackupView = NarOpenFileView(FullBinaryPath);
        }
        
        
        
        Result.ClusterSize = BM->ClusterSize;
        Result.DirectoryID   = 5;
        Result.DirectoryPath = (wchar_t*)ArenaAllocate(&Result.Memory.Arena, Kilobyte(32));
        Result.ParentIDs     = (uint32_t*)ArenaAllocateAligned(&Result.Memory.Arena, Result.TotalFC*sizeof(uint32_t)*2, 4);
        
        Result.Files = (file_explorer_file*)ArenaAllocateAligned(&Result.Memory.Arena, Result.TotalFC*sizeof(file_explorer_file), 8);
        
        ASSERT(Result.ParentIDs);
        ASSERT(Result.DirectoryPath);
        uint8_t* FB = (uint8_t*)ArenaAllocateAligned(&Result.Memory.Arena, 1024, 16);
        
        
        struct file_size_id_tuple{
            uint64_t FileSize;
            uint32_t FileID;
        }*FileSizeTuple = 0;
        
        FileSizeTuple = (file_size_id_tuple*)ArenaAllocateAligned(&Result.Memory.Arena, sizeof(*FileSizeTuple)*Result.TotalFC/2, 8);
        uint64_t FileSizeTupleCount = 0;
        Result.FileIDs = (uint32_t*)ArenaAllocateAligned(&Result.Memory.Arena, 4*Result.TotalFC, 4);
        
        double ParserAverageTime = 0.0;
        auto ParserStart = NarGetPerfCounter();
        
        for(uint64_t _fi = 0; _fi<Result.MFTSize/1024; _fi++){
            
            memcpy(&FB[0], (uint8_t*)Result.MFT + 1024*_fi, 1024);
            uint8_t *FileRecord = &FB[0];
            
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
            
            
            uint64_t FileSize = 0;
            multiple_pid MultPIDs = NarGetFileNameAndParentID(FileRecord);
            uint32_t FileID       = NarGetFileID(FileRecord);
            SYSTEMTIME WinFileCreated  = {0};
            SYSTEMTIME WinFileModified = {0};
            
            
            void* DataAttribute = NarFindFileAttributeFromFileRecord(FileRecord, NAR_DATA_FLAG);
            if(NULL != DataAttribute){
                // TODO(Batuhan) : I don't know what to do with reparse points.
                data_attr_header *DAHeader = (data_attr_header*)DataAttribute;
                if(DAHeader->NameLen == 0){
                    if(!!DAHeader->NonResidentFlag){
                        /*
                        uint64_t FirstVCN = *(uint64_t*)NAR_OFFSET(DataAttribute, 16);
                        uint64_t LastVCN  = *(uint64_t*)NAR_OFFSET(DataAttribute, 24); 
                        ASSERT(FirstVCN < 0xffffffffull);
                        ASSERT(LastVCN < 0xffffffffull);
                        */
                        FileSize = *(uint64_t*)NAR_OFFSET(DataAttribute, 56);
                    }
                    else{
                        FileSize = *(uint32_t*)NAR_OFFSET(DataAttribute, 16);
                        //NAR_BREAK;
                    }
                }
                
#if 0                
                uint32_t Flag = (DAHeader->NonResidentFlag == 0 && DAHeader->NameLen == 0);
                int i =0;
                for(void *Attribute = DataAttribute;
                    Attribute != 0;
                    Attribute = NarNextAttribute(Attribute)
                    )
                {
                    if(*(uint32_t*)Attribute > NAR_DATA_FLAG){
                        break;
                    }
                    data_attr_header *DAHeader = (data_attr_header*)Attribute;
                    Flag |= (DAHeader->NonResidentFlag == 0 && DAHeader->NameLen == 0);
                    if(Flag && i){
                        NAR_BREAK;
                    }
                    if(DAHeader->NameLen == 0){
                        i++;
                    }
                }
#endif
                
            }
            
            
            
            
            void* ATL = NarFindFileAttributeFromFileRecord(FileRecord, NAR_ATTRIBUTE_LIST);
            if(NULL != ATL){
                
                uint8_t ATLDataBuffer[4096];
                ASSERT(ATLDataBuffer != 0);
                
                void* ATLData = 0;
                size_t ATLLen = 0;
                
                uint8_t ATLNonResident = *(uint8_t*)NAR_OFFSET(ATL, 8);
                if(ATLNonResident){
                    //ASSERT(FALSE);
                }
            }
            
            // Extract file creation and modification time.
            void *StdAttr = NarFindFileAttributeFromFileRecord(FileRecord, NAR_STANDART_FLAG);
            if(NULL != StdAttr){
                uint32_t OffsetToData = *(uint32_t*)NAR_OFFSET(StdAttr, 20);
                
                void* AttrData = NAR_OFFSET(StdAttr, OffsetToData);
                
                uint64_t FileCreated  = *(uint64_t*)NAR_OFFSET(AttrData, 0);
                uint64_t FileModified = *(uint64_t*)NAR_OFFSET(AttrData, 8);
                
                if(0 == FileTimeToSystemTime((const FILETIME*)(void*)&FileCreated, &WinFileCreated)){
                    memset(&WinFileCreated, 0, sizeof(SYSTEMTIME));
                }
                if(0 == FileTimeToSystemTime((const FILETIME*)(void*)&FileModified, &WinFileModified)){
                    memset(&WinFileModified, 0, sizeof(SYSTEMTIME));
                }
                
            }// if stdattr
            
            
            // If file has size information of an another base file, we must save it in file size array, after parsing whole FS, we must re-iterate those array and correct sizes of linked ids. 
            if(!!NarIsFileLinked(FileRecord)){
                // $DATA attribute of a file might be in different file records, but only
                // first one, that represents first VCN, contains actual size. Remaining
                // attributes has size of 0, while they point to where they belong in terms
                // of VCN.
                // and another point of not adding if FileSize is 0 is, it would make literally zero difference if we would have added it or not. Because original
                // file's size is already 0!
                if(FileSize != 0){
                    uint32_t BaseID   = NarGetFileBaseID(FileRecord);
                    ASSERT(BaseID < 0xffffffffull);
                    FileID            = BaseID;
                    
                    FileSizeTuple[FileSizeTupleCount].FileSize = FileSize;
                    FileSizeTuple[FileSizeTupleCount].FileID   = BaseID;
                    FileSizeTupleCount++;
                    ASSERT(FileSizeTupleCount < Result.TotalFC/2);
                }
            }
            
            for(size_t _pidi = 0; 
                _pidi < MultPIDs.Len; 
                _pidi++)
            {
                TIMED_NAMED_BLOCK("USUAL MULTPIDS");
                name_pid NamePID = MultPIDs.PIDS[_pidi];
                if(NamePID.Name == NULL){
                    continue;
                }
                
                file_explorer_file *File = &Result.Files[Result.FileCount];
                
                uint64_t NameSize = (NamePID.NameLen+1)*2; 
                File->Name = (wchar_t*)LinearAllocate(&Result.Memory.StringAllocator, NameSize);
                
                // +1 for null termination
                memcpy(File->Name, NamePID.Name, NameSize - 2);
                File->NameLen      = NamePID.NameLen + 1;
                File->Size         = FileSize;
                File->FileID       = FileID;
                File->ParentFileID = NamePID.ParentFileID;
                File->IsDirectory  = Header->isDirectory;
                
                memcpy(&File->CreationTime, &WinFileCreated, sizeof(SYSTEMTIME));
                memcpy(&File->LastModifiedTime, &WinFileModified, sizeof(SYSTEMTIME));
                
                Result.ParentIDs[Result.FileCount] = NamePID.ParentFileID;
                Result.FileIDs[Result.FileCount]          = NamePID.FileID;
                Result.FileCount++;
                ASSERT(FileID < 0xffffffffull);
                ASSERT(Result.FileCount < Result.TotalFC);
            }
            
            
        }// for i < Result.TotalFC;
        
        double ParserTotalTime = NarTimeElapsed(ParserStart);
        ParserAverageTime = ParserTotalTime/(Result.MFTSize/1024);
        
        double TraverserAverageTime = 0.0;
        auto TraverserStart = NarGetPerfCounter();
        
        uint64_t Hit = 0;
        for(uint64_t _idc = 0; _idc < FileSizeTupleCount; _idc++){
            
            ASSERT(FileSizeTuple[_idc].FileSize != 0);
            ASSERT(FileSizeTuple[_idc].FileID > 0);
            
            uint64_t FileSize = FileSizeTuple[_idc].FileSize;
            uint64_t BaseID   = FileSizeTuple[_idc].FileID;
            
            file_explorer_file *File = FEFindFileWithID(&Result, BaseID);
            if(File){
                file_explorer_file *F = File;
                // scan in reverse to find file with same ID's
                while(F->FileID == BaseID) {
                    F->Size = FileSize;
                    F--;
                    Hit++;
                }
                
                F = ++File;
                while(F->FileID == BaseID){
                    F->Size = FileSize;
                    F++;
                    Hit++;
                }
            }
            
        }
        
        
        fprintf(stdout, "Hit count %I64u\n", Hit);
        double TraverserTotalTime = NarTimeElapsed(TraverserStart); 
        TraverserAverageTime = TraverserTotalTime/((double)FileSizeTupleCount);
        
        double ParserFilePerMs = (Result.MFTSize/1024)/(ParserTotalTime*1000.0);
        fprintf(stdout, "Parser total time %.5f sec, file per ms is %.3f, apprx %.5fGB/s\n", ParserTotalTime, ParserFilePerMs, (ParserFilePerMs*1000.0*1024.0)/Gigabyte(1));
        
        double TraverserFilePerMs = (FileSizeTupleCount)/(TraverserTotalTime*1000.0);
        fprintf(stdout, "Traverser total time %.5f sec, file per ms is %.3f, apprx %.5fGB/s\n", TraverserTotalTime, TraverserFilePerMs, (TraverserFilePerMs*1000.0*8)/Gigabyte(1));
        
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
    uint64_t StartIndice = (CurrentFile - &FE->Files[0]);
    StartIndice += 1;
    uint64_t ParentID    = CurrentFile->ParentFileID;
    for(uint64_t i = StartIndice; i<FE->FileCount; i++){
        if(FE->ParentIDs[i] == ParentID){
            return &FE->Files[i];
        }
    }
    return 0;
}

file_explorer_file*
FEFindFileWithID(file_explorer* FE, uint32_t ID){
    
    uint32_t Left  = 0;
    uint32_t Right = FE->FileCount;
    uint32_t Mid   = (Right - Left)/2;
    
    while(Right > Left && Mid!=Left){
        
        if(FE->FileIDs[Mid] == ID){
            return &FE->Files[Mid];
        }
        if(FE->FileIDs[Mid] > ID){
            Right = Mid;
        }
        if(FE->FileIDs[Mid] < ID){
            Left = Mid;
        }
        Mid = Left + (Right - Left)/2;
    }
    
    return 0;
}



wchar_t*
FEGetFileFullPath(file_explorer* FE, file_explorer_file* BaseFile){
    
    size_t StringSizeNeeded = 0;
    
    memory_restore_point R = ArenaGetRestorePoint(&FE->Memory.Arena);
    defer({ArenaRestoreToPoint(&FE->Memory.Arena, R);});
    
    file_explorer_file **Stack = (file_explorer_file**)ArenaAllocate(&FE->Memory.Arena, 1024*sizeof(file_explorer_file*));
    
    uint32_t SI = 0;
    
    for(file_explorer_file *File = BaseFile; ;File = FEFindFileWithID(FE, File->ParentFileID)){
        
        Stack[SI++] = File;
        if(File->ParentFileID == 5){
            break;
        }
        StringSizeNeeded += File->NameLen;
        
    }
    
    StringSizeNeeded += 10;
    StringSizeNeeded = StringSizeNeeded*2;
    
    size_t RI = 0;
    wchar_t* Result = (wchar_t*)LinearAllocate(&FE->Memory.StringAllocator, StringSizeNeeded);
    
    wchar_t VN[] = L"!:\\";
    VN[0] = FE->VolumeLetter;
    memcpy(Result + RI, VN, sizeof(VN));
    RI += 3;
    wchar_t Slash[] = L"\\";
    
    for(uint32_t i = SI-1;i>0; i--){
        file_explorer_file *File = Stack[i];
        memcpy(&Result[RI], File->Name, File->NameLen*2);
        RI += File->NameLen - 1;
        memcpy(&Result[RI], Slash, 2);
        RI++;
    }
    
    memcpy(&Result[RI], BaseFile->Name, BaseFile->NameLen*2);
    
    
    return Result;
}


bool IsValidAttrEntry(attribute_list_entry Entry){
    return (Entry.EntryType != 0);
}



/*
AttrListDataStart MUST be first entry in the attribute list, not the 
attribute start itself. NTFS sucks, you might not able to reach attribute
data from normal file record, it might be stored somewhere else in the disk.
It's better we isolate this layer so both codepaths can call this.
*/
attribute_list_contents
GetAttributeListContents(void* AttrListDataStart, uint64_t DataLen){
    
    attribute_list_contents Result = {};
    
    // skip first 24 bytes, header.
    uint32_t LenRemaining      = DataLen;
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
    uint64_t   FileSize        = 0;
    
    FileRecordHeader *Header = (FileRecordHeader*)BaseFileRecord;
    TIMED_BLOCK();
    
    uint64_t FileCount = 0;
    
    // find unique file ID's that contains file name attribute
    uint32_t UniqueFileID[16];
    uint32_t UniqueFileIDCount = 0;
    for(uint64_t i =0; i < 32; i++){
        
        attribute_list_entry Entry = Contents.Entries[i];
        
        if(false == IsValidAttrEntry(Entry)){
            break;
        }
        
        if(i == 0 && Entry.EntryType == NAR_FILENAME_FLAG){
            UniqueFileID[UniqueFileIDCount++] = Entry.EntryFileID;
            continue;
        }
        
        attribute_list_entry PrevEntry = Contents.Entries[i-1];
        
        if(Entry.EntryType == NAR_FILENAME_FLAG){
            if(PrevEntry.EntryType == NAR_FILENAME_FLAG && Entry.EntryFileID == PrevEntry.EntryFileID){
                
            }
            else{
                UniqueFileID[UniqueFileIDCount++] = Entry.EntryFileID;
            }
        }
        
    }
    
    ASSERT(UniqueFileIDCount > 0);
    
    for(uint64_t i =0; i<UniqueFileIDCount; i++){
        
        void *EntryFileRecord = (uint8_t*)MFTStart + UniqueFileID[i] * 1024;
        multiple_pid MultPIDs = NarGetFileNameAndParentID(EntryFileRecord);
        
        ASSERT(MultPIDs.Len > 0);
        for(uint64_t _pidi = 0; _pidi<MultPIDs.Len; _pidi++){
            name_pid NamePID  = MultPIDs.PIDS[_pidi];
            uint32_t NameLen  = NamePID.NameLen;
            wchar_t *Name     = NamePID.Name;
            uint64_t NameSize = (NameLen+1)*2; 
            
            Files[FileCount].Name    = (wchar_t*)LinearAllocate(StringAllocator, NameSize);
            Files[FileCount].NameLen = NameLen;
            
            memcpy(Files[FileCount].Name, Name, NameSize - 2);
            FileCount++;
        }
    }
    
    
    for(uint64_t i =0; i < 32; i++){
        
        attribute_list_entry Entry = Contents.Entries[i];
        if(false == IsValidAttrEntry(Entry)){
            break;
        }
        
        if(Entry.EntryType != NAR_DATA_FLAG
           && Entry.EntryType != NAR_STANDART_FLAG){
            continue;
        }
        
        
        void *EntryFileRecord = (uint8_t*)MFTStart + Entry.EntryFileID * 1024;
        void *Attr            = NarFindFileAttributeFromFileRecord(EntryFileRecord, Entry.EntryType);
        ASSERT(Attr != NULL);
        
        switch(Entry.EntryType){
            
#if 0     
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
#endif
            
            case NAR_DATA_FLAG:{
                if(FileSize != 0){
                    break;
                }
                FileSize = *(uint64_t*)NAR_OFFSET(Attr, 48);
                break;
            }
            case NAR_STANDART_FLAG:{
                //NAR_BREAK;
                break;
            }
            default:{
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


file_disk_layout
NarFindFileLayout(file_explorer *FE, file_explorer_file *File, nar_arena *Arena){
    
    file_disk_layout Result = {0};
    
    uint32_t FilesToVisit[80];
    uint32_t FTVCount        = 0;
    
    Result.MaxCount = 1024;
    
    // if attribute list is not resident
    // if attribute list is resident
    //void* FileRecord = NAR_OFFSET(FE->MFT, File->FileID*1024);
    uint8_t __FRMemory[1024];
    void* FileRecord = &__FRMemory[0];
    memcpy(FileRecord, NAR_OFFSET(FE->MFT, File->FileID*1024), 1024);
    ((uint8_t*)FileRecord)[510] = *(uint8_t*)NAR_OFFSET(FileRecord, 50);
    ((uint8_t*)FileRecord)[511] = *(uint8_t*)NAR_OFFSET(FileRecord, 51);
    
    memory_restore_point RestorePoint = ArenaGetRestorePoint(Arena);
    Result.LCN = (nar_record*)ArenaAllocateAligned(Arena, Result.MaxCount*sizeof(nar_record), 8);
    
    void* ATL  = NarFindFileAttributeFromFileRecord(FileRecord, NAR_ATTRIBUTE_LIST);
    
    
    if(NULL != ATL){
        
        uint8_t* ATLDataBuffer = (uint8_t*)ArenaAllocateAligned(&FE->Memory.Arena, FE->ClusterSize, 8);
        ASSERT(ATLDataBuffer != 0);
        
        void* ATLData = 0;
        size_t ATLLen = 0;
        
        uint8_t ATLNonResident = *(uint8_t*)NAR_OFFSET(ATL, 8);
        if(ATLNonResident){
            void* DataRunStart = NAR_OFFSET(ATL, 64);;
            nar_record Runs[64];
            uint32_t DataRunFound = 0;
            
            bool ParseResult = NarParseDataRun(DataRunStart, 
                                               Runs, 
                                               sizeof(Runs)/sizeof(nar_record), 
                                               &DataRunFound, 
                                               false);
            
            ASSERT(ParseResult);
            ASSERT(DataRunFound == 1);
            
            uint32_t TargetCluster = Runs[0].StartPos;
            uint32_t FEResult = NarReadBackup(&FE->FullbackupView, &FE->MetadataView, TargetCluster, 1, ATLDataBuffer, FE->ClusterSize, 0, 0);
            
            ATLData = &ATLDataBuffer[0];
            ATLLen  = *(uint32_t*)NAR_OFFSET(ATL, 48);
            ASSERT(FEResult == 4096);
            
        }
        else{
            ATLData = NAR_OFFSET(ATL, 24);
            ATLLen  = *(uint32_t*)NAR_OFFSET(ATL, 16);
        }
        
        uint32_t RemainingLen = ATLLen;
        void *CurrentRecord   = ATLData;
        uint32_t PrevID = 0;
        while(RemainingLen){
            uint32_t Len   = *(uint16_t*)NAR_OFFSET(CurrentRecord, 4);
            uint32_t Type  = *(uint32_t*)CurrentRecord;
            
            if(Type == NAR_DATA_FLAG){
                uint32_t AttrListFileID = *(uint32_t*)NAR_OFFSET(CurrentRecord, 16);
                if(PrevID != AttrListFileID){
                    FilesToVisit[FTVCount++] = AttrListFileID;
                    PrevID                   = AttrListFileID;
                    ASSERT(FTVCount < sizeof(FilesToVisit)/4);
                }
            }
            
            RemainingLen  -= Len;
            CurrentRecord = NAR_OFFSET(CurrentRecord, Len);
            
        }
        
        for(int i = 0; i<FTVCount;i++){
            void* FileRecord = (uint8_t*)FE->MFT + 1024*FilesToVisit[i];
            // there might be more than 1 $DATA attribute.
            
            INT16 FirstAttributeOffset = (*(int16_t*)((BYTE*)FileRecord + 20));
            
            for(void* FileAttribute = (uint8_t*)FileRecord + FirstAttributeOffset;
                FileAttribute != 0;
                FileAttribute = NarNextAttribute(FileAttribute))
            {
                
                uint32_t AttributeID = *(uint32_t*)FileAttribute;
                
                if(AttributeID > NAR_DATA_FLAG){
                    break;
                }
                
                if(AttributeID == NAR_DATA_FLAG){
                    data_attr_header *DAHeader = (data_attr_header*)FileAttribute;
                    ASSERT(DAHeader->NameLen == 0);
                    
                    uint32_t Of = *(uint16_t*)NAR_OFFSET(DAHeader, 32);
                    
                    void* DRStart = NAR_OFFSET(DAHeader, Of);
                    uint32_t RegionFound = 0;
                    bool r = NarParseDataRun(DRStart, &Result.LCN[Result.LCNCount], Result.MaxCount - Result.LCNCount, &RegionFound, false);
                    
                    
                    Result.LCNCount += RegionFound;
                    
                    ASSERT(Result.LCNCount < Result.MaxCount);
                    ASSERT(r);
                    
                    if(Result.LCNCount >= Result.MaxCount){
                        goto G_FAIL;
                    }
                    
                }
                
            }
            
        }
    }
    else{
        INT16 FirstAttributeOffset = (*(int16_t*)((BYTE*)FileRecord + 20));
        void* FileAttribute = (char*)FileRecord + FirstAttributeOffset;
        
        for(void* FileAttribute = (uint8_t*)FileRecord + FirstAttributeOffset;
            FileAttribute != 0;
            FileAttribute = NarNextAttribute(FileAttribute)){
            
            uint32_t AttributeID = *(uint32_t*)FileAttribute;
            if(AttributeID == NAR_DATA_FLAG){
                data_attr_header *DAHeader = (data_attr_header*)FileAttribute;
                ASSERT(DAHeader->NameLen == 0);
                
                if(DAHeader->NonResidentFlag == 0 && DAHeader->NameLen == 0){
                    ArenaRestoreToPoint(Arena, RestorePoint);
                    
                    uint16_t OffsetToData = *(uint16_t*)NAR_OFFSET(FileAttribute, 20);
                    Result.TotalSize = *(uint32_t*)NAR_OFFSET(FileAttribute, 16);
                    
                    Result.ResidentData = ArenaAllocate(Arena, Result.TotalSize);
                    memcpy(Result.ResidentData, NAR_OFFSET(FileAttribute, OffsetToData), Result.TotalSize);
                    
                    break;
                }
                
                uint32_t Of = *(uint16_t*)NAR_OFFSET(DAHeader, 32);
                
                void* DRStart = NAR_OFFSET(DAHeader, Of);
                uint32_t RegionFound = 0;
                bool r = NarParseDataRun(DRStart, &Result.LCN[Result.LCNCount], Result.MaxCount - Result.LCNCount, &RegionFound, false);
                
                
                Result.LCNCount += RegionFound;
                
                ASSERT(Result.LCNCount < Result.MaxCount);
                ASSERT(r);
                
                if(Result.LCNCount >= Result.MaxCount){
                    goto G_FAIL;
                }
            }
            
            if(AttributeID > NAR_DATA_FLAG){
                break;
            }
            
        }
        
    }
    
    
    
    if(Result.ResidentData == 0){
        ASSERT(Result.TotalSize == 0);
        
        Result.TotalSize = 0;
        for(uint64_t i = 0; i<Result.LCNCount; i++){
            Result.TotalSize += Result.LCN[i].Len*FE->ClusterSize;
        }
        Result.SortedLCN = (nar_record*)ArenaAllocateAligned(Arena, Result.LCNCount*sizeof(nar_record), 8);
        
        // it is guarenteed there wont be colliding blocks, no need to call mergeregionswithoutrealloc
        memcpy(Result.SortedLCN, Result.LCN, Result.LCNCount*sizeof(nar_record));
        qsort(Result.SortedLCN, Result.LCNCount, sizeof(nar_record), CompareNarRecords);
    }
    
    return Result;
    
    G_FAIL:
    return {0};
}

inline void
NarFreeFileRestoreSource(file_restore_source *Src){
    NarFreeFileView(Src->Backup);
    NarFreeFileView(Src->Metadata);
}


inline file_restore_source
NarInitFileRestoreSource(NarUTF8 MetadataName, NarUTF8 BinaryName){
    file_restore_source Result = {};
    
    Result.Metadata = NarOpenFileView(MetadataName);
    Result.Backup   = NarOpenFileView(BinaryName);
    
    ASSERT(Result.Metadata.Data);
    ASSERT(Result.Backup.Data);
    
    backup_metadata *BM = (backup_metadata*)Result.Metadata.Data;
    
    Result.BackupLCN = (nar_record*)((uint8_t*)Result.Metadata.Data + BM->Offset.RegionsMetadata);
    Result.LCNCount  = BM->Size.RegionsMetadata/sizeof(nar_record);
    Result.ID        = BM->ID;
    Result.Version   = BM->Version;
    Result.Type      = BM->BT;
    return Result;
}


inline file_restore_source 
NarInitFileRestoreSource(NarUTF8 RootDir, nar_backup_id ID, int32_t Version, nar_arena *StringArena){
    
    bool StringResult = false;
    memory_restore_point Restore = ArenaGetRestorePoint(StringArena);
    file_restore_source Result = {};
    
    uint32_t BForMN = GenerateMetadataNameUTF8(ID, Version, 0);
    uint32_t BForBN = GenerateBinaryFileNameUTF8(ID, Version, 0);
    NarUTF8 MetadataRelative = {};
    NarUTF8 BinaryRelative   = {};
    
    
    { // initialize relative paths 
        MetadataRelative = {(uint8_t*)ArenaAllocateZero(StringArena, BForMN), 0, BForMN};
        GenerateMetadataNameUTF8(ID, Version, &MetadataRelative);
        
        BinaryRelative   = {(uint8_t*)ArenaAllocateZero(StringArena, BForMN), 0, BForMN};
        GenerateBinaryFileNameUTF8(ID, Version, &BinaryRelative);
    }
    
    BForMN += RootDir.Len + 20;
    BForBN += RootDir.Len + 20;
    
    // allocate memory for full paths
    NarUTF8 MetadataName = {(uint8_t*)ArenaAllocateZero(StringArena, BForMN), 0, BForMN};
    NarUTF8 BinaryName   = {(uint8_t*)ArenaAllocateZero(StringArena, BForBN), 0, BForBN};
    
    { // Initialize both full paths as RootDir
        StringResult = NarStringCopy(&MetadataName, RootDir);
        ASSERT(StringResult == true);
        
        StringResult = NarStringCopy(&BinaryName, RootDir);
        ASSERT(StringResult == true);
    }
    
    
    { // append relative paths to root dir
        StringResult = NarStringConcatenate(&MetadataName, MetadataRelative);
        ASSERT(StringResult == true);
        
        StringResult = NarStringConcatenate(&BinaryName, BinaryRelative);
        ASSERT(StringResult == true);
    }
    
    Result = NarInitFileRestoreSource(MetadataName, BinaryName);
    
    ArenaRestoreToPoint(StringArena, Restore);
    
    return Result;
}

inline file_restore_ctx
NarInitFileRestoreCtx(file_disk_layout Layout, NarUTF8 RootDir, nar_backup_id ID, int Version, nar_arena *Arena){
    
    file_restore_ctx Result = {};
    
    Result.Layout = Layout;
    Result.Source = NarInitFileRestoreSource(RootDir, ID, Version, Arena);
    
    size_t PoolMemSize = Megabyte(1);
    size_t PoolSize    = PoolMemSize/4;
    
    void *PoolMem   = ArenaAllocateAligned(Arena, PoolMemSize, 8);
    
    Result.ClusterSize = ((backup_metadata*)Result.Source.Metadata.Data)->ClusterSize;
    
    Result.LCNPool         = NarInitPool(PoolMem, PoolMemSize, PoolSize);
    Result.StringAllocator = ArenaInit(ArenaAllocate(Arena, Kilobyte(400)), Kilobyte(400));
    
    Result.DecompBufferSize  = NAR_COMPRESSION_FRAME_SIZE*2;
    Result.DecompBuffer      = ArenaAllocate(Arena, Result.DecompBufferSize);
    
    
    Result.ActiveLCN      = (nar_record*)PoolAllocate(&Result.LCNPool);
    Result.ActiveLCNCount = Result.Layout.LCNCount;
    
    memcpy(Result.ActiveLCN, Result.Layout.SortedLCN, Result.Layout.LCNCount*sizeof(nar_record));
    
    
    Result.RootDir = NarStringCopy(RootDir, &Result.StringAllocator);
    Result.IIter   = NarInitRegionCoupleIter(Result.ActiveLCN, Result.Source.BackupLCN, Result.ActiveLCNCount, Result.Source.LCNCount);
    
    ASSERT(Result.ActiveLCNCount <= PoolSize);
    
    return Result;
}

inline file_restore_ctx
NarInitFileRestoreCtx(file_explorer *FE, file_explorer_file* Target, nar_arena *Arena){
    file_disk_layout Layout = NarFindFileLayout(FE, Target, Arena);
    file_restore_ctx Result = {};
    Result = NarInitFileRestoreCtx(Layout, FE->RootDir, FE->ID, FE->Version, Arena);
    return Result;
}

inline void
NarFreeFileRestoreCtx(file_restore_ctx *Ctx){
    NarFreeFileRestoreSource(&Ctx->Source);
}


inline file_restore_advance_result
NarAdvanceFileRestore(file_restore_ctx *ctx, void* Out, size_t OutSize){
    
    // FileRestore_Errors Result = FileRestore_Errors:Error_NoError;
    // fetch next region if this one is depleted
    file_restore_advance_result Result = {};
    
    if(ctx->Layout.ResidentData != NULL){
        if(ctx->Layout.TotalSize){
            memcpy(Out, ctx->Layout.ResidentData, ctx->Layout.TotalSize);
            Result.Offset = 0;
            Result.Len    = ctx->Layout.TotalSize;
            ctx->Layout.TotalSize = 0;
        }
        return Result;
    }
    
    if(ctx->ClustersLeftInRegion == 0){
        
        NarNextIntersectionIter(&ctx->IIter);
        
        nar_record FetchRegion = ctx->IIter.It;
        
        // check if end of IIter
        if(!NarIsRegionIterValid(ctx->IIter)){
            
            // exclude found backups from layout.
            {
                nar_record *ExcludedLCN = (nar_record*)PoolAllocate(&ctx->LCNPool);
                size_t i = 0;
                
                
                for(RegionCoupleIter Iter = NarStartExcludeIter(ctx->ActiveLCN, ctx->Source.BackupLCN, ctx->ActiveLCNCount, ctx->Source.LCNCount);
                    NarIsRegionIterValid(Iter);
                    NarNextExcludeIter(&Iter)
                    )
                {
                    printf("Ex reg :%8u\t%8u\n", Iter.It.StartPos, Iter.It.Len);
                    ExcludedLCN[i++] = Iter.It; 
                }
                
                if(ctx->Source.Version == NAR_FULLBACKUP_VERSION){
                    ASSERT(i == 0);
                }
                
                PoolDeallocate(&ctx->LCNPool, ctx->ActiveLCN);
                ctx->ActiveLCN      = ExcludedLCN;
                ctx->ActiveLCNCount = i;
                
                
                if(i == 0) {
                    return {};
                }
                if(ctx->Source.Version == NAR_FULLBACKUP_VERSION && i != 0){
                    ctx->Error = FileRestore_Errors::Error_EndOfBackups;
                    return {};
                }
                
            }
            
            
            
            int NewVersion = NAR_INVALID_BACKUP_VERSION;
            NarGetPreviousBackupInfo(ctx->Source.Version, ctx->Source.Type, &NewVersion);
            
            // initialize parameters for a new restore source
            { 
                NarFreeFileRestoreSource(&ctx->Source);
                ASSERT(ctx->Source.Version != NAR_FULLBACKUP_VERSION);
                
                // That was last backup, return error.
                if(ctx->Source.Version == NAR_FULLBACKUP_VERSION){
                    ctx->Error = FileRestore_Errors::Error_EndOfBackups;
                    return {};
                }
                else{
                    ctx->Source = NarInitFileRestoreSource(ctx->RootDir, 
                                                           ctx->Source.ID, 
                                                           NewVersion, 
                                                           &ctx->StringAllocator);
                    
                    // here, we have to do extraction iter...
                    ctx->IIter = NarInitRegionCoupleIter(ctx->ActiveLCN, 
                                                         ctx->Source.BackupLCN, 
                                                         ctx->ActiveLCNCount, 
                                                         ctx->Source.LCNCount);
                    return NarAdvanceFileRestore(ctx, Out, OutSize);
                }
            }
            
            // move to next backup if possible
        }
        
        ctx->ClustersLeftInRegion = ctx->IIter.It.Len;
        ctx->AdvancedSoFar        = 0;
    }
    
    
    
    size_t ClustersToRead = MIN(ctx->ClustersLeftInRegion, OutSize/ctx->ClusterSize);
    size_t ReadOffset     = ctx->IIter.It.StartPos + ctx->AdvancedSoFar;
    ASSERT(ClustersToRead != 0);
    //NarLCNToVCN();
    uint64_t BytesRead    = NarReadBackup(&ctx->Source.Backup, &ctx->Source.Metadata, 
                                          ReadOffset, ClustersToRead, 
                                          Out, OutSize,
                                          ctx->DecompBuffer, ctx->DecompBufferSize);
    
    ctx->AdvancedSoFar        += ClustersToRead;
    ctx->ClustersLeftInRegion -= ClustersToRead;
    
    size_t VCNWrite = NarLCNToVCN(ctx->Layout.LCN, ctx->Layout.LCNCount, ReadOffset);
    if(VCNWrite == (size_t)-1){
        ctx->Error    = FileRestore_Errors::Error_LCNToVCNMap;
        Result.Len    = 0;
        Result.Offset = 0;
    }
    
    //printf("%8u\t%8u\t%8u\n", ReadOffset, VCNWrite, ClustersToRead);
    
    Result.Len      = BytesRead;
    Result.Offset   = VCNWrite * ctx->ClusterSize;
    
    return Result; 
}




