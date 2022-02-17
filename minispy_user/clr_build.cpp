#include "nar_win32.hpp"
#include "nar.hpp"
#include "platform_io.hpp"

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
    
    
    #if 0
    public ref class CSNarFileEntry {
        public:
        CSNarFileEntry() {
            
        }
        
        CSNarFileEntry(file_explorer_file *File) {
            IsDirectory = File->IsDirectory;
            Size        = File->Size;
            ID          = File->FileID;
            UniqueID    = reinterpret_cast<uintptr_t>(File);
            Name        = gcnew System::String(File->Name);
            Ref         = File;
            CreationTime        = gcnew CSNarFileTime(File->CreationTime);
            LastModifiedTime    = gcnew CSNarFileTime(File->LastModifiedTime);
        }
        
        bool IsDirectory;
        uint64_t Size;
        uint64_t ID;
        uint64_t UniqueID;
        
        CSNarFileTime^ CreationTime;
        CSNarFileTime^ LastModifiedTime;
        System::String^ Name;
        file_explorer_file *Ref;
    };
    
    
    ref class CSNarFileExplorer;
    public ref class CSNarFileExportStream {
        private:
        file_restore_ctx* Ctx;
        void* Memory;
        size_t MemorySize;
        
        
        public:
        
        
        CSNarFileExportStream(CSNarFileExplorer^ FileExplorer, uint64_t UniqueTargetID);
        ~CSNarFileExportStream();        
        
        /*
            Returns false is stream terminates. Caller must check Error enum to determine if restore completed successfully.
            If it returns true, caller must set it's output stream's output position to TargetWriteOffset then must write exactly as 
            TargetWriteSize.
        */
        bool AdvanceStream(void* Buffer, size_t BufferSize);
        
        void FreeStreamResources();
        
        
        bool IsInit();
        
        size_t TargetWriteOffset;
        size_t TargetWriteSize;
        
        size_t TargetFileSize;
        
        FileRestore_Errors Error;
        
    };
    
    public ref class CSNarFileExplorer {
        private:
        
        file_explorer_file **__DirStack;
        file_explorer_file *__CurrentDir;
        uint32_t __DirStackMax;
        uint32_t __DSI;
        void* RestoreMemory;
        size_t RestoreMemorySize;
        
        nar_arena *Arena;
        
        public:
        
        file_explorer* FE;
        
        
        ~CSNarFileExplorer();
        CSNarFileExplorer(System::String^ MetadataFullPath);
        
        bool CW_IsInit();
        
        // Returns list of files-directories in current directory. It doesn't follow any order, caller is free to sort them in any order
        // they want.
        List<CSNarFileEntry^>^ CW_GetFilesInCurrentDirectory();
        
        // Set's current directory as given file entry.
        // Entry should be directory, otherwise function doesnt do anything and returns false
        bool CW_SelectDirectory(uint64_t UniqueFileID);
        
        // Pops up to upper directory, if possible.
        // Is equal to "up to" button in file explorer
        void CW_PopDirectory();
        
        // deconstructor calls this, but in managed code object disposing may be delayed by GC. if caller want to release memory early, it can do by this utility function. 
        void CW_Free();
        
        // Initiates export stream for given Target file entry.
        CSNarFileExportStream^ CW_SetupFileRestore(uint64_t UniqueTargetID);
        
        // DEBUG function for specific usage areas.
        CSNarFileExportStream^ CW_DEBUG_SetupFileRestore(int FileID);
        
        // Returns the current directory string
        System::String^ CW_GetCurrentDirectoryString();
        
    };
    #endif
    
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
        
        DiskTracker();
        ~DiskTracker();
        
        static List<VolumeInformation^>^ CW_GetVolumes();
        
        bool CW_InitTracker();
        bool DiskTracker::CW_RetryDriverConnection();
        
        bool CW_AddToTrack(wchar_t Letter, int Type);
        
        bool CW_RemoveFromTrack(wchar_t Letter);
        
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
	            if (CResult.Data == 0)  printf("GetDisksOnSystem returned NULL\n");
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
	        
	        SystemStringToWCharPtr(TaskName, wcTaskName);
	        SystemStringToWCharPtr(MetadataFileName, wcMetadataFileName);
	        SystemStringToWCharPtr(TaskDescription, wcTaskDescription);
	        

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
        	Restore_Ctx *RestoreCtx = (Restore_Ctx *)calloc(sizeof(*RestoreCtx), 1);
        	
        	wchar_t *Dir = (wchar_t *)calloc(Kilobyte(64), 2);
        	SystemStringToWCharPtr(MetadataDirectory, Dir);
        	UTF8 *DirUTF8 = NarWCHARToUTF8(Dir);

        	if (InitRestore(RestoreCtx, DirUTF8, BackupID, Version)) {
        		Result = reinterpret_cast<uint64_t>(RestoreCtx);
        	}

        	free(Dir);
        	free(DirUTF8);
        	return Result;
        }


        static void CW_FreeRestoreLocalSource(uint64_t RestoreHandle) {
        	Restore_Ctx *C = reinterpret_cast<Restore_Ctx *>(RestoreHandle);
        	FreeRestoreCtx(C);
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