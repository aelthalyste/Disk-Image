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
			
        };
        
        // useful if .net is going to feed us with network data
        struct{
            // to supress compiler warnings
            void *Buffer;
            size_t Len;
            size_t NewNeedle;
        };
    };
	    
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
    size_t MyCustomWrite(restore_target* Rt, void* B, size_t BufferLen){
        MyTargetVars *mtv = (MyTargetVars*)Rt->Impl;
        DWORD BytesWritten= 0;
        mtv->ErrorCode = WriteFile(File, mtv, BufferLen, &BytesWritten, 0);
    }
    size_t MySetNeedle(restore_target* Rt, size_t Needle){
        MyTargetVars *mtv = (MyTargetVars*)Rt->Impl;
        SetFilePointerEx(....);
    }
    restore_target InitMyRestoreTarget(){
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

    bool ContainsOS;
    //bool TerminateAtFault;

    char DiskType;
    
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


const void*
NarReadBackup(restore_source* Rs, size_t *AvailableBytes){	
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
	if(Rs->RegionIndice >= Rs->RecordsLen){
			return 0;	
	}

	size_t MaxClustersToAdvance 	= (Rs->MaxAdvanceSize/Rs->ClusterSize);
	size_t RemainingClusterInRegion = Rs->Regions[Rs->RegionIndice].Len - Rs->ClusterIndice;
	
	size_t ClustersToRead 			= MIN(MaxClustersToAdvance, RemainingClusterInRegion);
	size_t DataOffset 				= (size_t)(Rs->Regions[Rs->RegionIndice].StartPos + Rs->ClusterIndice)*Rs->ClusterSize;

	// Uncompressed file stream
	if(false == Rs->IsCompressed){
		Result = (unsigned char*)Rs->Bin.Data + DataOffset;
		if(AvailableBytes)
			*AvailableBytes = ClustersToRead * Rs->ClusterSize; 
		
		Rs->ClusterIndice += ClustersToRead;
		if(Rs->ClusterIndice >= Rs->Regions[Rs->RegionIndice].Len){
			Rs->ClusterIndice = 0;
			Rs->RegionIndice++;
		}
	}
	else{
		// Compressed file stream
		// not supported yet
		

		ASSERT(0);	
	}
	

	Rs->AbsoluteNeedleInBytes = DataOffset;

	return Result;
}


// i hate templates
static inline void 
narrestorefilesource_compilation_force_unit(){
	InitRestoreFileSource(std::wstring(L"soem file"), 0);
	InitRestoreFileSource(std::string("soem file"), 0);
}

template<typename StrType>
restore_source*
InitRestoreFileSource(StrType MetadataPath, nar_arena* Arena){
	
	restore_source *Result = (restore_source*)ArenaAllocate(Arena, sizeof(restore_source));
	memset(Result, 0, sizeof(restore_source));

	bool Error = true;
	
	Result->Metadata = NarOpenFileView(MetadataPath);	
	if(Result->Metadata.Data){
    	backup_metadata* bm = (backup_metadata*)Result->Metadata.Data;
		
		Result->ClusterSize 	= bm->ClusterSize;
		Result->IsCompressed 	= false;
    	StrType BinName;
    	GenerateBinaryFileName(bm->ID, bm->Version, BinName);

    	StrType Dir = NarGetFileDirectory(MetadataPath);		

    	Result->Bin = NarOpenFileView((Dir + BinName));
    	if(NULL != Result->Bin.Data){
    	    // check if backup is compressed
    	    
    	    ASSERT(bm->Offset.RegionsMetadata < Result->Metadata.Len);
    	    if(bm->Offset.RegionsMetadata < Result->Metadata.Len){
	  			Result->Regions 	= (nar_record*)((unsigned char*)Result->Metadata.Data + bm->Offset.RegionsMetadata);
				Result->RecordsLen 	= bm->Size.RegionsMetadata/sizeof(nar_record);	    	

   	     		if(bm->IsCompressed){
					Result->IsCompressed = true;
	       			Result->BfSize 	= bm->FrameSize;
    				Result->Bf 		= ArenaAllocate(Arena, bm->FrameSize);
	    		}

    			Error = false;
    	    }
    	    else{
				NAR_DEBUG("");
    	    }
			
    				
    	}
    	else{
    		NarFreeFileView(&Result->Metadata);
    	}
	}
        
	if(Error){
        Result = NULL;
	}

	return Result;
}


template<typename StrType>
restore_stream*
InitFileRestoreStream(StrType MetadataFile, restore_target* Target, nar_arena* Arena, size_t MaxAdvanceSize = (64ull*1024ull*1024ull)){
    
    restore_stream *Result = NULL;

 	Result 					= (restore_stream*)ArenaAllocate(Arena, sizeof(restore_stream));
	Result->Target 			= Target;
	Result->MaxAdvanceSize 	= MaxAdvanceSize;
    backup_metadata bm;
	    
    StrType RootDir = NarGetFileDirectory(MetadataFile);
    if(NarReadMetadata(MetadataFile, &bm)){
	    size_t SourceFileCount = 1;    
    	bm.Version
		if(bm.Version != NAR_FULLBACKUP_VERSION){
        	if(bm.BT == BackupType::Diff){
        		SourceFileCount 	= 2;
        		
        		Result->Sources 	= (restore_source**)ArenaAllocate(Arena, SourceFileCount*sizeof(restore_source*));
        		Result->SourceCap 	= SourceFileCount;
				Result->CSI 		= 0;

				// nar_backup_id ID, int Version, StrType &Res
				StrType fpath;
				StrType dpath;
				{
					GenerateMetadataName(bm.ID, NAR_FULLBACKUP_VERSION, fpath);
					GenerateMetadataName(bm.ID, bm.Version, dpath);
				}

				Result->Sources[0] 	= InitRestoreFileSource(RootDir + fpath, Arena);
				Result->Sources[1]	= InitRestoreFileSource(RootDir + dpath, Arena);
        	}
        	else if(bm.BT == BackupType::Inc){
				
				SourceFileCount 	= bm.Version + SourceFileCount;				

				Result->Sources 	= (restore_source**)ArenaAllocate(Arena, SourceFileCount*sizeof(restore_source*));
				Result->SourceCap 	= SourceFileCount;
				Result->CSI 		= 0;

				static_assert(NAR_FULLBACKUP_VERSION == -1, "Changing FULLBACKUP VERSION number breaks this loop, and probably many more hidden ones too");
				for(int i = NAR_FULLBACKUP_VERSION; i<=bm.Version; i++){
					StrType fpath;
					GenerateMetadataName(bm.ID, bm.Version, fpath);
					Result->Sources[i] = InitRestoreFileSource(RootDir + fpath, Arena);
				}
								
        	}
        	else{
        		// that shouldnt be possible
        		NAR_BREAK;
        	}

		}

        Result.ContainsOS = (!!(bm.IsOSVolume));
    }
    else{
    	Result = NULL;
    }
    
    return Result;
}


size_t
AdvanceStream(restore_stream *Stream){
    
    if(Stream == NULL) 
    	return false;
    if(Stream->CSI >= Stream->SourceCap){
    	NAR_DEBUG("End of stream source pipe list");
    	return false;
	}
		    
    size_t ReadLen = 0;
    restore_source *CS = Stream->Sources[Stream->CSI];

    const void* Mem = 0;
    //const void* Mem = CS->Read(CS, &ReadLen);
    if(Mem != NULL && ReadLen > 0){

        size_t NewNeedle = Stream->Target->SetNeedle(Stream->Target, CS->AbsoluteNeedleInBytes);    
        size_t BytesWritten = Stream->Target->Write(Stream->Target, Mem, ReadLen);
        if(BytesWritten != ReadLen){
            return false;
        }
    }
    else{
    	NAR_DEBUG("Source %I64u is depleted, moving to next one\n", Stream->CSI);
    	Stream->CSI++;
    	return AdvanceStream(Stream);
    }

    return true;
}







