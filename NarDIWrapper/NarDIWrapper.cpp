
#include "pch.h"


#include <msclr\marshal.h>
#include <msclr\marshal_cppstd.h>

#include <msclr/marshal.h>
#include <Windows.h>
#include <stdio.h>


#include "mspyUser.cpp"
#include "NarDIWrapper.h"


using namespace System;

#define CONVERT_TYPES(_in,_out) _out = msclr::interop::marshal_as<decltype(_out)>(_in);




namespace NarDIWrapper {
    
    
    CSNarFileExplorer::CSNarFileExplorer(){
        ctx = 0;
    }
    
    CSNarFileExplorer::~CSNarFileExplorer() {
        this->CW_Free();
    }
    
    bool CSNarFileExplorer::CW_Init(System::String^ SysRootDir, System::String^ SysMetadataName){
        
        ctx = (nar_backup_file_explorer_context*)malloc(sizeof(nar_backup_file_explorer_context));
        if(ctx == NULL){
            printf("Couldnt allocate memory for file explorer context\n");
        }
        
        wchar_t rootdir[1024];
        wchar_t mname[1024];
        memset(rootdir, 0, sizeof(rootdir));
        memset(mname, 0, sizeof(mname));
        
        SystemStringToWCharPtr(SysRootDir, rootdir);
        SystemStringToWCharPtr(SysMetadataName, mname);
        
        printf("Rootdir %S, name %S\n", rootdir, mname);
        
        return NarInitFileExplorerContext(ctx, rootdir, mname);
        
    }
    
    List<CSNarFileEntry^>^ CSNarFileExplorer::CW_GetFilesInCurrentDirectory(){
        
        List<CSNarFileEntry^>^ Result = gcnew List<CSNarFileEntry^>;     
        
        ULARGE_INTEGER templargeinteger = { 0 };
        FILETIME ft;
        SYSTEMTIME st;
        
        for(int i = 0; i<ctx->EList.EntryCount; i++){
            
            CSNarFileEntry^ Entry = gcnew CSNarFileEntry;
            Entry->Size = ctx->EList.Entries[i].Size;
            Entry->ID = i;
            Entry->Name = gcnew System::String(ctx->EList.Entries[i].Name);
            
            Entry->LastModifiedTime = gcnew CSNarFileTime;
            Entry->CreationTime = gcnew CSNarFileTime;
            
            templargeinteger.QuadPart = ctx->EList.Entries[i].CreationTime;
            ft.dwHighDateTime = templargeinteger.HighPart;
            ft.dwLowDateTime =  templargeinteger.LowPart;
            FileTimeToSystemTime(&ft, &st);
            
            Entry->CreationTime->Year = st.wYear;
            Entry->CreationTime->Month = st.wMonth;
            Entry->CreationTime->Day = st.wDay;
            Entry->CreationTime->Hour = st.wHour;
            Entry->CreationTime->Minute = st.wMinute;
            Entry->CreationTime->Second = st.wSecond;
            
            
            templargeinteger.QuadPart = ctx->EList.Entries[i].LastModifiedTime;
            ft.dwHighDateTime = templargeinteger.HighPart;
            ft.dwLowDateTime = templargeinteger.LowPart;
            FileTimeToSystemTime(&ft, &st);
            
            Entry->LastModifiedTime->Year = st.wYear;
            Entry->LastModifiedTime->Month = st.wMonth;
            Entry->LastModifiedTime->Day = st.wDay;
            Entry->LastModifiedTime->Hour = st.wHour;
            Entry->LastModifiedTime->Minute = st.wMinute;
            Entry->LastModifiedTime->Second = st.wSecond;
            
            Entry->IsDirectory = ctx->EList.Entries[i].IsDirectory;
            Result->Add(Entry);
            
        }
        
        printf("Build file list with length %i\n", Result->Count);
        
        return Result;
        
    }
    
    bool CSNarFileExplorer::CW_SelectDirectory(UINT64 ID){
        
        //NarFileExplorerPushDirectory(ctx, ID);
        return true;
    }
    
    void CSNarFileExplorer::CW_PopDirectory(){
        
        //NarFileExplorerPopDirectory(ctx);
        
    }
    
    void CSNarFileExplorer::CW_Free(){
        
#if 1        
        if (ctx) {
            NarReleaseFileExplorerContext(ctx);
            free(ctx);
            ctx = 0;
        }
#endif
        
    }
    
