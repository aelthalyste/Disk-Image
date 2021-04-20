

#include "file_explorer.h"


#define TIMED_BLOCK__(NAME, Number, ...) 
#define TIMED_BLOCK_(NAME, Number, ...)  
#define TIMED_BLOCK(...)                 
#define TIMED_NAMED_BLOCK(NAME, ...)     


inline lcn_from_mft_query_result
ReadLCNFromMFTRecord(void* RecordStart) {
    
    lcn_from_mft_query_result Result = { 0 };
    
    if (RecordStart == NULL) {
        Result.Flags = Result.FAIL;
        return Result;
    }
    
    // NOTE(Batuhan): mft file entries must begin with `FILE` data, but endianness make this `ELIF` 
    if (*(INT32*)RecordStart != 'ELIF') {
        Result.Flags = Result.FAIL;
        return Result;
    }
    
    void* FileRecord = RecordStart;
    INT16 FirstAttributeOffset = *(INT16*)((BYTE*)FileRecord + 20);
    void* FileAttribute = (BYTE*)FileRecord + FirstAttributeOffset;
    
    INT32 RemainingLen = *(INT32*)((BYTE*)FileRecord + 24); // Real size of the file record
    RemainingLen -= (FirstAttributeOffset + 8); //8 byte for end of record mark, remaining len includes it too.
    
    auto InsertToResult = [&](uint32_t Start, uint32_t Len) {
        Result.Records[Result.RecordCount++] = {Start, Len};
    };
    
    FileAttribute = NarFindFileAttributeFromFileRecord(RecordStart, NAR_DATA_FLAG);
    if(FileAttribute != NULL){
        
        UINT8 AttributeNameLen = *((UINT8*)FileAttribute + 9);
        UINT8 AttributeNonResident = *((UINT8*)FileAttribute + 8);
        if(!AttributeNonResident){
            Result.Flags |= Result.HAS_DATA_IN_MFT;
        }
        
        INT32 DataRunsOffset = *(INT32*)((BYTE*)FileAttribute + 32);
        void* DataRuns = (char*)FileAttribute + DataRunsOffset;
        
        char Size = *(BYTE*)DataRuns;
        uint8_t ClusterCountSize = (Size & 0x0F);
        uint8_t FirstClusterSize = (Size >> 4);
        
        INT64 ClusterCount = *(INT64*)((char*)DataRuns + 1);
        ClusterCount = ClusterCount & ~(0xFFFFFFFFFFFFFFFFULL << (ClusterCountSize * 8));
        
        
        INT64 FirstCluster = *(INT64*)((char*)DataRuns + 1 + ClusterCountSize);
        FirstCluster = FirstCluster & ~(0xFFFFFFFFFFFFFFFFULL << (FirstClusterSize * 8));
        
        // i being dumb here probably, but code is ok
        // 2s complement to support negative values
        if ((FirstCluster >> ((FirstClusterSize - 1) * 8 + 7)) & 1U) {
            FirstCluster = FirstCluster | ((0xFFFFFFFFFFFFFFFULL << (FirstClusterSize * 8)));
        }
        
        INT64 OldClusterStart = FirstCluster;
        void* D = (BYTE*)DataRuns + FirstClusterSize + ClusterCountSize + 1;
        
        // safe to convert to 32bit
        InsertToResult((uint32_t)FirstCluster, (uint32_t)ClusterCount);
        
        while (*(BYTE*)D) {
            
            Size = *(BYTE*)D;
            if (Size == 0) break;  
            
            ClusterCountSize = (Size & 0x0F);
            FirstClusterSize = (Size >> 4);
            
            if (ClusterCountSize == 0 || FirstClusterSize == 0)break;
            
            ClusterCount = *(INT64*)((BYTE*)D + 1);
            ClusterCount = ClusterCount & ~(0xFFFFFFFFFFFFFFFFULL << (ClusterCountSize * 8));
            
            FirstCluster = *(INT64*)((BYTE*)D + 1 + ClusterCountSize);
            FirstCluster = FirstCluster & ~(0xFFFFFFFFFFFFFFFFULL << (FirstClusterSize * 8));
            if ((FirstCluster >> ((FirstClusterSize - 1) * 8 + 7)) & 1U) {
                FirstCluster = FirstCluster | (0xFFFFFFFFFFFFFFFFULL << (FirstClusterSize * 8));
            }
            
            
            FirstCluster += OldClusterStart;
            OldClusterStart = FirstCluster;
            
            D = (BYTE*)D + (FirstClusterSize + ClusterCountSize + 1);
            
            InsertToResult((UINT32)FirstCluster, (UINT32)ClusterCount);
        }
        
    }
    
    return Result;
    
}



inline void
NarPushDirectoryStack(nar_backup_file_explorer_context *Ctx, UINT64 ID) {
    Ctx->HistoryStack.S[++Ctx->HistoryStack.I] = ID;
}

inline void
NarPopDirectoryStack(nar_backup_file_explorer_context* Ctx) {
    Ctx->HistoryStack.I--;
    if (Ctx->HistoryStack.I < 0) {
        Ctx->HistoryStack.I = 0;
    }
}


/*
sets file explorer's read pointer to given MFTID's disk offset, so next read call(1024 bytes) will
result reading MFTID'th mft file entry
*/
inline BOOLEAN
NarFileExplorerSetFilePtr2MFTID(nar_backup_file_explorer_context* Ctx, size_t TargetMFTID){
    
    nar_fe_volume_handle FEV = Ctx->FEHandle;
    nar_record* MFTRegions = Ctx->MFTRecords;
    UINT32 MFTRegionCount = Ctx->MFTRecordsCount;
    UINT32 ClusterSize = Ctx->ClusterSize;
    
    UINT64 FileCountInRegion = 0;
    UINT32 FileCountPerCluster = ClusterSize / 1024;
    UINT64 AdvancedFileCountSoFar = 0;
    
    UINT64 FileOffsetFromRegion = 0;
    UINT64 FileOffsetAtVolume = 0;
    
    for (UINT32 RegionIndex = 0; RegionIndex < MFTRegionCount; RegionIndex++) {
        
        FileCountInRegion = (UINT64)MFTRegions[RegionIndex].Len * (UINT64)FileCountPerCluster;
        if (FileCountInRegion + AdvancedFileCountSoFar >= TargetMFTID) {
            // found
            FileOffsetFromRegion = TargetMFTID - AdvancedFileCountSoFar;
            FileOffsetAtVolume = (UINT64)MFTRegions[RegionIndex].StartPos * (UINT64)ClusterSize + FileOffsetFromRegion * 1024;
            break;
        }
        
        AdvancedFileCountSoFar += FileCountInRegion;
        
    }
    
    return NarFileExplorerSetFilePointer(FEV, FileOffsetAtVolume);
    
}


