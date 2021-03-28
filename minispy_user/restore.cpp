#include "restore.h"


template<typename StrType>
static inline bool
NarReadMetadata(StrType path, backup_metadata* bm) {
    bool Result = false;
    if (NULL == bm)
        return Result;

    // TODO (Batuhan): I have to put crc32-like thing to validate metadata.
    if (NarFileReadNBytes(path, bm, sizeof(backup_metadata))) {
        Result = true;
    }
    else {
        NAR_DEBUG("Unable to read file %s", path.c_str());
    }

    return Result;
}




const void*
NarReadBackup(restore_source* Rs, size_t* AvailableBytes) {
    //Rs->Regions[RegionIndice];		
    const void* Result = 0;
    /*
        nar_file_view Bin;
        nar_file_view Metadata;

        const nar_record *Regions;
        size_t RecordLen;

        // internal stuff about which region we are currently processing, and which cluster
        size_t RegionIndice;
        size_t ClusterIndice;
    */

    ASSERT(Rs);
    ASSERT(AvailableBytes);
    ASSERT(Rs->Bin.Data);
    ASSERT(Rs->Metadata.Data);
    ASSERT(Rs->MaxAdvanceSize % Rs->ClusterSize == 0);

    // End of stream 
    if (Rs->RegionIndice >= Rs->RecordsLen) {
        return NULL;
    }
    
    if(NULL == AvailableBytes){
        NAR_BREAK;
        Rs->Error   = restore_source::Error_NullArg;
        Rs->Read    = NarReadZero;
        return Rs->Read(Rs, AvailableBytes);
    }
    if(Rs->Bin.Data == NULL || Rs->Metadata.Data == NULL){
        NAR_BREAK;
        Rs->Error   = restore_source::Error_NullFileViews;
        Rs->Read    = NarReadZero;
        return Rs->Read(Rs, AvailableBytes);
    }

    size_t MaxClustersToAdvance = (Rs->MaxAdvanceSize / Rs->ClusterSize);
    size_t RemainingClusterInRegion = Rs->Regions[Rs->RegionIndice].Len - Rs->ClusterIndice;

    size_t ClustersToRead = MIN(MaxClustersToAdvance, RemainingClusterInRegion);
    size_t DataOffset = (size_t)(Rs->Regions[Rs->RegionIndice].StartPos + Rs->ClusterIndice) * Rs->ClusterSize;

    // Uncompressed file stream
    if (false == Rs->IsCompressed) {
        Result = (unsigned char*)Rs->Bin.Data + DataOffset;
        // NOTE(Batuhan): updating region and cluster indices handled at end of the function
    }
    else {

        // fetch next frame
        if(Rs->BfNeedle == Rs->UncompressedSize){
            Rs->AdvancedSoFar += Rs->CompressedSize;
            Rs->BfNeedle = 0;
            
            void *NewFrame = (unsigned char*)Rs->Bin.Data + Rs->AdvancedSoFar;
            size_t Remaining = Rs->Bin.Len - Rs->AdvancedSoFar;
            
            Rs->CompressedSize   = ZSTD_findFrameCompressedSize(NewFrame, Remaining);
            Rs->UncompressedSize = ZSTD_getFrameContentSize(NewFrame, Remaining);
            
            if(Rs->UncompressedSize >= Rs->BfSize){
                NAR_BREAK;
                Rs->Error   = restore_source::Error_InsufficentBufferSize;
                Rs->Read    = NarReadZero;
                return Rs->Read(Rs, AvailableBytes);
            }
            
            ZSTD_outBuffer output = {0};
            ZSTD_inBuffer input = {0};
            {
                input.src   = NewFrame;
                input.size  = Remaining;
                input.pos   = 0;
                
                output.dst  = Rs->Bf;
                output.size = Rs->BfSize;
                output.pos  = 0;
            }
            
            size_t ZSTD_RetCode = ZSTD_decompressStream(Rs->DStream, &output, &input);
            if(ZSTD_isError(ZSTD_RetCode)){
                NAR_BREAK;
                Rs->Error       = restore_source::Error_Decompression;
                Rs->Read        = NarReadZero;
                Rs->ZSTDError   = ZSTD_RetCode;
                return Rs->Read(Rs, AvailableBytes);
            }
            
        }
         
        ASSERT(((Rs->UncompressedSize - Rs->BfNeedle) % Rs->ClusterSize) == 0);
        
        size_t ClusterRemainingInBf = (Rs->UncompressedSize - Rs->BfNeedle) / Rs->ClusterSize;
        
        ClustersToRead = MIN(ClusterRemainingInBf, ClustersToRead);
        Rs->BfNeedle += (ClustersToRead * Rs->ClusterSize);
        
        Result = (unsigned char*)Rs->Bf + Rs->BfNeedle;
        
    }

    Rs->ClusterIndice += ClustersToRead;
    if (Rs->ClusterIndice >= Rs->Regions[Rs->RegionIndice].Len) {
        Rs->ClusterIndice = 0;
        Rs->RegionIndice++;
    }
    *AvailableBytes = ClustersToRead * Rs->ClusterSize;

    Rs->AbsoluteNeedleInBytes = DataOffset;

    return Result;
}


