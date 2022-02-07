#include "precompiled.h"
#include "restore.h"
#include "nar.h"

#if 0
struct restore_cloud_read_result {
    uint64_t ReadOffset;
    uint64_t ReadSize;

    uint64_t WriteTarget;
    uint64_t Len;
    
    nar_backup_id BackupID;
    std::string FullbackupPath;
};

struct cloud_restore_ctx {
    nar_file_view MetadataView;
    nar_record *BackedUpRegions;
    uint64_t BackedUpRegionCap = 1024*1024;
    uint64_t BackedUpRegionLen = 0;
    RegionCoupleIter ExtractIter;
};

restore_cloud_read_result
AdvanceCloudRestore(cloud_restore_ctx *ctx, void *memory, uint64_t memory) {

} 
#endif

const void*
NarRestoreReadBackup(restore_source* Rs, size_t* AvailableBytes) {
    //Rs->Regions[RegionIndice];		
    const void* Result = 0;
    
    ASSERT(Rs);
    ASSERT(AvailableBytes);
    ASSERT(Rs->Bin.Data);
    ASSERT(Rs->Metadata.Data);
    ASSERT(Rs->MaxAdvanceSize % Rs->ClusterSize == 0);
    
    // NOTE(Batuhan): End of stream 
    if (Rs->RegionIndice >= Rs->RecordsLen) {
        return NULL;
    }
    
    
    if(NULL == AvailableBytes){
        NAR_BREAK;
        Rs->Error   = RestoreSource_Errors::Error_NullArg;
        Rs->Read    = NarReadZero;
        return Rs->Read(Rs, AvailableBytes);
    }
    
    if(Rs->Bin.Data == NULL || Rs->Metadata.Data == NULL){
        NAR_BREAK;
        Rs->Error   = RestoreSource_Errors::Error_NullFileViews;
        Rs->Read    = NarReadZero;
        return Rs->Read(Rs, AvailableBytes);
    }
    
    if(Rs->Regions[Rs->RegionIndice].Len == 0){
        Rs->RegionIndice++;
        Rs->ClusterIndice = 0;
        return Rs->Read(Rs, AvailableBytes);
    }
    
    ASSERT(Rs->MaxAdvanceSize % 4096 == 0);
    size_t MaxClustersToAdvance     = (Rs->MaxAdvanceSize / Rs->ClusterSize);
    size_t RemainingClusterInRegion = Rs->Regions[Rs->RegionIndice].Len - Rs->ClusterIndice;
    
    size_t ClustersToRead = MIN(MaxClustersToAdvance, RemainingClusterInRegion);
    size_t DataOffset     = ((size_t)Rs->Regions[Rs->RegionIndice].StartPos + Rs->ClusterIndice) * Rs->ClusterSize;
    
    // Uncompressed file stream
    if (false == Rs->IsCompressed) {
        Result = (unsigned char*)Rs->Bin.Data + Rs->AdvancedSoFar;
        
        ASSERT(Rs->AdvancedSoFar <= Rs->Bin.Len);
        ASSERT(Rs->AdvancedSoFar % 4096 == 0);
        
        Rs->AdvancedSoFar = Rs->AdvancedSoFar + ClustersToRead * Rs->ClusterSize;
        
        // NOTE(Batuhan): updating region and cluster indices is being handled at end of the function
    }
    else {
        
        // Fetch next compression frame if current one is depleted
        if(Rs->BfNeedle == Rs->DecompressedSize){
            
            Rs->AdvancedSoFar += Rs->CompressedSize;
            Rs->BfNeedle = 0;
            
            void *NewFrame = (unsigned char*)Rs->Bin.Data + Rs->AdvancedSoFar;
            size_t Remaining = Rs->Bin.Len - Rs->AdvancedSoFar;
            
            Rs->CompressedSize   = ZSTD_findFrameCompressedSize(NewFrame, Remaining);
            Rs->DecompressedSize = ZSTD_getFrameContentSize(NewFrame, Remaining);
            
            ASSERT(ZSTD_CONTENTSIZE_UNKNOWN != Rs->DecompressedSize);
            ASSERT(ZSTD_CONTENTSIZE_ERROR   != Rs->DecompressedSize);
            
            // NOTE(Batuhan): ZSTD based errors
            if(ZSTD_CONTENTSIZE_UNKNOWN == Rs->DecompressedSize
               || ZSTD_CONTENTSIZE_ERROR == Rs->DecompressedSize){
                
                if(ZSTD_CONTENTSIZE_UNKNOWN == Rs->DecompressedSize){
                    Rs->Error = RestoreSource_Errors::Error_DecompressionUnknownContentSize;
                }
                if(ZSTD_CONTENTSIZE_ERROR == Rs->DecompressedSize){
                    Rs->Error = RestoreSource_Errors::Error_DecompressionErrorContentsize;
                }
                
                Rs->Read    = NarReadZero;
                return Rs->Read(Rs, AvailableBytes);
            }
            else{
                //printf("%8u %8u\n", Rs->CompressedSize, Rs->DecompressedSize);
            }
            
            if(ZSTD_isError(Rs->CompressedSize)){
                Rs->Error = RestoreSource_Errors::Error_DecompressionCompressedSize;
                Rs->Read  = NarReadZero;
                return Rs->Read(Rs, AvailableBytes);
            }
            
            // NOTE(Batuhan): That shouldn't happen, but in any case we should check it
            if(Rs->DecompressedSize > Rs->BfSize){
                NAR_BREAK;
                Rs->Error   = RestoreSource_Errors::Error_InsufficentBufferSize;
                Rs->Read    = NarReadZero;
                return Rs->Read(Rs, AvailableBytes);
            }
            
            size_t ZSTD_RetCode = ZSTD_decompress(Rs->Bf, Rs->BfSize, 
                                                  NewFrame, Rs->CompressedSize);
            if(ZSTD_isError(ZSTD_RetCode)){
                NAR_BREAK;
                Rs->Error       = RestoreSource_Errors::Error_Decompression;
                Rs->Read        = NarReadZero;
                Rs->ZSTDError   = ZSTD_RetCode;
                return Rs->Read(Rs, AvailableBytes);
            }
            
        }
        
        ASSERT(((Rs->DecompressedSize - Rs->BfNeedle) % Rs->ClusterSize) == 0);
        
        size_t ClusterRemainingInBf = (Rs->DecompressedSize - Rs->BfNeedle) / Rs->ClusterSize;
        
        ClustersToRead = MIN(ClusterRemainingInBf, ClustersToRead);
        Result = (unsigned char*)Rs->Bf + Rs->BfNeedle;
        
        Rs->BfNeedle += (ClustersToRead * Rs->ClusterSize);
        
    }
    
    Rs->ClusterIndice += ClustersToRead;
    if (Rs->ClusterIndice >= Rs->Regions[Rs->RegionIndice].Len) {
        Rs->ClusterIndice = 0;
        Rs->RegionIndice++;
    }
    *AvailableBytes = ClustersToRead * Rs->ClusterSize;
    ASSERT((ClustersToRead*Rs->ClusterSize) % 4096 == 0);
    ASSERT(*AvailableBytes % 4096 == 0);
    ASSERT(DataOffset % 4096 == 0);
    
    ASSERT((ClustersToRead*Rs->ClusterSize) % 4096 == 0);
    ASSERT((*AvailableBytes % 4096 == 0));
    
    Rs->AbsoluteNeedleInBytes = DataOffset;
    
    //printf("Restore write : volume offset %8I64u size %8I64u backup read offset %8I64u\n", Rs->AbsoluteNeedleInBytes/4096ull, *AvailableBytes/4096ull, ((uint64_t)Rs->AdvancedSoFar - (uint64_t)ClustersToRead * Rs->ClusterSize)/4096ull);
    
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
        }
        else if (Rs->Type == restore_source::Type_NetworkSource) {
            // don't do anything
        }
    }
}