/*
Reads MFTID'th mft entry from given handle H.
 This  function DOES NOT PARSE ANYTHING AT ALL, just queries given MFT file entry from handle, and reads
EXACTLY 1024 bytes

Return: returns FALSE if MFTID CANT be queried from given volume(which is probable in backups, file might have been deleted)
*/
inline BOOLEAN
NarFileExplorerReadMFTID(nar_backup_file_explorer_context *Ctx, size_t MFTID, void *Out){
    
    BOOLEAN Result = FALSE;
    
    if(NarFileExplorerSetFilePtr2MFTID(Ctx, MFTID)){
        DWORD BytesRead = 0;
        
        if(NarFileExplorerReadVolume(Ctx->FEHandle, Out, 1024, &BytesRead) && BytesRead == 1024){
            Result = TRUE;
        }
        else{
            printf("Unable to read volume for MFT ID %I64u\n", MFTID);
        }
        
    }
    else{
        printf("Unable to set file pointer for mft id %I64u\n", MFTID);
    }
    
    return Result;
    
}

/**/
inline UINT64
NarGetFileSizeFromRecord(void *R){
    if(R == 0) return 0;
    void *D = NarFindFileAttributeFromFileRecord(R, NAR_DATA_FLAG);
    return *(UINT64*)NAR_OFFSET(D, 0x30);
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
    
    INT16 FirstAttributeOffset = (*(INT16*)((BYTE*)FileRecord + 20));
    void* FileAttribute = (char*)FileRecord + FirstAttributeOffset;
    
    INT32 RemainingLen = *(INT32*)((BYTE*)FileRecord + 24); // Real size of the file record
    RemainingLen -= (FirstAttributeOffset + 8); //8 byte for end of record mark, remaining len includes it too.
    
    while(RemainingLen > 0){
        
        if(*(INT32*)FileAttribute == AttributeID){
            return FileAttribute;
        }
        
        // it's guarenteed that attributes are sorted by id's in ascending order
        if(*(INT32*)FileAttribute > AttributeID){
            break;
        }
        
        //Just substract attribute size from remaininglen to determine if we should keep iterating
        INT32 AttrSize = (*(unsigned short*)((BYTE*)FileAttribute + 4));
        RemainingLen -= AttrSize;
        FileAttribute = (BYTE*)FileAttribute + AttrSize;
        if (AttrSize == 0) break;
        
    }
    
    return NULL;
}

/*
What it wants as input:
-Attribute list(0x20) attribute data, not an alternate file record that attribute list elements points to
-file explorer context  to move between different MFT entries(attributes in the list may have split across different mft entries)

What it MAY  return:
- Parsed BITMAP indices
- Parsed INDX_ALLOCATION regions
- Parsed INDX_ROOT information ()
*/
inline void
NarResolveAttributeList(nar_backup_file_explorer_context *Ctx, void *Attribute, size_t OriginalRecordID){
    
    
    if(Ctx == 0  || Attribute == 0) return;
    
    TIMED_BLOCK();
    
    char FileEntry[1024];
    BYTE BitmapBuffer[512];
    memset(BitmapBuffer,0, sizeof(BitmapBuffer));
    
    UINT32  Length       = *(UINT32*)NAR_OFFSET(Attribute, 0x04);
    BOOLEAN IsResident   = !(*(BOOLEAN*)NAR_OFFSET(Attribute, 0x08));
    
    UINT16  AttributeID  = *(UINT16*)NAR_OFFSET(Attribute, 0x0E);
    UINT32  AttributeLen = *(UINT32*)NAR_OFFSET(Attribute, 0x10);
    UINT32 FirstAttributeOffset = *(UINT16*)NAR_OFFSET(Attribute, 0x14); 
    
    
    //UINT16 RecordLength = *(UINT16*)NAR_OFFSET(Attribute, 0x04);
    
    // NOTE(Batuhan): zero if resident
    UINT64 StartingVCN  = *(UINT64*)NAR_OFFSET(Attribute, 0x08);
    
    
    /*
    have to find INDX_ROOT, INDX_ALLOCATION, BITMAP from the attribute list and their associated MFT ID's. Then for each of them, I have to parse that specific
    MFTID to extract neccecary data. For INDX_ROOT that is the file entry list, for INDX_ALLOC, that MUST be LCN regions, not file entries because file record might contain BITMAP that might be telling us to invalidate some of the clusters that are extracted directly from INDX_ALLOCATION attribute
    */
    
    Attribute = NAR_OFFSET(Attribute, FirstAttributeOffset);
    
    // NOTE(Batuhan): Assumed that one attribute is not split across different file records
    size_t BitmapMFTID = 0;
    size_t IndxAllocationMFTID = 0;
    size_t IndxRootMFTID = 0;
    
    for(INT32 RemLen = AttributeLen; RemLen > 0;){
        
        UINT32 AttType = *(UINT32*)Attribute;
        if(AttType != NAR_INDEX_ALLOCATION_FLAG && AttType != NAR_INDEX_ROOT_FLAG && AttType != NAR_BITMAP_FLAG){
            goto SKIP;
        }
        
        // mft record number length is 6 bytes, we have to shift 16 bits to 
        // truncate 8 bytes to 6.
        UINT64 MFTID = *(UINT64*)NAR_OFFSET(Attribute, 0x10);
        MFTID = (MFTID << 16);
        MFTID = (MFTID >> 16);
        
        if(AttType == NAR_INDEX_ALLOCATION_FLAG) IndxAllocationMFTID = MFTID;
        if(AttType == NAR_INDEX_ROOT_FLAG)       IndxRootMFTID = MFTID;
        if(AttType == NAR_BITMAP_FLAG)           BitmapMFTID =  MFTID;
        
        SKIP:
        
        UINT16 RecordLen = *(UINT16*)NAR_OFFSET(Attribute, 0x04);
        Attribute = (void*)NAR_OFFSET(Attribute, RecordLen);
        RemLen -= RecordLen;
        
    }
    
    /*
  // NOTE(Batuhan): About BITMAP and INDEX_ALLOCATION attributes
    i'th bit of BITMAP represents if INDEX_ALLOCATION's i'th cluster is in use, so lets say 
  636'th bit of the BITMAP data is not set(zero, 0), that means 636'th cluster of the INDEX_ALLOCATION needs to be removed from region array.
  
  What I assume right now, that unsused cluster id will be trimming point for rest of the region, meaning I can safely iterate next region without worrying if there are used regions after the unused cluster indx and if I have to insert new region for used ones etc etc.
  */
    
    // NOTE(Batuhan): Sane way of handling BITMAP stuff is, first parse INDX_ALLOCATION, then check if BITMAP is present(it usually is if its a hot directory), then remove(trim, as stated above) unused clusters from that region, then parse remaining clusters!
    
    void* IndxData = 0;
    
    INT32 BitmapLen = 0; 
    BYTE* BitmapData = 0;
    
    nar_record *IndxAllRegions = 0;
    uint32_t IndxAllRegionsCount = 0;
    
    if(IndxAllocationMFTID != 0  && IndxAllocationMFTID != OriginalRecordID){
        
        
        if(NarFileExplorerReadMFTID(Ctx, IndxAllocationMFTID, FileEntry)){
            IndxData = NarFindFileAttributeFromFileRecord(FileEntry, NAR_INDEX_ALLOCATION_FLAG);
        }
        else{
            // TODO(Batuhan): log error
        }
        
        
        if(IndxData){
            uint32_t RegionsReserved= 1024;
            IndxAllRegions = (nar_record*)malloc(RegionsReserved*sizeof(nar_record));
            
            if(NarParseIndexAllocationAttribute(IndxData, IndxAllRegions, RegionsReserved, &IndxAllRegionsCount)){
                // NOTE(Batuhan): Success!
            }
            else{
                // TODO(Batuhan): Memory might not have been enough to store regions in attribute, try to send something bigger once again
                // TODO(Batuhan): log?
            }
            
            
            if(BitmapMFTID != 0){
                
                if(BitmapMFTID != IndxAllocationMFTID){
                    NarFileExplorerReadMFTID(Ctx, BitmapMFTID, FileEntry);
                }
                
                void* BitmapAttributeStart = NarFindFileAttributeFromFileRecord(FileEntry, NAR_BITMAP_FLAG);
                BitmapLen = NarGetBitmapAttributeDataLen(BitmapAttributeStart);
                void *BitmapDataTemp = NarGetBitmapAttributeData(BitmapAttributeStart);
                
                // NOTE(Batuhan): unlikely, but have to check against that
                if(BitmapDataTemp != 0){
                    BitmapData = &BitmapBuffer[0];
                    memcpy(BitmapData, BitmapDataTemp, BitmapLen);
                }
                
            }
            
        }
        
        // NOTE(Batuhan): silently updates Ctx, nothing to worry about
        NarGetFileEntriesFromIndxClusters(Ctx, IndxAllRegions, IndxAllRegionsCount, BitmapData, BitmapLen);
        
    }
    
    return;
}