const void*
NarReadZero(restore_source* Rs, size_t *AvailableBytes){
    void *Result = 0;
    if(AvailableBytes)
        *AvailableBytes = 0;
    if(Rs){
        Rs->AbsoluteNeedleInBytes = 0;
    }
    return Result;
}

// i hate templates
static inline void
narrestorefilesource_compilation_force_unit() {
    InitRestoreFileSource(std::wstring(L"soem file"), 0);
    InitRestoreFileSource(std::string("soem file"), 0);
}

template<typename StrType>
restore_source*
InitRestoreFileSource(StrType MetadataPath, nar_arena* Arena, size_t MaxAdvanceSize) {

    restore_source* Result = (restore_source*)ArenaAllocate(Arena, sizeof(restore_source));
    memset(Result, 0, sizeof(restore_source));
    Result->Type            = restore_source::Type_FileSource;
    Result->MaxAdvanceSize  = MaxAdvanceSize;
    
    Result->Error = restore_source::Error_NoError;
    
    bool Error = true;

    Result->Metadata = NarOpenFileView(MetadataPath);
    if (Result->Metadata.Data) {
        backup_metadata* bm = (backup_metadata*)Result->Metadata.Data;

        Result->ClusterSize 	= bm->ClusterSize;
        Result->IsCompressed 	= false;
        Result->BytesToBeCopied = bm->Size.Regions;
         
        StrType BinName;
        GenerateBinaryFileName(bm->ID, bm->Version, BinName);

        StrType Dir = NarGetFileDirectory(MetadataPath);

        Result->Bin = NarOpenFileView((Dir + BinName));
        if (NULL != Result->Bin.Data) {
            // check if backup is compressed

            ASSERT(bm->Offset.RegionsMetadata < Result->Metadata.Len);
            if (bm->Offset.RegionsMetadata < Result->Metadata.Len) {
                Result->Regions = (nar_record*)((unsigned char*)Result->Metadata.Data + bm->Offset.RegionsMetadata);
                Result->RecordsLen = bm->Size.RegionsMetadata / sizeof(nar_record);

                if (bm->IsCompressed) {
                    Result->IsCompressed = true;
                    Result->BfSize = bm->FrameSize;
                    Result->Bf = ArenaAllocate(Arena, bm->FrameSize);
                }

                Error = false;
            }
            else {
                NAR_DEBUG("");
            }


        }
        else {
            NarFreeFileView(Result->Metadata);
        }
    }


    return Result;
}


