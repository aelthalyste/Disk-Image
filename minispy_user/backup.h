/* date = July 15th 2021 0:12 pm */
#pragma once

#include "precompiled.h"
#include "mspyLog.h"
#include "strsafe.h"


BOOLEAN
ConnectDriver(PLOG_CONTEXT Ctx);


BOOLEAN
SetupVSS();


BOOLEAN
SetIncRecords(HANDLE CommPort, volume_backup_inf* V);



ULONGLONG
NarGetLogFileSizeFromKernel(HANDLE CommPort, char Letter);


BOOLEAN
SetDiffRecords(HANDLE CommPort ,volume_backup_inf* V);



BOOLEAN
SetFullRecords(volume_backup_inf* V);



/*
  From given volume handle, extracts region information from volume
  Handle can be VSS handle or normal volume handle. VSS is preferred since volume information might change over time
  returns NULL if any error occurs
*/
nar_record*
GetVolumeRegionsFromBitmap(HANDLE VolumeHandle, uint32_t* OutRecordCount);


/*
*/
BOOLEAN
SetupStream(PLOG_CONTEXT C, wchar_t L, BackupType Type, DotNetStreamInf* SI, bool ShouldCompress);



BOOLEAN
TerminateBackup(volume_backup_inf* V, BOOLEAN Succeeded);




// Assumes CallerBufferSize >= NAR_COMPRESSION_FRAME_SIZE
uint32_t
ReadStream(volume_backup_inf* VolInf, void* CallerBuffer, unsigned int CallerBufferSize);




/*
Attachs filter
SetActive: default value is TRUE
*/
inline BOOLEAN
AttachVolume(char Letter);


BOOLEAN
DetachVolume(volume_backup_inf* VolInf);


BOOLEAN
RemoveVolumeFromTrack(LOG_CONTEXT *C, wchar_t L);



/*
 Just removes volume entry from kernel memory, does not unattaches it.
*/
inline BOOLEAN
NarRemoveVolumeFromKernelList(wchar_t Letter, HANDLE CommPort);





BOOLEAN
GetVolumesOnTrack(PLOG_CONTEXT C, volume_information* Out, unsigned int BufferSize, int* OutCount);

INT32
GetVolumeID(PLOG_CONTEXT C, wchar_t Letter);

/*
This operation just adds volume to list, does not starts to filter it,
until it's fullbackup is requested. After fullbackup, call AttachVolume to start filtering
*/
BOOLEAN
AddVolumeToTrack(PLOG_CONTEXT Context, wchar_t Letter, BackupType Type);


data_array<nar_record>
GetMFTandINDXLCN(char VolumeLetter, HANDLE VolumeHandle);


VSS_ID
GetShadowPath(std::wstring Drive, CComPtr<IVssBackupComponents>& ptr, wchar_t* OutShadowPath, size_t MaxOutCh);


inline BOOLEAN
InitVolumeInf(volume_backup_inf* VolInf, wchar_t Letter, BackupType Type);


BOOLEAN
CopyData(HANDLE S, HANDLE D, ULONGLONG Len);


BOOLEAN
SaveMetadata(char Letter, int Version, int ClusterSize, BackupType BT, data_array<nar_record> BackupRegions, nar_backup_id BackupID, bool IsCompressed, HANDLE VSSHandle);



inline int
NarGetVolumeDiskType(char Letter);

inline unsigned char
NarGetVolumeDiskID(char Letter);




inline BOOLEAN
NarIsOSVolume(char Letter) {
    char windir[256];
    GetWindowsDirectoryA(windir, 256);
    return windir[0] == Letter;
}