inline void*
NarGetBitmapAttributeData(void *BitmapAttributeStart){
    void *Result = 0;
    if(BitmapAttributeStart == NULL) return Result;
    
    UINT16 Aoffset =  *(UINT16*)NAR_OFFSET(BitmapAttributeStart, 0x14);
    Result = NAR_OFFSET(BitmapAttributeStart, Aoffset);
    
    return Result;
}

inline INT32
NarGetBitmapAttributeDataLen(void *BitmapAttributeStart){
    INT32 Result = 0;
    if(BitmapAttributeStart == NULL) return Result;
    
    Result = *(INT32*)NAR_OFFSET(BitmapAttributeStart, 0x10);
    
    return Result;
}


inline void
NarGetFileEntriesFromIndxClusters(nar_backup_file_explorer_context *Ctx, nar_record *Clusters, INT32 Count, BYTE* Bitmap, INT32 BitmapLen){
    
    if(Ctx == NULL || Clusters == NULL) return;
    
    TIMED_BLOCK();
    
    size_t ParsedClusterIndex = 0;
    size_t ByteIndex = 0;
    size_t BitIndex = 0;
    
    
    /*
  Each bitmap BIT represents availablity of that virtual cluster number of the indx_allocation clusters
  */
    
    for(size_t  _i_ = 0; 
        NarFileExplorerSetFilePointer(Ctx->FEHandle, (UINT64)Clusters[_i_].StartPos*Ctx->ClusterSize) && _i_ < Count;
        _i_++){
        
        size_t IndxBufferSize = (UINT64)Clusters[_i_].Len * Ctx->ClusterSize;
        void* IndxBuffer = malloc(IndxBufferSize);
        DWORD BytesRead = 0;
        
        if(NarFileExplorerReadVolume(Ctx->FEHandle, IndxBuffer, IndxBufferSize, &BytesRead)){
            
            size_t ceil = Clusters[_i_].Len;
            
            for(size_t _j_ =0; _j_ < ceil ; _j_++){
                
#if 1           
                ByteIndex = ParsedClusterIndex / 8;
                BitIndex = ParsedClusterIndex % 8;
                BOOLEAN ShouldParse = TRUE;
                
                if(Bitmap != 0){
                    if(ParsedClusterIndex >= BitmapLen * 8){
                        ShouldParse = FALSE;
                    }
                    else{
                        ShouldParse = (Bitmap[ByteIndex] & (1 << BitIndex));
                    }
                }
                
                if(ShouldParse){
                    void *IC = (char*)IndxBuffer + (UINT64)_j_ * Ctx->ClusterSize;
                    NarParseIndxRegion(IC, &Ctx->EList);
                }
#endif
                
                ParsedClusterIndex++;
            }
        }
        
        free(IndxBuffer);
    }
    
    
}


/*
// TODO(Batuhan): What is raw INDX_ROOT data ? data region of INDX_ROOT attribute or attribute itself?
Answer : attribute itself, which is same thing as data pointer by INDX_ALLOCATION data run
 
Can be called for raw INDX_ROOT data or data pointed by INDX_ALLOCATION, to parse 
file entries they contain
*/
inline void
NarParseIndxRegion(void *Data, nar_file_entries_list *EList){
    
    if(Data == NULL || EList == NULL) return;
    
    TIMED_BLOCK();
    
    Data= (char*)Data + 64;
    
    INT EntryParseResult = NAR_FEXP_END_MARK;
    do {
        
        EntryParseResult = NarFileExplorerGetFileEntryFromData(Data, &EList->Entries[EList->EntryCount]);
        if (EntryParseResult == NAR_FEXP_SUCCEEDED || EntryParseResult == NAR_FEXP_POSIX) {
            
            // do not accept posix files as FILE_ENTRY at all
            if (EntryParseResult == NAR_FEXP_SUCCEEDED) {
                TIMED_NAMED_BLOCK("Extending entry list");
                EList->EntryCount++;
                if(EList->EntryCount == EList->MaxEntryCount){
                    NarExtentFileEntryList(EList, EList->MaxEntryCount * 2);
                }
            }
            
            UINT16 EntrySize = NAR_FEXP_INDX_ENTRY_SIZE(Data);
            Data = (char*)Data + EntrySize;
        }
        
    } while (EntryParseResult != NAR_FEXP_END_MARK);
    
    return;
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
            OutRegions[InternalRegionsFound].StartPos = (uint32_t )(OldClusterStart + (int64_t)FirstCluster);
            OutRegions[InternalRegionsFound].Len = ClusterCount;
            InternalRegionsFound++;
        }
        
        
        if(InternalRegionsFound > MaxRegionLen){
            printf("attribute parser not enough memory[Line : %u]\n", __LINE__);
            goto NOT_ENOUGH_MEMORY;
        }
        
        OldClusterStart  = OldClusterStart + (int64_t)FirstCluster;
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





