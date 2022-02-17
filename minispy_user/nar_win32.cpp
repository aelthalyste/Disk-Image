#include "precompiled.h"

#include "nar_win32.hpp"
#include "performance.hpp"
#include "nar_compression.hpp"
#include "narstring.hpp"
#include "file_explorer.hpp"
#include "performance.hpp"

#pragma comment(lib, "fltLib.lib")
#pragma comment(lib, "vssapi.lib")
#pragma comment(lib, "liblz4_static.lib")
#pragma comment(lib, "libzstd.lib")

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
    if (NarDumpToFile(InputFN, Buffer, (int32_t)strlen(Buffer))) {
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
        printf("Volume %c's GUID %S\n", Letter, VolumeGUID);
        
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
        return 0;
    }
    
    return 1;    
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
            
            Insert->UnusedSize  = Insert->TotalSize;
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
                Insert->Volumes[PartitionID].Letter    = (char)PLayout.PartitionNumber;
                
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





#if 1
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
    return 0;
#if 0
    TIMED_BLOCK();

    if(NULL == Backup)   return 0;
    if(NULL == Metadata) return 0;
    
    
    uint64_t Result = 0;
    backup_metadata *BM = (backup_metadata *)Metadata->Data;
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
#endif
}
#endif



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
    return {0};
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
    
    return (char)Result[0];
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
        if(*Input != ' ' && *Input != '\t' && *Input != '\n' && *Input != '\r') {
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







int32_t
ConnectDriver(PLOG_CONTEXT Ctx) {
    int32_t Result = FALSE;
    HRESULT hResult = FALSE;
    
    DWORD PID = GetCurrentProcessId();
    NAR_CONNECTION_CONTEXT CTX = { 0 };
    
    char Buffer[1024];
    GetWindowsDirectoryA(Buffer, 1024);
    char VolumePath[] = " :";
    VolumePath[0] = Buffer[0];
    char DeviceName[128];
    QueryDosDeviceA(VolumePath, DeviceName, 128);
    char* IntPtr = DeviceName;
    while (IsNumeric(*IntPtr) && *(++IntPtr) != '\0');
    CTX.OsDeviceID = atoi(IntPtr);
    CTX.PID = PID;
    CTX.OsDeviceID = Buffer[0];
    DWORD BytesWritten = 256;
    //GetUserNameW(&CTX.UserName[0], &BytesWritten) && BytesWritten != 0
    
    {
        hResult = FilterConnectCommunicationPort(MINISPY_PORT_NAME,
                                                 0,
                                                 &CTX, sizeof(CTX),
                                                 NULL, &Ctx->Port);
        
        if (!IS_ERROR(hResult)) {
            Result = TRUE;
        }
        else {
            printf("Could not connect to filter: 0x%08x\n", hResult);
            printf("Program PID is %d\n", PID);
        }
        
    }
    
    
    return Result;
}



int32_t
SetIncRecords(HANDLE CommPort, volume_backup_inf* V) {
    
    int32_t Result = FALSE;
    if(V == NULL){
        printf("SetIncRecords: VolInf was null\n");
        return FALSE;
    }
    
    V->PossibleNewBackupRegionOffsetMark = NarGetLogFileSizeFromKernel(CommPort, (char)V->Letter);
    
    printf("PNBRO : %I64u\n", V->PossibleNewBackupRegionOffsetMark);
    printf("SIR: Volume %c, lko %I64u\n", V->Letter, V->IncLogMark.LastBackupRegionOffset);
    
    ASSERT((uint64_t)V->IncLogMark.LastBackupRegionOffset <= (uint64_t)V->PossibleNewBackupRegionOffsetMark);
    
    DWORD TargetReadSize = (DWORD)(V->PossibleNewBackupRegionOffsetMark - V->IncLogMark.LastBackupRegionOffset);
    
    V->Stream.Records.Data = 0;
    V->Stream.Records.Count = 0;
    
    std::wstring logfilepath = GenerateLogFilePath((char)V->Letter);
    HANDLE LogHandle = CreateFileW(logfilepath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
    
    // calling malloc with 0 is implementation defined behaviour, but not having logs is possible thing that might happen and no need to worry about it since
    // setupstream will include mft to stream, stream will not be empty.
    if(TargetReadSize != 0){
        
        if(NarSetFilePointer(LogHandle, (uint64_t)V->IncLogMark.LastBackupRegionOffset)){
            
            V->Stream.Records.Data = (nar_record*)malloc(TargetReadSize);
            V->Stream.Records.Count = TargetReadSize/sizeof(nar_record);
            DWORD BytesRead = 0;
            
            if(ReadFile(LogHandle, V->Stream.Records.Data, TargetReadSize, &BytesRead, 0) && BytesRead == TargetReadSize){
                Result = TRUE;
            }
            else{
                
                printf("SetIncRecords Couldnt read %lu, instead read %lu\n", TargetReadSize, BytesRead);
                if(BytesRead == 0){
                    free(V->Stream.Records.Data);
                    V->Stream.Records.Count = 0;
                }
                
            }
        }
        else{
            printf("Couldnt set file pointer\n");
        }
        
        if(Result == TRUE){
            
#if 0            
            qsort(V->Stream.Records.Data, V->Stream.Records.Count, sizeof(nar_record), CompareNarRecords);
            MergeRegions(&V->Stream.Records);
#endif
            
        }
        
    }
    else{
        V->Stream.Records.Data = 0;
        V->Stream.Records.Count = 0;
        Result = TRUE;
    }
    
    return Result;
}



ULONGLONG
NarGetLogFileSizeFromKernel(HANDLE CommPort, char Letter){
    
    ULONGLONG Result = 0;
    
    NAR_COMMAND Cmd = {};
    Cmd.Letter = Letter;
    Cmd.Type   = NarCommandType_FlushLog;
    
    if(NarGetVolumeGUIDKernelCompatible(Letter, &Cmd.VolumeGUIDStr[0])){
        
        DWORD BR = 0;
        NAR_LOG_INF Respond = {0};
        HRESULT hResult = FilterSendMessage(CommPort, &Cmd, sizeof(NAR_COMMAND), &Respond, sizeof(Respond), &BR);
        if(SUCCEEDED(hResult) && FALSE == Respond.ErrorOccured){
            Result = Respond.CurrentSize;
            printf("KERNEL RESPONSE: Log size of volume %c is %I64u\n", Letter, Result);
        }
        else{
            printf("FilterSendMessage failed, couldnt get log size from kernel side, err %i\n", hResult);
        }
        
    }
    else{
        printf("Unable to generate kernel GUID of volume %c\n", Letter);
    }
    
    return Result;
}




int32_t
SetDiffRecords(HANDLE CommPort ,volume_backup_inf* V) {
    
    TIMED_BLOCK();
    
    if(V == NULL) return FALSE;
    
    printf("Entered SetDiffRecords\n");
    int32_t Result = FALSE;
    
    V->PossibleNewBackupRegionOffsetMark = NarGetLogFileSizeFromKernel(CommPort, (char)V->Letter);
    std::wstring logfilepath = GenerateLogFilePath((char)V->Letter);
    HANDLE LogHandle = CreateFileW(logfilepath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
    
    
    if(LogHandle == INVALID_HANDLE_VALUE){
        // TODO
    }
    
    printf("Logname : %S\n", logfilepath.c_str());
    
    // NOTE (Batuhan): IMPORTANT, so if total log size exceeds 4GB, we cant backup succcessfully, since reading file larger than 4GB is 
    // problem by itself, one can do partially load file and continuously compress it via qsort & mergeregions. then final log size drastically
    // becames lower.
    
    // not so safe to truncate
    DWORD TargetReadSize = (DWORD)(V->PossibleNewBackupRegionOffsetMark - V->DiffLogMark.BackupStartOffset); 
    
    // calling malloc with 0 is not a good idea
    if(TargetReadSize != 0){
        
        if(NarSetFilePointer(LogHandle, V->DiffLogMark.BackupStartOffset)){
            
            V->Stream.Records.Data = (nar_record*)malloc(TargetReadSize);
            V->Stream.Records.Count = TargetReadSize / sizeof(nar_record);
            DWORD BytesRead = 0;
            if(ReadFile(LogHandle, V->Stream.Records.Data, TargetReadSize, &BytesRead,0) && BytesRead == TargetReadSize){
                printf("Succ read diff records, will sort and merge regions to save in space\n");
                Result = TRUE;
            }
            else{
                printf("SetDiffRecords Couldnt read %lu, instead read %lu\n", TargetReadSize, BytesRead);
                
                free(V->Stream.Records.Data);
                V->Stream.Records.Count = 0;
            }
            
        }
        else{
            printf("Couldnt set file pointer to beginning of the file\n");
        }
        
        if(Result == TRUE){
            
#if 0            
            qsort(V->Stream.Records.Data, V->Stream.Records.Count, sizeof(nar_record), CompareNarRecords);
            MergeRegions(&V->Stream.Records);
#endif
            
        }
        
    }
    else{
        V->Stream.Records.Data = 0;
        V->Stream.Records.Count = 0;
        Result = TRUE;
    }
    
    CloseHandle(LogHandle);
    
    return Result;
}




int32_t
SetFullRecords(volume_backup_inf* V) {
    
    //UINT* ClusterIndices = 0;
    
    uint32_t Count = 0;
    V->Stream.Records.Data  = GetVolumeRegionsFromBitmap(V->Stream.Handle, &Count);
    V->Stream.Records.Count = Count;
    
    return (V->Stream.Records.Data != NULL);
}



/*
  From given volume handle, extracts region information from volume
  Handle can be VSS handle or normal volume handle. VSS is preferred since volume information might change over time
  returns NULL if any error occurs
*/
nar_record*
GetVolumeRegionsFromBitmap(HANDLE VolumeHandle, uint32_t* OutRecordCount) {
    
    if(OutRecordCount == NULL) return NULL;
    
    nar_record* Records = NULL;
    uint32_t RecordCount = 0;
    
    STARTING_LCN_INPUT_BUFFER StartingLCN;
    StartingLCN.StartingLcn.QuadPart = 0;
    ULONGLONG MaxClusterCount = 0;
    
    // temporary allocation, will extend after determining exact size of the bitmap
    DWORD BufferSize = Megabyte(32); 
    
    VOLUME_BITMAP_BUFFER* Bitmap = (VOLUME_BITMAP_BUFFER*)malloc(BufferSize);
    
    if (Bitmap != NULL) {
        
        int HitC = 0;
        p:;
        HitC++;
        
        HRESULT R = DeviceIoControl(VolumeHandle, FSCTL_GET_VOLUME_BITMAP, &StartingLCN, sizeof(StartingLCN), Bitmap, BufferSize, 0, 0);
        if (SUCCEEDED(R)) {
            
            if(HitC == 1){
                if(BufferSize < Bitmap->BitmapSize.QuadPart + sizeof(VOLUME_BITMAP_BUFFER)){
                    Bitmap = (VOLUME_BITMAP_BUFFER*)realloc(Bitmap, Bitmap->BitmapSize.QuadPart + sizeof(VOLUME_BITMAP_BUFFER) + 5);
                    goto p;
                }
            }
            
            MaxClusterCount = (ULONGLONG)Bitmap->BitmapSize.QuadPart;
            uint32_t ClusterIndex = 0;
            UCHAR* BitmapIndex = Bitmap->Buffer;
            UCHAR BitmapMask = 1;
            //uint32_t CurrentIndex = 0;
            uint32_t LastActiveCluster = 0;
            
            uint32_t RecordBufferCount = 8 * 1024 * 1024;
            Records = (nar_record*)calloc(RecordBufferCount, sizeof(nar_record));
            
            BitmapMask = 1;
            
            auto Start = NarGetPerfCounter();
            
            while (BitmapIndex != (Bitmap->Buffer + Bitmap->BitmapSize.QuadPart)) {
                
                if(*BitmapIndex & BitmapMask){
                    Records[RecordCount++].StartPos = ClusterIndex;
                    //ASSERT(RecordCount < 46000);
                    while(BitmapIndex != (Bitmap->Buffer + Bitmap->BitmapSize.QuadPart)){
                        if(*BitmapIndex & BitmapMask){
                            Records[RecordCount - 1].Len++;
                            BitmapMask<<=1;
                            if(BitmapMask == 0){
                                BitmapMask = 1;
                                BitmapIndex++;
                            }
                            ClusterIndex++;
                        }
                        else{
                            break;
                        }
                    }
                    
                }
                else{
                    BitmapMask<<=1;
                    if (BitmapMask == 0) {
                        BitmapMask = 1;
                        BitmapIndex++;
                    }
                    ClusterIndex++;
                }
                
            }
            
            printf("Successfully set fullbackup records\n");
            
            double Elapsed = NarTimeElapsed(Start);
            printf("Time elapsed %.5f\n", Elapsed);
            
            
        }
        else {
            // NOTE(Batuhan): DeviceIOControl failed
            printf("Get volume bitmap failed [DeviceIoControl].\n");
            printf("Result was -> %d\n", R);
        }
        
    }
    else {
        printf("Couldn't allocate memory for bitmap buffer, tried to allocate %u bytes!\n", BufferSize);
    }
    
    
    free(Bitmap);
    
    if(Records != NULL){
        Records = (nar_record*)realloc(Records, RecordCount*sizeof(nar_record));
        (*OutRecordCount) = RecordCount;
    }
    else{
        (*OutRecordCount) = 0;
    }
    
    return Records;
}


/*
  From given volume handle, extracts region information from volume
  Handle can be VSS handle or normal volume handle. VSS is preferred since volume information might change over time
  returns NULL if any error occurs
*/
nar_record*
OLD_GetVolumeRegionsFromBitmap(HANDLE VolumeHandle, uint32_t* OutRecordCount) {
    
    if(OutRecordCount == NULL) return NULL;
    
    nar_record* Records = NULL;
    uint32_t RecordCount = 0;
    
    STARTING_LCN_INPUT_BUFFER StartingLCN;
    StartingLCN.StartingLcn.QuadPart = 0;
    ULONGLONG MaxClusterCount = 0;
    
    // temporary allocation, will extend after determining exact size of the bitmap
    DWORD BufferSize = Megabyte(32); 
    
    VOLUME_BITMAP_BUFFER* Bitmap = (VOLUME_BITMAP_BUFFER*)malloc(BufferSize);
    
    if (Bitmap != NULL) {
        
        int HitC = 0;
        p:;
        HitC++;
        
        HRESULT R = DeviceIoControl(VolumeHandle, FSCTL_GET_VOLUME_BITMAP, &StartingLCN, sizeof(StartingLCN), Bitmap, BufferSize, 0, 0);
        if (SUCCEEDED(R)) {
            
            if(HitC == 1){
                if(BufferSize < Bitmap->BitmapSize.QuadPart + sizeof(VOLUME_BITMAP_BUFFER)){
                    Bitmap = (VOLUME_BITMAP_BUFFER*)realloc(Bitmap, Bitmap->BitmapSize.QuadPart + sizeof(VOLUME_BITMAP_BUFFER) + 5);
                    goto p;
                }
            }
            
            MaxClusterCount = (ULONGLONG)Bitmap->BitmapSize.QuadPart;
            uint32_t ClusterIndex = 0;
            UCHAR* BitmapIndex = Bitmap->Buffer;
            UCHAR BitmapMask = 1;
            //uint32_t CurrentIndex = 0;
            uint32_t LastActiveCluster = 0;
            
            uint32_t RecordBufferCount = 1 * 1024 * 1024;
            Records = (nar_record*)calloc(RecordBufferCount, sizeof(nar_record));
            
            Records[0] = { 0,1 };
            RecordCount++; 
            
            ClusterIndex++;
            BitmapMask <<= 1;
            
            auto Start = NarGetPerfCounter();
            
            while (BitmapIndex != (Bitmap->Buffer + Bitmap->BitmapSize.QuadPart)) {
                
                if ((*BitmapIndex & BitmapMask) == BitmapMask) {
                    
                    if (LastActiveCluster == ClusterIndex - 1) {
                        Records[RecordCount - 1].Len++;
                    }
                    else {
                        nar_record dbg = {0};
                        dbg.StartPos = ClusterIndex;
                        dbg.Len      = 1;
                        //ASSERT(RecordCount < 45584);
                        Records[RecordCount++] = dbg;
                    }
                    
                    LastActiveCluster = ClusterIndex;
                }
                BitmapMask <<= 1;
                if (BitmapMask == 0) {
                    BitmapMask = 1;
                    BitmapIndex++;
                }
                ClusterIndex++;
                
            }
            
            printf("Successfully set fullbackup records\n");
            
            double Elapsed = NarTimeElapsed(Start);
            printf("Time elapsed %.5f\n", Elapsed);
            
        }
        else {
            // NOTE(Batuhan): DeviceIOControl failed
            printf("Get volume bitmap failed [DeviceIoControl].\n");
            printf("Result was -> %d\n", R);
        }
        
    }
    else {
        printf("Couldn't allocate memory for bitmap buffer, tried to allocate %u bytes!\n", BufferSize);
    }
    
    
    free(Bitmap);
    
    if(Records != NULL){
        Records = (nar_record*)realloc(Records, RecordCount*sizeof(nar_record));
        (*OutRecordCount) = RecordCount;
    }
    else{
        (*OutRecordCount) = 0;
    }
    
    return Records;
}



full_only_backup_ctx
SetupFullOnlyStream(wchar_t Letter, DotNetStreamInf *SI, bool ShouldCompress, bool RegionLock){
    
    full_only_backup_ctx Result = {0};
    bool Success = false;
    
    wchar_t ShadowPath[256];
    
    uint64_t SIAdress    = reinterpret_cast<uintptr_t>(SI);
    uint64_t StackAdress = reinterpret_cast<uintptr_t>(&Result);
    
    pcg32_random_t RandomSeed;
    RandomSeed.state = SIAdress ^ StackAdress;
    
    uint64_t LowerHalf = pcg32_random_r(&RandomSeed);
    uint64_t UpperHalf = pcg32_random_r(&RandomSeed);
    Result.Letter = (char)Letter;
    
    Result.Stream.ShouldCompress = ShouldCompress;
    Result.Stream.RegionLock     = RegionLock;
    
    {
        wchar_t V[] = L"!:\\";
        V[0] = Letter;
        DWORD SectorsPerCluster, BytesPerSector, ClusterCount;
        
        GetDiskFreeSpaceW(V, &SectorsPerCluster, &BytesPerSector, 0, &ClusterCount);
        Result.Stream.ClusterSize = SectorsPerCluster * BytesPerSector;
        Result.VolumeTotalClusterCount = ClusterCount;
    }
    
    Result.BackupID.Q = (UpperHalf << 32ull) | LowerHalf;
    Result.BackupID.Letter = (char)Letter;
    Result.BackupID = NarSetAsFullOnlyBackup(Result.BackupID);
    
    
    Result.PLCtx = NarSetupVSSListen(Result.BackupID);
    if(Result.PLCtx.ReadBuffer != 0){
        if(NarGetVSSPath(&Result.PLCtx, ShadowPath)){
            Result.Stream.Handle = CreateFileW(ShadowPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
            if(Result.Stream.Handle != INVALID_HANDLE_VALUE){
                
                // NOTE(Batuhan): Fetch backup indices
                Result.Stream.Records.Data = GetVolumeRegionsFromBitmap(Result.Stream.Handle, &Result.Stream.Records.Count);
                
#if 0           // we dont need _that_     
                data_array<nar_record> MFTandINDXRegions = GetMFTandINDXLCN((char)Letter, Result.Stream.Handle);
                Result.Stream.Records.Data = (nar_record*)realloc(Result.Stream.Records.Data, (Result.Stream.Records.Count + MFTandINDXRegions.Count) * sizeof(nar_record));
                memcpy(&Result.Stream.Records.Data[Result.Stream.Records.Count], MFTandINDXRegions.Data, MFTandINDXRegions.Count * sizeof(nar_record));
                
                Result.Stream.Records.Count += MFTandINDXRegions.Count;
                
                qsort(Result.Stream.Records.Data, Result.Stream.Records.Count, sizeof(nar_record), CompareNarRecords);
                MergeRegions(&Result.Stream.Records);
#endif
                
                Success = true;
                printf("Successfully initialized full only backup for letter %c\n", Letter);
            }
            else{
                printf("Unable to open vss handle!\n");
            }
            
        }
        else{
            printf("Unable to get vss path from process\n");
        }
        
    }
    else{
        printf("Unable to create and listen process\n"); 
    }
    
    
    if(true == Success){
                
        int TruncateIndex = -1;
        
        SI->ClusterCount = 0;
        SI->ClusterSize = Result.Stream.ClusterSize;
        
        for(unsigned int RecordIndex = 0; RecordIndex < Result.Stream.Records.Count; RecordIndex++){
            if((int64_t)Result.Stream.Records.Data[RecordIndex].StartPos + (int64_t)Result.Stream.Records.Data[RecordIndex].Len > (int64_t)Result.VolumeTotalClusterCount){
                TruncateIndex = RecordIndex;
                break;
            }
            else{
                SI->ClusterCount += Result.Stream.Records.Data[RecordIndex].Len;
            }
        }
        
        
        if(TruncateIndex > 0){
            printf("Found regions that exceeds volume size, truncating stream record array from %i to %i\n", Result.Stream.Records.Count, TruncateIndex);
            
            uint32_t NewEnd = Result.VolumeTotalClusterCount;
            Result.Stream.Records.Data[TruncateIndex].Len = NewEnd - Result.Stream.Records.Data[TruncateIndex].StartPos;
            
            Result.Stream.Records.Data = 
                (nar_record*)realloc(Result.Stream.Records.Data, (TruncateIndex + 1)*sizeof(nar_record));            
            Result.Stream.Records.Count = TruncateIndex + 1;
        }
        
        
        // NOTE(Batuhan): Compression stuff
        if(ShouldCompress){
            
            Result.Stream.CompressionBuffer = malloc(NAR_COMPRESSION_FRAME_SIZE);
            Result.Stream.BufferSize        = NAR_COMPRESSION_FRAME_SIZE;
            
            if(NULL != Result.Stream.CompressionBuffer){        
                Result.Stream.ShouldCompress = true;
                
                Result.Stream.CCtx = ZSTD_createCCtx();
                ASSERT(Result.Stream.CCtx);
                
                ZSTD_bounds ThreadBounds = ZSTD_cParam_getBounds(ZSTD_c_nbWorkers);
                if(!ZSTD_isError(ThreadBounds.error)){
                    ZSTD_CCtx_setParameter(Result.Stream.CCtx, ZSTD_c_nbWorkers, ThreadBounds.upperBound);
                }
                else{
                    NAR_BREAK;
                    printf("Couldn't query worker thread bounds for compression, zstd error code : %X\n", ThreadBounds.error);
                    ZSTD_CCtx_setParameter(Result.Stream.CCtx, ZSTD_c_nbWorkers, 0);
                }
                
                size_t RetCode = ZSTD_CCtx_setParameter(Result.Stream.CCtx, ZSTD_c_compressionLevel, ZSTD_strategy::ZSTD_lazy);
                ASSERT(!ZSTD_isError(RetCode));
                
                size_t BackupSize = (uint64_t)SI->ClusterCount*(uint64_t)SI->ClusterSize;
                
                Result.Stream.MaxCBI   = 2*BackupSize/(NAR_COMPRESSION_FRAME_SIZE);
                Result.Stream.CompInf  = (nar_record*)calloc(Result.Stream.MaxCBI, sizeof(nar_record));
                Result.Stream.CBII     = 0;
                
            }
            else{
                Result.Stream.ShouldCompress = false;        
                Result.Stream.BufferSize = 0;
            }
        }
        
        
    }
    else{
        // TODO(Batuhan): terminate process!
        Result = {};
    }
    
    Result.InitSuccess = Success;
    return Result;
}

#if 0
void
TerminateFullOnlyStream(full_only_backup_ctx *Ctx, bool ShouldSaveMetadata){
    
    printf("Terminating full only stream for volume %c\n", Ctx->Letter);
    
    
    if(ShouldSaveMetadata){
        SaveMetadata(Ctx->Letter, NAR_FULLBACKUP_VERSION, Ctx->Stream.ClusterSize, BackupType::Diff, Ctx->Stream.Records, Ctx->BackupID, Ctx->Stream.ShouldCompress, Ctx->Stream.CompInf, Ctx->Stream.CBII, metadatadir);
    }
    
    CloseHandle(Ctx->Stream.Handle);
    free(Ctx->Stream.Records.Data);
    if(Ctx->Stream.ShouldCompress){
        free(Ctx->Stream.CompressionBuffer);
    }
    if(Ctx->Stream.CCtx){
        ZSTD_freeCCtx(Ctx->Stream.CCtx);
    }
    
    NarTerminateVSS(&Ctx->PLCtx, true);
    NarFreeProcessListen(&Ctx->PLCtx);
    
}
#endif

/*
*/
int32_t
SetupStream(PLOG_CONTEXT C, wchar_t L, BackupType Type, uint64_t *BytesToTransferOut, char *BinaryExtensionOut, int32_t CompressionType) {
    
    if(C == 0)
        printf("Setupstream called with context zero\n");
    
    ASSERT(BinaryExtensionOut);
    ASSERT(BytesToTransferOut);
    ASSERT(C);

    if (BinaryExtensionOut)
        BinaryExtensionOut[0] = 0;
    if (BytesToTransferOut)
        *BytesToTransferOut = 0;
    
    if (BinaryExtensionOut) 
        strcpy(BinaryExtensionOut, NAR_BINARY_EXTENSION);

    int32_t Return = FALSE;
    int ID = GetVolumeID(C, L);
    
    if (ID < 0) {
        //If volume not found, try to add it
        printf("Couldnt find volume %c in list, adding it for stream setup\n", L);
        AddVolumeToTrack(C, L, Type);
        ID = GetVolumeID(C, L);
        if (ID < 0) {           
            printf("Unable to add volume %c to tracklist\n", L);
            return FALSE;
        }
    }
    
    volume_backup_inf* VolInf = &C->Volumes.Data[ID];    
    printf("Entered setup stream for volume %c, version %i\n", (char)L, VolInf->Version);
    
    VolInf->Stream.ClusterIndex = 0;
    VolInf->Stream.RecIndex = 0;
    VolInf->Stream.Records.Count = 0;
    
    auto AppendINDXnMFTLCNToStream = [&]() {
        TIMED_NAMED_BLOCK("AppendINDXMFTLCN");
        data_array<nar_record> MFTandINDXRegions = GetMFTandINDXLCN((char)L, VolInf->Stream.Handle);
        if (MFTandINDXRegions.Data != 0) {
            
            printf("Parsed MFTLCN for volume %c for version %i\n", (wchar_t)VolInf->Letter, VolInf->Version);
            
            VolInf->Stream.Records.Data = (nar_record*)realloc(VolInf->Stream.Records.Data, (VolInf->Stream.Records.Count + MFTandINDXRegions.Count) * sizeof(nar_record));
            memcpy(&VolInf->Stream.Records.Data[VolInf->Stream.Records.Count], MFTandINDXRegions.Data, MFTandINDXRegions.Count * sizeof(nar_record));
            
            VolInf->Stream.Records.Count += MFTandINDXRegions.Count;
            
            qsort(VolInf->Stream.Records.Data, VolInf->Stream.Records.Count, sizeof(nar_record), CompareNarRecords);
            MergeRegions(&VolInf->Stream.Records);
            
#if 0            
            for(size_t i =0; i<VolInf->Stream.Records.Count; i++){
                printf("MERGED REGIONS OVERALL : %9u %9u\n", VolInf->Stream.Records.Data[i].StartPos, VolInf->Stream.Records.Data[i].Len);
            }
#endif
            
        }
        else {
            printf("Couldnt parse MFT at setupstream function for volume %c, version %i\n", L, VolInf->Version);
        }
        
        FreeDataArray(&MFTandINDXRegions);
    };
    
    
    WCHAR Temp[] = L"!:\\";
    Temp[0] = VolInf->Letter;
    wchar_t ShadowPath[256];
    
    
#if 1    
    VolInf->PLCtx = NarSetupVSSListen(VolInf->BackupID);
    if(VolInf->PLCtx.ReadBuffer == 0){
        printf("Unable to setup and connect process\n");
        return FALSE;
    }
    
    if(!NarGetVSSPath(&VolInf->PLCtx, ShadowPath)){
        printf("Unable to get vss path from process\n");
        return  FALSE;
    }
#else  
    VolInf->SnapshotID = GetShadowPath(Temp, VolInf->VSSPTR, ShadowPath, sizeof(ShadowPath)/sizeof(ShadowPath[0]));
#endif
    
    
    // no overheat for attaching volume again and again
    if(FALSE == AttachVolume((char)VolInf->Letter)){
        printf("Cant attach volume\n");
        Return = FALSE;
    }
    
    
    VolInf->Stream.Handle = CreateFileW(ShadowPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
    if (VolInf->Stream.Handle == INVALID_HANDLE_VALUE) {
        printf("Can not open shadow path %S..\n", ShadowPath);
        Return = FALSE;
    }
    printf("Setup stream handle successfully\n");
    
    if (VolInf->Version == NAR_FULLBACKUP_VERSION) {
        printf("Fullbackup stream is preparing\n");
        
        //Fullbackup stream
        if (SetFullRecords(VolInf)) {
            Return = TRUE;
        }
        else {
            printf("SetFullRecords failed!\n");
        }
        

    }
    else {
        //Incremental or diff stream
        BackupType T = VolInf->BT;
        
        if (T == BackupType::Diff) {
            if (SetDiffRecords(C->Port,VolInf)) {
                Return = TRUE;
            }
            else {
                printf("Couldn't setup diff records for streaming\n");
            }
            
        }
        else if (T == BackupType::Inc) {
            if (SetIncRecords(C->Port, VolInf)) {
                Return = TRUE;
            }
            else {
                printf("Couldn't setup inc records for streaming\n");
            }
            
        }
    }
    
    
    if(Return != FALSE){
        volume_backup_inf* V = VolInf;
        
#if 0        
        printf("DRIVER REGIONS : \n");
        for(size_t i =0; i<V->Stream.Records.Count; i++){
            printf("Start : %9u Len : %9u\n", V->Stream.Records.Data[i].StartPos,  V->Stream.Records.Data[i].Len);
        }
#endif
        
        AppendINDXnMFTLCNToStream();
        int TruncateIndex = -1;
        
        uint64_t ClusterCount = 0;
        uint64_t ClusterSize  = V->ClusterSize;
        
        for(unsigned int RecordIndex = 0; RecordIndex < V->Stream.Records.Count; RecordIndex++){
            if((int64_t)V->Stream.Records.Data[RecordIndex].StartPos + (int64_t)V->Stream.Records.Data[RecordIndex].Len > (int64_t)V->VolumeTotalClusterCount){
                TruncateIndex = RecordIndex;
                break;
            }
            else{
                ClusterCount += V->Stream.Records.Data[RecordIndex].Len;
            }
        }
        
        ASSERT(BytesToTransferOut);
        if (BytesToTransferOut) 
            *BytesToTransferOut = ClusterCount * ClusterSize; 
        

        // @NOTE : truncate exceeding clusters
        if(TruncateIndex > 0){
            printf("Found regions that exceeds volume size, truncating stream record array from %i to %i\n", V->Stream.Records.Count, TruncateIndex);
            
            uint32_t NewEnd = V->VolumeTotalClusterCount;
            V->Stream.Records.Data[TruncateIndex].Len = NewEnd - V->Stream.Records.Data[TruncateIndex].StartPos;
            
            ASSERT(V->Stream.Records.Data[TruncateIndex].StartPos + V->Stream.Records.Data[TruncateIndex].Len <= V->VolumeTotalClusterCount);
            
            V->Stream.Records.Data = 
                (nar_record*)realloc(V->Stream.Records.Data, (TruncateIndex + 1)*sizeof(nar_record));            
            V->Stream.Records.Count = TruncateIndex + 1;
        }
        else if (TruncateIndex == 0) {
            printf("Error occured, trimming all stream record information\n");
            Return = FALSE;
            FreeDataArray(&V->Stream.Records);
        }


        // NOTE(Batuhan): Compression stuff
        if (TruncateIndex != 0) {
            
            if (CompressionType == NAR_COMPRESSION_LZ4) 
                printf("LZ4 compression not supported(yet!)\n");
            
            ASSERT(CompressionType != NAR_COMPRESSION_LZ4);

            if (CompressionType == NAR_COMPRESSION_ZSTD) {
                VolInf->Stream.CompressionBuffer = malloc(NAR_COMPRESSION_FRAME_SIZE);
                VolInf->Stream.BufferSize        = NAR_COMPRESSION_FRAME_SIZE;
                
                ASSERT(VolInf->Stream.CompressionBuffer);

                if(NULL != VolInf->Stream.CompressionBuffer){        
                    VolInf->Stream.ShouldCompress = true;
                    
                    VolInf->Stream.CCtx = ZSTD_createCCtx();
                    ASSERT(VolInf->Stream.CCtx);
                    
                    ZSTD_bounds ThreadBounds = ZSTD_cParam_getBounds(ZSTD_c_nbWorkers);
                    if(!ZSTD_isError(ThreadBounds.error)) {
                        ZSTD_CCtx_setParameter(VolInf->Stream.CCtx, ZSTD_c_nbWorkers, ThreadBounds.upperBound);
                    } else {
                        printf("Couldn't query worker thread bounds for compression, zstd error code : %X\n", ThreadBounds.error);
                    }
                    
                    size_t RetCode = ZSTD_CCtx_setParameter(VolInf->Stream.CCtx, ZSTD_c_compressionLevel, ZSTD_strategy::ZSTD_lazy);
                    ASSERT(!ZSTD_isError(RetCode));
                    
                    size_t BackupSize = ClusterCount * ClusterSize;
                    VolInf->Stream.MaxCBI   = 2*BackupSize/(NAR_COMPRESSION_FRAME_SIZE);
                    VolInf->Stream.CompInf  = (nar_record*)calloc(VolInf->Stream.MaxCBI, sizeof(nar_record));
                    VolInf->Stream.CBII     = 0;
                    
                }
                else{
                    VolInf->Stream.ShouldCompress = false;        
                    VolInf->Stream.BufferSize = 0;
                }

            } // endif zstd support

        }
        
    }
    


    VolInf->Stream.ClusterSize = VolInf->ClusterSize;
    
    return Return;
}



int32_t
TerminateBackup(volume_backup_inf* V, int32_t Succeeded, const char *DirectoryToEmitMetadata, char *OutputMetadataName) {
    
    int32_t Result = TRUE;
    if (!V) return FALSE;
    
    printf("Volume %c version %i backup operation will be terminated\n", V->Letter, V->Version);
    
    if(Succeeded) {

        ASSERT(DirectoryToEmitMetadata);
        char FP[256];
        char BackupIDStr[256];
        NarBackupIDToStr(V->BackupID, BackupIDStr, sizeof(BackupIDStr));

        FP[0] = 0;
        if (DirectoryToEmitMetadata != NULL && DirectoryToEmitMetadata[0] != 0) {
            strcat(FP, DirectoryToEmitMetadata);
            strcat(FP, "\\");
        }
        strcat(FP, BackupIDStr);
        strcat(FP, NAR_METADATA_EXTENSION);
        

        ASSERT(OutputMetadataName);
        if (OutputMetadataName) {
            strcpy(OutputMetadataName, FP);
        }

        if(!!SaveMetadata((char)V->Letter, V->Version, V->ClusterSize, V->BT, V->Stream.Records, V->BackupID, V->Stream.ShouldCompress, V->Stream.CompressionType, V->Stream.CompInf, V->Stream.CBII, FP)){
            
            if(V->BT == BackupType::Inc)
                V->IncLogMark.LastBackupRegionOffset = V->PossibleNewBackupRegionOffsetMark;
            

            V->PossibleNewBackupRegionOffsetMark = 0;
            V->Version++;
        }
        else{
            printf("Couldn't save metadata. Version %i volume %c\n", V->Version, V->Letter);
            Result = FALSE;
        }
    }
    else {
        
    }
     
    
    if(NULL != V->Stream.CompressionBuffer){
        V->Stream.BufferSize = 0;
        free(V->Stream.CompressionBuffer);
        V->Stream.CompressionBuffer = NULL;
    }
    
    if(NULL != V->Stream.CCtx) {
        ZSTD_freeCCtx(V->Stream.CCtx);
    }
    if(V->Stream.Handle != INVALID_HANDLE_VALUE) {
        CloseHandle(V->Stream.Handle);
        V->Stream.Handle = INVALID_HANDLE_VALUE;
    }
    if(V->Stream.Records.Data != NULL){
        FreeDataArray(&V->Stream.Records);
    }
    if(V->Stream.CompInf != 0){
        free(V->Stream.CompInf);
        V->Stream.CompInf = 0;
        V->Stream.CBII   = 0;
        V->Stream.MaxCBI = 0;
    }
    
    V->Stream.Records.Count = 0;
    V->Stream.RecIndex = 0;
    V->Stream.ClusterIndex = 0;
    
    NarTerminateVSS(&V->PLCtx, 1);
    NarFreeProcessListen(&V->PLCtx);
    
    return Result;
}




// Assumes CallerBufferSize >= NAR_COMPRESSION_FRAME_SIZE
uint32_t
ReadStream(backup_stream *Stream, void* CallerBuffer, uint32_t CallerBufferSize) {
    
    //TotalSize MUST be multiple of cluster size
    uint32_t Result = 0;
    
    void* BufferToFill = CallerBuffer;
    unsigned int TotalSize = CallerBufferSize;
    if(true == Stream->ShouldCompress){
        BufferToFill    = Stream->CompressionBuffer;
        TotalSize       = (uint32_t)Stream->BufferSize;
    }
    
    if (TotalSize == 0) {
        printf("Passed totalsize as 0, terminating now\n");
        return TRUE;
    }
    
    unsigned int RemainingSize = TotalSize;
    void* CurrentBufferOffset = BufferToFill;
    
    if ((uint32_t)Stream->RecIndex >= Stream->Records.Count) {
        printf("End of the stream\n", Stream->RecIndex, Stream->Records.Count);
        return Result;
    }
    
    
    while (RemainingSize) {
        
        if ((uint32_t)Stream->RecIndex >= Stream->Records.Count) {
            printf("Rec index was higher than record's count, result %u, rec_index %i rec_count %i\n", Result, Stream->RecIndex, Stream->Records.Count);
            break;
        }
        
        DWORD BytesReadAfterOperation = 0;
        
        uint64_t ClustersRemainingByteSize = (uint64_t)Stream->Records.Data[Stream->RecIndex].Len - (uint64_t)Stream->ClusterIndex;
        ClustersRemainingByteSize *= Stream->ClusterSize;
        
        
        DWORD ReadSize = (DWORD)MIN((uint64_t)RemainingSize, ClustersRemainingByteSize); 
        
        ULONGLONG FilePtrTarget = (ULONGLONG)Stream->ClusterSize * ((ULONGLONG)Stream->Records.Data[Stream->RecIndex].StartPos + (ULONGLONG)Stream->ClusterIndex);
        if (NarSetFilePointer(Stream->Handle, FilePtrTarget)) {
            
            Stream->BytesReadOffset = FilePtrTarget;
            
#if 0
            ASSERT(ReadSize % 4096 == 0);
            ASSERT(FilePtrTarget % 4096 == 0);
            ASSERT(((char*)CurrentBufferOffset - (char*)BufferToFill) % 4096 == 0);
            
            if(VolInf->Version != NAR_FULLBACKUP_VERSION){
                printf("BackupRead : Reading %9I64u clusters from volume offset %9I64u, writing it to buffer offset of %9I64u\n", ReadSize/4096ull, FilePtrTarget/4096, (char*)CurrentBufferOffset - (char*)BufferToFill);
            }
#endif
            
            BOOL OperationResult = ReadFile(Stream->Handle, CurrentBufferOffset, ReadSize, &BytesReadAfterOperation, 0);
            Result += BytesReadAfterOperation;
            ASSERT(BytesReadAfterOperation == ReadSize);
            
            
            if (!OperationResult || BytesReadAfterOperation != ReadSize) {
                printf("STREAM ERROR: Couldnt read %lu bytes, instead read %lu, error code %i\n", ReadSize, BytesReadAfterOperation, OperationResult);
                printf("rec_index % i rec_count % i, remaining bytes %I64u, offset at disk %I64u\n", Stream->RecIndex, Stream->Records.Count, ClustersRemainingByteSize, FilePtrTarget);
                printf("Total bytes read for buffer %u\n", Result);
                
                NarDumpToFile("STREAM_OVERFLOW_ERROR_LOGS", Stream->Records.Data, Stream->Records.Count*sizeof(nar_record));
                Stream->Error = BackupStream_Errors::Error_Read;
                
                goto ERR_BAIL_OUT;
            }
            
            
        }
        else {
            printf("Couldnt set file pointer to %I64d\n", FilePtrTarget);
            Stream->Error = BackupStream_Errors::Error_SetFP;
            goto ERR_BAIL_OUT;
        }
        
        int32_t ClusterToIterate       = (int32_t)(BytesReadAfterOperation / Stream->ClusterSize);
        Stream->ClusterIndex += ClusterToIterate;
        
        if ((uint32_t)Stream->ClusterIndex > Stream->Records.Data[Stream->RecIndex].Len) {
            printf("ClusterIndex exceeded region len, that MUST NOT happen at any circumstance\n");
            Stream->Error = BackupStream_Errors::Error_SizeOvershoot;
            goto ERR_BAIL_OUT;
        }
        if ((uint32_t)Stream->ClusterIndex == Stream->Records.Data[Stream->RecIndex].Len) {
            Stream->ClusterIndex = 0;
            Stream->RecIndex++;
        }
        
        
        if (BytesReadAfterOperation % Stream->ClusterSize != 0) {
            printf("Couldnt read cluster aligned size, error, read %i bytes instead of %i\n", BytesReadAfterOperation, ReadSize);
        }
        
        ASSERT(Result % 4096 == 0);
        if(Result % 4096 != 0){
            NAR_BREAK;
        }
        
        RemainingSize      -= BytesReadAfterOperation;
        CurrentBufferOffset = (char*)BufferToFill + (Result);
        
        
        if(Stream->RegionLock){
            break;
        }
        
    }
    
    if(true == Stream->ShouldCompress
       && Result > 0){
        
        size_t RetCode = 0;
        
        RetCode = ZSTD_compress2(Stream->CCtx, CallerBuffer, CallerBufferSize, BufferToFill, Result);
        
        ASSERT(!ZSTD_isError(RetCode));
        
        if(!ZSTD_isError(RetCode)){
            nar_record CompInfo;
            CompInfo.CompressedSize   = (uint32_t)RetCode;
            CompInfo.DecompressedSize = Result;
            
            if(Stream->MaxCBI > Stream->CBII){
                Stream->CompInf[Stream->CBII++] = CompInfo;
            }
            
            ASSERT(Stream->MaxCBI > Stream->CBII);
            
            Stream->BytesProcessed = Result;
            Result = (uint32_t)RetCode;
        }
        else{
            
#if 0
            if(input.pos != input.size){
                printf("Input buffer size %u, input pos %u\n", input.size, input.pos);
                printf("output buffer size %u, output pos %u\n", output.size, output.pos);
            }
#endif
            
            if (ZSTD_isError(RetCode)) {
                printf("ZSTD Error description : %s\n", ZSTD_getErrorName(RetCode));
            }
            printf("ZSTD RetCode : %I64u\n", RetCode);
            
            Stream->Error = BackupStream_Errors::Error_Compression;
            goto ERR_BAIL_OUT;
        }
        
    }
    
    return Result;
    
    ERR_BAIL_OUT:
    
    printf("ReadStream error : %s\n", Stream->GetErrorDescription());
    
    Result = 0;
    return Result;
    
}




/*
Attachs filter
SetActive: default value is TRUE
*/
inline int32_t
AttachVolume(char Letter) {
    
    int32_t Return = FALSE;
    HRESULT Result = 0;
    
    WCHAR Temp[] = L"!:\\";
    Temp[0] = (wchar_t)Letter;
    
    Result = FilterAttach(MINISPY_NAME, Temp, 0, 0, 0);
    if (SUCCEEDED(Result) || Result == ERROR_FLT_INSTANCE_NAME_COLLISION) {
        Return = TRUE;
    }
    else {
        printf("Can't attach filter\n");
    }
    
    return Return;
}


int32_t
DetachVolume(volume_backup_inf* VolInf) {
    int32_t Return = TRUE;
    HRESULT Result = 0;
    std::wstring Temp = L"!:\\";
    Temp[0] = VolInf->Letter;
    
    Result = FilterDetach(MINISPY_NAME, Temp.c_str(), 0);
    
    if (SUCCEEDED(Result)) {
        
    }
    else {
        printf("Can't detach filter\n");
        printf("Result was -> %d\n", Result);
        Return = FALSE;
    }
    
    return Return;
}


int32_t
RemoveVolumeFromTrack(LOG_CONTEXT *C, wchar_t L) {
    int32_t Result = FALSE;
    
    int ID = GetVolumeID(C, L);
    
    if (ID >= 0) {
        
        volume_backup_inf* V = &C->Volumes.Data[ID];
        DetachVolume(V);
        V->INVALIDATEDENTRY = TRUE;
        
        V->Letter = 0;
        
        if(NarRemoveVolumeFromKernelList(L, C->Port)){
            // TODO(Batuhan): 
        }
        
        FreeDataArray(&V->Stream.Records);
        V->Stream.RecIndex = 0;
        V->Stream.ClusterIndex = 0;
        CloseHandle(V->Stream.Handle);
        
        Result = TRUE;
    }
    else {
        printf("Unable to find volume %c in track list \n", L);
        Result = FALSE;
    }
    
    
    return Result;
}



/*
 Just removes volume entry from kernel memory, does not unattaches it.
*/
inline int32_t
NarRemoveVolumeFromKernelList(wchar_t Letter, HANDLE CommPort) {
    
    if(CommPort == INVALID_HANDLE_VALUE) return FALSE;
    int32_t Result = FALSE;
    
    NAR_COMMAND Command = {};
    Command.Type = NarCommandType_DeleteVolume;
    
    if(NarGetVolumeGUIDKernelCompatible(Letter, Command.VolumeGUIDStr)){
        DWORD BR = 0;
        HRESULT hResult = FilterSendMessage(CommPort, &Command, sizeof(NAR_COMMAND), 0,0, &BR);
        if(SUCCEEDED(hResult)){
            Result = TRUE;
        }
        else{
            printf("FilterSendMessage failed, couldnt remove volume from kernel side, err %i\n", hResult);
        }
    }
    else{
        printf("Couldnt get kernel compatible guid\n");
    }
    
    return FALSE;
}


int32_t
GetVolumesOnTrack(PLOG_CONTEXT C, volume_information* Out, unsigned int BufferSize, int* OutCount) {
    
    if (!Out || !C || !C->Volumes.Data) {
        return FALSE;
    }
    
    unsigned int VolumesFound = 0;
    unsigned int MaxFit = BufferSize / (unsigned int)sizeof(*Out);
    int32_t Result = FALSE;
    
    for (unsigned int i = 0; i < C->Volumes.Count; i++) {
        
        volume_backup_inf* V = &C->Volumes.Data[i];
        if (MaxFit == VolumesFound) {
            Result = FALSE;
            break;
        }
        
        if (V->INVALIDATEDENTRY) {
            continue;
        }
        
        Out[VolumesFound].Letter    = (char)V->Letter;
        Out[VolumesFound].Bootable  = V->IsOSVolume;
        Out[VolumesFound].DiskID    = NarGetVolumeDiskID((char)V->Letter);
        Out[VolumesFound].DiskType  = (char)NarGetVolumeDiskType((char)V->Letter);
        Out[VolumesFound].TotalSize = NarGetVolumeTotalSize((char)V->Letter);
        
        VolumesFound++;
        
    }
    
    if (Result == FALSE) {
        memset(Out, 0, BufferSize); //If any error occured, clear given buffer
    }
    else {
        *OutCount = (int)VolumesFound;
    }
    
    return Result;
}

int32_t
GetVolumeID(PLOG_CONTEXT C, wchar_t Letter) {
    
    INT ID = NAR_INVALID_VOLUME_TRACK_ID;
    if (C->Volumes.Data != NULL) {
        if (C->Volumes.Count != 0) {
            for (size_t i = 0; i < C->Volumes.Count; i++) {
                // TODO(Batuhan): check invalidated entry !
                
                if (!C->Volumes.Data[i].INVALIDATEDENTRY && C->Volumes.Data[i].Letter == Letter) {
                    ID = (int)i; // safe conversion
                    break;
                }
            }
        }
        else {
            printf("Context's volume array is empty");
        }
        
    }
    else {
        printf("Context's volume array was null\n");
    }
    
    return ID;
}



/*
This operation just adds volume to list, does not starts to filter it,
until it's fullbackup is requested. After fullbackup, call AttachVolume to start filtering
*/
int32_t
AddVolumeToTrack(PLOG_CONTEXT Context, wchar_t Letter, BackupType Type) {
    int32_t ErrorOccured = TRUE;
    
    volume_backup_inf VolInf;
    int32_t FOUND = FALSE;
    
    int32_t ID = GetVolumeID(Context, Letter);
    
    if (ID == NAR_INVALID_VOLUME_TRACK_ID) {
        NAR_COMMAND Command;
        Command.Type    = NarCommandType_AddVolume;
        Command.Letter  = (char)Letter;
        
        if (NarGetVolumeGUIDKernelCompatible(Letter, Command.VolumeGUIDStr)) {
            
            DWORD BytesReturned;
            printf("Volume GUID for %c : %S\n", (char)Letter, Command.VolumeGUIDStr);
            HRESULT hResult = FilterSendMessage(Context->Port, &Command, sizeof(Command), 0, 0, &BytesReturned);
            
            if (SUCCEEDED(hResult)) {
                if (InitVolumeInf(&VolInf, Letter, Type)) {
                    ErrorOccured = FALSE;
                }
                Context->Volumes.Insert(VolInf);
                printf("Volume %c inserted to the list\n", (char)Letter);
            }
            else {
                printf("KERNEL error occured: Couldnt add volume @ kernel side, error code %i, volume %c\n", hResult, (char)Letter);
            }
            
        }
        else {
            printf("Couldnt get volume kernel compatible volume GUID\n");
        }
    }
    else {
        printf("Volume %c is already in list\n", (char)Letter);
    }
    
    return !ErrorOccured;
}


data_array<nar_record>
GetMFTandINDXLCN(char VolumeLetter, HANDLE VolumeHandle) {
    
    TIMED_BLOCK();
    
    int32_t JustExtractMFTRegions = FALSE;
    
    uint32_t MEMORY_BUFFER_SIZE = Megabyte(16);
    uint32_t ClusterExtractedBufferSize = Megabyte(4);
    uint32_t MaxOutputLen  = ClusterExtractedBufferSize/sizeof(nar_record);
    uint32_t ClusterSize   = NarGetVolumeClusterSize(VolumeLetter);
    
    nar_record* ClustersExtracted      = (nar_record*)malloc(ClusterExtractedBufferSize);
    unsigned int ClusterExtractedCount = 0;
    uint32_t MFTRegionCount            = 0;
    
    if (ClustersExtracted != 0)
        memset(ClustersExtracted, 0, ClusterExtractedBufferSize);
    
    auto AutoCompressAndResizeOutput = [&](){
        
        if(ClusterExtractedCount >= MaxOutputLen/2){
            TIMED_NAMED_BLOCK("Autocompression stuff");
            
            printf("Sort&Merge %u regions\n", ClusterExtractedCount);
            qsort(ClustersExtracted, ClusterExtractedCount, sizeof(nar_record), CompareNarRecords);
            data_array<nar_record> temp;
            temp.Data = ClustersExtracted;
            temp.Count = ClusterExtractedCount;
            MergeRegions(&temp);
                        
            temp.Data = (nar_record*)realloc(temp.Data, MaxOutputLen*2*sizeof(nar_record));
            MaxOutputLen *= 2;
            ClustersExtracted = temp.Data;
            ClusterExtractedCount = temp.Count;
        }
        
    };
    
    void* FileBuffer = malloc(MEMORY_BUFFER_SIZE);
    uint32_t FileBufferCount = MEMORY_BUFFER_SIZE / 1024LL;
    
    if(NULL == FileBuffer
       || NULL == ClustersExtracted){
        free(ClustersExtracted);
        printf("Unable to allocate memory for cluster&file buffer\n");
        ClustersExtracted = 0;
        ClusterExtractedCount = 0;
        
        goto EARLY_TERMINATION;
    }
    
    if (FileBuffer) {
        char VolumeName[64];
        snprintf(VolumeName,sizeof(VolumeName), "\\\\.\\%c:", VolumeLetter);
        
        int16_t DirectoryFlag = 0x0002;
        int32_t FileRecordSize = 1024;
        int16_t FlagOffset = 22;
        
        
        if (VolumeHandle != INVALID_HANDLE_VALUE) {
            
            DWORD BR = 0;
            
            unsigned int RecCountByCommandLine = 0;
            if(NarGetMFTRegionsFromBootSector(VolumeHandle, ClustersExtracted, &MFTRegionCount, MaxOutputLen)){
                ClusterExtractedCount += MFTRegionCount;
            }
            else{
                free(ClustersExtracted);
                ClustersExtracted = 0;
                ClusterExtractedCount = 0;
                goto EARLY_TERMINATION;
            }
            
            
            // TODO (Batuhan): remove this after testing on windows server, looks like rest of the code finds some invalid regions on volume.
            if (JustExtractMFTRegions) {
                printf("Found %i regions\n", ClusterExtractedCount);
                for(uint32_t indx = 0; indx < ClusterExtractedCount; indx++){
                    printf("0x%X\t0x%X\n", ClustersExtracted[indx].StartPos, ClustersExtracted[indx].Len);
                }
                goto EARLY_TERMINATION;
            }
            
            unsigned int MFTRegionCount = ClusterExtractedCount;
            
            {
                size_t TotalFC = 0;
                printf("Region count %u\n", MFTRegionCount);
                for(unsigned int MFTOffsetIndex = 0; MFTOffsetIndex < MFTRegionCount; MFTOffsetIndex++){
                    TotalFC += ClustersExtracted[MFTOffsetIndex].Len;
                }
                printf("Total file count is %I64u\n", TotalFC*4ull);
            }
            
            for (unsigned int MFTOffsetIndex = 0; MFTOffsetIndex < MFTRegionCount; MFTOffsetIndex++) {
                
                ULONGLONG Offset = (ULONGLONG)ClustersExtracted[MFTOffsetIndex].StartPos * (uint64_t)ClusterSize;
                int32_t FilePerCluster = ClusterSize / 1024;
                ULONGLONG FileCount = (ULONGLONG)ClustersExtracted[MFTOffsetIndex].Len * (uint64_t)FilePerCluster;
                
                // set file pointer to actual records
                if (NarSetFilePointer(VolumeHandle, Offset)) {
                    
                    size_t FileRemaining = FileCount;
                    
                    while(FileRemaining > 0) {
                        // FileBufferCount > FileCount
                        // We can read all file records for given data region
                        
                        size_t TargetFileCount = MIN(FileBufferCount, FileRemaining);
                        FileRemaining -= TargetFileCount;
                        
                        BOOL RFResult = ReadFile(VolumeHandle, FileBuffer, (DWORD)TargetFileCount * 1024ul, &BR, 0);
                        
                        if (RFResult && BR == (TargetFileCount * 1024ul)) {
#if 1                          
                            
                            int64_t start = NarGetPerfCounter();
                            
                            for (uint64_t FileRecordIndex = 0; FileRecordIndex < TargetFileCount; FileRecordIndex++) {
                                
                                TIMED_NAMED_BLOCK("File record parser");
                                
                                void* FileRecord = (BYTE*)FileBuffer + (uint64_t)FileRecordSize * (uint64_t)FileRecordIndex;
                                
                                if (*(int32_t*)FileRecord != 'ELIF') {
                                    continue;
                                }
                                
                                // lsn, lsa swap to not confuse further parsing stages.
                                ((uint8_t*)FileRecord)[510] = *(uint8_t*)NAR_OFFSET(FileRecord, 50);
                                ((uint8_t*)FileRecord)[511] = *(uint8_t*)NAR_OFFSET(FileRecord, 51);
                                
                                uint32_t FileID = 0;
                                FileID = NarGetFileID(FileRecord);
                                
                                // manually insert $BITMAP & $LogFile data regions to stream.
                                if(FileID == 0||
                                   FileID == 2||
                                   FileID == 6){
                                    void *attr = NarFindFileAttributeFromFileRecord(FileRecord, 0x80);
                                    if(attr){
                                        if(!!(*(uint8_t*)NAR_OFFSET(attr, 8))){
                                            int16_t DataRunOffset = *(int16_t*)NAR_OFFSET(attr, 32);
                                            void* DataRun = NAR_OFFSET(attr, DataRunOffset);
                                            uint32_t RegFound = 0;
                                            if(false == NarParseDataRun(DataRun, &ClustersExtracted[ClusterExtractedCount], MaxOutputLen - ClusterExtractedCount, &RegFound, false)){
                                                printf("Error occured while parsing data run of special file case\n");
                                            }
                                            ClusterExtractedCount += RegFound;
                                        }
                                        else{
                                            printf("Skipped data run parsing for file 0x%X\n", FileID);
                                        }
                                    }
                                    else{
                                        if(FileID == 2)
                                            printf("Unable to find data attribute of $LogFile\n");
                                        if(FileID == 6)
                                            printf("Unable to find data attribute of $BITMAP\n");
                                        if(FileID == 0)
                                            printf("Unable to find data attribute of $MFT\n");
                                    }
                                }
                                
                                if(FileID == 0){
                                    void* MFTBitmap = NarFindFileAttributeFromFileRecord(FileRecord, 0xB0);
                                    if(NULL != MFTBitmap){
                                        uint32_t RegFound = 0;
                                        uint8_t *DataRun = (uint8_t*)NAR_OFFSET(MFTBitmap, 64);
                                        if(false == NarParseDataRun(DataRun, &ClustersExtracted[ClusterExtractedCount], MaxOutputLen - ClusterExtractedCount, &RegFound, false)){
                                            printf("Error occured while parsing data run of special file case\n");
                                        }
                                        ClusterExtractedCount += RegFound;
                                    }
                                    else{
                                        NAR_BREAK;
                                        printf("Unable to find bitmap of mft!\n");
                                    }
                                }
                                
                                void *IndxOffset = NarFindFileAttributeFromFileRecord(FileRecord, NAR_INDEX_ALLOCATION_FLAG);
                                
                                
                                if(IndxOffset != NULL){
                                    TIMED_NAMED_BLOCK("File record parser(valid ones)");
                                    
                                    AutoCompressAndResizeOutput();
                                    
                                    uint32_t RegFound = 0;
                                    NarParseIndexAllocationAttribute(IndxOffset, &ClustersExtracted[ClusterExtractedCount], MaxOutputLen - ClusterExtractedCount, &RegFound,
                                                                     false);
                                    ClusterExtractedCount += RegFound;
                                }
                                
                                // NOTE(Batuhan): EDGE CASE: Attributes that don't fit file record are stored in other file records, and attribute list 
                                // attribute holds which attribute is in which file etc.
                                // But, even attribute list may not fit original file record at all, meaning, it's data may be non-resident
                                // Without this information, some of the files probably may not be accessible at all, or be corrupted.
                                void* ATL = NarFindFileAttributeFromFileRecord(FileRecord, NAR_ATTRIBUTE_LIST);
                                if(NULL != ATL){
                                    uint8_t ATLNonResident = *(uint8_t*)NAR_OFFSET(ATL, 8);
                                    if(ATLNonResident){
                                        TIMED_NAMED_BLOCK("Non resident attrlist!");
                                        void* DataRunStart     = NAR_OFFSET(ATL, 64);;
                                        
                                        uint32_t DataRunFound  = 0;
                                        bool ParseResult       = NarParseDataRun(DataRunStart, 
                                                                                 &ClustersExtracted[ClusterExtractedCount], 
                                                                                 MaxOutputLen - ClusterExtractedCount, 
                                                                                 &DataRunFound, 
                                                                                 false);
                                        
                                        ClusterExtractedCount += DataRunFound;
                                    }
                                }
                                
                                
                            }
                            
#endif
                            
                            double time_sec = NarTimeElapsed(start);
                            printf("Processed %8u files in %.5f sec, file per ms %.5f\n", TargetFileCount, time_sec, (double)TargetFileCount/time_sec/1000.0);
                            
                        }
                        else {
                            printf("Couldnt read file records, read %u bytes instead of %u\n", BR, TargetFileCount*1024);
                            printf("Error code is %X\n", RFResult);
                        }
                        
                        
                    }// END OF WHILE(FILE REMAINING)
                    
                }
                else {
                    printf("Couldnt set file pointer to %I64d\n", Offset);
                }
                
            }
        }
        else {
            printf("VSSVolumeHandle was invalid \n");
        }
        
    }
    else {
        printf("FATAL: Couldnt allocate memory for file buffer\n");
        // NOTE(Batuhan): cant allocate memory
    }
    
    
    EARLY_TERMINATION:
    
    free(FileBuffer);
    
    
    data_array<nar_record> Result = { 0 };
    if(ClusterExtractedCount > 0){
        ClustersExtracted = (nar_record*)realloc(ClustersExtracted, ClusterExtractedCount * sizeof(nar_record));
    }
    
    Result.Data = ClustersExtracted;
    Result.Count = ClusterExtractedCount;
    
    if(Result.Count > 0){
        printf("Will be sorting&merging %u regions\n", Result.Count); 
        qsort(Result.Data, Result.Count, sizeof(nar_record), CompareNarRecords);
        MergeRegions(&Result);
    }
    
    size_t DebugRegionCount = 0;
    ULONGLONG VolumeSize = NarGetVolumeTotalSize(VolumeLetter);
    uint32_t TruncateIndex = 0;
    for (uint32_t i = 0; i < Result.Count; i++) {
        if ((ULONGLONG)Result.Data[i].StartPos * (ULONGLONG)ClusterSize + (ULONGLONG)Result.Data[i].Len * ClusterSize > VolumeSize) {
            TruncateIndex = i;
            printf("MFT PARSER : truncation index found %u, out of %u\n", i, Result.Count);
            break;
        }
        else{
            // printf("MFT PARSER region : %9u, length %9u\n", Result.Data[i].StartPos, Result.Data[i].Len);
        }
        DebugRegionCount += (size_t)Result.Data[i].Len;
    }
    
    
    if (TruncateIndex > 0) {
        printf("INDEX_ALLOCATION blocks exceeds volume size, truncating from index %i\n", TruncateIndex);
        Result.Data = (nar_record*)realloc(Result.Data, TruncateIndex * sizeof(nar_record));
        Result.Count = TruncateIndex;
    }
    else{
        
    }
    
    printf("Number of regions found %I64u, mb : %I64u\n", DebugRegionCount, DebugRegionCount*4096ull/(1024ull*1024ull));
    
    return Result;
}




VSS_ID
GetShadowPath(std::wstring Drive, CComPtr<IVssBackupComponents>& ptr, wchar_t* OutShadowPath, size_t MaxOutCh) {
    
    int32_t Error = TRUE;
    VSS_ID sid = { 0 };
    HRESULT res;
    
    res = CreateVssBackupComponents(&ptr);
    
    if(nullptr == ptr){
        printf("createbackup components returned %X\n", res);
        return sid;
        // early termination
    }
    
    if (S_OK == ptr->InitializeForBackup()) {
        if (S_OK == ptr->SetContext(VSS_CTX_BACKUP)) {
            if(S_OK == ptr->SetBackupState(false, false, VSS_BACKUP_TYPE::VSS_BT_COPY, false)){
                CComPtr<IVssAsync> async3;
                if(S_OK == ptr->GatherWriterMetadata(&async3)){
                    async3->Wait();
                    if (S_OK == ptr->StartSnapshotSet(&sid)) {
                        if (S_OK == ptr->AddToSnapshotSet((LPWSTR)Drive.c_str(), GUID_NULL, &sid)) {
                            CComPtr<IVssAsync> Async;
                            if (S_OK == ptr->PrepareForBackup(&Async)) {
                                Async->Wait();
                                CComPtr<IVssAsync> Async2;
                                if (S_OK == ptr->DoSnapshotSet(&Async2)) {
                                    Async2->Wait();
                                    Error = FALSE;
                                }
                                else{
                                    printf("DoSnapshotSet failed\n");
                                }
                            }
                            else{
                                printf("PrepareForBackup failed\n");
                            }
                        }
                        else{
                            printf("AddToSnapshotSet failed\n");
                        }
                    }
                    else{
                        printf("StartSnapshotSet failed\n");
                    }
                }
                else{
                    printf("GatherWriterMetadata failed\n");
                }
            }
            else{
                printf("SetBackupState failed\n");
            }
        }
        else{
            printf("SetContext failed\n");
        }
    }
    else{
        printf("InitializeForBackup failed\n");
    }
    
    if (!Error) {
        VSS_SNAPSHOT_PROP SnapshotProp;
        ptr->GetSnapshotProperties(sid, &SnapshotProp);
        
        size_t l = wcslen(SnapshotProp.m_pwszSnapshotDeviceObject);
        if(MaxOutCh > l){
            wcscpy(OutShadowPath, SnapshotProp.m_pwszSnapshotDeviceObject);
        }
        else{
            printf("Shadow path can't fit given string buffer\n");
        }
    }
    
    
    return sid;
}



inline int32_t
InitVolumeInf(volume_backup_inf* VolInf, wchar_t Letter, BackupType Type) {
    
    if(VolInf == NULL) return FALSE;
    *VolInf = {0};
    
    int32_t Result = TRUE;
    
    VolInf->Letter = Letter;
    VolInf->INVALIDATEDENTRY = FALSE;
    VolInf->BT = Type;
    
    VolInf->DiffLogMark.BackupStartOffset = 0;
    
    VolInf->Version = NAR_FULLBACKUP_VERSION;
    VolInf->ClusterSize = 0;
    
    
    VolInf->Stream.Records = { 0,0 };
    VolInf->Stream.Handle = 0;
    VolInf->Stream.RecIndex = 0;
    VolInf->Stream.ClusterIndex = 0;
    
    DWORD BytesPerSector = 0;
    DWORD SectorsPerCluster = 0;
    DWORD ClusterCount = 0;
    
    wchar_t DN[] = L"C:";
    DN[0] = Letter;
    
    VolInf->BackupID = NarGenerateBackupID((char)Letter);
    
    wchar_t V[] = L"!:\\";
    V[0] = Letter;
    if (GetDiskFreeSpaceW(V, &SectorsPerCluster, &BytesPerSector, 0, &ClusterCount)) {
        VolInf->ClusterSize = SectorsPerCluster * BytesPerSector;
        VolInf->VolumeTotalClusterCount = ClusterCount;
        WCHAR windir[MAX_PATH];
        GetWindowsDirectoryW(windir, MAX_PATH);
        if (windir[0] == Letter) {
            //Selected volume contains windows
            VolInf->IsOSVolume = TRUE;
        }
        
        printf("Initialized volume inf %c, cluster size %i, volume cluster count %i\n", VolInf->Letter, VolInf->ClusterSize, VolInf->VolumeTotalClusterCount);
        
    }
    else {
        printf("Cant get disk information from WINAPI\n");
    }
    // VolInf->IsOSVolume = FALSE;
    
    
    return Result;
}




int32_t
CopyData(HANDLE S, HANDLE D, ULONGLONG Len) {
    int32_t Return = TRUE;
    
    uint32_t BufSize = 8*1024*1024; //8 MB
    ULONGLONG TotalCopied = 0;
    
    void* Buffer = malloc(BufSize);
    
    if (Buffer != NULL) {
        ULONGLONG BytesRemaining = Len;
        DWORD BytesOperated = 0;
        if (BytesRemaining > BufSize) {
            
            while (BytesRemaining > BufSize) {
                if (ReadFile(S, Buffer, BufSize, &BytesOperated, 0)) {
                    BOOL bResult = WriteFile(D, Buffer, BufSize, &BytesOperated, 0);
                    if (!bResult || BytesOperated != BufSize) {
                        printf("COPY_DATA: Writefile failed with code %X\n", bResult);
                        printf("Tried to write -> %I64d, Bytes written -> %d\n", Len, BytesOperated);
                        Return = FALSE;
                        break;
                    }
                    else {
                        TotalCopied += BufSize;
                    }
                    
                    BytesRemaining -= BufSize;
                }
                else {
                    Return = FALSE;
                    //if readfile failed
                    break;
                }
                
            }
        }
        
        if (BytesRemaining > 0) {
            if (ReadFile(S, Buffer, (DWORD)BytesRemaining, &BytesOperated, 0) && BytesOperated == BytesRemaining) {
                
                if (!WriteFile(D, Buffer, (DWORD)BytesRemaining, &BytesOperated, 0) || BytesOperated != BytesRemaining) {
                    printf("COPY_DATA: Error occured while copying end of buffer\n");
                    printf("Bytes written -> %lu , instead of %I64u \n", BytesOperated, BytesRemaining);
                    Return = FALSE;
                }
                else {
                    TotalCopied += BytesRemaining;
                }
                
            }
            
        }
        
        
    }//If Buffer != NULL
    else {
        printf("Can't allocate memory for buffer\n");
        Return = FALSE;
    }
    
    free(Buffer);
    if (Return == FALSE) {
        printf("COPYFILE error detected, copied %I64d bytes instead of %I64d\n", TotalCopied, Len);
    }
    return Return;
}



/*
Version: indicates full backup if <0
ClusterSize: default 4096
BackupRegions: Must have, this data determines how i must map binary data to the volume at restore operation

// NOTE(Batuhan): file that contains this struct contains:
-- RegionMetadata:
-- MFTMetadata: (optional)
-- MFT: (optional)
-- Recovery: (optional)
*/
bool
SaveMetadata(char Letter, int Version, int ClusterSize, BackupType BT,
             data_array<nar_record> BackupRegions, nar_backup_id ID, bool IsCompressed, int32_t CompressionType, nar_record *CompInfo, size_t CompInfoCount, char *OutputFileName) {
    
    // @TODO : convert letter to uppercase
    int32_t Result = FALSE;

    Package_Creator package;
    init_package_creator(&package);
    
    backup_information binf;

    // save backup information
    {
        memset(&binf, 0, sizeof(binf));

        binf.Version = Version;
        binf.BT = BT;

        binf.SizeOfBinaryData = 0;        
        for (size_t i = 0; i < BackupRegions.Count; i++) {
            binf.SizeOfBinaryData      += (uint64_t)BackupRegions.Data[i].Len; 
            binf.VersionMaxWriteOffset  = (uint64_t)BackupRegions.Data[i].StartPos + (uint64_t)BackupRegions.Data[i].Len;
        }

        binf.SizeOfBinaryData      *= (uint64_t)ClusterSize;
        binf.VersionMaxWriteOffset *= (uint64_t)ClusterSize;
        binf.ClusterSize            = ClusterSize;
        binf.Letter                 = Letter;
        binf.DiskType               = NarGetVolumeDiskType(binf.Letter);
        binf.IsOSVolume             = NarIsOSVolume(binf.Letter);
        binf.OriginalSizeOfVolume   = NarGetVolumeTotalSize(binf.Letter);
        
        binf.IsBinaryDataCompressed = IsCompressed;
        binf.CompressionType        = CompressionType;
        binf.MetadataTimeStamp      = get_local_date();        

        /*
              layout of disks (fundamental level, gpt might contain extra data partitions)
              for extra information visit:
              MBR https://docs.microsoft.com/en-us/windows-hardware/manufacture/desktop/configure-biosmbr-based-hard-drive-partitions  
              GPT https://docs.microsoft.com/en-us/windows-hardware/manufacture/desktop/configure-uefigpt-based-hard-drive-partitions
              
                GPT
              System
              Msr
              windows
              recovery
              
              
              MBR
              system
              windows
              recovery
              
        */
        if(binf.IsOSVolume) {    
            GUID GUIDMSRPartition = { 0 }; // microsoft reserved partition guid
            GUID GUIDSystemPartition = { 0 }; // efi-system partition guid
            GUID GUIDRecoveryPartition = { 0 }; // recovery partition guid
            
            StrToGUID("{e3c9e316-0b5c-4db8-817d-f92df00215ae}", &GUIDMSRPartition);
            StrToGUID("{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}", &GUIDSystemPartition);
            StrToGUID("{de94bba4-06d1-4d40-a16a-bfd50179d6ac}", &GUIDRecoveryPartition);
            
            
            int DiskID = NarGetVolumeDiskID(Letter);
            char DiskPath[128];
            snprintf(DiskPath, sizeof(DiskPath), "\\\\?\\PhysicalDrive%i", DiskID);
            HANDLE DiskHandle = CreateFileA(DiskPath, GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
            if(DiskHandle != INVALID_HANDLE_VALUE){
                
                DWORD DLSize = 1024*2;
                DRIVE_LAYOUT_INFORMATION_EX *DL = (DRIVE_LAYOUT_INFORMATION_EX*)malloc(DLSize);
                DWORD Hell = 0;
                if (DeviceIoControl(DiskHandle, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DL, DLSize, &Hell, 0)) {
                    
                    if (DL->PartitionStyle == PARTITION_STYLE_GPT) {
                        
                        for (unsigned int PartitionIndex = 0; PartitionIndex < DL->PartitionCount; PartitionIndex++) {
                            
                            PARTITION_INFORMATION_EX* PI = &DL->PartitionEntry[PartitionIndex];
                            // NOTE(Batuhan): Finding recovery partition via GUID
                            if (IsEqualGUID(PI->Gpt.PartitionType, GUIDRecoveryPartition)) {
                                binf.RecoveryPartitionSize = PI->PartitionLength.QuadPart;
                            }
                            if(IsEqualGUID(PI->Gpt.PartitionType, GUIDSystemPartition)){
                                binf.EFIPartitionSize = PI->PartitionLength.QuadPart;
                            }
                            
                        }
                        
                    }
                    else if(DL->PartitionStyle == PARTITION_STYLE_MBR){
                        
                        for (unsigned int PartitionIndex = 0; PartitionIndex < DL->PartitionCount; ++PartitionIndex){
                            
                            PARTITION_INFORMATION_EX* PI = &DL->PartitionEntry[PartitionIndex];
                            // System partition
                            if (PI->Mbr.BootIndicator == TRUE) {
                                binf.SystemPartitionSize = PI->PartitionLength.QuadPart;
                                break;
                            }
                            
                        }
                        
                    }
                    
                }
                else {
                    printf("DeviceIoControl with argument IOCTL_DISK_GET_DRIVE_LAYOUT_EX failed for drive %i, cant append recovery file to backup metadata\n", DiskID, Letter);
                }
                
                free(DL);
                CloseHandle(DiskHandle);

            }
            else{
                printf("Couldn't open disk %i as file, for volume %c's metadata @ version %i\n", DiskID, Letter, Version);
            }

        }

        package.add_entry("backup_information", &binf, sizeof(binf), PACKAGE_ENTRY_NO_TAG);
    }






    // add product name
    char ProductName[512];
    {
        memset(ProductName, 0, sizeof(ProductName));
        NarGetProductName(ProductName);
        package.add_entry("product_name", ProductName, strlen(ProductName) + 1, PACKAGE_ENTRY_NO_TAG);  
    }


    // add computer name
    char ComputerName[100];
    {
        DWORD ComputerNameSize = sizeof(ComputerName);
        GetComputerNameA(ComputerName, &ComputerNameSize);
        package.add_entry("computer_name", ComputerName, strlen(ComputerName) + 1, PACKAGE_ENTRY_NO_TAG); 
    }


    // add backup regions.
    {
        package.add_entry("regions", BackupRegions.Data, BackupRegions.Count * sizeof(BackupRegions.Data[0]), PACKAGE_ENTRY_NO_TAG);
    }   

    

    // NOTE(Batuhan): Recovery partition's data is stored on metadata only if it's both full and OS volume backup    
    if (IsCompressed && CompInfo && CompInfoCount) {
        package.add_entry("compression_information", CompInfo, CompInfoCount * sizeof(CompInfo[0]), PACKAGE_ENTRY_NO_TAG);
    }


    // reserve empty task name and description for business stuff, after backup they will fill this field
    char temp[512];
    {
        memset(temp, 0, sizeof(temp));
        package.add_entry("task_name"       , temp, sizeof(temp), PACKAGE_ENTRY_NO_TAG); 
        package.add_entry("task_description", temp, sizeof(temp), PACKAGE_ENTRY_NO_TAG); 
    }


    // @Incomplete : 
    // @TODO : We don't need to store actual mft(we can't, it's too big). There will be another piece of code
    // to handle adding indexed fs information to package.    
    // @TODO : indexed file system.
    /* 

    */



    // write actual package!    
    {   
        if (!package.build_package_to_file(OutputFileName)) {
            printf("Unable to save metadata as package. build_package_to_file failed. Output file name was (%s), volume %c, version %d", OutputFileName, Letter, Version);
            free_package_creator(&package);
            return false;
        }
    }

    free_package_creator(&package);

    return true;
}






inline void
NarGetProductName(char* OutName) {
    
    HKEY Key;
    if (RegOpenKeyA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", &Key) == ERROR_SUCCESS) {
        DWORD H = 0;
        if(RegGetValueA(Key, 0, "ProductName", RRF_RT_ANY, 0, OutName, &H) != ERROR_SUCCESS){
            printf("Error occured while fetching product name %d\n", GetLastError());
        }
        //RegCloseKey(Key);
    }
    
}



inline nar_backup_id
NarGenerateBackupID(char Letter){
    
    nar_backup_id Result = {0};
    
    SYSTEMTIME T;
    GetLocalTime(&T);
    Result.Year   = (unsigned short)T.wYear;
    Result.Month  = (char)T.wMonth;
    Result.Day    = (char)T.wDay;
    Result.Hour   = (char)T.wHour;
    Result.Min    = (char)T.wMinute;
    Result.Letter = (unsigned char)Letter;
    
    return Result;
}



/*
Its not best way to initialize a struct
*/
LOG_CONTEXT*
NarLoadBootState() {
    
    printf("Entered NarLoadBootState\n");
    LOG_CONTEXT* Result = 0 ;
    Result = new LOG_CONTEXT;
    memset(Result, 0, sizeof(LOG_CONTEXT));
    
    bool FoundAny = false;
    
    Result->Port = INVALID_HANDLE_VALUE;
    Result->Volumes = { 0,0 };
    
    char FileNameBuffer[64];
    if (GetWindowsDirectoryA(FileNameBuffer, 64)) {
        
        strcat(FileNameBuffer, "\\");
        strcat(FileNameBuffer, NAR_BOOTFILE_NAME);
        printf("Boot file full path %s\n", FileNameBuffer);
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
                            
                            int32_t bResult = InitVolumeInf(&VolInf, (char)BootTrackData[i].Letter, (BackupType)BootTrackData[i].BackupType);
                            
                            if (!!bResult) {
                                
                                VolInf.BackupID = {0};
                                VolInf.BackupID = BootTrackData[i].BackupID;
                                
                                VolInf.Version  = BootTrackData[i].Version;
                                FoundAny = true;
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
    
    if (false == FoundAny) {
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

int32_t
NarSaveBootState(LOG_CONTEXT* CTX) {
    
    if (CTX == NULL || CTX->Volumes.Data == NULL){
        printf("Either CTX or its volume data was NULL, terminating NarSaveBootState\n");
        return FALSE;
    }
    
    int32_t Result = TRUE;
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
                    Pack.BackupID         = CTX->Volumes.Data[i].BackupID;
                    
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



int32_t
NarEditTaskNameAndDescription(const wchar_t* FileName, const wchar_t* TaskName, const wchar_t* TaskDescription) {
    ASSERT(false);
    return 0;
}


BOOLEAN
NarSetFilePointer(HANDLE File, uint64_t V) {
    LARGE_INTEGER MoveTo = { 0 };
    MoveTo.QuadPart = V;
    LARGE_INTEGER NewFilePointer = { 0 };
    SetFilePointerEx(File, MoveTo, &NewFilePointer, FILE_BEGIN);
    return MoveTo.QuadPart == NewFilePointer.QuadPart;
}


bool
NarParseIndexAllocationAttribute(void *IndexAttribute, nar_record *OutRegions, uint32_t MaxRegionLen, uint32_t *OutRegionsFound, bool BitmapCompatibleInsert){
    
    if(IndexAttribute == NULL 
       || OutRegions == NULL 
       || OutRegionsFound == NULL) 
        return FALSE;
    
    TIMED_NAMED_BLOCK("Index singular");
    
    int32_t DataRunsOffset = *(int32_t*)NAR_OFFSET(IndexAttribute, 32);
    void* D = NAR_OFFSET(IndexAttribute, DataRunsOffset);
    
    return NarParseDataRun(D, OutRegions, MaxRegionLen, OutRegionsFound, BitmapCompatibleInsert);
}


/*
BitmapCompatibleInsert = inserts cluster one by one, so caller can easily zero-out unused ones
*/
bool
NarParseDataRun(void* DatarunStart, nar_record *OutRegions, uint32_t MaxRegionLen, uint32_t *OutRegionsFound, bool BitmapCompatibleInsert){
    
    // So it looks like dataruns doesnt actually tells you LCN, to save up space, they kinda use smt like 
    // winapi's deviceiocontrol routine, maybe the reason for fetching VCN-LCN maps from winapi is weird because 
    // thats how its implemented at ntfs at first place. who knows
    // so thats how it looks
    /*
    first one is always absolute LCN in the volume. rest of it is addition to previous one. if file is fragmanted, value might be
    negative. so we dont have to check some edge cases here.
    second data run will be lets say 0x11 04 43
    and first one                    0x11 10 10
    starting lcn is 0x10, but second data run does not start from 0x43, it starts from 0x10 + 0x43
  
    LCN[n] = LCN[n-1] + datarun cluster
    */
    bool Result = true;
    uint32_t InternalRegionsFound = 0;
    void* D = DatarunStart;
    int64_t OldClusterStart = 0;
    
    while (*(BYTE*)D) {
        
        BYTE Size = *(BYTE*)D;
        int32_t ClusterCountSize = 0;
        int32_t FirstClusterSize = 0;
        int32_t ClusterCount     = 0;
        int32_t FirstCluster     = 0;
        if (Size == 0) break;
        
        // extract 4bit nibbles from size
        ClusterCountSize = (Size & 0x0F);
        FirstClusterSize = (Size >> 4);
        
        
        ClusterCount = *(uint32_t*)((BYTE*)D + 1);
        ClusterCount = ClusterCount & ~(0xffffffffu << (ClusterCountSize * 8));
        
        FirstCluster = 0;
        if(((char*)D + 1 + ClusterCountSize)[FirstClusterSize - 1] & 0x80){
            FirstCluster = -1;
        }
        memcpy(&FirstCluster, (char*)D + 1 + ClusterCountSize, FirstClusterSize);
        
        
        if (ClusterCountSize == 0 || FirstClusterSize == 0){
            printf("ERROR case : case zero len\n");
            break;
        }
        if (ClusterCountSize > 4  || FirstClusterSize > 4){
            printf("ERROR case : 1704  ccs 0x%X fcs 0x%X\n", ClusterCountSize, FirstClusterSize);
            break;
        }
        
        
        if(BitmapCompatibleInsert){
            if((InternalRegionsFound + ClusterCount) < MaxRegionLen){
                int64_t plcholder = (int64_t)FirstCluster + OldClusterStart;
                for(size_t i =0; i<(size_t)ClusterCount; i++){
                    // safe conversion
                    OutRegions[InternalRegionsFound].StartPos = (uint32_t)(plcholder + i);
                    OutRegions[InternalRegionsFound].Len      = 1;
                    InternalRegionsFound++;
                }
            }
            else{
                printf("parser not enough memory %d\n", __LINE__);
                goto NOT_ENOUGH_MEMORY;
            }
        }
        else{
            OutRegions[InternalRegionsFound].StartPos = (uint32_t)(OldClusterStart + (int64_t)FirstCluster);
            OutRegions[InternalRegionsFound].Len = ClusterCount;
            InternalRegionsFound++;
        }
        
        
        if(InternalRegionsFound > MaxRegionLen){
            printf("attribute parser not enough memory[Line : %u]\n", __LINE__);
            goto NOT_ENOUGH_MEMORY;
        }
        
        OldClusterStart = OldClusterStart + (int64_t)FirstCluster;
        D = (BYTE*)D + (FirstClusterSize + ClusterCountSize + 1);
    }
    
    *OutRegionsFound = InternalRegionsFound;
    return Result;
    
    NOT_ENOUGH_MEMORY:;
    
    Result = FALSE;
    printf("No more memory left to insert index_allocation records to output array\n");
    *OutRegionsFound = 0;
    return Result;
    
}



uint32_t
NarGetFileID(void* FileRecord){
    uint32_t Result = *(uint32_t *)NAR_OFFSET(FileRecord, 44);
    return Result;
}


inline int32_t
NarGetVolumeClusterSize(char Letter) {
    char V[] = "!:\\";
    V[0] = Letter;
    
    int32_t Result = 0;
    DWORD SectorsPerCluster = 0;
    DWORD BytesPerSector = 0;
    
    if (GetDiskFreeSpaceA(V, &SectorsPerCluster, &BytesPerSector, 0, 0)){
        Result = SectorsPerCluster * BytesPerSector;
    }
    else{
        printf("Couldnt get disk free space for volume %c\n", Letter);
    }
    
    return Result;
}



/*
CAUTION: This function does NOT lookup attributes from ATTRIBUTE LIST, so if attribute is not resident in original file entry, function wont return it

// NOTE(Batuhan): Function early terminates in attribute iteration loop if it finds attribute with higher ID than given AttributeID parameter

For given MFT FileEntry, returns address AttributeID in given FileRecord. Caller can use return value to directly access given AttributeID 
Function returns NULL if attribute is not present 
*/
inline void*
NarFindFileAttributeFromFileRecord(void *FileRecord, int32_t AttributeID){
    
    if(NULL == FileRecord) return 0;
    
    TIMED_BLOCK();
    
    int16_t FirstAttributeOffset = (*(int16_t*)((BYTE*)FileRecord + 20));
    void* FileAttribute = (char*)FileRecord + FirstAttributeOffset;
    
    int32_t RemainingLen = *(int32_t*)((BYTE*)FileRecord + 24); // Real size of the file record
    RemainingLen      -= (FirstAttributeOffset + 8); //8 byte for end of record mark, remaining len includes it too.
    
    while(RemainingLen > 0){
        
        if(*(int32_t*)FileAttribute == AttributeID){
            return FileAttribute;
        }
        
        // it's guarenteed that attributes are sorted by id's in ascending order
        if(*(int32_t*)FileAttribute > AttributeID){
            break;
        }
        
        //Just substract attribute size from remaininglen to determine if we should keep iterating
        int32_t AttrSize = (*(unsigned short*)((BYTE*)FileAttribute + 4));
        RemainingLen -= AttrSize;
        FileAttribute = (BYTE*)FileAttribute + AttrSize;
        if (AttrSize == 0) break;
        
    }
    
    return NULL;
}


bool
NarGetMFTRegionsFromBootSector(HANDLE Volume, 
                               nar_record* Out, 
                               uint32_t* OutLen, 
                               uint32_t Capacity){
    
    BOOLEAN Result = false;
    char bf[4096];
    
    if(NarSetFilePointer(Volume, 0)){
        DWORD br = 0;
        static_assert(sizeof(bf) == 4096);
        ReadFile(Volume, bf, 4096, &br, 0);
        if(br == sizeof(bf)){
            
            uint32_t MftClusterOffset = *(uint32_t*)NAR_OFFSET(bf, 48);
            uint64_t MFTVolOffset = MftClusterOffset*4096ull;
            if(NarSetFilePointer(Volume, MFTVolOffset)){
                static_assert(sizeof(bf) >= 1024);
                memset(bf, 0, 1024);
                ReadFile(Volume, bf, 1024, &br, 0);
                if(br == 1024){
                    
                    uint8_t *FileRecord = (uint8_t*)&bf[0];
                    // lsn, lsa swap to not confuse further parsing stages.
                    FileRecord[510] = *(uint8_t*)NAR_OFFSET(FileRecord, 50);
                    FileRecord[511] = *(uint8_t*)NAR_OFFSET(FileRecord, 51);
                    void* Indx = NarFindFileAttributeFromFileRecord(FileRecord, NAR_DATA_FLAG);
                    if(Indx != NULL){
                        NarParseIndexAllocationAttribute(Indx, Out, Capacity, OutLen, false);
                        Result = true;
                    }
                    
                }
                
            }
            
            
        }
    }
    
    return Result;
}