/*
Its not best way to initialize a struct
*/
LOG_CONTEXT*
NarLoadBootState() {
    
    printf("Entered NarLoadBootState\n");
    LOG_CONTEXT* Result;
    BOOLEAN bResult = FALSE;
    Result = new LOG_CONTEXT;
    memset(Result, 0, sizeof(LOG_CONTEXT));
    
    Result->Port = INVALID_HANDLE_VALUE;
    Result->Volumes = { 0,0 };
    
    char FileNameBuffer[64];
    if (GetWindowsDirectoryA(FileNameBuffer, 64)) {
        
        strcat(FileNameBuffer, "\\");
        strcat(FileNameBuffer, NAR_BOOTFILE_NAME);
        HANDLE File = CreateFileA(FileNameBuffer, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
        if (File != INVALID_HANDLE_VALUE) {
            
            // Check aligment, not a great way to check integrity of file
            DWORD Size = GetFileSize(File, 0);
            if (Size > sizeof(nar_boot_track_data) * NAR_MAX_VOLUME_COUNT) {
                printf("Bootfile too large, might be corrupted\n");
            }
            else if (Size % sizeof(nar_boot_track_data) != 0) {
                printf("Bootfile is not aligned\n");
            }
            else {
                
                printf("Found previous backup files, will initialize CONTEXT accordingly\n");
                
                DWORD BR = 0;
                nar_boot_track_data* BootTrackData = (nar_boot_track_data*)malloc(Size);
                if (BootTrackData != NULL) {
                    
                    if (ReadFile(File, BootTrackData, Size, &BR, 0) && BR == Size) {
                        
                        volume_backup_inf VolInf;
                        memset(&VolInf, 0, sizeof(VolInf));
                        int BootTrackDataCount = (int)(Size / (DWORD)sizeof(BootTrackData[0]));
                        
                        for (int i = 0; i < BootTrackDataCount; i++) {
                            
                            bResult = InitVolumeInf(&VolInf, (char)BootTrackData[i].Letter, (BackupType)BootTrackData[i].BackupType);
                            
                            if (!!bResult) {
                                
                                VolInf.BackupID = {0};
                                VolInf.BackupID = BootTrackData[i].BackupID;
                                
                                VolInf.Version  = BootTrackData[i].Version;
                                
                                // NOTE(Batuhan): Diff backups only need to know file pointer position they started backing up. For single-tracking system we use now, that's 0
                                if((BackupType)BootTrackData[i].BackupType == BackupType::Diff){
                                    // NOTE(Batuhan): We don't support different volume track units yet, 
                                    // for multi-tracked system this value may be different for each unit
                                    VolInf.DiffLogMark.BackupStartOffset = 0;
                                }
                                else{
                                    // Incremental backups needs to know where they left off
                                    VolInf.IncLogMark.LastBackupRegionOffset = BootTrackData[i].LastBackupOffset;
                                }
                                
                                Result->Volumes.Insert(VolInf);
                                
                                printf("BOOTFILELOADER : Added volume %c, version %u, lko %I64u to track list.", VolInf.Letter, VolInf.Version, VolInf.IncLogMark.LastBackupRegionOffset);
                                
                            }
                            else{
                                printf("Couldnt initialize volume backup inf for volume %c\n", BootTrackData[i].Letter);
                            }
                            
                        }
                        
                    }
                    else {
                        printf("Couldnt read boot file, read %i, file size was %i\n", BR, Size);
                        DisplayError(GetLastError());
                    }
                    
                }
                else {
                    printf("Couldnt allocate memory for nar_boot_track_data \n");
                    BootTrackData = 0;
                }
                
                free(BootTrackData);
                
            }
            
        }
        else {
            printf("File %s doesnt exist\n", FileNameBuffer);
            // File doesnt exist
        }
        
        CloseHandle(File);
        
    }
    else {
        printf("Couldnt get windows directory, error %i\n", GetLastError());
    }
    
    if (!Result) {
        delete Result;
        Result = NULL;
    }
    
    return Result;
}



/*
    Saves the current program state into a file, so next time computer boots driver can recognize it 
    and setup itself accordingly.

    saved states:
        - Letter (user-kernel)
        - Version (user)
        - BackupType (user)
        - LastBackupRegionOffset (user)
*/

BOOLEAN
NarSaveBootState(LOG_CONTEXT* CTX) {
    
    if (CTX == NULL || CTX->Volumes.Data == NULL){
        printf("Either CTX or its volume data was NULL, terminating NarSaveBootState\n");
        return FALSE;
    }
    
    BOOLEAN Result = TRUE;
    nar_boot_track_data Pack;
    
    char FileNameBuffer[64];
    if (GetWindowsDirectoryA(FileNameBuffer, 64)) {
        strcat(FileNameBuffer, "\\");
        strcat(FileNameBuffer, NAR_BOOTFILE_NAME);
        
        HANDLE File = CreateFileA(FileNameBuffer, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
        if (File != INVALID_HANDLE_VALUE) {
            printf("Created file to write boot information\n");
            for (unsigned int i = 0; i < CTX->Volumes.Count; i++) {
                Pack.Letter = 0;
                Pack.Version = -2;
                
                if (!CTX->Volumes.Data[i].INVALIDATEDENTRY) {
                    
                    Pack.Letter = (char)CTX->Volumes.Data[i].Letter;
                    Pack.Version = (char)CTX->Volumes.Data[i].Version; // for testing purposes, 128 version looks like fine
                    Pack.BackupType = (char)CTX->Volumes.Data[i].BT;
                    Pack.LastBackupOffset = (uint64_t)CTX->Volumes.Data[i].IncLogMark.LastBackupRegionOffset;
                    
                    DWORD BR = 0;
                    
                    if (WriteFile(File, &Pack, sizeof(Pack), &BR, 0) && BR == sizeof(Pack)) {
                        printf("Successfully saved volume [%c]'s [%i]th version information to boot file\n", Pack.Letter, Pack.Version);
                    }
                    else {
                        printf("Coulndt save volume [%c]'s [%i]th version information to boot file\n", Pack.Letter, Pack.Version);
                        Result = FALSE;
                    }
                    
                }
                else{
                    continue;    
                }
                
                
            }
            
            
        }
        else {
            printf("Couldnt open file %s, can not save boot information\n", FileNameBuffer);
            Result = FALSE;
        }
        
        CloseHandle(File);
        
    }
    
    
    return Result;
    
}


data_array<volume_information>
NarGetVolumes();


inline BOOLEAN
NarEditTaskNameAndDescription(const wchar_t* FileName, const wchar_t* TaskName, const wchar_t* TaskDescription);




inline nar_backup_id
NarGenerateBackupID(char Letter);


BOOLEAN
NarGetBackupsInDirectory(const wchar_t* Directory, backup_metadata* B, int BufferSize, int* FoundCount);


inline BOOLEAN
NarFileNameExtensionCheck(const wchar_t *Path, const wchar_t *Extension);

VOID
DisplayError(DWORD Code);


void
MergeRegions(data_array<nar_record>* R);


inline BOOLEAN
IsRegionsCollide(nar_record R1, nar_record R2);


int
CompareNarRecords(const void* v1, const void* v2);
