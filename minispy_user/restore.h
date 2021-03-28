#pragma once

#include "memory.h"
#include "platform_io.h"
#include "nar.h"
#include "../inc/minispy.h"

#include <string>


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
            size_t  UncompressedSize;
            
            size_t ZSTDError;
            
        	ZSTD_DStream* DStream;
        };
        
        // useful if .net is going to feed us with network data
        struct{
            // to supress compiler warnings
            void *Buffer;
            size_t Len;
            size_t NewNeedle;
        };
    };
    
    
    // NOTE(Batuhan): 
    enum{
        Error_NoError,
        Error_InsufficentBufferSize,
        Error_Decompression,
        Error_NullCompBuffer,
        Error_NullFileViews,
        Error_NullArg,
        Error_Count
    }Error;
    
    
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


/*
    Example usage of structure
    struct MyTargetVars{
        HANDLE File;
        DWORD ErrorCode;
    };
    //size_t MyCustomWrite(restore_target* Rt, void* B, size_t BufferLen){
        MyTargetVars *mtv = (MyTargetVars*)Rt->Impl;
        DWORD BytesWritten= 0;
        mtv->ErrorCode = WriteFile(File, mtv, BufferLen, &BytesWritten, 0);
    }
    //size_t MySetNeedle(restore_target* Rt, size_t Needle){
        MyTargetVars *mtv = (MyTargetVars*)Rt->Impl;
        SetFilePointerEx(....);
    }
    //restore_target InitMyRestoreTarget(){
        restore_target Result;
        Result.impl = ...;
        Result.Write = MyCustomWrite;
        Result.SetNeedle = MySetNeedle;
        return Result;
    }
*/
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
    
    enum{
        Error_NoError,
        Error_Read,
        Error_Needle,
        Error_Write,
        Error_Count
    }Error;
    
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
InitRestoreFileSource(StrType MetadataPath, nar_arena* Arena, size_t MaxAdvanceSize = 1024ull * 1024ull * 64ull);

void
FreeRestoreSource(restore_source *Rs);
////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename StrType>
restore_stream*
InitFileRestoreStream(StrType MetadataFile, restore_target* Target, nar_arena* Arena, size_t MaxAdvanceSize = 1024ull * 1024ull * 64ull);

void
FreeRestoreStream(restore_stream *Stream);
////////////////////////////////////////////////////////////////////////////////////////////////////////


const void*
NarReadBackup(restore_source* Rs, size_t *AvailableBytes);

template<typename StrType>
static inline bool
NarReadMetadata(StrType path, backup_metadata *bm);

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



