#include "restore.h"


template<typename StrType>
static inline bool
NarReadMetadata(StrType path, backup_metadata *bm){
    bool Result = false;
    if(NULL == bm) 
        return Result;
    
    // TODO (Batuhan): I have to put crc32-like thing to validate metadata.
    if(NarFileReadNBytes(path, bm, sizeof(backup_metadata))){
        Result = true;
    }
    else{
        NAR_DEBUG("Unable to read file %s", path.c_str());
    }
    
    return Result;
}




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
		return NULL;	
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
    		NarFreeFileView(Result->Metadata);
    	}
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
    Result->Type            = Result::FileSource;
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
                Restore->BytesToBeCopied += Result->Sources[0].Len;
                Restore->BytesToBeCopied += Result->Sources[1].Len;
                

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
                    Restore->BytesToBeCopied += Result->Sources[i].Len;
                }
								
        	}
        	else{
        		// that shouldnt be possible
        		NAR_BREAK;
        	}

		}
        else{
            Restore->Sources            = (restore_source**)ArenaAllocate(Arena, 8);
            Restore->Sources[0]         = InitRestoreFileSource(MetadataPath);
            Restore->SourceCap          = 1;
            Restore->CSI                = 0;
            Restore->BytesToBeCopied    = Result->Sources[0].Len;
        }
        
    }
    else{
    	Result = NULL;
    }
    
    return Result;
}


void
FreeRestoreSource(restore_source *Rs){
    ASSERT(Rs);
    if(Rs->Type == restore_source::FileStream){
        NarFreeFileView(Rs->Bin);
        NarFreeFileView(Rs->Metadata);
        if(Rs->DStream){
            size_t RetCode = ZSTD_freeDStream(Rs->DStream);
            ASSERT(!ZSTD_isError(RetCode));
        }
    }
    else if(Rs->Type == restore_source::NetworkStream){
        // don't do anything
    }
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