inline void
NarGetFileListFromMFTID(nar_backup_file_explorer_context *Ctx, size_t TargetMFTID) {
    
#if 1
    nar_file_entries_list* EList  = &Ctx->EList;
    nar_record* MFTRegions        = Ctx->MFTRecords;
    UINT32 MFTRegionCount         = Ctx->MFTRecordsCount;
    UINT32 ClusterSize            = Ctx->ClusterSize; 
    nar_fe_volume_handle FEHandle = Ctx->FEHandle;
#endif
    
    
    BYTE Buffer[1024];
    BYTE BitmapBuffer[512];
    memset(BitmapBuffer, 0, sizeof(BitmapBuffer));
    
    memset(Buffer, 0, sizeof(Buffer));
    
    uint32_t IndexRegionsFound = 0;
    
    nar_record INDX_ALL_REGIONS[64];
    memset(INDX_ALL_REGIONS, 0, sizeof(INDX_ALL_REGIONS));
    DWORD BytesRead = 0;
    
    void* IndxRootAttr = 0;
    void* IndxAllAttr  = 0;
    void* BitmapAttr   = 0; 
    void* AttrListAttr = 0;
    BYTE* BitmapData = 0;
    INT32 BitmapLen = 0;
    
    if (NarFileExplorerReadMFTID(Ctx, TargetMFTID, Buffer)) {
        
        TIMED_NAMED_BLOCK("After reading mftid");
        
        // do parsing stuff
        void* FileRecord = &Buffer[0];
        INT16 FlagOffset = 22;
        
        INT16 Flags = *(INT16*)((BYTE*)FileRecord + FlagOffset);
        INT16 DirectoryFlag = 0x0002;
        
        if((Flags & DirectoryFlag) == DirectoryFlag){
            
            IndxRootAttr = NarFindFileAttributeFromFileRecord(FileRecord, NAR_INDEX_ROOT_FLAG);
            IndxAllAttr  = NarFindFileAttributeFromFileRecord(FileRecord, NAR_INDEX_ALLOCATION_FLAG);
            BitmapAttr   = NarFindFileAttributeFromFileRecord(FileRecord, NAR_BITMAP_FLAG);
            AttrListAttr = NarFindFileAttributeFromFileRecord(FileRecord, NAR_ATTRIBUTE_LIST);
            
            // NOTE(Batuhan): ntfs madness : if file has too many information, it cant fit into the single 1KB entry.
            /*
      so some smart ass decided it would be wise to split some attributes into different disk locations, so i have to check if any file attribute is non-resident, if so, should check if that one is INDEX_ALLOCATION attribute, and again if so, should go read that disk offset and parse it. pretty trivial but also boring
      */
            // NOTE(Batuhan): Not sure If a record can contain both attribute list and indx_allocation stuff
            NarResolveAttributeList(Ctx, AttrListAttr, TargetMFTID);
            
            uint32_t OutLen = 0;
            // NOTE(Batuhan): INDEX_ALLOCATION handling
            if(NarParseIndexAllocationAttribute(IndxAllAttr, &INDX_ALL_REGIONS[IndexRegionsFound], 64 - IndexRegionsFound, &OutLen)){
                // NOTE(Batuhan): successfully parsed indx allocation
                IndexRegionsFound += OutLen;
            }
            else{
                // TODO(Batuhan): error log ?
            }
            
            // NOTE(Batuhan): INDEX_ROOT handling
            NarParseIndxRegion(IndxRootAttr, EList);
            
            
            // TODO(Batuhan): BITMAP handling
            {
                if(BitmapAttr){
                    BitmapLen = NarGetBitmapAttributeDataLen(BitmapAttr);
                    BYTE* temp = (BYTE*)NarGetBitmapAttributeData(BitmapAttr);
                    BitmapData = &BitmapBuffer[0];
                    memcpy(BitmapData, temp, BitmapLen);
                }
            }
            
            
        }
        
        /*
    $ROOT file has very very very unique special case when parsing a normal file record, it turns out
    i can't use general file record parsing function just for that very special case, so here, _almost_ same version of NarGetFileEntriesFromIndxClusters
    */
        
        if(TargetMFTID == 5){
            
            size_t ParsedClusterIndex = 0;
            size_t ByteIndex = 0;
            size_t BitIndex = 0;
            
            for(size_t _i_ = 0; 
                NarFileExplorerSetFilePointer(Ctx->FEHandle, (UINT64)INDX_ALL_REGIONS[_i_].StartPos*Ctx->ClusterSize) && _i_ < IndexRegionsFound;
                _i_++){
                
                size_t IndxBufferSize = (UINT64)INDX_ALL_REGIONS[_i_].Len * Ctx->ClusterSize;
                void* IndxBuffer = malloc(IndxBufferSize);
                DWORD BytesRead = 0;
                
                if(NarFileExplorerReadVolume(Ctx->FEHandle, IndxBuffer, IndxBufferSize, &BytesRead)){
                    for(size_t _j_ =0; _j_ <  INDX_ALL_REGIONS[_i_].Len; _j_++){
                        
                        ByteIndex = ParsedClusterIndex / 8;
                        BitIndex = ParsedClusterIndex % 8;
                        BOOLEAN ShouldParse = TRUE;
                        
                        if(BitmapData != 0){
                            if(ParsedClusterIndex >= BitmapLen * 8){
                                ShouldParse = FALSE;
                            }
                            else{
                                ShouldParse = (BitmapData[ByteIndex] & (1 << BitIndex));
                            }
                        }
                        
                        if(ShouldParse){
                            void *IC = (char*)IndxBuffer + (UINT64)_j_ * Ctx->ClusterSize;
                            if(TargetMFTID == 5 && _i_ == 0 && _j_ == 0) IC = (char*)IC + 24;
                            NarParseIndxRegion(IC, EList);
                        }
                        
                        ParsedClusterIndex++;
                        
                    }
                }
                
                free(IndxBuffer);
            }
            
        }
        else{
            NarGetFileEntriesFromIndxClusters(Ctx, INDX_ALL_REGIONS, IndexRegionsFound, BitmapData, BitmapLen);
        }
        
        
    }
    else {
        // couldnt read file entry
    }
    
}