template<typename StrType>
restore_stream*
InitFileRestoreStream(StrType MetadataFile, restore_target* Target, nar_arena* Arena, size_t MaxAdvanceSize) {

    restore_stream* Result = NULL;

    Result = (restore_stream*)ArenaAllocate(Arena, sizeof(restore_stream));
    Result->Target = Target;
    
    
    backup_metadata bm;

    StrType RootDir = NarGetFileDirectory(MetadataFile);
    if (NarReadMetadata(MetadataFile, &bm)) {
        size_t SourceFileCount = 1;
            if (bm.Version != NAR_FULLBACKUP_VERSION) {
                if (bm.BT == BackupType::Diff) {
                    SourceFileCount = 2;

                    Result->Sources = (restore_source**)ArenaAllocate(Arena, SourceFileCount * sizeof(restore_source*));
                    Result->SourceCap = SourceFileCount;
                    Result->CSI = 0;

                    // nar_backup_id ID, int Version, StrType &Res
                    StrType fpath;
                    StrType dpath;
                    {
                        GenerateMetadataName(bm.ID, NAR_FULLBACKUP_VERSION, fpath);
                        GenerateMetadataName(bm.ID, bm.Version, dpath);
                    }

                    Result->Sources[0] = InitRestoreFileSource(RootDir + fpath, Arena, MaxAdvanceSize);
                    Result->Sources[1] = InitRestoreFileSource(RootDir + dpath, Arena, MaxAdvanceSize);
                    Result->BytesToBeCopied += Result->Sources[0]->BytesToBeCopied;
                    Result->BytesToBeCopied += Result->Sources[1]->BytesToBeCopied;


                }
                else if (bm.BT == BackupType::Inc) {

                    SourceFileCount = bm.Version + SourceFileCount;

                    Result->Sources = (restore_source**)ArenaAllocate(Arena, SourceFileCount * sizeof(restore_source*));
                    Result->SourceCap = SourceFileCount;
                    Result->CSI = 0;

                    static_assert(NAR_FULLBACKUP_VERSION == -1, "Changing FULLBACKUP VERSION number breaks this loop, and probably many more hidden ones too");
                    
                    for (int i = NAR_FULLBACKUP_VERSION; i <= bm.Version; i++) {
                        StrType fpath;
                        GenerateMetadataName(bm.ID, bm.Version, fpath);
                        Result->Sources[i] = InitRestoreFileSource(RootDir + fpath, Arena, MaxAdvanceSize);
                        Result->BytesToBeCopied += Result->Sources[i]->BytesToBeCopied;
                    }

                }
                else {
                    // that shouldnt be possible
                    NAR_BREAK;
                }

            }
            else {
                Result->Sources = (restore_source**)ArenaAllocate(Arena, 8);
                Result->Sources[0] = InitRestoreFileSource(MetadataFile, Arena, MaxAdvanceSize);
                Result->SourceCap = 1;
                Result->CSI = 0;
                Result->BytesToBeCopied = Result->Sources[0]->BytesToBeCopied;
            }

    }
    else {
        Result = NULL;
    }

    return Result;
}


void
FreeRestoreStream(restore_stream* Stream) {
    if (NULL != Stream) {
        for (size_t i = 0; i < Stream->SourceCap; i++) {
            FreeRestoreSource(Stream->Sources[i]);
        }
        FreeRestoreTarget(Stream->Target);
    }
    else {
        // do nothing
    }
}


void
FreeRestoreSource(restore_source* Rs) {
    if (NULL != Rs) {
        if (Rs->Type == restore_source::Type_FileSource) {
            NarFreeFileView(Rs->Bin);
            NarFreeFileView(Rs->Metadata);
            if (Rs->DStream) {
                size_t RetCode = ZSTD_freeDStream(Rs->DStream);
                ASSERT(!ZSTD_isError(RetCode));
            }
        }
        else if (Rs->Type == restore_source::Type_NetworkSource) {
            // don't do anything
        }
    }
}


size_t
AdvanceStream(restore_stream* Stream) {

    size_t Result = 0;
    if (Stream == NULL)
        return Result;
    if (Stream->CSI >= Stream->SourceCap) {
        NAR_DEBUG("End of stream source pipe list");
        return Result;
    }

    size_t ReadLen = 0;
    restore_source* CS = Stream->Sources[Stream->CSI];

    const void* Mem = CS->Read(CS, &ReadLen);
    if (Mem != NULL && ReadLen > 0) {

        size_t NewNeedle = Stream->Target->SetNeedle(Stream->Target, CS->AbsoluteNeedleInBytes);
        if(NewNeedle != CS->AbsoluteNeedleInBytes){
            Stream->Error = restore_stream::Error_Needle;
        }
        
        size_t BytesWritten = Stream->Target->Write(Stream->Target, Mem, ReadLen);
        if (BytesWritten != ReadLen) {
            Stream->Error = restore_stream::Error_Write;
        }
        Result = BytesWritten;
        
    }
    else {
        
        if(Stream->Sources[Stream->CSI]->Error != restore_source::Error_NoError){
            Stream->Error = restore_stream::Error_Read;
        }
        else{
            NAR_DEBUG("Source %I64u is depleted, moving to next one\n", Stream->CSI);
            Stream->CSI++;
            return AdvanceStream(Stream);
        }
        
    }

    return Result;
}



