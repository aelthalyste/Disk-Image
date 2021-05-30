#pragma once

#include "memory.h"
#include "platform_io.h"
#include "nar.h"
#include "../inc/minispy.h"

#include <string>


/*
In order to export these symbosl to .NET, we have to put public keyword before enum
*/

#if _MANAGED
public
#endif

enum class RestoreSource_Errors: int{
    Error_NoError,
    Error_InsufficentBufferSize,
    Error_DecompressionUnknownContentSize,
    Error_DecompressionErrorContentsize,
    Error_DecompressionCompressedSize,
    Error_Decompression,
    Error_NullCompBuffer,
    Error_NullFileViews,
    Error_NullArg,
    Error_Count
};


#if _MANAGED
public
#endif

enum class RestoreStream_Errors: int{
    Error_NoError,
    Error_Read,
    Error_Needle,
    Error_Write,
    Error_Count
};

struct restore_source{
    
    union{
        // offline restore
        struct {
            nar_file_view Bin; 
            nar_file_view Metadata;
            
            const nar_record *Regions;
            size_t RecordsLen;
            
            // internal stuff about which region we are currently processing, and which cluster
            size_t RegionIndice; 
            size_t ClusterIndice;
            
			size_t ClusterSize;
			bool IsCompressed;
            
            size_t  AdvancedSoFar;
        	// Pre-allocated buffer for decompressing backup.
			void    *Bf;
            
            size_t  BfSize;        	
			size_t  BfNeedle;
            
            size_t  CompressedSize;
            size_t  DecompressedSize;
            
            size_t ZSTDError;
        };
        
        // useful if .net is going to feed us with network data
        struct{
            // to supress compiler warnings
            void *Buffer;
            size_t Len;
            size_t NewNeedle;
        };
    };
    
    
    RestoreSource_Errors Error;
    
    
    enum{
        Type_FileSource,
        Type_NetworkSource,
        Type_Count
    }Type;
    
    
    size_t BytesToBeCopied;
    
    // Recommended needle position for restore_target in absolute file position(that might exceed backup file's max size)
    // this value is LCN of latest read * clustersize of the backup
    // For volume targets, for ecah Read, it's recommended to call SetNeedle(AbsoluteNeedleInBytes);
    size_t AbsoluteNeedleInBytes; 
    
	// To prevent long-deadlocks on system, we can limit max # bytes can be copied to target. Some backups might have hundreds of consequent data blocks.
	// For compressed backups, copy-size is at LEAST as decompressed size, to prevent deadlocks due to 0 byte copy.
    // Min advance size is forced to be 1mb
    size_t MaxAdvanceSize = (64*1024*1024);
    
    /*
        Returns NULL if end of stream
        
        Returns read only memory adress to be copied to other end of the stream.
        If backup is compressed, it's equal to Bf member, otherwise it will be pointing to file mapping adress of binary data
        
        After calling read, caller must set target stream's needle to AbsoluteNeedleInBytes, then copy contents.
        
        AvailableBytes: Indicates how many bytes can be copied to other end of the stream. 
            -For NOT compressed backups, that value might be larger than 4GB, maybe hundreds of GBs. 
            -For compressed backups, it will be less than BfSize.
        
    */
    const void* (*Read)(restore_source* Rs, size_t *AvailableBytes);
    
};



struct restore_target{
	// Holds user defined parameters that can be useful when implementing write-setneedle functions
    void* Impl; // this is either HANDLE or FILE*, these variants supports file level restore so we don't need to use tagged-union like stuff.
    
    
    // One can provide it's own implementation of Write(like memory mapped file or classic volume target) to set restore's target    
	// advantage of this use style is caller can change state of restore_target by just simply changing Write and SetNeedle fnctor pointers.
    size_t (*Write)(restore_target* Rt, const void *Buffer, size_t BufferLen);
	size_t (*SetNeedle)(restore_target* Rt, size_t TargetFilePointer);
    
};

struct restore_stream{
	restore_source **Sources;
	restore_target *Target;
    
    RestoreStream_Errors Error;
    RestoreSource_Errors SrcError;
    
    size_t CSI;
	size_t SourceCap;
    