size_t
AdvanceStream(restore_stream* Stream) {
    
    size_t Result = 0;
    size_t ReadLen = 0;
    
    if (Stream == NULL){
        return Result;
    }
    
    if (Stream->CSI >= Stream->SourceCap) {
        //NAR_DEBUG("End of stream source pipe list");
        return Result;
    }
    
    
    restore_source* CS = Stream->Sources[Stream->CSI];
    
    const void* Mem = CS->Read(CS, &ReadLen);
    
    Stream->SrcError = CS->Error;
    
    if (Mem != NULL && ReadLen > 0) {
        
        size_t NewNeedle = Stream->Target->SetNeedle(Stream->Target, CS->AbsoluteNeedleInBytes);
        if(NewNeedle != CS->AbsoluteNeedleInBytes){
            Stream->Error = RestoreStream_Errors::Error_Needle;
        }
        
        size_t BytesWritten = Stream->Target->Write(Stream->Target, Mem, ReadLen);
        if (BytesWritten != ReadLen) {
            Stream->Error = RestoreStream_Errors::Error_Write;
        }
        Result = BytesWritten;
        
    }
    else {
        
        if(Stream->Sources[Stream->CSI]->Error != RestoreSource_Errors::Error_NoError){
            Stream->Error = RestoreStream_Errors::Error_Read;
        }
        else{
            NAR_DBG_ERR("Source %llu is depleted, moving to next one\n", (uint64_t)Stream->CSI);
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
        //printf("SetNeedle %8I64u %8I64u\n", TargetFilePointer/4096, TargetFilePointer);
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
    
    ASSERT(MemSize <= 0xffffffffull);
    
    DWORD BytesWritten = 0;
    if (WriteFile(Rt->Impl, Mem, (DWORD)MemSize, &BytesWritten, 0) && BytesWritten == MemSize) {
        // success
    }
    else {
        NAR_DEBUG("Unable to write %llu bytes to restore target\n", (uint64_t)MemSize);
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
            
        }
        else {
            NAR_DEBUG("Couldn't lock volume %c\n", Letter);
        }
        
        if (DeviceIoControl(Volume, FSCTL_DISMOUNT_VOLUME, 0, 0, 0, 0, 0, 0)) {
            
        }
        else {
            // printf("Couldnt dismount volume\n");
        }
        
        Result = (restore_target*)ArenaAllocate(Arena, sizeof(restore_target));
        ASSERT(Result);
        if(Result){
            Result->Impl = Volume;
            Result->Write = NarWriteVolume;
            Result->SetNeedle = NarSetNeedleVolume;
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
        fclose((FILE*)Rt->Impl);
    }
}


size_t
NarWriteVolume(restore_target* Rt, const void* Mem, size_t MemSize) {
    size_t Ret = fwrite(Mem, MemSize, 1, (FILE*)Rt->Impl);
    int NarErrorNo      = errno;
    char* NarErrDesc    = 0; 
    ASSERT(Ret == 1);
    if(Ret == 0){
        NarErrDesc = strerror(NarErrorNo);
        NAR_DEBUG("%s\n", NarErrDesc);
    }
    return MemSize;
}

size_t
NarSetNeedleVolume(restore_target* Rt, size_t TargetFilePointer) {
    int Ret = fseeko64((FILE*)Rt->Impl, (off64_t)TargetFilePointer, 0);
    ASSERT(Ret == 0);
    return TargetFilePointer;
}

restore_target*
InitVolumeTarget(std::string VolumePath, nar_arena* Arena) {
    restore_target* Result = 0;
    
    FILE* F = fopen(VolumePath.c_str(), "wb");
    int NarFOPENError = errno;
    char *NarErrDescription = NULL;
    ASSERT(NULL != F);
    if (F) {
        Result = (restore_target*)ArenaAllocate(Arena, sizeof(restore_target));
        Result->Impl = F;
        Result->Write       = NarWriteVolume;
        Result->SetNeedle   = NarSetNeedleVolume;
    }
    else {
        NarErrDescription = strerror(NarFOPENError);
    }
    
    return Result;
}


#endif

