
#include "pch.h"


#include <msclr\marshal.h>
#include <msclr\marshal_cppstd.h>

#include <msclr/marshal.h>
#include <Windows.h>
#include <stdio.h>


#include "NarDIWrapper.h"
#include "mspyLog.h"

#if 0
#include "nar.cpp"
#include "platform_io.cpp"
#include "file_explorer.cpp"
#include "restore.cpp"
#include "backup.cpp"
#include "narstring.cpp"
#endif

using namespace System;

#define CONVERT_TYPES(_in,_out) _out = msclr::interop::marshal_as<decltype(_out)>(_in);

#if 1
namespace NarDIWrapper {
    
    
    CSNarFileExplorer::CSNarFileExplorer(System::String^ MetadataFullPath){
        Arena  = (nar_arena*)calloc(1, sizeof(*Arena));
        *Arena = ArenaInit(calloc(Kilobyte(64), 1), Kilobyte(64), 4);
        
        wchar_t *WSTR  = (wchar_t*)ArenaAllocateZeroAligned(Arena, MetadataFullPath->Length + 2, 2);
        
        SystemStringToWCharPtr(MetadataFullPath, WSTR);
        
        __DirStackMax = 1024;
        __DirStack = (file_explorer_file**)ArenaAllocate(Arena, __DirStackMax);
        
        NarUTF8 MetadataPath = NarWCHARToUTF8(WSTR, Arena);
        FE  = (file_explorer*)ArenaAllocate(Arena, sizeof(*FE));
        *FE = NarInitFileExplorer(MetadataPath);
        __DSI = 0;
        __CurrentDir = (__DirStack[__DSI] = FEFindFileWithID(FE, 5));
    }
    
    CSNarFileExplorer::~CSNarFileExplorer() {
        NarFreeFileExplorer(FE);
        free(Arena->Memory);
        free(Arena);
    }
    
    bool CSNarFileExplorer::CW_IsInit(){
        return (FE->MetadataView.Data != 0);
    }
    
    
    
    List<CSNarFileEntry^>^ CSNarFileExplorer::CW_GetFilesInCurrentDirectory(){
        
        List<CSNarFileEntry^>^ Result = gcnew List<CSNarFileEntry^>;
        
        for(file_explorer_file *File = FEStartParentSearch(FE, __CurrentDir->FileID); 
            File != 0;
            File = FENextFileInDir(FE, File))
        {
            CSNarFileEntry^ F = gcnew CSNarFileEntry(File);
            Result->Add(F);
        }
        
        return Result;
    }
    
    bool CSNarFileExplorer::CW_SelectDirectory(uint64_t UniqueTargetID){
        __CurrentDir = (__DirStack[++__DSI] = (file_explorer_file*)reinterpret_cast<void*>(UniqueTargetID));
        return true;
    }
    
    void CSNarFileExplorer::CW_PopDirectory(){
        if(__DSI != 0){
            __CurrentDir =  __DirStack[--__DSI];
        }
    }
    
    void CSNarFileExplorer::CW_Free(){
        if (RestoreMemory) {
            free(RestoreMemory);
        }
    }
    
    CSNarFileExportStream^ CSNarFileExplorer::CW_SetupFileRestore(uint64_t UniqueTargetID) {
        CSNarFileExportStream^ Result = gcnew CSNarFileExportStream(this, UniqueTargetID);
        return Result;
    }
    
    CSNarFileExportStream^ CSNarFileExplorer::CW_DEBUG_SetupFileRestore(int FileID) {
        file_explorer_file* File = FEFindFileWithID(FE, FileID);
        //CSNarFileEntry^ Entry = gcnew CSNarFileEntry(File);
        return gcnew CSNarFileExportStream(this, reinterpret_cast<uintptr_t>(File));
    }
    
    System::String^ CSNarFileExplorer::CW_GetCurrentDirectoryString() {
        wchar_t *WSTR = FEGetFileFullPath(FE, __CurrentDir);
        return gcnew System::String(WSTR);
    }
    
    wchar_t DiskTracker::GetDiskType(int DiskId){
        return L'R';
    }
    