/*
Be careful about return value, its not a fail-pass thing
    Parses INDEX_ALLOCATION data, not actual MFT file data
*/
inline INT
NarFileExplorerGetFileEntryFromData(void *IndxCluster, nar_file_entry *OutFileEntry){
    
    
    TIMED_BLOCK();
    
    memset(OutFileEntry, 0, sizeof(*OutFileEntry));
    
    
    UINT16 EntrySize = *(UINT16*)((char*)IndxCluster + 8);
    UINT16 AttrSize = *(UINT16*)((char*)IndxCluster + 10);
    if (EntrySize == 16 || EntrySize == 24 || AttrSize == 0) {
        return NAR_FEXP_END_MARK;
    }
    
    // check if POSIX entry
    if (*((char*)IndxCluster + NAR_POSIX_OFFSET) == NAR_POSIX) {
        return NAR_FEXP_POSIX;
    }
    
    UINT8 NameLen = *(UINT8*)((char*)IndxCluster + NAR_NAME_LEN_OFFSET);
    UINT64 MagicNumber = *(UINT64*)IndxCluster;
    MagicNumber = MagicNumber & (0x0000FFFFFFFFFFFF);
    
    // DO MASK AND TRUNCATE 2 BYTES TO FIND MFTINDEX OF THE FILE
    
    // useful information here
    UINT64 CreationTime = *(UINT64*)((char*)IndxCluster + NAR_TIME_OFFSET);
    UINT64 ModificationTime = *(UINT64*)((char*)IndxCluster + NAR_TIME_OFFSET + 8);
    UINT64 FileSize = *(UINT64*)((char*)IndxCluster + NAR_SIZE_OFFSET);
    UINT32 Attributes = *(UINT32*)((char*)IndxCluster + NAR_ATTRIBUTE_OFFSET);
    
    char* NamePtr = ((char*)IndxCluster + NAR_NAME_OFFSET);
    if (*NamePtr == 0) {
        // that shouldnt happen
    }
    
    memcpy(&OutFileEntry->Name[0], NamePtr, NameLen * sizeof(wchar_t));
    
    // push stuff
    OutFileEntry->MFTFileIndex = MagicNumber;
    OutFileEntry->Size = FileSize;
    OutFileEntry->CreationTime = CreationTime;
    OutFileEntry->LastModifiedTime = ModificationTime;
    
    OutFileEntry->IsDirectory = FALSE;
    
    // from observation in hex editor. hope we wont miss anything
    if (Attributes >= 0x10000000) {
        OutFileEntry->IsDirectory = TRUE;
    }
    
    return NAR_FEXP_SUCCEEDED;
    
    
}


inline void 
NarFileExplorerPushDirectory(nar_backup_file_explorer_context* Ctx, UINT32 SelectedListID) {
    
    TIMED_BLOCK();
    
    if (!Ctx) {
        printf("Context was null\n");
        return;
    }
    
    if (SelectedListID > Ctx->EList.EntryCount) {
        printf("ID exceeds size\n");
        return;
    }
    if (Ctx->EList.Entries != 0 && !Ctx->EList.Entries[SelectedListID].IsDirectory) {
        printf("Not a directory\n");
        return;
    }
    
    UINT64 NewMFTID = Ctx->EList.Entries[SelectedListID].MFTFileIndex;
    
    Ctx->EList.MFTIndex = NewMFTID;
    wcscat(Ctx->CurrentDirectory, Ctx->EList.Entries[SelectedListID].Name);
    wcscat(Ctx->CurrentDirectory, L"\\");
    NarPushDirectoryStack(Ctx, NewMFTID);
    
    Ctx->EList.EntryCount = 0;
    
    NarGetFileListFromMFTID(Ctx, NewMFTID);
    
    
    
}

inline void
NarFileExplorerPopDirectory(nar_backup_file_explorer_context* Ctx) {
    
    NarPopDirectoryStack(Ctx);
    
    Ctx->EList.EntryCount = 0;
    Ctx->EList.MFTIndex = Ctx->HistoryStack.S[Ctx->HistoryStack.I];
    NarGetFileListFromMFTID(Ctx, Ctx->HistoryStack.S[Ctx->HistoryStack.I]);
    
    INT32 slen = (INT32)wcslen(Ctx->CurrentDirectory);
    INT32 CutPoint = slen - 1;
    for(int i = slen - 2; i >= 0; i--){
        if(Ctx->CurrentDirectory[i] == L'\\'){
            CutPoint = i;
            break;
        }
    }
    
    Ctx->CurrentDirectory[CutPoint + 1] = L'\0';
    
}

inline void
NarFileExplorerPrint(nar_backup_file_explorer_context* Ctx) {
    if (!Ctx) return;
    if (Ctx->EList.Entries == 0) return;
    
    for (UINT32 i = 0; i < Ctx->EList.EntryCount; i++) {
        printf("%i\t-> ", i);
        if (Ctx->EList.Entries[i].IsDirectory) {
            printf("DIR: %-50S\ %I64u KB\n", Ctx->EList.Entries[i].Name, Ctx->EList.Entries[i].Size / 1024);
        }
        else {
            printf("%-50S %I64u KB\n", Ctx->EList.Entries[i].Name, Ctx->EList.Entries[i].Size / 1024);
        }
    }
    
}


/*
       Caller MUST CALL NarFreeMFTRegionsByCommandLine to free memory allocated by this function, otherwise, it will be leaked
*/
nar_record*
NarGetMFTRegionsByCommandLine(char Letter, unsigned int* OutRecordCount){
    
    char CmdBuffer[512];
    char OutputName[] = "naroutput.txt";
    
    snprintf(CmdBuffer, sizeof(CmdBuffer), "fsutil file queryextents %c:\\$MFT > %s", Letter, OutputName);
    system(CmdBuffer);
    
    file_read FileContents = NarReadFile(OutputName);
    
    char* str = (char*)FileContents.Data;
    
    if (str == NULL || OutRecordCount == NULL) return 0;
    
    
    size_t ResultSize = 128 * sizeof(nar_record);
    nar_record* Result = (nar_record*)malloc(ResultSize);
    memset(Result, 0, ResultSize);
    
    unsigned int RecordFound = 0;
    
    char *prev = str;
    char *next = str;
    
    char *end = (char*)str + FileContents.Len;
    
    char ClustersHexStr[128];
    char LCNHexStr[128];
    char Line[128];
    memset(ClustersHexStr, 0, sizeof(ClustersHexStr));
    memset(LCNHexStr, 0, sizeof(LCNHexStr));
    memset(Line, 0, sizeof(Line));
    
    while(TRUE){
        
        while(next < end && *next != '\n' && *next != -3){
            next++;
        }
        
        if(next == end) 
            break;
        
        if(next != prev){
            
            // some lines might be ending with /r/n depending on if file is created as text file in windows. delete /r to make parsing easier 
            int StrLen = (int)(next - prev); // acceptable conversion
            
            if (*(next - 1) == '\r') StrLen--;
            
            memcpy(Line, prev, StrLen);
            Line[StrLen] = 0; // null termination by hand
            
            
            // there isnt any overheat to search all of them at once, we are dealing with very short strings here
            char *p1 = strstr(Line, "VCN:");
            char *p2 = strstr(Line, "Clusters:");
            char *p3 = strstr(Line, "LCN:");
            
            
            if (p1 && p2 && p3) {
                
                p2 += sizeof("Clusters:");
                size_t LCNHexStrLen = (size_t)(p3 - p2) + 1;
                memcpy(LCNHexStr, p2, LCNHexStrLen);
                LCNHexStr[LCNHexStrLen] = 0;
                
                p3 += sizeof("LCN:");
                size_t ClustersHexStrLen = strlen(p3);
                memcpy(ClustersHexStr, p3, ClustersHexStrLen);
                ClustersHexStr[ClustersHexStrLen] = 0;
                
                long LCNStart = strtol(ClustersHexStr, 0, 16);
                long LCNLen = strtol(LCNHexStr, 0, 16);
                
                Result[RecordFound].StartPos = LCNStart;
                Result[RecordFound].Len = LCNLen;
                RecordFound++;
                
                // valid line
            }
            else {
                // invalid line
            }
            
            prev = (++next);
            
            
        }
        else{
            break;
        }
        
    }
    
    *OutRecordCount = RecordFound;
    
    Result = (nar_record*)realloc(Result, RecordFound * sizeof(nar_record));
    FreeFileRead(FileContents);
    
    return Result;
    
}

