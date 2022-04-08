#define BG_BUILD_AS_DLL 0
#include "nar_win32.hpp"
#include "nar.hpp"
#include "platform_io.hpp"


#define BG_IMPLEMENTATION
#include "bg.hpp"


using namespace System;
using namespace System::Text;
using namespace System::Collections::Generic;
using namespace System::Collections;

#pragma once

#include <msclr/marshal.h>
#define NAR_ENABLE_DOTNET_FILE_EXPLORER 0

using namespace System;
using namespace System::Text;
using namespace System::Collections::Generic;
using namespace System::Collections;

void
SystemStringToWCharPtr(System::String^ SystemStr, wchar_t* Destination) {
    
    pin_ptr<const wchar_t> wch = PtrToStringChars(SystemStr);
    size_t ConvertedChars = 0;
    size_t SizeInBytes = (SystemStr->Length + 1) * 2;
    memcpy(Destination, wch, SizeInBytes);
    
}

namespace NarNative{
    
    public ref class ExtensionSearcher{
        public:
        
        static List<System::String^>^ FindExtensionAllVolumes(List<System::String^> ^Extensions){
            
            return nullptr;
#if NAR_ENABLE_DOTNET_FILE_EXPLORER
            List<System::String^>^ Result = gcnew List<System::String^>;
            
            wchar_t **ExtensionList = (wchar_t **)calloc(Extensions->Count, 8);
            uint64_t ArenaSize = 0;
            for (uint64_t i =0; i < Extensions->Count; i++) {
                ArenaSize += Extensions[i]->Length;
            }

            ArenaSize += (Extensions->Count * 5);
            ArenaSize *= 2; // sizeof(wchar_t)

            nar_arena StringArena   = ArenaInit(calloc(ArenaSize, 1), ArenaSize);

            for (uint64_t i =0; i < Extensions->Count; i++) {
                ExtensionList[i] = (wchar_t *)ArenaAllocate(&StringArena, Extensions[i]->Length * 2 + 2);
                SystemStringToWCharPtr(Extensions[i], ExtensionList[i]);
            }
            
            
            DWORD Drives = GetLogicalDrives();
            // NOTE(Batuhan): skip volume A and B
            for (int CurrentDriveIndex = 2; CurrentDriveIndex < 26; CurrentDriveIndex++) {
                
                if (Drives & (1 << CurrentDriveIndex)) {
                    char letter = ('A' + (char)CurrentDriveIndex);
                    HANDLE VolumeHandle = NarOpenVolume(letter);
                    if(VolumeHandle != INVALID_HANDLE_VALUE){
                        
                        extension_finder_memory ExMemory = NarSetupExtensionFinderMemory(VolumeHandle);
                        
                        if(ExMemory.MFTRecords != NULL){
                            extension_search_result NativeResult = NarFindExtensions(letter, 
                                                                                     VolumeHandle, 
                                                                                     ExtensionList,
                                                                                     Extensions->Count,
                                                                                     &ExMemory);
                            Result->Capacity += NativeResult.Len;
                            for(size_t i =0; i<NativeResult.Len; i++){
                                Result->Add(gcnew System::String(NativeResult.Files[i]));
                            } 
                            
                            NarFreeExtensionFinderMemory(&ExMemory);
                        }
                        
                    }
                    
                }
                else {
                    
                }
            }
            
            return Result;
#endif
        }
        
        static List<System::String^>^ FindExtension(wchar_t VolumeLetter, List<System::String^> ^Extensions){

        	return nullptr;
#if NAR_ENABLE_DOTNET_FILE_EXPLORER
            List<System::String^>^ Result = gcnew List<System::String^>;
            
            wchar_t **ExtensionList = (wchar_t**)calloc(Extensions->Count, 8);
            uint64_t ArenaSize = 0;
            for (uint64_t i =0; i < Extensions->Count; i++) {
                ArenaSize += Extensions[i]->Length;
            }

            ArenaSize += (Extensions->Count * 5);
            ArenaSize *= 2; // sizeof(wchar_t)

            nar_arena StringArena   = ArenaInit(calloc(ArenaSize, 1), ArenaSize);

            for (uint64_t i =0; i < Extensions->Count; i++) {
                ExtensionList[i] = (wchar_t*)ArenaAllocate(&StringArena, Extensions[i]->Length * 2 + 2);
                SystemStringToWCharPtr(Extensions[i], ExtensionList[i]);
            }

            HANDLE VolumeHandle = NarOpenVolume(VolumeLetter);
            if(VolumeHandle != INVALID_HANDLE_VALUE){
                
                extension_finder_memory ExMemory = NarSetupExtensionFinderMemory(VolumeHandle);
                
                if(ExMemory.MFTRecords != NULL){
                    extension_search_result NativeResult = NarFindExtensions(VolumeLetter, 
                                                                             VolumeHandle, 
                                                                             ExtensionList, 
                                                                             Extensions->Count,
                                                                             &ExMemory);
                    Result->Capacity += NativeResult.Len;
                    
                    for(size_t i =0; i<NativeResult.Len; i++){
                        Result->Add(gcnew System::String(NativeResult.Files[i]));
                    }
                }
                
                NarFreeExtensionFinderMemory(&ExMemory);
            }
            
            free(ExtensionList);
            free(StringArena.Memory);
            return Result;
#endif

        }
        
    };
    
};

