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

        	// Pre-allocated buffer for decompressing backup.
			void   *Bf;
			size_t	BfSize;        	
			size_t 	BfNeedle;
        	
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
	
    enum{
        FileStream,
        NetworkStream,
        Count
    }Type;
    
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
    const void (*Read)(restore_source* Rs, size_t *AvailableBytes);
    
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
    void* Impl;
    
    // One can provide it's own implementation of Write(like memory mapped file or classic volume target) to set restore's target    
	// advantage of this use style is caller can change state of restore_target by just simply changing Write and SetNeedle fnctor pointers.
    size_t (*Write)(restore_target* Rt, const void *Buffer, size_t BufferLen);
	size_t (*SetNeedle)(restore_target* Rt, size_t TargetFilePointer);
    
};

struct restore_stream{
	restore_source **Sources;
	restore_target *Target;
    
    size_t CSI;
	size_t SourceCap;
    
    // Total bytes needs to be copied to successfully reconstruct given backup. This is cummulative sum from full backup to given version
    size_t BytesToBeCopied; 
    
    // Resultant operation's target size, if target version is not fullbackup, TargetRestoreSize != BytesToBeCopied
    // (it must be but I'm just too lazy to implement that)
    size_t TargetRestoreSize;
};



template<typename StrType>
restore_source*
InitRestoreFileSource(StrType MetadataPath, nar_arena* Arena);


// Returns how many bytes copied from source to target
// 0 means either error or source(s) are deplated.
size_t
AdvanceStream(restore_stream *Stream);


const void*
NarBackupRead(restore_source* Rs, size_t *AvailableBytes);

template<typename StrType>
restore_source*
InitRestoreFileSource(StrType MetadataPath, nar_arena* Arena);

template<typename StrType>
restore_stream*
InitFileRestoreStream(StrType MetadataFile, restore_target* Target, nar_arena* Arena);

const void*
NarReadBackup(restore_source* Rs, size_t *AvailableBytes);

template<typename StrType>
static inline bool
NarReadMetadata(StrType path, backup_metadata *bm);

#if _WIN32

size_t
NarSetNeedleVolume(restore_target* Rt, size_t TargetFilePointer){
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
NarWriteVolume(restore_target *Rt, const void *Mem, size_t MemSize){
    
    DWORD BytesWritten = 0;
    if(WriteFile(Rt->Impl, Mem, MemSize, &BytesWritten, 0) && BytesWritten == MemSize){
        // success
    }
    else{
        NAR_DEBUG("Unable to write %lu bytes to restore target\n");
    }
    
    return (size_t)BytesWritten;
}

restore_target*
InitVolumeTarget(char Letter, nar_arena *Arena){
    
    restore_target *Result = NULL;
    if((Letter <= 'z' && Letter >= 'a') 
        || Letter >= 'A' && Letter <= 'Z'){
        
        if(Letter >= 'Z')
            Letter = Letter - ('a' - 'A');
    
    }
    else{
        return Result;
    }
    
    char VolumePath[16];
    snprintf(VolumePath, 16, "\\\\.\\%c:", Letter);
    HANDLE Volume = CreateFileA(VolumePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
    if(Volume != INVALID_HANDLE_VALUE){
        if (DeviceIoControl(Volume, FSCTL_LOCK_VOLUME, 0, 0, 0, 0, 0, 0)) {
            Result              = (restore_target*)ArenaAllocate(Arena, sizeof(restore_target));
            Result->Impl        = Volume;
            Result->Write       = NarWriteVolume;
            Result->SetNeedle   = NarSetNeedleVolume;
        }
        else {
            NAR_DEBUG("Couldn't lock volume %c\n", Letter);
        }
    }
    
    return Result;
}

#elif __linux__

#include <stdio.h>

size_t
NarWriteVolume(restore_target* Rt, const void* Mem, size_t MemSize){
    size_t Ret = fwrite(Mem, MemSize, 1, Rt->Impl);
    ASSERT(Ret == 1);
    return MemSize;
}

size_t
NarSetNeedleVolume(restore_target* Rt, size_t TargetFilePointer){
    int Ret=fseeko64(Rt->Impl, (off64_t)TargetFilePointer, 0);
    ASSERT(Ret == 0);
    return TargetFilePointer;
}

restore_target*
InitVolumeTarget(std::string VolumePath, nar_arena* Arena){
    static_assert(false, "not implemented");
    restore_target *Result = 0;
    
    FILE *F = fopen(VolumePath.c_str(), "rb");
    if(F){
        Result = (restore_target*)ArenaAllocate(Arena, sizeof(restore_target));
        Result->Impl        = F;
        Result->Write       = NarWriteVolume;
        Result->SetNeedle   = NarSetNeedleVolume;        
    }
    else{
        NAR_DEBUG("Unable to open volume %s\n", VolumePath.c_str());
    }
    
    return Result;
}



#endif