inline void 
NarFreeMFTRegionsByCommandLine(nar_record *records){
    free(records);
}

inline void
NarReleaseFileExplorerContext(nar_backup_file_explorer_context* Ctx) {
    
    if (!Ctx) return; 
    
    
    if(Ctx->HandleOption == NAR_FE_HAND_OPT_READ_BACKUP_VOLUME){
        free(Ctx->MFTRecords);
    }
    else{
        NarFreeMFTRegionsByCommandLine(Ctx->MFTRecords);
    }
    
    NarFreeFEVolumeHandle(Ctx->FEHandle);
    NarFreeFileEntryList(&Ctx->EList);
    
    
}



inline BOOLEAN
NarInitFileExplorerContextFromVolume(nar_backup_file_explorer_context *Ctx, char Letter){
    
    ASSERT(0);
    return FALSE;
#if 0
    if (!Ctx) goto NAR_FAIL_TERMINATE;
    
    memset(Ctx, 0, sizeof(*Ctx));
    
    Ctx->HandleOption = NAR_FE_HAND_OPT_READ_BACKUP_VOLUME;
    if (NarInitFileEntryList(&Ctx->EList, 10000)) {
        
        memset(Ctx->CurrentDirectory, 0, sizeof(Ctx->CurrentDirectory));
        memset(Ctx->RootDir, 0, sizeof(Ctx->RootDir));
        memset(&Ctx->HistoryStack, 0, sizeof(Ctx->HistoryStack));
        
        // NarInitFEVolumeHandleFromVolume(&Ctx->FEHandle, Letter))
        if (false) {
            
            BOOLEAN Status = FALSE;
            Ctx->Letter = Letter;
            
            Ctx->ClusterSize = NarGetVolumeClusterSize(Letter);
            Ctx->MFTRecords = (nar_record*)NarGetMFTRegionsByCommandLine(Letter, &Ctx->MFTRecordsCount);
            Status = TRUE;
            
            Ctx->EList.EntryCount = 0;
            Ctx->EList.MFTIndex = NAR_ROOT_MFT_ID;
            Ctx->HistoryStack.I = -1;
            
            NarGetFileListFromMFTID(Ctx, NAR_ROOT_MFT_ID);
            NarPushDirectoryStack(Ctx, NAR_ROOT_MFT_ID);
            
            wchar_t vb[] = L"!:\\";
            vb[0] = (wchar_t)Ctx->Letter;
            wcscat(Ctx->CurrentDirectory, vb);
            
        }
        else {
            goto NAR_FAIL_TERMINATE;
        }
    }
    else {
        goto NAR_FAIL_TERMINATE;
    }
    
    return TRUE;
    
    NAR_FAIL_TERMINATE:
    if(Ctx){
        NarReleaseFileExplorerContext(Ctx);
    }
    
    return FALSE;
#endif
    
}


/*
    Ctx = output
    HandleOptions
        NAR_READ_MOUNTED_VOLUME = Reads mounted local disk VolumeLetter and ignores rest of the parameters, and makes FEV->BMEX NULL to. this will lead FEV to become normal volume handle
        NAR_READ_BACKUP_VOLUME 2 = Tries to find backup files in RootDir, if one not given, searches current running directory.
                                    Then according to backup region information, handles read-seak operations in wrapped function
    
*/
inline BOOLEAN
NarInitFileExplorerContext(nar_backup_file_explorer_context* Ctx, const wchar_t *RootDir, const wchar_t *MetadataName) {    
    ASSERT(0);
    return FALSE;
}


/*
    *Args: FEV = file explorer handle that has initialized via NarInitFEVolumeHandle
         *NewFilePointer = Target file pointer to be set in volume, not in actual binary file. So if handle has been initialized as *READ_BACKUP_DATA,
        *function figures out how to set a file pointer in a way that if caller requests readfile, it acts like if it were actually reading from *that offset.
        *what caller has to be careful is, there is no guarentee that reading N bytes from that position will give caller sequential data. It is * guarenteed there
        * are at least one cluster sized data resides at that file offset, but due to nature of the backup system, next clusters may not exist in * that particular 
        * file.
*/
inline BOOLEAN
NarFileExplorerSetFilePointer(nar_fe_volume_handle FEV, UINT64 NewFilePointer){
    
    ASSERT(0);
    BOOLEAN Result = FALSE;
    
#if 0
    
    if(FEV.VolumeHandle != INVALID_HANDLE_VALUE){
        if(FEV.BMEX == NULL){
            // normal file handle, safe to call NarSetFilePointer
            Result = NarSetFilePointer(FEV.VolumeHandle, NewFilePointer);
        }
        else{
            
            UINT64 NewFilePtrInCluster = NewFilePointer / FEV.BMEX->M.ClusterSize;
            UINT64 ClusterOffset =  NewFilePointer % FEV.BMEX->M.ClusterSize;
            printf("Converted new file pointer to cluster space, new cluster %I64u, addition in bytes %I64u",NewFilePtrInCluster, ClusterOffset);
            
            // backup file handle, must calculate relative offset.
            UINT64 RelativePointerInCluster = FindPointOffsetInRecords(FEV.BMEX->RegionsMetadata.Data, FEV.BMEX->RegionsMetadata.Count, NewFilePtrInCluster);
            if(RelativePointerInCluster != NAR_POINT_OFFSET_FAILED){
                
                printf("Converted absolute file ptr %I64u to relative file cluster pointer %I64d, with addition of %I64u bytes\n", NewFilePointer, RelativePointerInCluster, ClusterOffset);
                UINT64 RelativeFilePointerTarget = RelativePointerInCluster * FEV.BMEX->M.ClusterSize + ClusterOffset;
                
                Result = NarSetFilePointer(FEV.VolumeHandle, RelativeFilePointerTarget);                
                if(!Result){
                    printf("Couldnt set relative file pointer %I64d\n", RelativeFilePointerTarget);
                }
                else{
                    Result = TRUE;
                }
                
            }
            else{
                printf("Failed to move absolute file ptr(%I64u)\n", NewFilePointer);
                // failed
            }
            
        }
    }
    else{
        printf("Volume handle was invalid\n");
    }
#endif
    return Result;
}

inline BOOLEAN
NarFileExplorerReadVolume(nar_fe_volume_handle FEV, void* Buffer, DWORD ReadSize, DWORD* OutBytesRead){
    
    // so, since we are handling INDEX_ALLOCATION regions, all file requests should be valid, but i worry about that
    // mft may throw up some special regions for weird directories
    // for now, ill accept as everything will be fine, if any error occurs in file explorer, i might need to implement some safe procedure
    // to be sure that read length doesnt exceed current region
    
    return ReadFile(FEV.VolumeHandle, Buffer, ReadSize, OutBytesRead, 0);
}