#if 1

namespace NarDIWrapper {
    
    
    public ref class StreamInfo {
        public:
        System::UInt64 BytesToRead;
        System::String^ BinaryExtension;
    };
    
    public ref class DiskInfo {
        public:
        uint64_t Size;
        wchar_t  Type; // MBR, RAW, GPT
        int ID;
    };
    
    public ref class CSNarFileTime {
        public:
        
        CSNarFileTime(){
            
        }
        
        CSNarFileTime(WORD pYear, WORD pMonth, WORD pDay, WORD pHour, WORD pMinute, WORD pSecond){
            Year   = pYear;
            Month  = pMonth;
            Day    = pDay;
            Hour   = pHour;
            Minute = pMinute;
            Second = pSecond;
        }
        
        CSNarFileTime(SYSTEMTIME Time) {
            Year    = Time.wYear;
            Month   = Time.wMonth;
            Day     = Time.wDay;
            Hour    = Time.wHour;
            Minute  = Time.wMinute;
            Second  = Time.wSecond;
        }
        
        WORD Year;
        WORD Month;
        WORD Day;
        WORD Hour;
        WORD Minute;
        WORD Second;
        
    };
    
    public ref class BackupMetadata {
        
        public:
        BackupMetadata(){
        	// backup id's are stored as uint64_t in .net code
        	static_assert(sizeof(unsigned long long) == sizeof(nar_backup_id));
        }
        ~BackupMetadata(){

        }

        unsigned long long BackupID;
        wchar_t Letter;
        int     BackupType;
        int     Version;
        int     OSVolume;
        
        CSNarFileTime^ BackupDate;
        // TODO(Batuhan): 
        size_t VolumeTotalSize;
        size_t VolumeUsedSize;
        size_t BytesNeedToCopy; // just for this version, not cumilative
        size_t MaxWriteOffset;  // last write offset that is going to be made
        
        size_t EFIPartSize;
        size_t SystemPartSize;
        
        wchar_t DiskType;

        System::String^ Metadataname; // 
        System::String^ WindowsName;
        System::String^ TaskName;
        System::String^ TaskDescription;
        System::String^ ComputerName;
        System::String^ IpAdress;
                
        /*
            ComputerName
            IpAdress
            UsedSize
            VolumeSize
        */
    };
    