    // Total bytes needs to be copied to successfully reconstruct given backup. This is cummulative sum from full backup to given version
    size_t BytesToBeCopied; 
    
    // Resultant operation's target size, if target version is not fullbackup, TargetRestoreSize != BytesToBeCopied
    // (it must be but I'm just too lazy to implement that)
    size_t TargetRestoreSize;
};


// Returns how many bytes copied from source to target
// 0 means either error or source(s) are deplated.
size_t
AdvanceStream(restore_stream *Stream);

const void*
NarBackupRead(restore_source* Rs, size_t *AvailableBytes);


// returns NULL every call.
const void*
NarReadZero(restore_source* Rs, size_t *AvailableBytes);


////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename StrType>
restore_source*
InitRestoreFileSource(StrType MetadataPath, nar_arena* Arena, size_t MaxAdvanceSize) {
    
    restore_source* Result = (restore_source*)ArenaAllocate(Arena, sizeof(restore_source));
    memset(Result, 0, sizeof(restore_source));
    Result->Type            = restore_source::Type_FileSource;
    Result->MaxAdvanceSize  = MaxAdvanceSize;
    Result->Read  = NarReadBackup;
    Result->Error = RestoreSource_Errors::Error_NoError;
    
    bool Error = true;
    
    Result->Metadata = NarOpenFileView(MetadataPath);
    if (Result->Metadata.Data) {
        backup_metadata* bm = (backup_metadata*)Result->Metadata.Data;
        
        Result->ClusterSize = bm->ClusterSize;
        
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
                    Result->BfSize       = bm->FrameSize;
                    Result->Bf           = ArenaAllocate(Arena, bm->FrameSize + Megabyte(1));
                }
                else{
                    // NOTE(Batuhan): nothing special here
                }
                
                Error = false;
            }
            else {
                printf("Error : Regions metadata < metadata.len\n");
            }
            
            
        }
        else {
            NarFreeFileView(Result->Metadata);
        }
    }
    else{
        printf("Unable to open metadata\n");
    }
    
    if(true == Error){
        Result->Read = NarReadZero;
    }
    
    return Result;
}


void
FreeRestoreSource(restore_source *Rs);
////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////////

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
                
                Result->Sources   = (restore_source**)ArenaAllocate(Arena, SourceFileCount * sizeof(restore_source*));
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
                
                SourceFileCount   = bm.Version + SourceFileCount + 1;
                Result->Sources   = (restore_source**)ArenaAllocate(Arena, SourceFileCount * sizeof(restore_source*));
                Result->SourceCap = SourceFileCount;
                Result->CSI       = 0;
                
                static_assert(NAR_FULLBACKUP_VERSION == -1, "Changing FULLBACKUP VERSION number breaks this loop, and probably many more hidden ones too");
                
                for (int i = NAR_FULLBACKUP_VERSION; i <= bm.Version; i++) {
                    StrType fpath;
                    GenerateMetadataName(bm.ID, i, fpath);
                    printf("Inc backup file path : %S\n", fpath.c_str());
                    Result->Sources[i + 1]   = InitRestoreFileSource(RootDir + fpath, Arena, MaxAdvanceSize);
                    Result->BytesToBeCopied += Result->Sources[i + 1]->BytesToBeCopied;
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
FreeRestoreStream(restore_stream *Stream);
////////////////////////////////////////////////////////////////////////////////////////////////////////


const void*
NarReadBackup(restore_source* Rs, size_t *AvailableBytes);

template<typename StrType>
bool
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

#if _WIN32

size_t
NarSetNeedleVolume(restore_target* Rt, size_t TargetFilePointer);

// expects memsize to be lower than 4gb
size_t 
NarWriteVolume(restore_target *Rt, const void *Mem, size_t MemSize);

restore_target*
InitVolumeTarget(char Letter, nar_arena *Arena);

void
FreeRestoreTarget(restore_target* Rt);


#elif __linux__

#include <stdio.h>

void
FreeRestoreTarget(restore_target* Rt);

size_t
NarWriteVolume(restore_target* Rt, const void* Mem, size_t MemSize);

size_t
NarSetNeedleVolume(restore_target* Rt, size_t TargetFilePointer);

restore_target*
InitVolumeTarget(std::string VolumePath, nar_arena* Arena);



#endif