inline BOOLEAN
NarInitFEVolumeHandleFromVolume(nar_fe_volume_handle *FEV, char VolumeLetter){
    return FALSE;
}


inline void 
NarFreeFEVolumeHandle(nar_fe_volume_handle FEV) {
    CloseHandle(FEV.VolumeHandle);
    FEV.VolumeHandle = INVALID_HANDLE_VALUE;
}



/*
    Searches file FileName, in backup versions, which must exist in given RootDir. If RootDir is NULL it searches in
    where program runs, starts from StartingVersion and lowers it until either file is not found or fullbackup id is reached.

    That information is useful for pre restore operation. After that lookup we will be
*/
nar_file_version_stack
NarSearchFileInVersions(const wchar_t *RootDir, nar_backup_id ID, INT32 CeilVersion, const wchar_t *FileName){
    
    nar_file_version_stack Result;
    memset(&Result, 0, sizeof(Result));
    
    if(FileName == NULL || RootDir == NULL){
        return Result;
    }
    
    INT32 StartingVersion = CeilVersion;
    
    INT32 VersStackID = 0;
    for(int i = CeilVersion; i >= NAR_FULLBACKUP_VERSION; i--){
        
        if(i < NAR_FULLBACKUP_VERSION){
            printf("Version ID cant be lower than NAR_FULLBACKUP_VERSION(RootDir: %S, FileName: %S, Volume letter: %c, Selected version %i)\n", RootDir, FileName, ID.Letter, CeilVersion);
            break;
        }
        
        nar_fe_search_result SearchResult = NarSearchFileInVolume(RootDir, FileName, ID, i);
        if(SearchResult.Found == TRUE){
            Result.FileAbsoluteMFTOffset[Result.VersionsFound] = SearchResult.AbsoluteMFTRecordOffset;
            Result.StartingVersion = i;
            Result.VersionsFound++;
        }
        
    }
    
    return Result;
    
}



/*
    Search's for specific file named FileName in given Handle. returns negative value if fails to do
*/
inline nar_fe_search_result
NarSearchFileInVolume(const wchar_t* arg_RootDir, const wchar_t *arg_FileName, nar_backup_id ID, INT32 Version){
    
    TIMED_BLOCK();
    
    wchar_t FileName[512];
    wchar_t RootDir[512];
    wcscpy(FileName, arg_FileName);
    wcscpy(RootDir, arg_RootDir);
    
    nar_fe_search_result Result = {0};    
    nar_backup_file_explorer_context Ctx = {0};
    
    // TODO(Batuhan): check if that actually equals to error state
    if(FileName == NULL) return Result;;
    
    std::wstring __t;
    GenerateMetadataName(ID, Version, __t);
    
    if(NarInitFileExplorerContext(&Ctx,  RootDir, __t.c_str())){
        
        TIMED_NAMED_BLOCK("File search by name(wcstok)");
        
        wchar_t *Token = 0;        
        wchar_t *TokenStatus = 0;
        Token = wcstok(FileName, L"\\", &TokenStatus);
        
        // skip first token since first one will always be volume name skip it.
        Token = wcstok(0, L"\\", &TokenStatus);
        
        INT32 FoundFileListID = -1;
        
        while(Token != NULL){
            
            BOOLEAN FileFound = FALSE;
            // search for file named Token
            
            INT32 EntryCount = Ctx.EList.EntryCount;
            
            for(int i =0; i< EntryCount; i++){
                
                TIMED_NAMED_BLOCK("File entry iteration");
                if(wcscmp(Ctx.EList.Entries[i].Name, Token) == 0){
                    FileFound = TRUE;
                    FoundFileListID = i;
                    break;
                }
                
            }
            
            if(!FileFound) {
                FoundFileListID = -1;
                break;
            }
            
            NarFileExplorerPushDirectory(&Ctx, FoundFileListID);
            
            Token = wcstok(0, L"\\", &TokenStatus);
        }
        
        
        // NOTE(Batuhan): Found the file
        if(FoundFileListID >= 0){
            
            Result.Found = TRUE;
            UINT64 FileMFTID = Ctx.EList.Entries[FoundFileListID].MFTFileIndex;
            
            // NOTE(Batuhan): Find absolute offset from mft records, then use FindPointOffsetInRecords to convert that abs offset to relative one
            INT64 MFTAbsolutePosition = -1;
            
            INT32 FileRecordPerCluster = Ctx.ClusterSize / 1024;
            INT64 AdvancedFileCountSoFar = 0;
            
            for(size_t i = 0; i<Ctx.MFTRecordsCount; i++){
                
                uint64_t FileCountInRegion = (uint64_t)FileRecordPerCluster * (uint64_t)Ctx.MFTRecords[i].Len;
                if(AdvancedFileCountSoFar + FileCountInRegion > FileMFTID){
                    
                    INT64 RemainingFileCountInRegion = FileMFTID - AdvancedFileCountSoFar;
                    
                    INT64 FileOffsetFromRegion = (INT64)Ctx.MFTRecords[i].StartPos * (INT64)FileRecordPerCluster + (INT64)RemainingFileCountInRegion;
                    FileOffsetFromRegion = FileOffsetFromRegion * 1024;
                    MFTAbsolutePosition = FileOffsetFromRegion;
                    break;
                    
                }
                else{
                    AdvancedFileCountSoFar += FileCountInRegion;
                    // continue iterating
                }
                
                
                
            }
            
            // I have to refactor file explorer code
            ASSERT(0);
            
            if(0){
                // 
                // succ found absolute position from mft 
                
                Result.AbsoluteMFTRecordOffset = MFTAbsolutePosition;
                Result.Found = TRUE;
                Result.FileMFTID = FileMFTID;
                
            }
            else{
                printf("NARFE Handle's context was null, early terminating(File: %S, RootDir: %S, VolumeLetter: %c, Version: %i)\n", FileName, RootDir, ID.Letter, Version);
            }
            
            
        }
        else{
            printf("Failed to find file %S in in volume %c in version %i in root dir %S\n", FileName, ID.Letter, Version, RootDir);
        }
        
        
    }
    else{
        printf("Couldnt initialize file explorer context for volume %c for version %i to restore file %S from root directory %S\n", ID.Letter, Version, FileName, RootDir);
    }
    
    
    NarReleaseFileExplorerContext(&Ctx);
    
    return Result;
    
}



