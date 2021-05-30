#include "precompiled.h"
#include "file_explorer.h"


#define TIMED_BLOCK__(NAME, Number, ...) 
#define TIMED_BLOCK_(NAME, Number, ...)  
#define TIMED_BLOCK(...)                 
#define TIMED_NAMED_BLOCK(NAME, ...)     


/**/
inline UINT64
NarGetFileSizeFromRecord(void *R){
    if(R == 0) return 0;
    void *D = NarFindFileAttributeFromFileRecord(R, NAR_DATA_FLAG);
    return *(UINT64*)NAR_OFFSET(D, 0x30);
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


extension_search_result
NarFindExtensions(char VolumeLetter, HANDLE VolumeHandle, wchar_t *Extension, nar_arena *Arena) {
    extension_search_result Result = {0};
    
    BOOLEAN JustExtractMFTRegions  = FALSE;
    uint64_t ScratchArenaSize 	 = 0;
    void* ScratchMemory            = 0;
	nar_arena ScratchArena         = {0};
    
    uint32_t MFTRecordsCapacity = 256;
    uint64_t FileRecordSize     = 1024;
    uint64_t TotalFC            = 0;
    nar_record* MFTRecords      = (nar_record*)ArenaAllocateAligned(Arena, MFTRecordsCapacity*sizeof(nar_record), sizeof(nar_arena));
    wchar_t **NameMap           = NULL;
    uint32_t *IndiceMap 	    = NULL;
    uint32_t *IndiceArr 	    = NULL;
    uint8_t  *FileBuffer        = NULL;
    uint8_t  *FileNameSize      = NULL;
    uint64_t FileBufferSize     = Megabyte(128);
    uint64_t ArrLen     	    = 0;
    
    size_t ExtensionLen = wcslen(Extension);
    double ParserTotalTime    = 0;
    double TraverserTotalTime = 0;
    int64_t ParserLoopCount    = 0;
    
    DWORD BR = 0;
    unsigned int MFTRecordsCount  = 0;
    
    uint64_t ClusterSize    = NarGetVolumeClusterSize(VolumeLetter);
    
    bool tres = NarGetMFTRegionsFromBootSector(VolumeHandle, 
                                               MFTRecords, 
                                               &MFTRecordsCount, 
                                               MFTRecordsCapacity);
    
    if(tres){
        TotalFC = 0;
        for(unsigned int MFTOffsetIndex = 0; MFTOffsetIndex < MFTRecordsCount; MFTOffsetIndex++){
            TotalFC += MFTRecords[MFTOffsetIndex].Len;
        }
        TotalFC *= 4;
        printf("Total file count is %I64u\n", TotalFC);
        
        ScratchArenaSize 	 = (TotalFC*350 + FileBufferSize);
        ScratchMemory         = VirtualAlloc(0, ScratchArenaSize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);  
        ScratchArena         = ArenaInit(ScratchMemory, ScratchArenaSize);
        
        NameMap  	 = (wchar_t**)ArenaAllocateAligned(&ScratchArena, TotalFC*8ull, 8);        
        FileBuffer    =  (uint8_t*)ArenaAllocateAligned(&ScratchArena, FileBufferSize, 16);
        IndiceMap 	= (uint32_t*)ArenaAllocateAligned(&ScratchArena, TotalFC*4, sizeof(IndiceMap[0]));
        IndiceArr 	= (uint32_t*)ArenaAllocateAligned(&ScratchArena, TotalFC*4, sizeof(IndiceArr[0]));
    	FileNameSize  =  (uint8_t*)ArenaAllocateAligned(&ScratchArena, TotalFC, 1);
    }
    else{
        return {0};
    }
    
    uint32_t BufferStartFileID = 0;
    uint64_t VCNOffset = 0;
    
    
    
    for (uint64_t MFTOffsetIndex = 0; 
         MFTOffsetIndex < MFTRecordsCount; 
         MFTOffsetIndex++) 
    {
        
        
        uint64_t FilePerCluster = ClusterSize / 1024ull;
        uint64_t Offset = (uint64_t)MFTRecords[MFTOffsetIndex].StartPos * (uint64_t)ClusterSize;
        
        // set file pointer to actual records
        if (NarSetFilePointer(VolumeHandle, Offset)) {
            
            uint64_t FileRemaining   = (uint64_t)MFTRecords[MFTOffsetIndex].Len * (uint64_t)FilePerCluster;
            while(FileRemaining){
                
                uint64_t FBCount       = FileBufferSize/1024ull;
                size_t TargetFileCount = MIN(FileRemaining, FBCount);
                FileRemaining         -= TargetFileCount;
                
                ReadFile(VolumeHandle, 
                         FileBuffer, 
                         TargetFileCount*1024ull, 
                         &BR, 0);
                ASSERT(BR == TargetFileCount*1024);
                
                if(BR == TargetFileCount*1024ull){
                    
                    //int64_t start = NarGetPerfCounter();
                    //ParserLoopCount += TargetFileCount;
                    for (uint64_t FileRecordIndex = 0; FileRecordIndex < TargetFileCount; FileRecordIndex++) {
                        
                        void* FileRecord = (BYTE*)FileBuffer + (uint64_t)FileRecordSize * (uint64_t)FileRecordIndex;
                        
                        // file flags are at 22th offset in the record
                        if (*(int32_t*)FileRecord != 'ELIF') {
                            // block doesnt start with 'FILE0', skip
                            continue;
                        }
                        
#if 0                        
                        void *AttributeList = NarFindFileAttributeFromFileRecord(FileRecord, NAR_ATTRIBUTE_LIST);                        
#endif
                        
                        FileRecordHeader *h = (FileRecordHeader*)FileRecord;
                        if(h->inUse == 0){
                            continue;
                        }
                        
                        // lsn, lsa swap to not confuse further parsing stages.
                        ((uint8_t*)FileRecord)[510] = *(uint8_t*)NAR_OFFSET(FileRecord, 50);
                        ((uint8_t*)FileRecord)[511] = *(uint8_t*)NAR_OFFSET(FileRecord, 51);
                        
                        uint32_t FileID   = 0;
                        uint32_t ParentID = 0;
                        
                        name_and_parent_id NamePID = NarGetFileNameAndParentID(FileRecord);
                        
                        if(NamePID.Name != 0){
                            
                            // NamePID.Name[0] != L'$'
                            IndiceMap[NamePID.FileID] = NamePID.ParentFileID;
                            
                            uint64_t NameSize = (NamePID.NameLen + 1) * 2; 
                            NameMap[NamePID.FileID] = (wchar_t*)ArenaAllocate(&ScratchArena, NameSize);
                            
                            memcpy(NameMap[NamePID.FileID], NamePID.Name, NameSize - 2);
                            FileNameSize[NamePID.FileID] = NamePID.NameLen + 1; // +1 for null termination
                            
                            if(ExtensionLen < FileNameSize[NamePID.FileID]){
                                
                                wchar_t *LastChars = &NameMap[NamePID.FileID][FileNameSize[NamePID.FileID] - ExtensionLen - 1];
                                bool Add = true;
                                for(uint64_t wi = 0; wi < ExtensionLen; wi++){
                                    if(LastChars[wi] != Extension[wi]){
                                        Add = false;
                                        break;
                                    }
                                }
                                
                                if(Add){
                                    IndiceArr[ArrLen++] = NamePID.FileID;
                                }
                                
                            }
                            
                        }
                        else{
                        }
                        
                    }
                    
                    //ParserTotalTime += NarTimeElapsed(start);
                    
                }
                else{
                    //failed!
                }
                
                
            }
            
            
        }
    }
    
    
    //int64_t TraverserStart = NarGetPerfCounter();
    
    uint64_t SkippedFileCount = 0;
    wchar_t** FilenamesExtended = (wchar_t**)ArenaAllocateAligned(Arena, ArrLen*8, 8);
    uint32_t *stack = (uint32_t*)ArenaAllocateAligned(&ScratchArena, 1024*4, 4);
    
    for(uint64_t s = 0; s<ArrLen; s++){
        
        uint32_t si = 0;
        uint16_t TotalFileSize = 0;
        memset(&stack[0], 0, sizeof(stack));
        
        for(uint32_t ParentID = IndiceMap[IndiceArr[s]]; 
            ParentID != 5 && ParentID != 0; 
            ParentID    = IndiceMap[ParentID])
        {
            stack[si++] = ParentID;
            _mm_prefetch((const char*)&IndiceMap[ParentID], _MM_HINT_T0);
        }
        
        //TODO prefetch next element into the cache
        for(uint32_t ParentID = IndiceMap[IndiceArr[s]]; 
            ParentID != 5 && ParentID != 0; ParentID    = IndiceMap[ParentID]){
            TotalFileSize += FileNameSize[ParentID];
        }
        
        // additional memory for trailing backlashes
        FilenamesExtended[s] = (wchar_t*)ArenaAllocate(Arena, TotalFileSize*2 + 400);
        uint64_t WriteIndex = 0;
        
        {
            wchar_t tmp[] = L"C:\\";
            tmp[0] = (wchar_t)VolumeLetter;
            memcpy(&FilenamesExtended[s][WriteIndex], &tmp[0], 6);
            WriteIndex += 3;        
        }
        
        
        for(uint32_t i =0; i<si; i++){
            if(stack[si - i - 1] != 5 
               && stack[si - i - 1] != 0 
               && FileNameSize[stack[si - i - 1]] != 0)
            {
                _mm_prefetch((const char*)&FileNameSize[stack[si - i - 1]], _MM_HINT_T0);
                _mm_prefetch((const char*)&NameMap[stack[si - i - 1]], _MM_HINT_T0);
                
                if(!(FileNameSize[stack[si - i - 1]] != 0)){
                    break;
                }
                
                uint64_t FSize = FileNameSize[stack[si - i - 1]] - 1;// remove null termination
                if(FSize == (uint64_t)-1){
                    break;
                }	
                
                {
                    memcpy(&FilenamesExtended[s][WriteIndex], NameMap[stack[si - i - 1]], FSize*2);
                    WriteIndex += FSize;
                    
                    memcpy(&FilenamesExtended[s][WriteIndex], L"\\", 2);
                    WriteIndex += 1;
                }
                
            }
            else{
            }
        }
        
        uint64_t FSize = FileNameSize[IndiceArr[s]] - 1;
        memcpy(&FilenamesExtended[s][WriteIndex], NameMap[IndiceArr[s]], FSize*2);
        WriteIndex += FSize;
        FilenamesExtended[s][WriteIndex] = 0;        
        
    }
    
    //TraverserTotalTime = NarTimeElapsed(TraverserStart);
    
#if 0    
    for(size_t i =0; i<ArrLen; i++){
        //printf("%S\n", FilenamesExtended[i]);
    }
#endif
    
    //printf("Parser, %9u files in %.5f sec, file per ms %.5f\n", TotalFC, ParserTotalTime, (double)TotalFC/ParserTotalTime/1000.0);
    //printf("Traverser %8u files in %.5f sec, file per ms %.5f\n", ArrLen, TraverserTotalTime, (double)ArrLen/TraverserTotalTime/1000.0);
    //printf("Match count %I64u\n", ArrLen);
	
	VirtualFree(ScratchMemory, ScratchArenaSize, MEM_RELEASE);
    Result.Files = FilenamesExtended;
    Result.Len   = ArrLen;
    
    return Result;
}


uint32_t
NarGetFileID(void* FileRecord){
    return *(uint32_t*)NAR_OFFSET(FileRecord, 44);
}

name_and_parent_id
NarGetFileNameAndParentID(void *FileRecord){
    
    name_and_parent_id Result = {0};
    Result.FileID = NarGetFileID(FileRecord);
    
    // doesnt matter if it's posix or normal file name attribute
    void* FNAttribute = NarFindFileAttributeFromFileRecord(FileRecord, 0x30);
    if(FNAttribute){
        // POSIX, skip
        if(*(uint8_t*)NAR_OFFSET(FNAttribute, 89) == 2){
            uint32_t AttLen = *(uint32_t*)NAR_OFFSET(FNAttribute, 4);
            FNAttribute = NAR_OFFSET(FNAttribute, AttLen);
            
            if(*(uint32_t*)FNAttribute != 0x30){
                memset(&Result, 0, sizeof(Result));
                goto bail;
            }
            
        }
        
        Result.Name = (wchar_t*)NAR_OFFSET(FNAttribute, 90);
        Result.NameLen  = *(uint8_t*)NAR_OFFSET(FNAttribute, 88);
        Result.ParentFileID = *(uint32_t*)NAR_OFFSET(FNAttribute, 24);
    }
    else{
        int hodl = 25;
    }
    
    bail:;
    
    return Result;
}

