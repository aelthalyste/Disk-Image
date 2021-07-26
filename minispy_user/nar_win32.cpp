#include "nar_win32.h"

inline BOOLEAN
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



inline BOOLEAN
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



inline void
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



inline BOOLEAN
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

inline BOOLEAN
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


inline BOOLEAN
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


inline unsigned char
NarGetVolumeDiskID(char Letter) {
    
    wchar_t VolPath[512];
    wchar_t Vol[] = L"!:\\";
    Vol[0] = Letter;
    
    unsigned char Result = (unsigned char)NAR_INVALID_DISK_ID;
    DWORD BS = 1024 * 2; //1 KB
    DWORD T = 0;
    
    VOLUME_DISK_EXTENTS* Ext = (VOLUME_DISK_EXTENTS*)VirtualAlloc(0, BS, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    
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
        DisplayError(GetLastError());
    }
    
    VirtualFree(Ext, 0, MEM_RELEASE);
    
    return Result;
}



/*
Returns:
'R'aw
'M'BR
'G'PT
*/
inline int
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
                    DisplayError(GetLastError());
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
inline BOOLEAN
NarIsVolumeAvailable(char Letter){
    DWORD Drives = GetLogicalDrives();
    return !(Drives & (1 << (Letter - 'A')));
}



/*
    Returns first available volume letter that is not used by system
*/
inline char
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
                continue;
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


/*
Unlike generatemetadata, binary functions, this one generates absolute path of the log file. Which is 
under windows folder
C:\Windows\Log....
*/
inline std::wstring
GenerateLogFilePath(char Letter) {
    wchar_t NameTemp[]= L"NAR_LOG_FILE__.nlfx";
    NameTemp[13] = (wchar_t)Letter;
    
    std::wstring Result = L"C:\\Windows\\";
    Result += std::wstring(NameTemp);
    
    return Result;
}


#pragma warning(push)
#pragma warning(disable:4477)
inline void
StrToGUID(const char* guid, GUID* G) {
    if (!G) return;
    sscanf(guid, "{%8X-%4hX-%4hX-%2hX%2hX-%2hX%2hX%2hX%2hX%2hX%2hX}", &G->Data1, &G->Data2, &G->Data3, &G->Data4[0], &G->Data4[1], &G->Data4[2], &G->Data4[3], &G->Data4[4], &G->Data4[5], &G->Data4[6], &G->Data4[7]);
}
#pragma warning(pop)



ULONGLONG
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



inline wchar_t*
NarUTF8ToWCHAR(NarUTF8 s, nar_arena *Arena){
    wchar_t *Result = 0;
    int ChNeeded = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s.Str, s.Len, 0, 0);
    
    ASSERT(ChNeeded != 0);
    
    uint32_t BytesNeeded = (ChNeeded + 1)*2; // +1 for null termination
    ASSERT(BytesNeeded);
    
    Result = (wchar_t*)ArenaAllocate(Arena, BytesNeeded*2);
    ASSERT(Result);
    
    memset(Result, 0, BytesNeeded);
    int WResult = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s.Str, s.Len, Result, ChNeeded);
    ASSERT(WResult == ChNeeded);
    
    return Result;
}

inline NarUTF8
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
