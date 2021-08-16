#include "precompiled.h"
#include "nar_win32.h"
#include "performance.h"
#include "compression.h"
#include "narstring.h"

BOOLEAN
NarRemoveLetter(char Letter){
    char Buffer[128];
    
    snprintf(Buffer, sizeof(Buffer), 
             "select volume %c\n"
             "remove letter %c\n"
             "exit\n", Letter, Letter);
    
    
    char InputFN[] = "NARDPRLINPUT";
    if(NarDumpToFile(InputFN, Buffer, (unsigned int)strlen(Buffer))){
        snprintf(Buffer, sizeof(Buffer), "diskpart /s %s", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    
    printf("COULDNT DUMP CONTENTS TO FILE, RETURNING FALSE CODE (NARREMOVELETTER)\n");
    return FALSE;
}



BOOLEAN
NarFormatVolume(char Letter) {
    
    char Buffer[2096];
    memset(Buffer, 0, 2096);
    
    snprintf(Buffer, sizeof(Buffer), 
             ""
             "select volume %c\n"
             "format fs = ntfs quick override\n"
             "exit\n", Letter);
    
    char InputFN[] = "NARDPFORMATVOLUME";
    if (NarDumpToFile(InputFN, Buffer, (unsigned int)strlen(Buffer))) {
        snprintf(Buffer, sizeof(Buffer), "diskpart /s %s", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    
    return FALSE;
}



void
NarRepairBoot(char OSVolumeLetter, char BootPartitionLetter) {
    char Buffer[128];
    snprintf(Buffer, sizeof(Buffer), 
             "bcdboot %c:\\Windows /s %c: /f ALL", OSVolumeLetter, BootPartitionLetter);
    system(Buffer);
}




BOOLEAN
NarSetVolumeSize(char Letter, int TargetSizeMB) {
    BOOLEAN Result = 0;
    int VolSizeMB = 0;
    char Buffer[1024];
    char FNAME[] = "NARDPSCRPT";
    snprintf(Buffer, sizeof(Buffer), "%c:\\", Letter);
    
    ULARGE_INTEGER TOTAL_SIZE = { 0 };
    GetDiskFreeSpaceExA(Buffer, 0, &TOTAL_SIZE, 0);
    VolSizeMB = (int)(TOTAL_SIZE.QuadPart / (1024ull * 1024ull)); // byte to MB
    
    printf("Volume %c's size is %i, target size %i\n", Letter, VolSizeMB, TargetSizeMB);
    
    if (VolSizeMB == TargetSizeMB) {
        return TRUE;
    }
    
    /*
  extend size = X // extends volume by X. Doesnt resize, just adds X
  shrink desired = X // shrink volume to X, if X is bigger than current size, operation fails
  */
    if (VolSizeMB > TargetSizeMB) {
        // shrink volume
        ULONGLONG Diff = (unsigned int)(VolSizeMB - TargetSizeMB);
        snprintf(Buffer, sizeof(Buffer), "select volume %c\nshrink desired = %I64u\nexit\n", Letter, Diff);
    }
    else if(VolSizeMB < TargetSizeMB){
        // extend volume
        ULONGLONG Diff = (unsigned int)(TargetSizeMB - VolSizeMB);
        snprintf(Buffer, sizeof(Buffer), "select volume %c\nextend size = %I64u\nexit\n", Letter, Diff);
    }
    else {
        // VolSizeMB == TargetSizeMB
        // do nothing
        return TRUE;
    }
    
    //NarDumpToFile(const char *FileName, void* Data, int Size)
    if (NarDumpToFile(FNAME, Buffer, (uint32_t)strlen(Buffer))) {
        char CMDBuffer[128];
        snprintf(CMDBuffer, sizeof(CMDBuffer), "diskpart /s %s", FNAME);
        system(CMDBuffer);
        // TODO(Batuhan): maybe check output
        Result = TRUE;
    }
    
    return Result;
}

BOOLEAN
CreatePartition(int Disk, char Letter, unsigned size) {
    BOOLEAN Result = FALSE;
    char Buffer[1024];
    
    if (Letter >= 'a' && Letter <= 'z') Letter += ('A' - 'a'); //convert to uppercase
    
    DWORD Drives = GetLogicalDrives();
    if (Drives & (1 << (Letter - 'A'))) {
        printf("Volume exists in system\n");
        return FALSE;
    }
    snprintf(Buffer, sizeof(Buffer), "select disk %i\n"
             "create partition primary size = %u\n"
             "format fs = \"ntfs\" quick"
             "assign letter = \"%c\"\n", Disk, size, Letter);
    char FileName[] = "NARDPSCRPT";
    
    if (NarDumpToFile(FileName, Buffer, (unsigned int)strlen(Buffer))) {
        char CMDBuffer[128];
        snprintf(CMDBuffer, sizeof(CMDBuffer), "diskpart /s %s", FileName);
        system(CMDBuffer);
        // TODO(Batuhan): maybe check output
        Result = TRUE;
    }
    
    return Result;
}


ULONGLONG
NarGetVolumeTotalSize(char Letter) {
    char Temp[] = "!:\\";
    Temp[0] = Letter;
    ULARGE_INTEGER L = { 0 };
    GetDiskFreeSpaceExA(Temp, 0, &L, 0);
    return L.QuadPart;
}

ULONGLONG
NarGetVolumeUsedSize(char Letter){
    char Temp[] = "!:\\";
    Temp[0] = Letter;
    ULARGE_INTEGER total = { 0 };
    ULARGE_INTEGER free = { 0 };
    GetDiskFreeSpaceExA(Temp, 0, &total, &free);
    return (total.QuadPart - free.QuadPart);
}



BOOLEAN
NarCreateCleanGPTBootablePartition(int DiskID, int VolumeSizeMB, int EFISizeMB, int RecoverySizeMB, char VolumeLetter, char BootVolumeLetter) {
    
    char Buffer[2096];
    int BufferIndex = 0;
    memset(Buffer, 0, 2096);
    
    BufferIndex += snprintf(Buffer, sizeof(Buffer), 
                            "select disk %i\n" // ARG0 DiskID
                            "clean\n"
                            "convert gpt\n" // format disk as gpt
                            "select partition 1\n"
                            "delete partition override\n", DiskID);
    
    if(0 != RecoverySizeMB){
        BufferIndex += snprintf(&Buffer[BufferIndex], sizeof(Buffer) - BufferIndex,
                                "create partition primary size = %i\n"
                                "assign letter %c\n"
                                "format fs = ntfs quick\n"
                                "remove letter %c\n"
                                "set id = \"de94bba4-06d1-4d40-a16a-bfd50179d6ac\"\n"
                                "gpt attributes = 0x8000000000000001\n", 
                                RecoverySizeMB, BootVolumeLetter, BootVolumeLetter);
    }
    else{
        printf("Recovery partition's size was zero, skipping it\n");
    }
    
    BufferIndex += snprintf(&Buffer[BufferIndex], sizeof(Buffer) - BufferIndex,
                            "create partition efi size = %i\n"
                            "format fs = fat32 quick\n"
                            "assign letter %c\n"
                            "create partition msr size = 16\n" // win10
                            "create partition primary size = %i\n"
                            "assign letter = %c\n"
                            "format fs = \"ntfs\" quick\n"
                            "exit\n", 
                            EFISizeMB, 
                            BootVolumeLetter,
                            VolumeSizeMB,
                            VolumeLetter);
    
    
    char InputFN[] = "NARDPINPUT";
    // NOTE(Batuhan): safe conversion
    if (NarDumpToFile(InputFN, Buffer, (INT32)strlen(Buffer))) {
        snprintf(Buffer, sizeof(Buffer), "diskpart /s %s > NARDPOUTPUT.txt", InputFN);
        system(Buffer);
        printf("%s\n", Buffer);
        return TRUE;
    }
    
    return FALSE;
    
}

BOOLEAN
NarCreateCleanGPTPartition(int DiskID, int VolumeSizeMB, char Letter) {
    char Buffer[2096];
    memset(Buffer, 0, 2096);
    
    
    // Convert MB to byte
    if (NarGetDiskTotalSize(DiskID) < (ULONGLONG)VolumeSizeMB * 1024 * 1024) {
        printf("Not enough space to create partition\n");
        return FALSE;
    }
    
    snprintf(Buffer, sizeof(Buffer),
             ""
             "select disk %i\n"
             "clean\n"
             "convert gpt\n" // format disk as gpt
             "create partition primary size = %i\n"
             "assign letter = %c\n"
             "format fs = \"ntfs\" quick\n"
             "exit\n", DiskID, VolumeSizeMB, Letter);
    
    
    char InputFN[] = "NARDPINPUT";
    if (NarDumpToFile(InputFN, Buffer, (unsigned int)strlen(Buffer))) {
        snprintf(Buffer, sizeof(Buffer),"diskpart /s %s > NARDPOUTPUT.txt", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    
    return FALSE;
}


BOOLEAN
NarCreateCleanMBRPartition(int DiskID, char VolumeLetter, int VolumeSize) {
    
    char Buffer[2048];
    snprintf(Buffer,sizeof(Buffer), 
             "select disk %i\n"
             "clean\n"
             "convert mbr\n"
             "create partition primary size %i\n"
             "format fs = ntfs quick\n"
             "assign letter %c\n"
             "exit\n",
             DiskID, VolumeSize, VolumeLetter
             );
    
    char InputFN[] = "NARDPINPUT";
    if (NarDumpToFile(InputFN, Buffer, (unsigned int)strlen(Buffer))) {
        snprintf(Buffer, sizeof(Buffer), "diskpart /s %s > NARDPOUTPUT.txt", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    
    
    return FALSE;
    
}


BOOLEAN
NarCreateCleanMBRBootPartition(int DiskID, char VolumeLetter, int VolumeSizeMB, int SystemPartitionSizeMB, int RecoveryPartitionSizeMB,
                               char BootPartitionLetter) {
    
    char Buffer[2048];
    memset(Buffer, 0, 2048);
    int indx = 0;
    indx = snprintf(Buffer, sizeof(Buffer), 
                    "select disk %i\n"
                    "clean\n"
                    "convert mbr\n"
                    // SYSTEM PARTITION
                    "create partition primary size = %i\n"
                    "assign letter %c\n"
                    "format quick fs = ntfs label = System\n"
                    "active\n"
                    // WINDOWS PARTITION
                    "create partition primary size %i\n" // in MB
                    "format quick fs = ntfs label = Windows\n"
                    "assign letter %c\n", 
                    DiskID, SystemPartitionSizeMB, BootPartitionLetter, VolumeSizeMB, VolumeLetter);
    
    if(0 != RecoveryPartitionSizeMB){
        indx += snprintf(Buffer, sizeof(Buffer)-indx,
                         "create partition primary size %i\n"
                         "set id = 27\n", 
                         RecoveryPartitionSizeMB);
    }
    
    snprintf(&Buffer[indx], sizeof(Buffer) - indx,
             "exit\n");
    
    char InputFN[] = "NARDPINPUT";
    if (NarDumpToFile(InputFN, Buffer, (unsigned int)strlen(Buffer))) {
        snprintf(Buffer, sizeof(Buffer), "diskpart /s %s > NARDPOUTPUT.txt", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    
    
    return FALSE;
    
}


/*
For some reason, kernel and user GUID is not compatible for one character, which is just question mark (?)
. to be kernel compatible, one must call this function to communicate with kernel, otherwise kernel cant distinguish
given GUID and will return error.

VolumeGUID MUST have size of 98 bytes, (49 unicode char)
*/
BOOLEAN
NarGetVolumeGUIDKernelCompatible(wchar_t Letter, wchar_t *VolumeGUID) {
    
    if (VolumeGUID == NULL) return FALSE;
    BOOLEAN Result = FALSE;
    wchar_t MountPoint[] = L"!:\\";
    MountPoint[0] = Letter;
    
    
    wchar_t LocalGUID[50];
    // This function appends trailing backslash to string, we can ignore that, but still need to pass 50 cch long buffer
    // to not to fail in the function. silly way to do thing
    if (GetVolumeNameForVolumeMountPointW(MountPoint, LocalGUID, 50)) {     
        memcpy(VolumeGUID, LocalGUID, 49*sizeof(wchar_t));
        VolumeGUID[1] = L'?';
        // GetVolumeNameForVolumeMountPointW returns string with trailing backslash (\\), i dont remember that thing's name but it is it
        // but kernel doesnt use that, so just remove it
        if(VolumeGUID[48] == L'\\'){
            VolumeGUID[48] = '\0'; 
        }
        Result = TRUE;
    }
    
    
    return Result;
}


unsigned char
NarGetVolumeDiskID(char Letter) {
    
    wchar_t VolPath[MAX_PATH];
    wchar_t Vol[] = L"!:\\";
    Vol[0] = Letter;
    
    unsigned char Result = (unsigned char)NAR_INVALID_DISK_ID;
    DWORD BS = 1024 * 2; //1 KB
    DWORD T = 0;
    
    VOLUME_DISK_EXTENTS* Ext = (VOLUME_DISK_EXTENTS*)malloc(BS);
    
    // TODO : We probably don't need kernel GUID at all... fix this
    if(NarGetVolumeGUIDKernelCompatible((wchar_t)Letter,VolPath)){
        
        HANDLE Drive = CreateFileW(VolPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        
        if (Drive != INVALID_HANDLE_VALUE) {
            
            if (DeviceIoControl(Drive, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0, Ext, BS, &T, 0)) {
                //wchar_t DiskPath[512];
                // L"\\\\?\\PhysicalDrive%i";
                Result = (unsigned char)Ext->Extents[0].DiskNumber;
            }
            else {
                printf("DeviceIoControl failed with argument VOLUME_GET_VOLUME_DISK_EXTENTS for volume %c\n", Letter);
            }
            
        }
        else {
            printf("Cant open drive %c as file\n", Letter);
        }
        
        CloseHandle(Drive);
        
    }
    else{
        printf("Couldnt get kernel compatible volume GUID\n");
    }
    
    free(Ext);
    
    return Result;
}



/*
Returns:
'R'aw
'M'BR
'G'PT
*/
int
NarGetVolumeDiskType(char Letter) {
    char VolPath[512];
    char Vol[] = "!:\\";
    Vol[0] = Letter;
    
    int Result = NAR_DISKTYPE_RAW;
    DWORD BS = 1024 * 8; //8 KB
    DWORD T = 0;
    
    void* Buf = VirtualAlloc(0, 2 * BS, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    VOLUME_DISK_EXTENTS* Ext = (VOLUME_DISK_EXTENTS*)(Buf);
    DRIVE_LAYOUT_INFORMATION_EX* DL = (DRIVE_LAYOUT_INFORMATION_EX*)((char*)Buf + BS);
    
    
    
    GetVolumeNameForVolumeMountPointA(Vol, VolPath, 512);
    VolPath[strlen(VolPath) - 1] = '\0'; //Remove trailing slash
    HANDLE Drive = CreateFileA(VolPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    HANDLE Disk = INVALID_HANDLE_VALUE;
    
    if (Drive != INVALID_HANDLE_VALUE) {
        
        if (DeviceIoControl(Drive, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0, Ext, BS, &T, 0)) {
            wchar_t DiskPath[512];
            // L"\\\\?\\PhysicalDrive%i";
            wsprintfW(DiskPath, L"\\\\?\\PhysicalDrive%i", Ext->Extents[0].DiskNumber);
            Disk = CreateFileW(DiskPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
            if (Disk != INVALID_HANDLE_VALUE) {
                
                if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DL, BS, &T, 0)) {
                    if (DL->PartitionStyle == PARTITION_STYLE_MBR) {
                        Result = NAR_DISKTYPE_MBR;
                    }
                    if (DL->PartitionStyle == PARTITION_STYLE_GPT) {
                        Result = NAR_DISKTYPE_GPT;
                    }
                    if (DL->PartitionStyle == PARTITION_STYLE_RAW) {
                        Result = NAR_DISKTYPE_RAW;
                    }
                }
                else {
                    printf("DeviceIOControl GET_DRIVE_LAYOUT_EX failed\n");
                }
                
            }
            else {
                printf("Can't open disk as file\n");
            }
            
        }
        
        
    }
    else {
        printf("Cant open drive %c as file\n", Letter);
    }
    
    VirtualFree(Buf, 0, MEM_RELEASE);
    CloseHandle(Drive);
    CloseHandle(Disk);
    return Result;
}



/*
Expects Letter to be uppercase
*/
BOOLEAN
NarIsVolumeAvailable(char Letter){
    DWORD Drives = GetLogicalDrives();
    return !(Drives & (1 << (Letter - 'A')));
}



/*
    Returns first available volume letter that is not used by system
*/
char
NarGetAvailableVolumeLetter() {
    
    DWORD Drives = GetLogicalDrives();
    // NOTE(Batuhan): skip volume A and B
    for (int CurrentDriveIndex = 2; CurrentDriveIndex < 26; CurrentDriveIndex++) {
        if (Drives & (1 << CurrentDriveIndex)) {
            
        }
        else {
            return ('A' + (char)CurrentDriveIndex);
        }
    }
    
    return 0;
}


// returns # of disks, returns 0 if information doesnt fit in array
data_array<disk_information>
NarGetDisks() {
    
    data_array<disk_information> Result = { 0, 0 };
    Result.Data = (disk_information*)malloc(sizeof(disk_information) * 32);
    Result.Count = 32;
    memset(Result.Data, 0, sizeof(disk_information) * 32);
    
    DWORD DriveLayoutSize = 1024 * 4;
    int DGEXSize = 1024 * 4;
    
    DWORD MemorySize = DriveLayoutSize + DGEXSize;
    void* Memory = VirtualAlloc(0, MemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    
    DRIVE_LAYOUT_INFORMATION_EX* DriveLayout = (DRIVE_LAYOUT_INFORMATION_EX*)(Memory);
    DISK_GEOMETRY_EX* DGEX = (DISK_GEOMETRY_EX*)((char*)Memory + DriveLayoutSize);    
    
    
    char StrBuf[255];
    int Found = 0;
    DWORD HELL;
    
    for (unsigned int DiskID = 0; DiskID < 32; DiskID++){
        
        snprintf(StrBuf, sizeof(StrBuf), "\\\\?\\PhysicalDrive%i", DiskID);
        HANDLE Disk = CreateFileA(StrBuf, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0,0);
        if(Disk == INVALID_HANDLE_VALUE){
            continue;
        }
        
        Result.Data[Found].ID = (int)DiskID;
        
        if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DriveLayout, DriveLayoutSize, &HELL, 0)) {
            
            if (DriveLayout->PartitionStyle == PARTITION_STYLE_MBR) {
                Result.Data[Found].Type = NAR_DISKTYPE_MBR;
            }
            if (DriveLayout->PartitionStyle == PARTITION_STYLE_GPT) {
                Result.Data[Found].Type = NAR_DISKTYPE_GPT;
            }
            if (DriveLayout->PartitionStyle == PARTITION_STYLE_RAW) {
                Result.Data[Found].Type = NAR_DISKTYPE_RAW;
            }
            
            if(DriveLayout->PartitionStyle != PARTITION_STYLE_RAW
               && DriveLayout->PartitionStyle != PARTITION_STYLE_GPT
               && DriveLayout->PartitionStyle != PARTITION_STYLE_MBR){
                printf("Drive layout style was undefined, %s\n", StrBuf);
            }
            
        }
        else {
            printf("Can't get drive layout for disk %s\n", StrBuf);
        }
        
        if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, 0, 0, DGEX, (DWORD)DGEXSize, &HELL, 0)) {
            Result.Data[Found].Size = (ULONGLONG)DGEX->DiskSize.QuadPart;
        }
        else {
            printf("Can't get drive layout for disk %s\n", StrBuf);
        }
        
        CloseHandle(Disk);
        memset(Memory, 0, MemorySize);
        Found++;
        
    }
    
    
    VirtualFree(Memory, 0, MEM_RELEASE);
    Result.Count = Found;
    if (Found) {
        Result.Data = (disk_information*)realloc(Result.Data, sizeof(Result.Data[0]) * Result.Count);
    }
    else {
        free(Result.Data);
        Result.Data = 0;
    }
    
    return Result;
    
}

// returns 0 if one is not found
char NarVolumeNameGUIDToLetter(wchar_t* VolumeName, wchar_t *Out, size_t MaxOutCount)
{
    
    BOOL   Success   = FALSE;
    
    //
    //  Obtain all of the paths
    //  for this volume.
    //Success = GetVolumePathNamesForVolumeNameW(VolumeName, Out, MaxOutCount, &MaxOutCount);
    
    if (!Success) 
    {
        printf("GetVolumePathNamesForVolumeNameW failed for volume %s with error code %d\n", VolumeName, GetLastError());
        return 0 ;
    }
    
}


#if 1
disk_information_ex*
NarGetPartitions(nar_arena *Arena, size_t* OutCount) {
    
    disk_information_ex *Result = (disk_information_ex*)ArenaAllocateZeroAligned(Arena, 32*sizeof(*Result), 8);
    
    size_t Found = 0;
    DWORD DriveLayoutSize = 1024 * 4;
    int DGEXSize = 1024 * 4;
    
    DWORD MemorySize = DriveLayoutSize + DGEXSize;
    void* Memory = malloc(MemorySize);
    
    DRIVE_LAYOUT_INFORMATION_EX* DriveLayout = (DRIVE_LAYOUT_INFORMATION_EX*)(Memory);
    DISK_GEOMETRY_EX* DGEX = (DISK_GEOMETRY_EX*)((char*)Memory + DriveLayoutSize);    
    
    char StrBuf[255];
    DWORD HELL;
    
    uint32_t Disks = NarGetDiskListFromDiskPart();
    
    for (uint32_t DiskID = 0; DiskID < 32; DiskID++){
        
        if(!(Disks & (1<<DiskID))){
            continue;
        }
        
        snprintf(StrBuf, sizeof(StrBuf), "\\\\?\\PhysicalDrive%i", DiskID);
        
        HANDLE Disk = CreateFileA(StrBuf, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0,0);
        if(Disk == INVALID_HANDLE_VALUE){
            continue;
        }
        
        disk_information_ex *Insert = &Result[Found++];
        Insert->DiskID = DiskID;
        
        
        if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, 0, 0, DGEX, (DWORD)DGEXSize, &HELL, 0)) {
            Insert->TotalSize = DGEX->DiskSize.QuadPart;
        }
        else {
            printf("Can't get drive layout for disk %s\n", StrBuf);
        }
        
        
        if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DriveLayout, DriveLayoutSize, &HELL, 0)) {
            
            if (DriveLayout->PartitionStyle == PARTITION_STYLE_MBR) {
                Insert->DiskType = NAR_DISKTYPE_MBR;
            }
            if (DriveLayout->PartitionStyle == PARTITION_STYLE_GPT) {
                Insert->DiskType = NAR_DISKTYPE_GPT;
            }
            if (DriveLayout->PartitionStyle == PARTITION_STYLE_RAW) {
                Insert->DiskType = NAR_DISKTYPE_RAW;
            }
            
            if(DriveLayout->PartitionStyle != PARTITION_STYLE_RAW
               && DriveLayout->PartitionStyle != PARTITION_STYLE_GPT
               && DriveLayout->PartitionStyle != PARTITION_STYLE_MBR){
                printf("Drive layout style was undefined, %s\n", StrBuf);
                continue;
            }
            
            if(DriveLayout->PartitionCount == 0 || DriveLayout->PartitionStyle == PARTITION_STYLE_RAW){
                continue;
            }
            
            Insert->Volumes = (volume_information*)ArenaAllocateZero(Arena, DriveLayout->PartitionCount * sizeof(volume_information));
            
            Insert->UnusedSize = Insert->TotalSize;
            Insert->VolumeCount = DriveLayout->PartitionCount;
            
            for(size_t PartitionID = 0;
                PartitionID < DriveLayout->PartitionCount;
                PartitionID++)
            {
                auto PLayout = DriveLayout->PartitionEntry[PartitionID];
                Insert->UnusedSize -= PLayout.PartitionLength.QuadPart;
                
                Insert->Volumes[PartitionID].TotalSize = PLayout.PartitionLength.QuadPart;
                Insert->Volumes[PartitionID].DiskID    = Insert->DiskID;
                Insert->Volumes[PartitionID].DiskType  = Insert->DiskType;
                Insert->Volumes[PartitionID].Letter    = PLayout.PartitionNumber;
                
                if(Insert->DiskType == NAR_DISKTYPE_GPT){
                    wcscpy(Insert->Volumes[PartitionID].VolumeName, PLayout.Gpt.Name);
                    char TempLetter = NarGetVolumeLetterFromGUID(PLayout.Gpt.PartitionId);
                    if(TempLetter != 0){
                        char VolumeString[] = "!:\\";
                        VolumeString[0] = TempLetter;
                        ULARGE_INTEGER TotalSize = { 0 };
                        ULARGE_INTEGER FreeSize = { 0 };
                        
                        GetDiskFreeSpaceExA(VolumeString, 0, &TotalSize, &FreeSize);
                        
                        Insert->Volumes[PartitionID].FreeSize = FreeSize.QuadPart;
                        Insert->Volumes[PartitionID].Letter = TempLetter;
                    }
                }
                if(Insert->DiskType == NAR_DISKTYPE_MBR){
                    
                }
                
            }
            
            
        }
        else {
            printf("Can't get drive layout for disk %s\n", StrBuf);
        }
        
        
        CloseHandle(Disk);
        memset(Memory, 0, MemorySize);
        Found++;
    }
    
    
    free(Memory);
    if(OutCount){
        *OutCount = Found;
    }
    
    
    return Result;
}
#endif


/*
Unlike generatemetadata, binary functions, this one generates absolute path of the log file. Which is 
under windows folder
C:\Windows\Log....
*/
std::wstring
GenerateLogFilePath(char Letter) {
    wchar_t NameTemp[]= L"NAR_LOG_FILE__.nlfx";
    NameTemp[13] = (wchar_t)Letter;
    
    std::wstring Result = L"C:\\Windows\\";
    Result += std::wstring(NameTemp);
    
    return Result;
}


#pragma warning(push)
#pragma warning(disable:4477)
void
StrToGUID(const char* guid, GUID* G) {
    if (!G) return;
    sscanf(guid, "{%8X-%4hX-%4hX-%2hX%2hX-%2hX%2hX%2hX%2hX%2hX%2hX}", &G->Data1, &G->Data2, &G->Data3, &G->Data4[0], &G->Data4[1], &G->Data4[2], &G->Data4[3], &G->Data4[4], &G->Data4[5], &G->Data4[6], &G->Data4[7]);
}
#pragma warning(pop)



uint64_t
NarGetDiskTotalSize(int DiskID) {
    
    ULONGLONG Result = 0;
    DWORD Temp = 0;
    char StrBuffer[128];
    snprintf(StrBuffer, sizeof(StrBuffer), "\\\\?\\PhysicalDrive%i", DiskID);
    
    unsigned int DGEXSize = 1024 * 1; // 1 KB
    DISK_GEOMETRY_EX* DGEX = (DISK_GEOMETRY_EX*)malloc(DGEXSize);
    if (DGEX != NULL) {
        HANDLE Disk = CreateFileA(StrBuffer,
                                  GENERIC_WRITE | GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        
        if (Disk != INVALID_HANDLE_VALUE) {
            if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, 0, 0, DGEX, DGEXSize, &Temp, 0)) {
                Result = (ULONGLONG)DGEX->DiskSize.QuadPart;
            }
            else {
                printf("Failed to call DeviceIoControl with IOCTL_DISK_GET_DRIVE_GEOMETRY_EX parameter for disk %i\n", DiskID);
            }
        }
        else {
            printf("Can't open %s\n", StrBuffer);
        }
        
        CloseHandle(Disk);
        free(DGEX);    	
    }
    else{
        printf("Unable to allocate memory for DISK_GEOMETYRY_EX, for disk %d\n", DiskID);
    }
    
    
    return Result;
}



wchar_t*
NarUTF8ToWCHAR(NarUTF8 s, nar_arena *Arena){
    wchar_t *Result = 0;
    int ChNeeded = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s.Str, s.Len, 0, 0);
    
    ASSERT(ChNeeded != 0);
    
    uint32_t BytesNeeded = (ChNeeded + 1)*2; // +1 for null termination
    ASSERT(BytesNeeded);
    
    Result = (wchar_t*)ArenaAllocate(Arena, BytesNeeded);
    ASSERT(Result);
    
    memset(Result, 0, BytesNeeded);
    int WResult = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s.Str, s.Len, Result, ChNeeded);
    ASSERT(WResult == ChNeeded);
    
    return Result;
}

NarUTF8
NarWCHARToUTF8(wchar_t *Str, nar_arena *Arena){
    NarUTF8 Result = {};
    size_t StrLen = wcslen(Str);
    
    Result.Len = WideCharToMultiByte(CP_UTF8, 0, Str, StrLen, NULL, 0, NULL, NULL) + 1;
    
    Result.Str = (uint8_t*)ArenaAllocate(Arena, Result.Len);
    Result.Cap = Result.Len;
    memset(Result.Str, 0, Result.Cap);
    
    WideCharToMultiByte(CP_UTF8, 0, Str, StrLen, (LPSTR)Result.Str, Result.Len, NULL, NULL);
    
    return Result;
}




/*
Doesn't fail if reads less than ReadSiz, instead returns how many bytes it read.
ARGS:
Backup, Metadata: File views of according backup.
AbsoluteClusterOffset: (in clusters) Absolute volume offset we are trying to read.
ReadSizeInCluster    : (in clusters) How many clusters we should be reading.
Output               : Output buffer we should fill.
OutputMaxSize        : Max size of the output buffer.
ZSTDBuffer           : If backup is compressed, this buffer will be used to temporarily store decompressed data. This value might be NULL, if so, function allocates its own buffer. 
If buffer doesn't have enought space to contain decompressed data, function fails.
ZSTDBufferSize       : Size of given buffer, if no buffer is given this value is ignored.
*/
uint64_t
NarReadBackup(nar_file_view *Backup, nar_file_view *Metadata, 
              uint64_t AbsoluteClusterOffset, uint64_t ReadSizeInCluster, 
              void *Output, uint64_t OutputMaxSize,
              void *ZSTDBuffer, size_t ZSTDBufferSize)
{
    
    TIMED_BLOCK();
    if(NULL == Backup){
        return 0;
    }
    if(NULL == Metadata){
        return 0;
    }
    
    uint64_t Result = 0;
    backup_metadata *BM = (backup_metadata*)Metadata->Data;
    ASSERT(BM);
    
    nar_record *CompInfo = BM->CompressionInfoOffset ? (nar_record*)((uint8_t*)Metadata->Data + BM->CompressionInfoOffset) : 0;
    size_t CompInfoCount = BM->CompressionInfoCount;
    // ASSERT(BM->CompressionInfoCount > 0);
    // ASSERT(CompInfo);
    
    nar_record* Records       = (nar_record*)((uint8_t*)Metadata->Data + BM->Offset.RegionsMetadata);
    uint64_t RecordCount      = BM->Size.RegionsMetadata/sizeof(nar_record);
    
    point_offset OffsetResult = FindPointOffsetInRecords(Records, RecordCount, AbsoluteClusterOffset);
    
    uint64_t BackupOffsetInBytes = 0;
    uint64_t AvailableBytes      = 0;
    uint64_t ReadSizeInBytes     = 0;
    
    {
        uint64_t BackupClusterOffset = OffsetResult.Offset;
        uint64_t AvailableClusters   = OffsetResult.Readable;
        if(AvailableClusters < ReadSizeInCluster){
            printf("Requested bytes exceeds available region size in the backup! Requested %I64u, available %I64u\n", ReadSizeInCluster, AvailableClusters);
            ReadSizeInCluster = AvailableClusters;
        }
        ReadSizeInBytes     = (uint64_t)BM->ClusterSize * ReadSizeInCluster;
        BackupOffsetInBytes = (uint64_t)BM->ClusterSize * BackupClusterOffset;
    }
    
    if(BM->IsCompressed){
        
        uint64_t CompressedSize      = 0;
        uint64_t Remaining           = 0;
        uint64_t DecompSize          = 0;
        uint64_t DecompAdvancedSoFar = 0;
        uint64_t BackupRemainingLen  = Backup->Len;
        uint64_t RemainingReadSize   = ReadSizeInBytes;
        uint8_t* DataNeedle          = Backup->Data;
        
        
        for(size_t CII = 0;BackupRemainingLen>0 && RemainingReadSize > 0 ; CII++)
        {
            uint64_t DecompSize   = 0;
            size_t CompressedSize = 0;
            ASSERT(CompInfo);
            
            if(false){
                DecompSize     = CompInfo[CII].DecompressedSize;
                CompressedSize = CompInfo[CII].CompressedSize;
            }
            else{
                DecompSize    = ZSTD_getFrameContentSize(DataNeedle, BackupRemainingLen);
                CompressedSize  = ZSTD_findFrameCompressedSize(DataNeedle, BackupRemainingLen);       
                if(ZSTD_isError(DecompSize)){
                    ZSTD_ErrorCode ErrorCode = ZSTD_getErrorCode(DecompSize);
                    const char* ErrorString  = ZSTD_getErrorString(ErrorCode);
                    printf("ZSTD error when determining frame content size, error : %s\n", ErrorString); 
                    return 0;
                }
                if(ZSTD_isError(CompressedSize)){
                    ZSTD_ErrorCode ErrorCode = ZSTD_getErrorCode(DecompSize);
                    const char* ErrorString  = ZSTD_getErrorString(ErrorCode);
                    printf("ZSTD error when determining frame compressed size, error : %s\n", ErrorString); 
                    return 0;
                }
            }
            
            
            // found!
            if(BackupOffsetInBytes < DecompAdvancedSoFar + DecompSize){
                
                bool ZSTDBufferLocal = (ZSTDBuffer == NULL);
                
                if(ZSTDBufferLocal){
                    ZSTDBuffer     = malloc(DecompSize);
                    ZSTDBufferSize = DecompSize;
                }
                
                ASSERT(ZSTDBufferSize >= DecompSize);
                ASSERT(NULL != ZSTDBuffer);
                
                
                size_t ZSTD_RetCode = ZSTD_decompress(ZSTDBuffer, ZSTDBufferSize, DataNeedle, CompressedSize);
                
                ASSERT(!ZSTD_isError(ZSTD_RetCode));
                
                uint64_t BufferOffset        = BackupOffsetInBytes - DecompAdvancedSoFar;
                uint64_t BufferReadableBytes = DecompSize - BufferOffset;
                uint64_t CopySize            = MIN(RemainingReadSize, BufferReadableBytes);
                
                memcpy(Output, (uint8_t*)ZSTDBuffer + BufferOffset, CopySize);
                Output = (uint8_t*)Output + CopySize;
                
                
                BackupOffsetInBytes += CopySize;
                RemainingReadSize   -= CopySize;
                
                if(ZSTDBufferLocal){
                    free(ZSTDBuffer);
                    ZSTDBuffer = 0;
                }
                
            }
            
            DecompAdvancedSoFar += DecompSize;
            
            BackupRemainingLen -= CompressedSize;
            DataNeedle          = DataNeedle + (CompressedSize);
        }
        
        // decompress
    }
    else{
        memcpy(Output, (uint8_t*)Backup->Data + BackupOffsetInBytes, ReadSizeInBytes);
    }
    
    
    return ReadSizeInBytes;
}




HANDLE 
NarCreateVSSPipe(uint32_t BufferSize, uint64_t Seed, char *Name, size_t MaxNameCb)
{
    static DWORD id = 0;
    
    snprintf(Name, MaxNameCb, "\\\\.\\Pipe\\NARBULUT_PIPE.%08x.%08llx", GetCurrentProcessId(), Seed);
    
    SECURITY_ATTRIBUTES SAttr = {};
    SAttr.nLength = sizeof(SAttr);
    SAttr.bInheritHandle = TRUE;
    
    HANDLE Result = CreateNamedPipeA(
                                     Name,
                                     PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                     PIPE_TYPE_BYTE | PIPE_WAIT,
                                     10,
                                     BufferSize,
                                     BufferSize,
                                     0,
                                     &SAttr);
    
    if(Result == INVALID_HANDLE_VALUE){
        fprintf(stderr, "CreateNamedPipeA error %d, path %s\n", GetLastError(), Name);
    }
    else{
        printf("Named pipe : %s\n", Name);
    }
    return Result;
    
}

process_listen_ctx
NarSetupVSSListen(nar_backup_id ID){
    
    process_listen_ctx Result = {};
    Result.BufferSize   = 512;
    char NameBF[MAX_PATH];
    
    Result.PipeHandle   = NarCreateVSSPipe(Result.BufferSize, ID.Q, NameBF, sizeof(NameBF));
    
    if(Result.PipeHandle != INVALID_HANDLE_VALUE){
        Result.ReadBuffer   = (char*)malloc(Result.BufferSize);
        Result.WriteBuffer  = (char*)malloc(Result.BufferSize);
        
        STARTUPINFOA sinfo = {};
        sinfo.cb = sizeof(sinfo);
        
        char CmdLine[MAX_PATH + 5];
        
        snprintf(CmdLine, sizeof(CmdLine), "%s %c", NameBF, ID.Letter);
        
        BOOL OK = CreateProcessA("C:\\Program Files\\NarDiskBackup\\standalone_vss.exe", CmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, NULL, &sinfo, &Result.PInfo);
        
        if(!OK){
            printf("CreateProcessA failed with code %d\n", GetLastError());
            goto FAIL;
        }
        
        DWORD Resume = ResumeThread(Result.PInfo.hThread);
        ASSERT(Resume);
        if(!Resume){
            printf("ResumeThread failed with code %d\n", GetLastError());
            goto FAIL;
        }
        
        
        OVERLAPPED ov = {};
        OK = ConnectNamedPipe(Result.PipeHandle, &ov);
        bool Connected = false;
        // poll for 2.5s
        DWORD Garbage = 0;
        uint32_t WaitMs = 1000;
        if(!GetOverlappedResultEx(Result.PipeHandle, &ov, &Garbage, WaitMs, FALSE)){
            printf("Couldn't estabilish connection to process in %u ms, failed, error code : %d\n", WaitMs, GetLastError());
            goto FAIL;
        }
        
    }
    else{
        printf("Unable to create pipe\n");
        Result = {};
    }
    
    return Result;
    
    FAIL:
    NarFreeProcessListen(&Result);
}

void
NarFreeProcessListen(process_listen_ctx *Ctx){
    
	BOOL OK =  TerminateProcess(Ctx->PInfo.hProcess, 0);
	if(!OK){
		printf("Unable to terminate vss process, ret code %d, error code %d\n", GetLastError());
	}
	
    
    free(Ctx->ReadBuffer);
    free(Ctx->WriteBuffer);
    CloseHandle(Ctx->PipeHandle);
    CloseHandle(Ctx->PInfo.hThread);
    CloseHandle(Ctx->PInfo.hProcess);
    
    memset(Ctx, 0, sizeof(*Ctx));
    return;
}

bool
NarGetVSSPath(process_listen_ctx *Ctx, wchar_t *Out){
    
    bool Result = false;
    memset(Ctx->WriteBuffer, 0, Ctx->BufferSize);
    *(uint32_t*)Ctx->WriteBuffer = ProcessCommandType_GetVSSPath;
    WriteFile(Ctx->PipeHandle, Ctx->WriteBuffer, Ctx->BufferSize, 0, &Ctx->WriteOverlapped);
    DWORD Garbage = 0;
    
    if(GetOverlappedResultEx(Ctx->PipeHandle, &Ctx->WriteOverlapped, &Garbage, 1000, FALSE)){
        ASSERT(Garbage == Ctx->BufferSize);
        Garbage = 0;
        
        ReadFile(Ctx->PipeHandle, Ctx->ReadBuffer, Ctx->BufferSize, 0, &Ctx->ReadOverlapped);
        if(GetOverlappedResultEx(Ctx->PipeHandle, &Ctx->ReadOverlapped, &Garbage, 120000, FALSE)){
            wchar_t *Needle = (wchar_t*)Ctx->ReadBuffer;
            int i = 0;
            for(i =0; *Needle !=0; i++){
                Out[i] = *Needle++;
            }
            Out[i] = 0;
            Result = true;
        }
        else{
            printf("Process didn't answer in given time(120s), abandoning standalone_vss.exe, err code %d\n", GetLastError());
        }
    }
    else{
        printf("Process didn't answer in given time(1s), abandoning standalone_vss.exe, err : %d\n", GetLastError());
    }
    
    if(Result == false){
        Out[0] = 0;
    }
    
    return Result;
}

void 
NarTerminateVSS(process_listen_ctx *Ctx, uint8_t Success){
    
    bool Result = false;
    memset(Ctx->WriteBuffer, 0, Ctx->BufferSize);
    
    *(uint32_t*)Ctx->WriteBuffer       = ProcessCommandType_TerminateVSS;
    *((uint32_t*)Ctx->WriteBuffer + 1) = (uint32_t)Success;
    
    WriteFile(Ctx->PipeHandle, Ctx->WriteBuffer, Ctx->BufferSize, 0, &Ctx->WriteOverlapped);
    DWORD Garbage = 0;
    
    if(GetOverlappedResultEx(Ctx->PipeHandle, &Ctx->WriteOverlapped, &Garbage, 200, FALSE)){
        ASSERT(Garbage == Ctx->BufferSize);
        Garbage = 0;
        
        ReadFile(Ctx->PipeHandle, Ctx->ReadBuffer, Ctx->BufferSize, 0, &Ctx->ReadOverlapped);
        if(GetOverlappedResultEx(Ctx->PipeHandle, &Ctx->ReadOverlapped, &Garbage, 5000, FALSE)){
            
        }
        else{
        	printf("VSS process didn't respond in given time(5s), terminate process failed\n");
        }
    }
    
    
}



BOOLEAN
SetupVSS() {
    /* 
        NOTE(Batuhan): in managed code we dont need to initialize these stuff. since i am shipping code as textual to wrapper, i can detect clr compilation and switch to correct way to initialize
        vss stuff
     */
    
    
    BOOLEAN Return = TRUE;
    HRESULT hResult = 0;
    
    // TODO (Batuhan): Remove that thing if 
    hResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (!SUCCEEDED(hResult)) {
        printf("Failed CoInitialize function %d\n", hResult);
        Return = FALSE;
    }
    
    hResult = CoInitializeSecurity(
                                   NULL,                           //  Allow *all* VSS writers to communicate back!
                                   -1,                             //  Default COM authentication service
                                   NULL,                           //  Default COM authorization service
                                   NULL,                           //  reserved parameter
                                   RPC_C_AUTHN_LEVEL_PKT_PRIVACY,  //  Strongest COM authentication level
                                   RPC_C_IMP_LEVEL_IMPERSONATE,    //  Minimal impersonation abilities
                                   NULL,                           //  Default COM authentication settings
                                   EOAC_DYNAMIC_CLOAKING,          //  Cloaking
                                   NULL                            //  Reserved parameter
                                   );
    
    if (!SUCCEEDED(hResult)) {
        printf("Failed CoInitializeSecurity function %d\n", hResult);
        Return = FALSE;
    }
    return Return;
    
}


//Returns # of volumes detected
data_array<volume_information>
NarGetVolumes() {
    
    data_array<volume_information> Result = { 0,0 };
    wchar_t VolumeString[] = L"!:\\";
    char WindowsLetter = 'C';
    {
        char WindowsDir[512];
        GetWindowsDirectoryA(WindowsDir, 512);
        WindowsLetter = WindowsDir[0];
    }
    
    DWORD Drives = GetLogicalDrives();
    
    for (int CurrentDriveIndex = 0; CurrentDriveIndex < 26; CurrentDriveIndex++) {
        
        if (Drives & (1 << CurrentDriveIndex)) {
            
            VolumeString[0] = (wchar_t)('A' + (char)CurrentDriveIndex);
            ULARGE_INTEGER TotalSize = { 0 };
            ULARGE_INTEGER FreeSize = { 0 };
            
            volume_information T = { 0 };
            
            if (GetDiskFreeSpaceExW(VolumeString, 0, &TotalSize, &FreeSize)) {
                T.Letter = 'A' + (char)CurrentDriveIndex;
                T.TotalSize = TotalSize.QuadPart;
                T.FreeSize = FreeSize.QuadPart;
                
                T.Bootable = (WindowsLetter == T.Letter);
                T.DiskType = (char)NarGetVolumeDiskType(T.Letter);
                T.DiskID = NarGetVolumeDiskID(T.Letter);
                
                {
                    WCHAR fileSystemName[MAX_PATH + 1] = { 0 };
                    DWORD serialNumber = 0;
                    DWORD maxComponentLen = 0;
                    DWORD fileSystemFlags = 0;
                    GetVolumeInformationW(VolumeString, T.VolumeName, sizeof(T.VolumeName), &serialNumber, &maxComponentLen, &fileSystemFlags, fileSystemName, sizeof(fileSystemName));
                }
                
                Result.Insert(T);
            }
            
            
        }
        
    }
    
    return Result;
    
}


void
GUIDToStr(char *Out, GUID guid){
    sprintf(Out, "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}


char
NarGetVolumeLetterFromGUID(GUID G){
    
    wchar_t TempVolumeName[128];
    wchar_t GStr[50];
    
    StringFromGUID2(G,
                    GStr,
                    50);
    
    wsprintfW(TempVolumeName, L"\\\\?\\Volume%s", GStr);
    TempVolumeName[wcslen(TempVolumeName)] = L'\\';
    wchar_t Result[64];
    memset(Result,0,sizeof(Result));
    DWORD T;
    GetVolumePathNamesForVolumeNameW(TempVolumeName, Result, 64, &T);
    
    return Result[0];
}



char* 
NarConsumeNextLine(char *Input, char* Out, size_t MaxBf, char* InpEnd){
    size_t i =0;
    char *Result = 0;
    for(i =0; Input[i] != '\r' && Input + i < InpEnd; i++);
    if(i<MaxBf){
        memcpy(Out, Input, i);
        Out[i] = 0;
        if(Input + (i + 2) < InpEnd){
            Result = &Input[i + 2];
        }
    }
    return Result;
}

char*
NarConsumeNextToken(char *Input, char *Out, size_t MaxBf, char* End){
    
    char* ResultEnd   = 0;
    char* ResultStart = 0;
    
    while(Input < End){
        if(*Input == ' ' || *Input == '\t' || *Input == '\n' || *Input == '\r') {
            Input++;
        }
        else {
            ResultStart = Input;
            break;
        }
    }
    
    while(Input < End){
        if(*Input == ' ' && *Input != '\t' && *Input != '\n' && *Input != '\r') {
            Input++;
        }
        else {
            ResultEnd = Input;
            break;
        }
    }
    
    
    ASSERT(ResultEnd && ResultStart);
    //success
    if(ResultEnd && ResultStart){
        size_t Size = (ResultEnd - ResultStart);
        if(Size <= MaxBf){
            memcpy(Out, ResultStart, Size);
            Out[Size] = 0;
        }
    }
    else{
        
    }
    
    
    return ++Input;
}


/*
Returns array of bits that represents availability. 
First bit corresponds disk with id0,
second corresponds disk with id 1 etc ect. 
*/
uint32_t
NarGetDiskListFromDiskPart() {
    
    uint32_t Result = 0;
    char InputFN[] = "DiskPartListDiskCmd";
    char OutputFN[] = "DiskpartListDiskOutput";
    char DPCommand[] = "list disk";
    char Token[64];
    char Cmd[128];
    
    size_t DataLen = 512;
    char *Data = (char*)malloc(DataLen);
    defer({free(Data);});
    
    snprintf(Cmd, sizeof(Cmd), "diskpart /s %s > %s", InputFN, OutputFN);
    
    
    
    if(NarDumpToFile(InputFN, DPCommand, sizeof(DPCommand))){
        system(Cmd);
        
        file_read Read = NarReadFile(OutputFN);
        if(Read.Data != 0){
            
            
            if(Read.Len > 512){
                DataLen = Read.Len + 1;
                Data = (char*)realloc(Data, DataLen);
            }
            
            memset(Data, 0, DataLen);
            memcpy(Data, Read.Data, Read.Len);
            FreeFileRead(Read);
            
            char *Start = (char*)Data;
            char *End   = (char*)Data + Read.Len;
            char *Needle = Start;
            
            char Line[128];
            
            snprintf(Token, sizeof(Token), "Disk 0");
            Needle = strstr(Start, "Disk 0");
            
            if(Needle != 0){
                
                Result |= 0x1;
                Needle++;
                
                while( (Needle = strstr(Needle, "Disk")) != 0 && *Needle != 0){
                    int DiskID = atoi(Needle + 5);
                    Result |= (1<<DiskID);
                    Needle++;
                }
                
            }
            
            
        }
        else{
            // TODO(Batuhan): log
        }
    }
    else{
        // TODO(Batuhan): 
    }
    
    
    return Result;
}