    public ref class VolumeInformation {
        public:
        ULONGLONG TotalSize; //in bytes!
        ULONGLONG FreeSize;
        System::String^ VolumeName;
        BOOLEAN Bootable; // Healthy && NTFS && !Boot
        char Letter;
        INT8 DiskID;
        char DiskType;
    };
    
        
    public ref class ExistingBackupInfo {
        public:
        bool Exists;
        wchar_t Letter;
        int Version;
    };
    
    
    public ref class BackupReadResult{
        
        public:
        BackupReadResult(){
            
        }
        
        uint32_t WriteSize  = 0;
        uint32_t DecompressedSize = 0;
        BackupStream_Errors Error = BackupStream_Errors::Error_NoError;
    };
    
    
    public ref class RestoreStream {
        
        bool SetDiskRestore(int DiskID, wchar_t Letter, bool arg_RepairBoot, bool OverrideDiskType, wchar_t AlternativeDiskType) {
            
            #if 0
            char DiskType = (char)BM->DiskType;
            if (OverrideDiskType)
                DiskType = (char)AlternativeDiskType;
            
            RepairBoot = arg_RepairBoot;
            
            int VolSizeMB = BM->VolumeTotalSize/(1024ull* 1024ull) + 1;
            int SysPartitionMB = BM->EFIPartSize / (1024ull * 1024ull);
            int RecPartitionMB = 0;
            
            
            BootLetter = 0;
            {
                DWORD Drives = GetLogicalDrives();
                
                for (int CurrentDriveIndex = 0; CurrentDriveIndex < 26; CurrentDriveIndex++) {
                    if (Drives & (1 << CurrentDriveIndex) || ('A' + (char)CurrentDriveIndex) == (char)Letter) {
                        continue;
                    }
                    else {
                        BootLetter = ('A' + (char)CurrentDriveIndex);
                        break;
                    }
                }
            }
            
            if (BootLetter != 0) {
                if (DiskType == NAR_DISKTYPE_GPT) {
                    if (BM->OSVolume) {
                        NarCreateCleanGPTBootablePartition(DiskID, VolSizeMB, SysPartitionMB, RecPartitionMB, Letter, BootLetter);
                    }
                    else {
                        NarCreateCleanGPTPartition(DiskID, VolSizeMB, Letter);
                    }
                }
                if (DiskType == NAR_DISKTYPE_MBR) {
                    if (BM->OSVolume) {
                        NarCreateCleanMBRBootPartition(DiskID, Letter, VolSizeMB, SysPartitionMB, RecPartitionMB, BootLetter);
                    }
                    else {
                        NarCreateCleanMBRPartition(DiskID, Letter, VolSizeMB);
                    }
                }
                
            }
            else {
                return false;
            }
            
            
            IsDiskRestore = true;
            return true;
            #endif
            return false;
        }
        
        
    };
    
    
    //static uint64_t CW_SetupFullOnlyStream(StreamInfo^ StrInf, wchar_t Letter, bool ShouldCompress, bool RegionLock);
    //static BackupReadResult^ CW_ReadFullOnlyStream(uint64_t BackupID, void* Data, uint32_t Size);
    //
    //static void CW_TerminateFullOnlyBackup(uint64_t BackupID, bool ShouldSaveMetadata, System::String^ MetadataPath);
    
    
    public ref class DiskTracker
    {
        public:
        
        DiskTracker() {
            if(msInit == 0){
                msInit = true;
                C = NarLoadBootState();
                
                // when loading, always check for old states, if one is not presented, create new one from scratch
                if (C == NULL) {
                    printf("Couldn't load from boot file, initializing new CONTEXT\n");
                    C = new LOG_CONTEXT;
                }
                else {
                    // found old state
                    printf("Succ loaded boot state from file\n");
                    for(int i =0; i<C->Volumes.Count; i++){
                        printf("WR BOOT LOAD : Volume %c, version %d, type %d\n", C->Volumes.Data[i].Letter, C->Volumes.Data[i].Version, C->Volumes.Data[i].BT);
                    }
                }
                
                C->Port = INVALID_HANDLE_VALUE;
                int32_t CDResult = ConnectDriver(C);
                msIsDriverConnected = CDResult;
                
            }

        }

        ~DiskTracker() {
            // We don't ever need to free anything, this class-object is supposed to be ALWAYS alive with program 
        }
        
        
        bool CW_InitTracker() {
            return msIsDriverConnected;
        }

        static List<VolumeInformation^>^ CW_GetVolumes() {
            data_array<volume_information> V = NarGetVolumes();
        
            List<VolumeInformation^>^ Result = gcnew  List<VolumeInformation^>;
            
            for (int i = 0; i < V.Count; i++) {
                
                VolumeInformation^ BI = gcnew VolumeInformation;
                BI->TotalSize  = V.Data[i].TotalSize;
                BI->FreeSize   = V.Data[i].FreeSize;
                BI->Bootable   = V.Data[i].Bootable;
                BI->DiskID     = (int8_t)V.Data[i].DiskID;
                BI->DiskType   = V.Data[i].DiskType;
                BI->Letter     = V.Data[i].Letter;
                BI->VolumeName = gcnew System::String(V.Data[i].VolumeName);
                if (BI->VolumeName->Length == 0) {
                    BI->VolumeName = L"Local Disk";
                }
                
                Result->Add(BI);
            }
            
            FreeDataArray(&V);
            return Result;
        }
        

        bool CW_RetryDriverConnection() {
            if (C == 0) {
                int32_t CDResult = ConnectDriver(C);
                msIsDriverConnected = CDResult;
            }
            return msIsDriverConnected;
        }
        
        bool CW_AddToTrack(wchar_t Letter, int Type) {
            return AddVolumeToTrack(C, Letter, (BackupType)Type);
        }
        
        bool CW_RemoveFromTrack(wchar_t Letter) {
            BOOLEAN Result = FALSE;
            if(RemoveVolumeFromTrack(C, Letter)){
                if(NarSaveBootState(C)){
                    Result = TRUE;
                }
                else{
                    printf("WRAPPER : Failed to save state of the program\n");
                }
            }
            else{
                printf("WRAPPER : Failed to remove volume %c from track list\n", Letter);
            }
            return Result;
        }
        
        bool CW_SetupStream(wchar_t L, int BT, StreamInfo^ StrInf, bool ShouldCompress) {

        	uint64_t BytesToTransfer = 0;
        	char BinaryExtension[256];
        	memset(BinaryExtension, 0, sizeof(BinaryExtension));
        	if (SetupStream(C, L, (BackupType)BT, &BytesToTransfer, BinaryExtension, NAR_COMPRESSION_ZSTD)) {
        		// everything is ok!
        		StrInf->BytesToRead     = BytesToTransfer;
        		StrInf->BinaryExtension = gcnew System::String(BinaryExtension); 	
        		return true;
        	}
        	else {
        		return false;
        	}

        }
        
        BackupReadResult^ CW_ReadStream(wchar_t VolumeLetter, void* Data, int Size) {
        	
        	BackupReadResult^ Result = gcnew BackupReadResult();

        	int32_t ID = GetVolumeID(C, VolumeLetter);
        	if (ID!=NAR_INVALID_VOLUME_TRACK_ID) {
        		auto Volume = &C->Volumes.Data[ID];
        		uint32_t Readed = ReadStream(&Volume->Stream, Data, Size);
    			Result->Error = Volume->Stream.Error;
    			Result->WriteSize        = Readed;
    			Result->DecompressedSize = Volume->Stream.BytesProcessed;
        	}
        	else {
        		// fatal error! that shouldn't be possible
        	}

        	return Result;
        }
                
        System::String^ CW_TerminateBackup(bool Succeeded, wchar_t VolumeLetter, System::String^ MetadataDirectory) {
			
			System::String^ Result = nullptr;
			int32_t ID = GetVolumeID(C, VolumeLetter);
			if (ID != NAR_INVALID_VOLUME_TRACK_ID) {

				// convert wchar to utf8(actually, ascii)
				char *DirectoryToEmitMetadata = NULL;
				{
					wchar_t DirectoryToEmitMetadataWCHR[267];
					memset(DirectoryToEmitMetadataWCHR, 0, sizeof(DirectoryToEmitMetadataWCHR));

					SystemStringToWCharPtr(MetadataDirectory, DirectoryToEmitMetadataWCHR);
					DirectoryToEmitMetadata = NarWCHARToUTF8(DirectoryToEmitMetadataWCHR);
				}

				char OutputMetadataName[256];
				memset(OutputMetadataName, 0, sizeof(OutputMetadataName));
				if (!!TerminateBackup(&C->Volumes.Data[ID], Succeeded, DirectoryToEmitMetadata, OutputMetadataName)) {
					Result = gcnew System::String(OutputMetadataName);
				}
				else {

				}

				free(DirectoryToEmitMetadata);
			}
			else {

			}

			return Result;
        }
        
        unsigned long long CW_IsVolumeExists(wchar_t Letter){
	        unsigned long long Result = 0;
	        INT32 bcid = GetVolumeID(C, Letter);
	        if(bcid != NAR_INVALID_VOLUME_TRACK_ID){
	            Result = C->Volumes.Data[bcid].BackupID.Q;
	        }
	        return Result;
	    }

        static wchar_t CW_GetFirstAvailableVolumeLetter() {
	        return NarGetAvailableVolumeLetter();
        }

        static List<BackupMetadata^>^ CW_GetBackupsInDirectory(System::String^ RootDir) {
        	return nullptr;
        }

        static List<DiskInfo^>^ CW_GetDisksOnSystem() {

	        data_array<disk_information> CResult = NarGetDisks();
	        if (CResult.Data == NULL || CResult.Count == 0) {
	            if (CResult.Count == 0) printf("Found 0 disks on the system\n");
	            if (CResult.Data  == 0) printf("GetDisksOnSystem returned NULL\n");
	            return nullptr;
	        }
	        
	        List<DiskInfo^>^ Result = gcnew List<DiskInfo^>;
	        printf("Found %i disks on the system\n", CResult.Count);
	        for (int i = 0; i < CResult.Count; i++) {
	            DiskInfo^ temp = gcnew DiskInfo;
	            temp->ID   = CResult.Data[i].ID;
	            temp->Size = CResult.Data[i].Size;
	            temp->Type = CResult.Data[i].Type;
	            Result->Add(temp);
	        }
	        FreeDataArray(&CResult);
	        
	        return Result;
        }

        static BOOLEAN CW_MetadataEditTaskandDescriptionField(System::String^ MetadataFileName, System::String^ TaskName, System::String^ TaskDescription) {
	        wchar_t wcTaskName[128];
	        wchar_t wcTaskDescription[512];
	        wchar_t wcMetadataFileName[MAX_PATH];
	        
	        SystemStringToWCharPtr(TaskName        , wcTaskName);
	        SystemStringToWCharPtr(MetadataFileName, wcMetadataFileName);
	        SystemStringToWCharPtr(TaskDescription , wcTaskDescription);

	        return NarEditTaskNameAndDescription(wcMetadataFileName, wcTaskName, wcTaskDescription);
        }
        
        static bool CW_IsVolumeAvailable(wchar_t Letter) {
	        return NarIsVolumeAvailable((char)Letter);
        }

        //static uint64_t CW_SetupFullOnlyStream(StreamInfo^ StrInf, wchar_t Letter, bool ShouldCompress);
        //static uint64_t CW_SetupDiskCloneStream(StreamInfo^ StrInf, wchar_t Letter);
        //static BackupReadResult^ CW_ReadFullOnlyStream(uint64_t BackupID, void* Data, uint32_t Size);
        //static void CW_TerminateFullOnlyBackup(uint64_t BackupID, bool ShouldSaveMetadata, System::String^ MetadataPath);
        
        static wchar_t GetDiskType(int DiskID) {
	        // @Incomplete :
	        // @TODO : 
	        return L'R';
        }

        static wchar_t GetVolumeType(wchar_t VolumeLetter) {
	        return NarGetVolumeDiskType((char)VolumeLetter);
        }

        static bool CW_CopyDiskLayout(int SourceDiskID, int TargetDiskID) {
        	ASSERT(false);
        	return false;
        }


        static uint64_t CW_PrepareRestoreFromLocalSource(System::String ^MetadataDirectory, uint64_t BackupIDAsUINT, int32_t Version) {
        	uint64_t Result = 0;
        	nar_backup_id BackupID;
        	BackupID.Q = BackupIDAsUINT;
        	local_restore_ctx *RestoreCtx = (local_restore_ctx *)calloc(sizeof(*RestoreCtx), 1);
        	
        	wchar_t *Dir = (wchar_t *)calloc(Kilobyte(64), 2);
        	SystemStringToWCharPtr(MetadataDirectory, Dir);
        	UTF8 *DirUTF8 = NarWCHARToUTF8(Dir);

            RestoreCtx->BinaryFiles = NarGetBinaryFilesInDirectory(DirUTF8, BackupID, Version);
            if (RestoreCtx->BinaryFiles) {
                if (InitRestore(&RestoreCtx->rctx, DirUTF8, BackupID, Version)) {
                    Result = reinterpret_cast<uint64_t>(RestoreCtx);
                }
            }

            if (Result == 0) {
                FreeRestoreCtx(&RestoreCtx->rctx);
                if (RestoreCtx->BinaryFiles) 
                    NarFreeBinaryFilesInDirectory(RestoreCtx->BinaryFiles);                
            }

        	free(Dir);
        	free(DirUTF8);
        	return Result;
        }


        static void CW_FreeRestoreLocalSource(uint64_t RestoreHandle) {
        	local_restore_ctx *C = reinterpret_cast<local_restore_ctx *>(RestoreHandle);
            NarFreeBinaryFilesInDirectory(C->BinaryFiles);
            FreeRestoreCtx(&C->rctx);
            free(C->Buffer);
        	free(C);
        }
 
        static uint64_t CW_GetRestoreSize(uint64_t RestoreHandle) {
        	Restore_Ctx *C = reinterpret_cast<Restore_Ctx *>(RestoreHandle);
        	return C->target_file_size;
        }

        static uint64_t CW_PrepareRestoreTargetNewDisk(System::String ^MetadataPath, int32_t TargetLetter, bool OverrideDiskType, wchar_t NewDiskType) {

        	wchar_t *Dir = (wchar_t *)calloc(Kilobyte(64), 2);
        	SystemStringToWCharPtr(MetadataPath, Dir);
        	UTF8 *PathUTF8 = NarWCHARToUTF8(Dir);        	

        	uint64_t Result = 0;
        	restore_target *RestoreTarget = (restore_target *)calloc(sizeof(*RestoreTarget), 1);
        	if (NarPrepareRestoreTargetWithNewDisk(RestoreTarget, PathUTF8, TargetLetter)) {
        		Result = reinterpret_cast<uint64_t>(RestoreTarget);
        	}
        	else {
        		free(RestoreTarget);
        		// @Log : 
        		// @Incomplete :
        		// @TODO : 
        	}

        	free(Dir);
        	free(PathUTF8);
        	return Result;
        }

        static uint64_t CW_PrepareRestoreTargetVolume(System::String ^MetadataPath, int32_t TargetLetter) {

        	wchar_t *Dir = (wchar_t *)calloc(Kilobyte(64), 2);
        	SystemStringToWCharPtr(MetadataPath, Dir);
        	UTF8 *PathUTF8 = NarWCHARToUTF8(Dir);        	
            
            uint64_t Result = 0;
            restore_target *Target = (restore_target *)calloc(sizeof(*Target), 1);
            if (NarPrepareRestoreTargetVolume(Target, PathUTF8, TargetLetter)) {
            	Result = reinterpret_cast<uint64_t>(Target);
            }
            else {
            	free(Target);
            	// @TODO :
            	// @Log : 
            	// @Incomplete : 
            }

            free(Dir);
            free(PathUTF8);
        	return Result;
        }

        static uint64_t CW_PrepareRestoreTargetFile(System::String ^MetadataPath) {
        	// @TODO : 
        	// @Incomplete : 
        	return 0;
        }

        static void CW_FreeRestoreTarget(uint64_t RestoreTargetHandle) {
        	restore_target *Target = reinterpret_cast<restore_target *>(RestoreTargetHandle);
        	NarFreeRestoreTarget(Target);
        	free(Target);
        }

        static bool CW_FeedRestoreTarget(uint64_t RestoreTargetHandle, const void *Buffer, int32_t BufferSize) {
        	return NarFeedRestoreTarget(reinterpret_cast<restore_target *>(RestoreTargetHandle), Buffer, BufferSize);
        }


        private:
        
        static volatile SHORT msInit    = 0;
        static volatile SHORT msIsDriverConnected = 0;
        static LOG_CONTEXT* C = nullptr;
    };
    
    
    
    
}


#endif