/*
    Args: 
    RootDir: Path to backups to be searched
    FileName: File name to be searched in backup volumes, MUST BE FULL PATH LIKE C:\\PROGRAM FILES\\SOME PROGRAM\\RANDOM.EXE
    RestoreTo: Where to restore file, optional, if NULL, file will be restored to current directory


    Notes: This procedure automatically detects backup type and restores file accordingly. If backup type is == DIFFERENTIAL, only information we need is FIRST EXISTING VERSION + LATEST VERSION.
    However if type == INCREMENTAL, we MUST restore each file from FIRST EXISTING VERSION to LATEST BACKUP(which is Version passed by that function)

    For each version to be iterated, read MFT record of the file via FileStack, detect file LCN's, then read existing regions from backup(since backup may not contain every region on volume).
    Then convert these LCN's to VCN's, write it to actual file that exists in file system.

*/
inline BOOLEAN
NarRestoreFileFromBackups(const wchar_t *RootDir, const wchar_t *FileName, const wchar_t *RestoreTo, nar_backup_id ID, INT32 InVersion){
    
    ASSERT(0);
    return FALSE;
    
#if 0    
    if(RootDir == NULL || FileName == NULL) return FALSE;
    
    BOOLEAN Result = FALSE;
    HANDLE RestoreFileHandle = INVALID_HANDLE_VALUE;
    
    {
        wchar_t fn[512];
        memset(fn, 0, sizeof(fn));
        wcscat(fn, RestoreTo);
        size_t pl = wcslen(fn);
        NarGetFileNameFromPath(FileName, &fn[pl], 512 - pl);
        
        printf("File(%S)'s target restore path is %S\n", FileName, fn);
        RestoreFileHandle = CreateFileW(fn, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
        if(RestoreFileHandle == INVALID_HANDLE_VALUE){
            printf("Coulnd't create target restore path %S\n", fn);
            return FALSE;
        }
    }
    
    void* MemoryBuffer = malloc(1024);
    UINT64 RealFileSize = 0;
    
    // NOTE(Batuhan): search file in versions from InVersion to FULL
    nar_file_version_stack FileStack = NarSearchFileInVersions(RootDir, ID, InVersion, FileName);
    if(FileStack.VersionsFound < 0){
        printf("Failed to create file history stack for file %S for volume %c based on version %i\n", FileName, ID.Letter, InVersion);
        goto ABORT;
    }
    
    
    INT32 VersionUpTo = FileStack.StartingVersion + FileStack.VersionsFound;
    nar_fe_volume_handle FEHandle = {0};
    
    // NOTE(Batuhan): for each version, restore a part of the file that exists in that particular backup, skip version if none of the regions 
    // exists in the region. Be careful regions that are not existing in backup doesnt mean error, rather means file hasnt changed in that backup.
    for (int VersionID = FileStack.StartingVersion;
         VersionID < VersionUpTo;  
         VersionID++) {
        
        FEHandle = {0};
        
        std::wstring metadatapath = GenerateMetadataName(ID, VersionID);
        
        if (NarInitFEVolumeHandleFromBackup(&FEHandle, RootDir, GenerateMetadataName(ID, VersionID).c_str())) {
            
            NarFileExplorerSetFilePointer(FEHandle, FileStack.FileAbsoluteMFTOffset[VersionID - FileStack.StartingVersion]);
            memset(MemoryBuffer, 0, 1024);
            
            DWORD BytesRead = 0;
            if (!NarFileExplorerReadVolume(FEHandle, MemoryBuffer, 1024, &BytesRead) || BytesRead != 1024) {
                printf("Couldn't read mft record of the file %S from volume %c's %i'th backup. Aborting file restore opeartion!\n", FileName, ID.Letter, VersionID);
                goto ABORT;
            }
            
            // NOTE(Batuhan): What I copy is allocated regions of the file, but user cares about real size of the region
            {
                RealFileSize = NarGetFileSizeFromRecord(MemoryBuffer);
            }
            
            printf("Successfully read file %S's mft record from volume %c's %i'th version\n", FileName, ID.Letter, VersionID);
            
            lcn_from_mft_query_result QResult = ReadLCNFromMFTRecord(MemoryBuffer);
            if(QResult.Flags & QResult.FAIL){
                printf("Couldnt parse file %S's mft file record at volume %c's version %i", FileName, ID.Letter, VersionID);
                goto ABORT;
            }
            
            INT32 ISectionCount = 0;
            nar_record* IntersectionRegions = 0;
            NarGetRegionIntersection(FEHandle.BMEX->RegionsMetadata.Data, QResult.Records, &IntersectionRegions, FEHandle.BMEX->RegionsMetadata.Count, QResult.RecordCount, &ISectionCount);
            
            // IMPORTANT TODO(Batuhan): special code for data in 
            // copy intersections
            if(QResult.DataLen != 0){
                printf("Found resident data in file, offset %i, size %i\n", QResult.DataOffset, QResult.DataLen);
                
                DWORD BytesWritten = 0;
                if(WriteFile(RestoreFileHandle, (char*)MemoryBuffer + QResult.DataOffset, QResult.DataLen, &BytesWritten, 0) && BytesWritten == QResult.DataLen){
                    
                }
                else{
                    printf("Unable to write resident file data, bytes written %l\n", BytesWritten);
                    DisplayError(GetLastError());
                }
            }
            
            
            if(ISectionCount != 0){
                
                // NOTE(Batuhan): copy intersections of backup regions and file regions
                for (INT32 RegIndex = 0; RegIndex < ISectionCount; RegIndex++) {
                    
                    UINT64 CopyLen   = (UINT64)IntersectionRegions[RegIndex].Len      * FEHandle.BMEX->M.ClusterSize;
                    UINT64 CopyStart = (UINT64)IntersectionRegions[RegIndex].StartPos * FEHandle.BMEX->M.ClusterSize;
                    
                    if (NarFileExplorerSetFilePointer(FEHandle, CopyStart)) {
                        
                        if (CopyData(FEHandle.VolumeHandle, RestoreFileHandle, CopyLen) == FALSE) {
                            printf("Failed to copy file %S's %I64u bytes from volume %c's version %i at offset %I64u\n", FileName, CopyLen, ID.Letter, VersionID, CopyStart);
                        }
                        
                    }
                    else {
                        // TODO log
                    }
                    
                }
                
            }
            else{
                printf("Couldn't find any regions that intersects with file's regions in volume %c version %i, for file %S, skipping this version\n",ID.Letter, VersionID, FileName);
            }
            
            NarFreeRegionIntersection(IntersectionRegions);
            
        }
        else{
            printf("Couldn't initialize volume %c's handle to restore file %S from version %i. File's integrity might be broken, thus resulting corrupt file. Aborting restore!\n", ID.Letter, FileName, VersionID);
            goto ABORT;
        }
        
        
    }
    
    
    
    Result = TRUE;
    
    ABORT:
    
    // NOTE(Batuhan): truncate file to real size
    NarSetFilePointer(RestoreFileHandle, RealFileSize);
    SetEndOfFile(RestoreFileHandle);
    CloseHandle(RestoreFileHandle);
    
    free(MemoryBuffer);
    NarFreeFEVolumeHandle(FEHandle);
    static_assert(false, "I shouldn't be compiling!");
    return Result;
    
#endif
    
}