    wchar_t DiskTracker::GetVolumeType(wchar_t VolumeLetter){
        return NarGetVolumeDiskType(VolumeLetter);
    }
    
    
    
    CSNarFileExportStream::CSNarFileExportStream(CSNarFileExplorer^ FileExplorer, uint64_t UniqueTargetID) {
        MemorySize = Megabyte(40);
        Memory = VirtualAlloc(0, MemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        
        file_explorer_file *Target = (file_explorer_file*)reinterpret_cast<void*>(UniqueTargetID);
        
        if (Memory) {
            nar_arena TempArena = ArenaInit(Memory, MemorySize);
            Ctx = (file_restore_ctx*)ArenaAllocateZero(&TempArena, sizeof(*Ctx));
            *Ctx = NarInitFileRestoreCtx(FileExplorer->FE, Target, &TempArena);
            TargetFileSize = Target->Size;
            uint8_t CmpBuffer[sizeof(*Ctx)];
            memset(CmpBuffer, 0, sizeof(*Ctx));
            if (memcmp(Ctx, CmpBuffer, sizeof(*Ctx)) == 0) {
                VirtualFree(Memory, MemorySize, MEM_RELEASE);
                Memory = 0;
            }
            
        }
    }
    
    CSNarFileExportStream::~CSNarFileExportStream() {
        FreeStreamResources();
    }
    
    bool CSNarFileExportStream::IsInit() {
        return (Memory != 0);
    }
    
    bool CSNarFileExportStream::AdvanceStream(void* Buffer, size_t BufferSize) {
        if (Memory) {
            file_restore_advance_result Result = NarAdvanceFileRestore(Ctx, Buffer, BufferSize);
            TargetWriteOffset = Result.Offset;
            TargetWriteSize = Result.Len;
            Error = Ctx->Error;
            return (Error == FileRestore_Errors::Error_NoError && TargetWriteSize != 0);
        }
        return false;
    }
    
    void CSNarFileExportStream::FreeStreamResources() {
        if (Memory) {
            NarFreeFileRestoreCtx(Ctx);
            VirtualFree(Memory, MemorySize, MEM_RELEASE);
            Ctx = 0;
            Memory = 0;
        }
    }
    
    
    
    
    DiskTracker::DiskTracker() {
        
        
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
    
    
    DiskTracker::~DiskTracker() {
        // We don't ever need to free anything, this class-object is supposed to be ALWAYS alive with program 
    }
    
    
    
    bool DiskTracker::CW_InitTracker() {
        return msIsDriverConnected;
    }
    
    bool DiskTracker::CW_RetryDriverConnection() {
        if (C == 0) {
            int32_t CDResult = ConnectDriver(C);
            msIsDriverConnected = CDResult;
        }
        return msIsDriverConnected;
    }
    
    bool DiskTracker::CW_AddToTrack(wchar_t L, int Type) {
        return AddVolumeToTrack(C, L, (BackupType)Type);
    }
    
    bool DiskTracker::CW_RemoveFromTrack(wchar_t Letter) {
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
    
    bool DiskTracker::CW_SetupStream(wchar_t L, int BT, StreamInfo^ StrInf, bool ShouldCompress) {
        
        DotNetStreamInf SI = { 0 };
        printf("WRAPPER : SETUP STREAM CALLED, LETTER %c\n", L);
        if (SetupStream(C, L, (BackupType)BT, &SI, ShouldCompress)) {
            StrInf->ClusterCount     = SI.ClusterCount;
            StrInf->ClusterSize      = SI.ClusterSize;
            StrInf->FileName         = gcnew String(SI.FileName.c_str());
            StrInf->MetadataFileName = gcnew String(SI.MetadataFileName.c_str());
            StrInf->CopySize         = (UINT64)StrInf->ClusterSize * (UINT64)StrInf->ClusterCount;
            return true;
        }
        else{
        	printf("WRAPPER: Setupstream returned false!\n");
        }
        
        return false;
    }
    
    
    
    // returns 0 if one is not present
    wchar_t DiskTracker::CW_GetFirstAvailableVolumeLetter(){
        return NarGetAvailableVolumeLetter();
    }
    
    bool DiskTracker::CW_IsVolumeAvailable(wchar_t Letter){
        return NarIsVolumeAvailable(Letter);
    }
    
    
    BackupReadResult^ DiskTracker::CW_ReadStream(void* Data, wchar_t VolumeLetter, int Size) {
        
        BackupReadResult ^Res = gcnew BackupReadResult;
        
        int VolID = GetVolumeID(C, VolumeLetter);
        if(VolID != NAR_INVALID_VOLUME_TRACK_ID){
            
            uint32_t WriteSize     = ReadStream(&C->Volumes.Data[VolID].Stream, Data, Size);
            Res->WriteSize        = WriteSize;
            Res->DecompressedSize = 0;
            
            if(true == C->Volumes.Data[VolID].Stream.ShouldCompress){
                Res->DecompressedSize = C->Volumes.Data[VolID].Stream.BytesProcessed;
            }
            Res->Error = C->Volumes.Data[VolID].Stream.Error;
            
            
        }
        // couldnt find volume in the Context, which shoudlnt happen at all
        
        return Res;
    }
    
    uint64_t DiskTracker::CW_SetupFullOnlyStream(StreamInfo^ StrInf, wchar_t Letter, bool ShouldCompress){
        
        DotNetStreamInf SI = {};
        uint64_t Result = 0;
        full_only_backup_ctx Ctx = SetupFullOnlyStream(Letter, &SI, ShouldCompress);
        
        if(Ctx.InitSuccess){
            StrInf->ClusterCount     = SI.ClusterCount;
            StrInf->ClusterSize      = SI.ClusterSize;
            StrInf->FileName         = gcnew String(SI.FileName.c_str());
            StrInf->MetadataFileName = gcnew String(SI.MetadataFileName.c_str());
            StrInf->CopySize         = (UINT64)StrInf->ClusterSize * (UINT64)StrInf->ClusterCount;
            
            full_only_backup_ctx *Temp = (full_only_backup_ctx*)malloc(sizeof(*Temp));
            memcpy(Temp, &Ctx, sizeof(Ctx));
            Result = reinterpret_cast<uintptr_t>(Temp);
            return Result;
        }
        
        return 0;
    }
    
    uint64_t DiskTracker::CW_SetupDiskCloneStream(StreamInfo^ StrInf, wchar_t Letter){
        DotNetStreamInf SI = {};
        uint64_t Result = 0;
        full_only_backup_ctx Ctx = SetupFullOnlyStream(Letter, &SI, false, true);
        
        if(Ctx.InitSuccess){
            StrInf->ClusterCount     = SI.ClusterCount;
            StrInf->ClusterSize      = SI.ClusterSize;
            StrInf->FileName         = gcnew String(SI.FileName.c_str());
            StrInf->MetadataFileName = gcnew String(SI.MetadataFileName.c_str());
            StrInf->CopySize         = (UINT64)StrInf->ClusterSize * (UINT64)StrInf->ClusterCount;
            
            full_only_backup_ctx *Temp = (full_only_backup_ctx*)malloc(sizeof(*Temp));
            memcpy(Temp, &Ctx, sizeof(Ctx));
            Result = reinterpret_cast<uintptr_t>(Temp);
            return Result;
        }
        return 0;
    }
    
    BackupReadResult^ DiskTracker::CW_ReadFullOnlyStream(uint64_t BackupID, void *Data, uint32_t Size){
        
        BackupReadResult ^Res = gcnew BackupReadResult;
        auto Ctx = (full_only_backup_ctx*)reinterpret_cast<void*>(BackupID);
        
        uint32_t WriteSize    = ReadStream(&Ctx->Stream, Data, Size);
        Res->WriteSize        = WriteSize;
        Res->DecompressedSize = 0;
        
        if(true == Ctx->Stream.ShouldCompress){
            Res->DecompressedSize = Ctx->Stream.BytesProcessed;
        }
        
        Res->ReadOffset = Ctx->Stream.BytesReadOffset;
        Res->Error = Ctx->Stream.Error;
        return Res;
    }
    
    void DiskTracker::CW_TerminateFullOnlyBackup(uint64_t BackupID, bool ShouldSaveMetadata, System::String^ MetadataPath) {
        
        wchar_t path[512];
        SystemStringToWCharPtr(MetadataPath, path);

        auto Ctx = (full_only_backup_ctx*)reinterpret_cast<void*>(BackupID);
        TerminateFullOnlyStream(Ctx, ShouldSaveMetadata, path);
        free(Ctx);
    }
    
    // returns false if internal error occured
    bool DiskTracker::CW_CheckStreamStatus(wchar_t Letter) {
        int VolID = GetVolumeID(C, Letter);
        if (VolID != NAR_INVALID_VOLUME_TRACK_ID) {
            return C->Volumes.Data[VolID].Stream.Error == BackupStream_Errors::Error_NoError;
        }
    }
    
    int DiskTracker::CW_HintBufferSize() {
        return NAR_COMPRESSION_FRAME_SIZE + Megabyte(4);
    }
    
    
    bool DiskTracker::CW_TerminateBackup(bool Succeeded, wchar_t VolumeLetter, System::String^ MetadataDirectory) {

        wchar_t path[512];
        SystemStringToWCharPtr(MetadataDirectory, path);

        INT32 VolID = GetVolumeID(C, VolumeLetter);
        
        if(VolID != NAR_INVALID_VOLUME_TRACK_ID){
            if(TerminateBackup(&C->Volumes.Data[VolID], Succeeded, path)){
                return NarSaveBootState(C);
            }
            else{
                printf("WRAPPER : Terminate backup failed!\n");
            }
        }
        
        return FALSE;   
    }
    
    // Returns backup id of the volume if it exists in backup system(and in kernel)
    unsigned long long DiskTracker::CW_IsVolumeExists(wchar_t Letter){
        unsigned long long Result = 0;
        INT32 bcid = GetVolumeID(C, Letter);
        if(bcid != NAR_INVALID_VOLUME_TRACK_ID){
            Result = C->Volumes.Data[bcid].BackupID.Q;
        }
        return Result;
    }
    
    List<VolumeInformation^>^ DiskTracker::CW_GetVolumes() {
        data_array<volume_information> V = NarGetVolumes();
        
        List<VolumeInformation^>^ Result = gcnew  List<VolumeInformation^>;
        
        for (int i = 0; i < V.Count; i++) {
            
            VolumeInformation^ BI = gcnew VolumeInformation;
            BI->TotalSize = V.Data[i].TotalSize;
            BI->FreeSize = V.Data[i].FreeSize;
            BI->Bootable = V.Data[i].Bootable;
            BI->DiskID = V.Data[i].DiskID;
            BI->DiskType = V.Data[i].DiskType;
            BI->Letter = V.Data[i].Letter;
            BI->VolumeName = gcnew System::String(V.Data[i].VolumeName);
            if (BI->VolumeName->Length == 0) {
                BI->VolumeName = L"Local Disk";
            }
            
            Result->Add(BI);
        }
        
        FreeDataArray(&V);
        return Result;
    }
    
    List<DiskInfo^>^ DiskTracker::CW_GetDisksOnSystem() {
        data_array<disk_information> CResult = NarGetDisks();
        if (CResult.Data == NULL || CResult.Count == 0) {
            if (CResult.Count == 0) printf("Found 0 disks on the system\n");
            if (CResult.Data == 0) printf("GetDisksOnSystem returned NULL\n");
            
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
    
    List<BackupMetadata^>^ DiskTracker::CW_GetBackupsInDirectory(System::String^ SystemStrRootDir) {
        wchar_t RootDir[256];
        SystemStringToWCharPtr(SystemStrRootDir, RootDir);
        
        List<BackupMetadata^>^ ResultList = gcnew List<BackupMetadata^>;
        int MaxMetadataCount = 128;
        int Found = 0;
        backup_metadata* BMList = (backup_metadata*)malloc(sizeof(backup_metadata) * MaxMetadataCount);
        
        BOOLEAN bResult = NarGetBackupsInDirectory(RootDir, BMList, MaxMetadataCount, &Found);
        if (bResult && Found <= MaxMetadataCount) {
            

            for (int i = 0; i < Found; i++) {
                std::wstring pth;
                
                BackupMetadata^ BMet = gcnew BackupMetadata;
                BMet->Letter = (wchar_t)BMList[i].Letter;
                BMet->BackupType = (int)BMList[i].BT;
                BMet->DiskType = BMList[i].DiskType;
                BMet->OSVolume = BMList[i].IsOSVolume;
                BMet->Version = BMList[i].Version;
                
                BMet->WindowsName      = gcnew System::String(BMList[i].ProductName);
                BMet->TaskName         = gcnew System::String(BMList[i].TaskName);
                BMet->TaskDescription  = gcnew System::String(BMList[i].TaskDescription);
                BMet->ComputerName     = gcnew System::String(BMList[i].ComputerName);
                BMet->IpAdress         = gcnew System::String(" ");
                
                BMet->VolumeTotalSize = BMList[i].VolumeTotalSize;
                BMet->VolumeUsedSize = BMList[i].VolumeUsedSize;
                BMet->BytesNeedToCopy = BMList[i].Size.Regions;
                BMet->MaxWriteOffset =  BMList[i].VersionMaxWriteOffset;
                *BMet->BackupID = BMList[i].ID;
                
                BMet->EFIPartSize       = BMList[i].GPT_EFIPartitionSize;
                BMet->SystemPartSize    = BMList[i].MBR_SystemPartitionSize;
                
                BMet->BackupDate = gcnew CSNarFileTime(BMList[i].BackupDate.wYear, BMList[i].BackupDate.wMonth, BMList[i].BackupDate.wDay, BMList[i].BackupDate.wHour, BMList[i].BackupDate.wMinute, BMList[i].BackupDate.wSecond);
                
                GenerateBinaryFileName(*BMet->BackupID, BMet->Version, pth);
                BMet->Fullpath = gcnew System::String((std::wstring(RootDir) + pth).c_str());
                
                GenerateMetadataName(*BMet->BackupID, BMet->Version, pth);
                BMet->Metadataname = gcnew System::String(pth.c_str());
                
                ResultList->Add(BMet);
            }
        }
        else {
            if (bResult == FALSE) {
                printf("NarGetBackupsInDirectory returned FALSE\n");
            }
            if (Found > MaxMetadataCount) {
                printf("Found metadata count exceeds maxmetdatacount\n");
            }
        }
        
        free(BMList);
        
        return ResultList;
    }
    
    
    BOOLEAN DiskTracker::CW_MetadataEditTaskandDescriptionField(System::String^ MetadataFileName, System::String^ TaskName, System::String^ TaskDescription){
        
        wchar_t wcTaskName[128];
        wchar_t wcTaskDescription[512];
        wchar_t wcMetadataFileName[MAX_PATH];
        
        SystemStringToWCharPtr(TaskName, wcTaskName);
        SystemStringToWCharPtr(MetadataFileName, wcMetadataFileName);
        SystemStringToWCharPtr(TaskDescription, wcTaskDescription);
        
        return NarEditTaskNameAndDescription(wcMetadataFileName, wcTaskName, wcTaskDescription);
    }
    
    List<CSLog^>^ DiskTracker::CW_GetLogs(){
        
        List<CSLog^> ^Result = gcnew List<CSLog^>;
        
#if 0        
        if(WaitForSingleObject(GlobalLogMutex, 100) == WAIT_OBJECT_0){
            for(int i = 0; i<GlobalLogCount; i++){
                CSLog^ Log = gcnew CSLog;
                Log->LogStr = gcnew System::String(GlobalLogs[i].LogString);
                Log->Time = gcnew CSNarFileTime(
                                                GlobalLogs[i].Time.YEAR + 2000, 
                                                GlobalLogs[i].Time.MONTH, 
                                                GlobalLogs[i].Time.DAY,
                                                GlobalLogs[i].Time.HOUR,
                                                GlobalLogs[i].Time.MIN,
                                                GlobalLogs[i].Time.SEC
                                                );
                Result->Add(Log);
            }
            
            ArenaReset(&GlobalMemoryArena);
            GlobalLogCount = 0;
            
            ReleaseMutex(GlobalLogMutex);
        }
        else{
            printf("Couldnt lock log mutex\n");
        }
#endif
        
        return Result;
    }
    
    
    
    
    
    
}

#endif