    void CSNarFileExplorer::CW_RestoreFile(INT64 ID, System::String^ SysBackupDirectory, System::String^ SysTargetDir) {
        
#if 0
        if(ctx == NULL || ctx->EList.Entries == 0 || ctx->EList.EntryCount < ID){
            if(!ctx) printf("File explorer context was null\n");
            if(!ctx->EList.Entries) printf("File explorer's entry list was null\n");
            if(ctx->EList.EntryCount  < ID) printf("Given file ID exceeds maximum file ID in entry list\n");
            return;
        }
        
        std::wstring SelectedFilePath = std::wstring(ctx->CurrentDirectory);
        SelectedFilePath += ctx->EList.Entries[ID].Name;
        
        wchar_t TargetDirectory[512];
        wchar_t RootDir[512];
        SystemStringToWCharPtr(SysTargetDir, TargetDirectory);
        SystemStringToWCharPtr(SysBackupDirectory, RootDir);
        
        NarRestoreFileFromBackups(RootDir, SelectedFilePath.c_str(), TargetDirectory, ctx->FEHandle.BMEX->M.ID, NAR_FULLBACKUP_VERSION);
#endif   
        
    }
    
    System::String^ CSNarFileExplorer::CW_GetCurrentDirectoryString() {
        System::String^ result = gcnew System::String(ctx->CurrentDirectory);
        return result;
    }
    
    
    
    
    DiskTracker::DiskTracker() {
        C = NarLoadBootState();
        // when loading, always check for old states, if one is not presented, create new one from scratch
        if (C == NULL) {
            printf("Coulndt load from boot file, initializing new CONTEXT\n");
            // couldnt find old state
            C = (LOG_CONTEXT*)malloc(sizeof(LOG_CONTEXT));
            memset(C, 0, sizeof(LOG_CONTEXT));
        }
        else {
            // found old state
            printf("Succ loaded boot state from file\n");
        }
        
        C->Port = INVALID_HANDLE_VALUE;
    }
    
    DiskTracker::~DiskTracker() {
        //Do deconstructor things
        
        free(C->Volumes.Data);
        
        delete C;
    }
    
    
    
    bool DiskTracker::CW_InitTracker() {
        return ConnectDriver(C);
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
                printf("Failed to save state of the program\n");
            }
        }
        else{
            printf("Failed to remove volume %c from track list\n", Letter);
        }
        return Result;
    }
    
    bool DiskTracker::CW_SetupStream(wchar_t L, int BT, StreamInfo^ StrInf, bool ShouldCompress) {
        
        DotNetStreamInf SI = { 0 };
        if (SetupStream(C, L, (BackupType)BT, &SI, ShouldCompress)) {
            
            StrInf->ClusterCount = SI.ClusterCount;
            StrInf->ClusterSize = SI.ClusterSize;
            StrInf->FileName = gcnew String(SI.FileName.c_str());
            StrInf->MetadataFileName = gcnew String(SI.MetadataFileName.c_str());
            StrInf->CopySize = (UINT64)StrInf->ClusterSize * (UINT64)StrInf->ClusterCount;
            
            return true;
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
            
            uint32_t WriteSize     = ReadStream(&C->Volumes.Data[VolID], Data, Size);
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
    
    // returns false if internal error occured
    bool DiskTracker::CW_CheckStreamStatus(wchar_t Letter) {
        int VolID = GetVolumeID(C, Letter);
        if (VolID != NAR_INVALID_VOLUME_TRACK_ID) {
            return C->Volumes.Data[VolID].Stream.Error == BackupStream_Errors::Error_NoError;
        }
    }
    
    int DiskTracker::CW_HintBufferSize() {
        return NAR_COMPRESSION_FRAME_SIZE + Megabyte(1);
    }
    
    
    bool DiskTracker::CW_TerminateBackup(bool Succeeded, wchar_t VolumeLetter) {
        
        INT32 VolID = GetVolumeID(C, VolumeLetter);
        
        if(VolID != NAR_INVALID_VOLUME_TRACK_ID){
            if(TerminateBackup(&C->Volumes.Data[VolID], Succeeded)){
                return NarSaveBootState(C);
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
            temp->ID = CResult.Data[i].ID;
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
                BMet->IpAdress         = gcnew System::String("NOT IMPLEMENTED");
                
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
            
            NarScratchReset();
            GlobalLogCount = 0;
            
            ReleaseMutex(GlobalLogMutex);
            
        }
        else{
            printf("Couldnt lock log mutex\n");
        }
        
        return Result;
    }
    
    // TODO(Batuhan): helper functions, like which volume we are streaming etc.
    
}