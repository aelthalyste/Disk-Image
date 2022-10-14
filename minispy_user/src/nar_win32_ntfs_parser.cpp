#include "precompiled.h"

#include <string.h>
#include <stdint.h>

#include "file_explorer.hpp"
#include "performance.hpp"
#include "nar_compression.hpp"
#include "nar_ntfs_utils.hpp"

#if 1





file_explorer_memory
NarInitFileExplorerMemory(uint32_t TotalFC){
    file_explorer_memory Result = {0};
    
    uint64_t StringAllocatorSize     = TotalFC*550 + Megabyte(600);
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
    
    printf("File explorer initializing with metadata %s\n", MetadataPath.Str);
    
    file_explorer Result = {0};
    
    Result.MetadataView   = NarOpenFileView(MetadataPath);
    
    
    if(NULL != Result.MetadataView.Data){
        backup_metadata* BM = (backup_metadata*)Result.MetadataView.Data;
        
        Result.MFT     = (uint8_t*)Result.MetadataView.Data + BM->Offset.MFT;
        Result.MFTSize = BM->Size.MFT;
        Result.TotalFC = (Result.MFTSize / (1024))*3/2;
        
        Result.Memory  = NarInitFileExplorerMemory((uint32_t)Result.TotalFC);
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
            
            printf("File explorer backup view initializing with name %s\n", FullBinaryPath.Str);
            
            Result.FullbackupView = NarOpenFileView(FullBinaryPath);
        }
        
        
        
        Result.ClusterSize = BM->ClusterSize;
        Result.DirectoryID   = 5;
        Result.DirectoryPath = (wchar_t*)ArenaAllocate(&Result.Memory.Arena, Kilobyte(32));
        Result.ParentIDs     = (uint32_t*)ArenaAllocateAligned(&Result.Memory.Arena, Result.TotalFC*sizeof(uint32_t)*2, 4);
        
        Result.Files = (file_explorer_file*)ArenaAllocateAligned(&Result.Memory.Arena, (Result.TotalFC  + 1024 * 16)*sizeof(file_explorer_file), 8);
        
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
                else{
                    //NAR_BREAK;
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
            uint32_t BaseID   = FileSizeTuple[_idc].FileID;
            
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
    
    uint64_t Left  = 0;
    uint64_t Right = FE->FileCount;
    uint64_t Mid   = (Right - Left)/2;
    
    while(Right > Left && Mid!=Left){
        
        if(FE->FileIDs[Mid] == ID) {
            return &FE->Files[Mid];
        }
        if(FE->FileIDs[Mid] > ID) {
            Right = Mid;
        }
        if(FE->FileIDs[Mid] < ID) {
            Left = Mid;
        }
        Mid = Left + (Right - Left) / 2;
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









file_disk_layout
NarFindFileLayout(file_explorer *FE, file_explorer_file *File, nar_arena *Arena){
#if 1
    file_disk_layout Result = {0};
    
    uint32_t FilesToVisit[80];
    uint32_t FTVCount        = 0;
    
    auto FullRestorePoint = ArenaGetRestorePoint(Arena);
    
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
    Result.LCN = (nar_kernel_record*)ArenaAllocateAligned(Arena, Result.MaxCount*sizeof(nar_kernel_record), 8);
    
    void* ATL  = NarFindFileAttributeFromFileRecord(FileRecord, NAR_ATTRIBUTE_LIST);
    
    
    if(NULL != ATL){
        
        uint8_t* ATLDataBuffer = (uint8_t*)ArenaAllocateAligned(&FE->Memory.Arena, FE->ClusterSize, 8);
        ASSERT(ATLDataBuffer != 0);
        
        void* ATLData = 0;
        size_t ATLLen = 0;
        
        uint8_t ATLNonResident = *(uint8_t*)NAR_OFFSET(ATL, 8);
        if(ATLNonResident){
            void* DataRunStart = NAR_OFFSET(ATL, 64);;
            nar_kernel_record Runs[64];
            uint32_t DataRunFound = 0;
            
            bool ParseResult = NarParseDataRun(DataRunStart, 
                                               Runs, 
                                               sizeof(Runs)/sizeof(nar_kernel_record), 
                                               &DataRunFound, 
                                               false);
            
            ASSERT(ParseResult);
            ASSERT(DataRunFound == 1);
            
            uint64_t TargetCluster = Runs[0].StartPos;
            uint64_t FEResult = NarReadBackup(&FE->FullbackupView, &FE->MetadataView, TargetCluster, 1, ATLDataBuffer, FE->ClusterSize, 0, 0);
            
            ATLData = &ATLDataBuffer[0];
            ATLLen  = *(uint32_t*)NAR_OFFSET(ATL, 48);
            ASSERT(FEResult == 4096);
            
        }
        else{
            ATLData = NAR_OFFSET(ATL, 24);
            ATLLen  = *(uint32_t*)NAR_OFFSET(ATL, 16);
        }
        
        uint64_t RemainingLen = ATLLen;
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
            
            int16_t FirstAttributeOffset = (*(int16_t*)((BYTE*)FileRecord + 20));
            
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
                    
                    if(DAHeader->NameLen == 0){
                        goto G_FAIL;
                    }
                    
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
        int16_t FirstAttributeOffset = (*(int16_t*)((BYTE*)FileRecord + 20));
        void* FileAttribute = (char*)FileRecord + FirstAttributeOffset;
        
        for(void* FileAttribute = (uint8_t*)FileRecord + FirstAttributeOffset;
            FileAttribute != 0;
            FileAttribute = NarNextAttribute(FileAttribute)){
            
            uint32_t AttributeID = *(uint32_t*)FileAttribute;
            if(AttributeID == NAR_DATA_FLAG){
                data_attr_header *DAHeader = (data_attr_header*)FileAttribute;
                
                if(DAHeader->Sparse != 0){
                    goto G_FAIL;
                }
                
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
        Result.SortedLCN = (nar_kernel_record*)ArenaAllocateAligned(Arena, Result.LCNCount*sizeof(nar_kernel_record), 8);
        
        // it is guarenteed there wont be colliding blocks, no need to call mergeregionswithoutrealloc
        memcpy(Result.SortedLCN, Result.LCN, Result.LCNCount*sizeof(nar_kernel_record));
        qsort(Result.SortedLCN, Result.LCNCount, sizeof(nar_kernel_record), CompareNarRecords);
    }
    
    return Result;
    
    G_FAIL:
    ArenaRestoreToPoint(Arena, FullRestorePoint);
#endif

    return {0};

}



/*
CAUTION : Assumes we can insert as much as we can into Files array.

Args:
MFTStart, start of MFT, assumes it's sequential all the way in the memory.
BaseFileRecord : Record that attribute array belongs to.
Files: Array to append found file(s). Since there might be hard links we may
append more than one file to array.

Return: Returns how many files appended to Files. 0 is unexpected.
*/
static inline uint64_t SolveAttributeListReferences(const void* MFTStart,
    void* BaseFileRecord,
    attribute_list_contents Contents, file_explorer_file* Files,
    linear_allocator* StringAllocator
) {

    // Those common fields applies to all hard links if any exist.
    // at the end of the function those are applied to each file.
    SYSTEMTIME WinFileCreated = { 0 };
    SYSTEMTIME WinFileModified = { 0 };
    uint64_t   FileSize = 0;

    FileRecordHeader* Header = (FileRecordHeader*)BaseFileRecord;
    TIMED_BLOCK();

    uint64_t FileCount = 0;

    // find unique file ID's that contains file name attribute
    uint32_t UniqueFileID[16];
    uint32_t UniqueFileIDCount = 0;
    for (uint64_t i = 0; i < 32; i++) {

        attribute_list_entry Entry = Contents.Entries[i];

        if (false == IsValidAttrEntry(Entry)) {
            break;
        }

        if (i == 0 && Entry.EntryType == NAR_FILENAME_FLAG) {
            UniqueFileID[UniqueFileIDCount++] = Entry.EntryFileID;
            continue;
        }

        attribute_list_entry PrevEntry = Contents.Entries[i - 1];

        if (Entry.EntryType == NAR_FILENAME_FLAG) {
            if (PrevEntry.EntryType == NAR_FILENAME_FLAG && Entry.EntryFileID == PrevEntry.EntryFileID) {

            }
            else {
                UniqueFileID[UniqueFileIDCount++] = Entry.EntryFileID;
            }
        }

    }

    ASSERT(UniqueFileIDCount > 0);

    for (uint64_t i = 0; i < UniqueFileIDCount; i++) {

        void* EntryFileRecord = (uint8_t*)MFTStart + UniqueFileID[i] * 1024;
        multiple_pid MultPIDs = NarGetFileNameAndParentID(EntryFileRecord);

        ASSERT(MultPIDs.Len > 0);
        for (uint64_t _pidi = 0; _pidi < MultPIDs.Len; _pidi++) {
            name_pid NamePID = MultPIDs.PIDS[_pidi];
            uint32_t NameLen = NamePID.NameLen;
            wchar_t* Name = NamePID.Name;
            uint64_t NameSize = (NameLen + 1) * 2;

            Files[FileCount].Name = (wchar_t*)LinearAllocate(StringAllocator, NameSize);
            Files[FileCount].NameLen = NameLen;

            memcpy(Files[FileCount].Name, Name, NameSize - 2);
            FileCount++;
        }
    }


    for (uint64_t i = 0; i < 32; i++) {

        attribute_list_entry Entry = Contents.Entries[i];
        if (false == IsValidAttrEntry(Entry)) {
            break;
        }

        if (Entry.EntryType != NAR_DATA_FLAG
            && Entry.EntryType != NAR_STANDART_FLAG) {
            continue;
        }


        void* EntryFileRecord = (uint8_t*)MFTStart + Entry.EntryFileID * 1024;
        void* Attr = NarFindFileAttributeFromFileRecord(EntryFileRecord, Entry.EntryType);
        ASSERT(Attr != NULL);

        switch (Entry.EntryType) {

#if 0     
        case NAR_FILENAME_FLAG: {
            //DOS FILE NAME
            if (*(uint8_t*)NAR_OFFSET(Attr, 89) != 2) {
                uint32_t NameLen = *(uint8_t*)NAR_OFFSET(Attr, 88);
                wchar_t* Name = (wchar_t*)NAR_OFFSET(Attr, 90);
                uint64_t NameSize = (NameLen + 1) * 2;

                Files[FileCount].Name = (wchar_t*)LinearAllocate(StringAllocator, NameSize);
                Files[FileCount].NameLen = NameLen;

                memcpy(Files[FileCount].Name, Name, NameSize - 2);
                FileCount++;
            }

            break;
        }
#endif

        case NAR_DATA_FLAG: {
            if (FileSize != 0) {
                break;
            }
            FileSize = *(uint64_t*)NAR_OFFSET(Attr, 48);
            break;
        }
        case NAR_STANDART_FLAG: {
            //NAR_BREAK;
            break;
        }
        default: {
            break;
        }
        }
    }


    for (uint64_t i = 0; i < FileCount; i++) {
        Files[i].Size = FileSize;
        memcpy(&Files[i].CreationTime, &WinFileCreated, sizeof(SYSTEMTIME));
        memcpy(&Files[i].LastModifiedTime, &WinFileModified, sizeof(SYSTEMTIME));
    }

    return FileCount;
}

inline void
NarFreeFileRestoreSource(file_restore_source *Src){
    NarFreeFileView(Src->Backup);
    NarFreeFileView(Src->Metadata);
}


file_restore_source
NarInitFileRestoreSource(NarUTF8 MetadataName, NarUTF8 BinaryName){
    file_restore_source Result = {};
    
    Result.Metadata = NarOpenFileView(MetadataName);
    
    if(NULL != Result.Metadata.Data){
        Result.Backup   = NarOpenFileView(BinaryName);
        if(NULL != Result.Backup.Data){
            
        }
        else{
            printf("Unable to open backup binary file, name %s\n", BinaryName.Str);
            goto FAIL;
        }
    }
    else{
        printf("Unable to open backup metadata file, name %s\n", MetadataName.Str);
        goto FAIL;
    }
    
    ASSERT(Result.Metadata.Data);
    ASSERT(Result.Backup.Data);
    
    backup_metadata *BM = (backup_metadata*)Result.Metadata.Data;
    
    Result.BackupLCN = (nar_kernel_record*)((uint8_t*)Result.Metadata.Data + BM->Offset.RegionsMetadata);
    Result.LCNCount  = BM->Size.RegionsMetadata/sizeof(nar_kernel_record);
    Result.ID        = BM->ID;
    Result.Version   = BM->Version;
    Result.Type      = BM->BT;
    return Result;
    
    FAIL:
    return {};
}


file_restore_source 
NarInitFileRestoreSource(NarUTF8 RootDir, nar_backup_id ID, int32_t Version, nar_arena *StringArena){
    
    bool StringResult = false;
    memory_restore_point Restore = ArenaGetRestorePoint(StringArena);
    file_restore_source  Result = {};
    
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

template<typename Type> bool
IsValid(const Type& v){
    Type ZeroStruct = {};
    memset(&ZeroStruct, 0, sizeof(ZeroStruct));
    return !(memcmp(&v, &ZeroStruct, sizeof(Type)) == 0);
}

file_restore_ctx
NarInitFileRestoreCtx(file_disk_layout Layout, NarUTF8 RootDir, nar_backup_id ID, int Version, nar_arena *Arena){
    
    file_restore_ctx Result = {};
    
    Result.Layout = Layout;
    Result.Source = NarInitFileRestoreSource(RootDir, ID, Version, Arena);
    
    if(!IsValid(Result.Source)){
        printf("File restore source was invalid!\n");
        return {};
    }
    
    size_t PoolMemSize = Megabyte(1);
    size_t PoolSize    = PoolMemSize/4;
    
    void *PoolMem   = ArenaAllocateAligned(Arena, PoolMemSize, 8);
    
    Result.ClusterSize = ((backup_metadata*)Result.Source.Metadata.Data)->ClusterSize;
    
    Result.LCNPool         = NarInitPool(PoolMem, PoolMemSize, PoolSize);
    Result.StringAllocator = ArenaInit(ArenaAllocate(Arena, Kilobyte(400)), Kilobyte(400));
    
    Result.DecompBufferSize  = NAR_COMPRESSION_FRAME_SIZE*2;
    Result.DecompBuffer      = ArenaAllocate(Arena, Result.DecompBufferSize);
    
    
    Result.ActiveLCN      = (nar_kernel_record*)PoolAllocate(&Result.LCNPool);
    Result.ActiveLCNCount = Result.Layout.LCNCount;
    
    memcpy(Result.ActiveLCN, Result.Layout.SortedLCN, Result.Layout.LCNCount*sizeof(nar_kernel_record));
    
    
    Result.RootDir = NarStringCopy(RootDir, &Result.StringAllocator);
    Result.IIter   = NarInitRegionCoupleIter(Result.ActiveLCN, Result.Source.BackupLCN, Result.ActiveLCNCount, Result.Source.LCNCount);
    
    ASSERT(Result.ActiveLCNCount <= PoolSize);
    
    return Result;
}

file_restore_ctx
NarInitFileRestoreCtx(file_explorer *FE, file_explorer_file* Target, nar_arena *Arena){
    file_disk_layout Layout = NarFindFileLayout(FE, Target, Arena);
    file_restore_ctx Result = {};
    Result = NarInitFileRestoreCtx(Layout, FE->RootDir, FE->ID, FE->Version, Arena);
    return Result;
}

void
NarFreeFileRestoreCtx(file_restore_ctx *Ctx){
    NarFreeFileRestoreSource(&Ctx->Source);
}



file_restore_advance_result
NarAdvanceFileRestore(file_restore_ctx *ctx, void* Out, size_t OutSize){
    file_restore_advance_result Result ={};

#if 1    
    // FileRestore_Errors Result = FileRestore_Errors:Error_NoError;
    // fetch next region if this one is depleted
    
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
        
        nar_kernel_record FetchRegion = ctx->IIter.It;
        
        // check if end of IIter
        if(!NarIsRegionIterValid(ctx->IIter)){
            
            // exclude found backups from layout.
            {
                nar_kernel_record *ExcludedLCN = (nar_kernel_record*)PoolAllocate(&ctx->LCNPool);
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
    
    // printf("%8u\t%8u\t%8u\n", ReadOffset, VCNWrite, ClustersToRead);
    
    Result.Len      = BytesRead;
    Result.Offset   = VCNWrite * ctx->ClusterSize;

#endif

    return Result; 

}


#endif