#if _WIN32

size_t
NarSetNeedleVolume(restore_target* Rt, size_t TargetFilePointer) {
    size_t Result = 0;
    {
        LARGE_INTEGER MoveTo = { 0 };
        MoveTo.QuadPart = TargetFilePointer;
        LARGE_INTEGER NewFilePointer = { 0 };
        SetFilePointerEx(Rt->Impl, MoveTo, &NewFilePointer, FILE_BEGIN);
        Result = NewFilePointer.QuadPart;
    }
    return Result;
}

// expects memsize to be lower than 4gb
size_t
NarWriteVolume(restore_target* Rt, const void* Mem, size_t MemSize) {

    DWORD BytesWritten = 0;
    if (WriteFile(Rt->Impl, Mem, MemSize, &BytesWritten, 0) && BytesWritten == MemSize) {
        // success
    }
    else {
        NAR_DEBUG("Unable to write %I64u bytes to restore target\n", MemSize);
    }

    return (size_t)BytesWritten;
}

restore_target*
InitVolumeTarget(char Letter, nar_arena* Arena) {

    restore_target* Result = NULL;
    if ((Letter <= 'z' && Letter >= 'a')
        || Letter >= 'A' && Letter <= 'Z') {

        if (Letter >= 'Z')
            Letter = Letter - ('a' - 'A');

    }
    else {
        return Result;
    }

    char VolumePath[16];
    snprintf(VolumePath, 16, "\\\\.\\%c:", Letter);
    HANDLE Volume = CreateFileA(VolumePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
    if (Volume != INVALID_HANDLE_VALUE) {
        if (DeviceIoControl(Volume, FSCTL_LOCK_VOLUME, 0, 0, 0, 0, 0, 0)) {
            Result = (restore_target*)ArenaAllocate(Arena, sizeof(restore_target));
            Result->Impl = Volume;
            Result->Write = NarWriteVolume;
            Result->SetNeedle = NarSetNeedleVolume;
        }
        else {
            NAR_DEBUG("Couldn't lock volume %c\n", Letter);
        }
    }

    return Result;
}

void
FreeRestoreTarget(restore_target* Rt) {
    if (Rt) {
        CloseHandle(Rt->Impl);
    }
}



#elif __linux__

#include <stdio.h>

void
FreeRestoreTarget(restore_target* Rt) {
    if (Rt) {
        fclose(Rt->Impl);
    }
}


size_t
NarWriteVolume(restore_target* Rt, const void* Mem, size_t MemSize) {
    size_t Ret = fwrite(Mem, MemSize, 1, Rt->Impl);
    ASSERT(Ret == 1);
    return MemSize;
}

size_t
NarSetNeedleVolume(restore_target* Rt, size_t TargetFilePointer) {
    int Ret = fseeko64(Rt->Impl, (off64_t)TargetFilePointer, 0);
    ASSERT(Ret == 0);
    return TargetFilePointer;
}

restore_target*
InitVolumeTarget(std::string VolumePath, nar_arena* Arena) {
    static_assert(false, "not implemented");
    restore_target* Result = 0;

    FILE* F = fopen(VolumePath.c_str(), "rb");
    if (F) {
        Result = (restore_target*)ArenaAllocate(Arena, sizeof(restore_target));
        Result->Impl = F;
        Result->Write = NarWriteVolume;
        Result->SetNeedle = NarSetNeedleVolume;
    }
    else {
        NAR_DEBUG("Unable to open volume %s\n", VolumePath.c_str());
    }

    return Result;
}



#endif

