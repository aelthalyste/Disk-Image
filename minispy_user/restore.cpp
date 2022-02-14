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

#if 0
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

#endif











