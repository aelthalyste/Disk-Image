#include "nar_extension_searcher.hpp"
#include <Windows.h>
#include "nar_compression.hpp"


#ifndef MIN
#define MIN(a,b)   ( (a) < (b) ? (a) : (b) )
#endif

#ifndef MAX
#define MAX(a,b)   ( (a) > (b) ? (a) : (b) )
#endif

#ifndef Megabyte
#define Megabyte(x) ((x) * 1024ull * 1024ull)
#endif

#ifndef Gigabyte
#define Gigabyte(x) (Megabyte(x) * 1024ull)
#endif

#ifndef assert
#include <assert.h>
#endif

void NarFreeExtensionFinderMemory(extension_finder_memory *Memory){
    VirtualFree(Memory->Arena.Memory, 0, MEM_RELEASE);
    NarFreeLinearAllocator(&Memory->StringAllocator);
    memset(Memory, 0, sizeof(*Memory));
}

extension_finder_memory NarSetupExtensionFinderMemory(HANDLE VolumeHandle){
    extension_finder_memory Result = {0};
    
    uint32_t MFTRecordsCapacity = 256;
    uint64_t TotalFC            = 0;
    nar_fs_region* MFTRecords      = (nar_fs_region*)calloc(1024, sizeof(nar_fs_region));
    
    uint64_t FileBufferSize        = Megabyte(16);
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
        
        Result.MFTRecords     = (nar_fs_region*)ArenaAllocate(&Result.Arena, MFTRecordsCount*sizeof(nar_fs_region));
        Result.MFTRecordCount = MFTRecordsCount;
        memcpy(Result.MFTRecords, MFTRecords, sizeof(nar_fs_region)*MFTRecordsCount);
        
        Result.DirMappingMemory = ArenaAllocate(&Result.Arena, TotalFC*sizeof(name_pid));
        Result.PIDArrMemory = ArenaAllocate(&Result.Arena, TotalFC*sizeof(name_pid));
        
        assert(Result.PIDArrMemory);
        assert(Result.DirMappingMemory);
        assert(Result.MFTRecords);
        assert(Result.FileBuffer);
        
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

extension_search_result NarFindExtensions(char VolumeLetter, HANDLE VolumeHandle, wchar_t **ExtensionList, size_t ExtensionListCount, extension_finder_memory *Memory) {
    
    extension_search_result Result = {0};
    
    uint64_t TotalNumberOfSizeUsedForDataRuns = 0;
    uint64_t TotalNumberOfFilesUsingDataRuns = 0;    
    uint64_t DataRunBufferCap = 150 * 1024 * 1024;

    uint8_t *DataRunBuffer = (uint8_t *)malloc(DataRunBufferCap);


    name_pid *DirectoryMapping  = (name_pid*)Memory->DirMappingMemory; 
    name_pid *PIDResultArr      = (name_pid*)Memory->PIDArrMemory; 
    uint64_t ArrLen             = 0;
    uint64_t FileRecordSize     = 1024; 
    double ParserTotalTime    = 0;
    double TraverserTotalTime = 0;
    uint64_t ParserLoopCount  = 0;
    DWORD BR = 0;
    
    uint64_t ClusterSize     = NarGetVolumeClusterSize(VolumeLetter);
    size_t *ExtensionLenList = (size_t*)ArenaAllocateZero(&Memory->Arena, ExtensionListCount * sizeof(size_t));

    for (uint64_t i = 0; i < ExtensionListCount; i++) {
        ExtensionLenList[i] = wcslen(ExtensionList[i]);
    }
    
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
                
                auto ReadFileResult = ReadFile(VolumeHandle, 
                         Memory->FileBuffer, 
                         (DWORD)(TargetFileCount*1024ull), 
                         &BR, 0);
                assert(BR == TargetFileCount*1024);
                assert(ReadFileResult);

                if(ReadFileResult && BR == TargetFileCount*1024ull){
                    
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
                        
                        uint8_t ATLNonResident = 0;

                        void *AttributeList = NarFindFileAttributeFromFileRecord(FileRecord, NAR_ATTRIBUTE_LIST);                        
                        if(AttributeList){
                            
                            void*   ATLData = 0;
                            uint32_t ATLLen = 0;
                            ATLNonResident = *(uint8_t*)NAR_OFFSET(AttributeList, 8);
                                
                            if(!ATLNonResident){
                                ATLData = NAR_OFFSET(AttributeList, 24);
                                ATLLen  = *(uint32_t*)NAR_OFFSET(AttributeList, 16);
                                //ASSERT(ATLLen < 512);
                            }
                            
                        }
                        if(!NarIsFileLinked(FileRecord)){
                            assert(NarFindFileAttributeFromFileRecord(FileRecord, NAR_STANDART_FLAG));
                        }

                        // @NOTE : that was about gatherin some statistics and doing compression
                        // benchmarks to see how much we can save.  
                        #if 0
                        void *DataAttribute = NarFindFileAttributeFromFileRecord(FileRecord, NAR_DATA_FLAG);;
                        if (DataAttribute != NULL) {
                            
                            data_attr_header *DAHeader = (data_attr_header*)DataAttribute;
                            if (DAHeader->NonResidentFlag) {
                                uint32_t DataRunOffset = *(uint32_t *)NAR_OFFSET(DataAttribute, 32);
                                void *DataRuns = NAR_OFFSET(DataAttribute, DataRunOffset);
                                uint8_t t = *(uint8_t*)DataRuns;

                                memcpy(DataRunBuffer + TotalNumberOfSizeUsedForDataRuns, NAR_OFFSET(DataRuns, 1), t);

                                TotalNumberOfSizeUsedForDataRuns += t;
                                TotalNumberOfFilesUsingDataRuns++;

                            }

                        }
                        #endif
                        
                        // if(MultPIDs.Len == 0 && DEBUG_isATLNONRESIDENT && r->isDirectory) {
                        //     printf("HIT : %d\n", FileID);
                        // }

                        for(size_t _pidi = 0; _pidi < MultPIDs.Len; _pidi++){
                            name_pid NamePID = MultPIDs.PIDS[_pidi];
                            
                            if(NamePID.Name == NULL)
                                continue;

                            
                            if(IsDirectory){
                                DirectoryMapping[FileID] = NamePID;
                                uint32_t NameSize        = (NamePID.NameLen + 1) * 2;
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

                                for (size_t ExtensionCompIndx = 0; 
                                    ExtensionCompIndx < ExtensionListCount; 
                                    ExtensionCompIndx++) 
                                {
                                    
                                    size_t ExtensionLen = ExtensionLenList[ExtensionCompIndx];
                                    wchar_t *Extension  = ExtensionList[ExtensionCompIndx];
                                    // NOTE(Batuhan): if _pidi > 0, we know that file name matches, if it wouldn't be we wouldn't even see _pidi =1, check below for break condition. 
                                    if(ExtensionLen <= NamePID.NameLen){
                                        
                                        wchar_t *LastChars = &NamePID.Name[NamePID.NameLen - ExtensionLen];
                                        bool Add = true;
                                        for(uint64_t wi = 0; wi < ExtensionLen; wi++){
                                            // @TODO : convert all extensions to at beginning of the function. 
                                            if(towlower(LastChars[wi]) != towlower(Extension[wi])){
                                                Add = false;
                                                break;
                                            }
                                        }
                                        
                                        if(Add) {
                                            PIDResultArr[ArrLen] = NamePID;
                                            uint64_t NameSize = (NamePID.NameLen + 1) * 2;
                                            PIDResultArr[ArrLen].Name = (wchar_t*)LinearAllocate(&Memory->StringAllocator, NameSize);
                                            memcpy(PIDResultArr[ArrLen].Name, NamePID.Name, NameSize - 2);
                                            PIDResultArr[ArrLen].NameLen = NamePID.NameLen + 1;
                                            ArrLen++;
                                        }
                                        
                                    }

                                }

                            }

                            
                        }
                        


                        
                        if(MultPIDs.Len == 0 
                           && NULL != AttributeList
                           && IsDirectory){
                            // resolve the attribute list and
                            // redirect this entry.

                            /*
                            // @NOTE : there is a case we are missing right now (17.10.2022). if a directory's attribute list is non resident and doesnt contain the name
                            in the file record(which also means we wont be able to fetch parent file id) we miss everything about that directory!
                            at traverser stage, this causes stack to miss and crash.

                            one might say, hey if an attribute list is a non resident one, fetch it right now!.
                            well, just no. Why bother initiating a IO operation randomly. skip the entry here.
                            and at traverser stage, if we found a hole in a directory entry and we surely need to know it, then parse the atl and its entires.

                            this is, for now, not implemented. 
                            */

                            assert(DirectoryMapping[FileID].Name == NULL);

                            if (ATLNonResident) {
                                // atl is non resident
                                // @NOTE : save atl cluster in case we need it at traverser stage.
                                DirectoryMapping[FileID].ATLCluster = 0;
                                assert(false);
                            }
                            else {
                                // atl is resident
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
                                            assert(IsDirectory);
                                            
                                            DirectoryMapping[FileID] = {};
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
            ParentID > 5;
            ParentID = DirectoryMapping[ParentID].ParentFileID)
        {
            if(DirectoryMapping[ParentID].Name == 0) {
                // unresolved external attribute list
                if(DirectoryMapping[ParentID].ParentFileID == 0) {
                                      
                }
                assert(DirectoryMapping[ParentID].ParentFileID != 0);
                continue;
            }
            stack[si++]    = ParentID;
            TotalFileNameSize += DirectoryMapping[ParentID].NameLen;
            //assert(DirectoryMapping[ParentID].FileID != 0);
        }
        assert(si!=0);
        
        // additional memory for trailing backlashes
        FilenamesExtended[s] = (wchar_t*)LinearAllocate(&Memory->StringAllocator, TotalFileNameSize*2 + 200);
        uint64_t WriteIndex = 0;
        
                        
        {
            wchar_t tmp[] = L"!:\\";
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
                // @TODO : remove memcpy with simple decrement-replace
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
    
    // uint64_t DstCap = DataRunBufferCap + 256;
    // void *CompressedBuffer = malloc(DstCap);

    // int LZ4CompressedSize = LZ4_compress_default((const char *)DataRunBuffer, (char *)CompressedBuffer, TotalNumberOfSizeUsedForDataRuns, DstCap);
    // size_t ZSTDCompressedSize = ZSTD_compress(CompressedBuffer, DstCap, DataRunBuffer, TotalNumberOfSizeUsedForDataRuns, 4);
    // fprintf("zstd compressed size : ");

    // ASSERT(!ZSTD_isError(ZSTDCompressedSize));

#if 0
    for(size_t i =0; i<ArrLen; i++){
        // printf("%S\n", FilenamesExtended[i]);
    }
#endif
    
    double ParserFilePerMs    = (double)Memory->TotalFC/ParserTotalTime/1000.0;
    // double TraverserFilePerms = (double)ArrLen/TraverserTotalTime/1000.0;
    
    fprintf(stdout, "Parser, %9llu files in %.5f sec, file per ms %.5f, throughput : %.4f GB/s \n", Memory->TotalFC, ParserTotalTime, (double)Memory->TotalFC/ParserTotalTime/1000.0, ParserFilePerMs*1000.0*1024.0/Gigabyte(1));
    fprintf(stdout, "Traverser %9llu files in %.5f sec, file per ms %.5f\n", ArrLen, TraverserTotalTime, (double)ArrLen/TraverserTotalTime/1000.0);
    //fprintf(stdout, "Allocating count %8u in  %.5f sec, allocation per ms %.5f\n", AllocationCount, AllocatorTimeElapsed, (double)AllocationCount/AllocatorTimeElapsed/(1000.0));
    
    //printf("Match count %I64u\n", ArrLen);
    
    
    Result.Files = FilenamesExtended;
    Result.Len   = ArrLen;
    
    return Result;
}

