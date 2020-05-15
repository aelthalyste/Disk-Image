#include <Windows.h>
#include <stdio.h>

void
CreateMBRPartition(int DiskID, int size) {
    char C[2096];
    
    sprintf(C, ""
            "select disk %i\n"
            "clean\n"
            "create partition primary size = 100\n"
            "format quick fs = ntfs label \"System\" \n"
            "assign letter = \"S\" \n"
            "active\n"
            "create partition primary\n"
            "shrink minimum = %i\n"
            "format quick fs = ntfs label =\"Windows\" \n"
            "assign letter = \"W\"\n"
            "create partition primary\n"
            "format quick fs = ntfs label  \"Recovery\" \n"
            "assign letter \"R\" \n"
            "set id=27\n"
            "exit\n"
            , DiskID, size);
    
    char T[] = "DiskPart /s TempFile";
    HANDLE File = CreateFileW(L"TempFile", GENERIC_WRITE, 0, 0, CREATE_NEW, 0, 0);
    
    if (File != INVALID_HANDLE_VALUE) {
        DWORD BT = 0;
        DWORD Size = strlen(C);
        if (WriteFile(File, C, Size, &BT, 0) && BT == Size) {
            CloseHandle(File);
            system(T);
        }
        else {
            printf("Cant write to file\n");
        }
    }
    else {
        printf("Cant open file\n");
    }
    
    return;
    /*
        rem == CreatePartitions-BIOS.txt ==
        rem == These commands are used with DiskPart to
        rem    create three partitions
        rem    for a BIOS/MBR-based computer.
        rem    Adjust the partition sizes to fill the drive
        rem    as necessary. ==
        select disk 0
        clean
        rem == 1. System partition ======================
        create partition primary size=100
        format quick fs=ntfs label="System"
        assign letter="S"
        active
        rem == 2. Windows partition =====================
        rem ==    a. Create the Windows partition =======
        create partition primary
        rem ==    b. Create space for the recovery tools
        rem       ** Update this size to match the size of
        rem          the recovery tools (winre.wim)
        rem          plus some free space.
        shrink minimum=650
        rem ==    c. Prepare the Windows partition ======
        format quick fs=ntfs label="Windows"
        assign letter="W"
        rem == 3. Recovery tools partition ==============
        create partition primary
        format quick fs=ntfs label="Recovery"
        assign letter="R"
        set id=27
        list volume
        exit
        */
}


void
CreateGPTPartition(int DiskID, int size) {
    
    char C[2096];
    memset(C, 0, 2096);
    
    sprintf(C, ""
            "select disk %i\n"
            "clean\n"
            "convert gpt\n"
            "create partition efi size = 100\n"
            "format quick fs=fat32 label = \"System\" \n"
            "assign letter = \"S\" \n"
            "create partition msr size = 16\n"
            "create partition primary\n"
            "shrink minimum= %i\n"
            "assign letter = \"W\"\n"
            "create partition primary\n"
            "format quick fs = ntfs label =\"Recovery tools\"\n"
            "assign letter \"R\"\n"
            "set id = \"\de94bba4-06d1-4d40-a16a-bfd50179d6ac\"\n"
            "gpt attributes=0x8000000000000001\n"
            "exit\n", DiskID, size);
    
    char T[] = "DiskPart /s \\TempFile";
    HANDLE File = CreateFileW(L"TempFile", GENERIC_WRITE, 0, 0, CREATE_NEW, 0, 0);
    
    if (File != INVALID_HANDLE_VALUE) {
        DWORD BT = 0;
        DWORD Size = strlen(C);
        if (WriteFile(File, C, Size, &BT, 0) && BT == Size) {
            CloseHandle(File);
            system(T);
        }
        else {
            printf("Cant write to file\n");
        }
    }
    else {
        printf("Cant open file\n");
    }
    
    return;
    
    /*
                rem == CreatePartitions-UEFI.txt ==
                    rem == These commands are used with DiskPart to
                    rem    create four partitions
                    rem    for a UEFI/GPT-based PC.
                    rem    Adjust the partition sizes to fill the drive
                    rem    as necessary. ==
                    select disk 0
                    clean
                    convert gpt
                    rem == 1. System partition =========================
                    create partition efi size=100
                    rem    ** NOTE: For Advanced Format 4Kn drives,
                rem               change this value to size = 260 **
                    format quick fs=fat32 label="System"
                    assign letter="S"
                    rem == 2. Microsoft Reserved (MSR) partition =======
                    create partition msr size=16
                    rem == 3. Windows partition ========================
                    rem ==    a. Create the Windows partition ==========
                    create partition primary
                    rem ==    b. Create space for the recovery tools ===
                    rem       ** Update this size to match the size of
                    rem          the recovery tools (winre.wim)
                    rem          plus some free space.
                    shrink minimum=650
                    rem ==    c. Prepare the Windows partition =========
                    format quick fs=ntfs label="Windows"
                    assign letter="W"
                    rem === 4. Recovery tools partition ================
                    create partition primary
                    format quick fs=ntfs label="Recovery tools"
                    assign letter="R"
                    set id="de94bba4-06d1-4d40-a16a-bfd50179d6ac"
                    gpt attributes=0x8000000000000001
                    list volume
                    exit
                    */
}

#include <Windows.h>
#include <rpcdcep.h>
#include <rpcdce.h>

void
StrToGUID(const char* guid, GUID* G) {
    if (!G) return;
    sscanf(guid, "{%8X-%4hX-%4hX-%2hX%2hX-%2hX%2hX%2hX%2hX%2hX%2hX}", &G->Data1, &G->Data2, &G->Data3, &G->Data4[0], &G->Data4[1], &G->Data4[2], &G->Data4[3], &G->Data4[4], &G->Data4[5], &G->Data4[6], &G->Data4[7]);
}

struct disk_information {
    ULONGLONG SizeGB; //In GB!
    ULONGLONG Unallocated; // IN GB!
    char Type[4]; // string, RAW,GPT,MBR, one byte for NULL termination
    int ID;
};


/*
Volume ###  Ltr  Label        Fs     Type        Size     Status     Info
  ----------  ---  -----------  -----  ----------  -------  ---------  --------
  Volume 0     N                NTFS   Simple      5000 MB  Healthy
  Volume 1     D   HDD          NTFS   Simple       926 GB  Healthy    Pagefile
  Volume 2     C                NTFS   Partition    230 GB  Healthy    Boot
  Volume 3                      FAT32  Partition    100 MB  Healthy    System
*/
struct volume_information {
    ULONGLONG SizeMB; //in MB! 
    BOOLEAN Bootable; // Healthy && NTFS && !Boot
    char Letter;
    char FileSystem[6]; // FAT32, NTFS, FAT, 1 byte for NULL termination
};

// Up to 2GB
struct file_read {
    void* Data;
    int Len;
};

inline BOOLEAN
IsNumeric(char val) {
  return val >= '0' && val <= '9';
}

file_read
NarReadFile(const char* FileName);

BOOLEAN
NarDumpToFile(const char* FileName, void* Data, int Size);

BOOLEAN
SetVolumeSize(char Letter, int TargetSizeMB);

BOOLEAN
CreatePartition(int Disk, char Letter, unsigned size);

int
NarGetDisks(disk_information* Result, int NElements);

int
GetVolumes(volume_information* Result, int NElements);

BOOLEAN
CreateBootPartition(int DiskID, int EFISize, int IsGPT, int Size) {
    // TODO(Batuhan): create EFI, MSR then call CreatePartition
    return FALSE;
}

file_read
NarReadFile(const char* FileName) {
    file_read Result = { 0 };
    HANDLE File = CreateFileA(FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if (File != INVALID_HANDLE_VALUE) {
        DWORD BytesRead = 0;
        Result.Len = GetFileSize(File, 0);
        if (Result.Len == 0) return Result;
        Result.Data = malloc(Result.Len);
        if (Result.Data) {
            ReadFile(File, Result.Data, Result.Len, &BytesRead, 0);
            if (BytesRead == Result.Len) {
                // NOTE success
            }
            else {
                free(Result.Data);
                printf("Read %i bytes instead of %i\n", BytesRead, Result.Len);
            }
            
        }
        CloseHandle(File);
    }
    else {
        printf("Can't create file: %S\n", FileName);
    }
    //CreateFileA(FNAME, GENERIC_WRITE, 0,0 ,CREATE_NEW, 0,0)
    return Result;
}

BOOLEAN
NarDumpToFile(const char* FileName, void* Data, int Size) {
    BOOLEAN Result = FALSE;
    HANDLE File = CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (File != INVALID_HANDLE_VALUE) {
        DWORD BytesWritten = 0;
        WriteFile(File, Data, Size, &BytesWritten, 0);
        if (BytesWritten == Size) {
            Result = TRUE;
        }
        else {
            printf("Written %i bytes instead of %i\n", BytesWritten, Size);
        }
        CloseHandle(File);
    }
    else {
        printf("Can't create file: %S\n", FileName);
    }
    //CreateFileA(FNAME, GENERIC_WRITE, 0,0 ,CREATE_NEW, 0,0)
    return Result;
}


BOOLEAN
SetVolumeSize(char Letter, int TargetSizeMB) {
    BOOLEAN Result = 0;
    ULONGLONG VolSizeMB = 0;
    char Buffer[1024];
    char FNAME[] = "NARDPSCRPT";
    
    
    ULARGE_INTEGER TOTAL_SIZE = { 0 };
    ULARGE_INTEGER A, B;
    GetDiskFreeSpaceExA("", 0, &TOTAL_SIZE, 0);
    VolSizeMB = TOTAL_SIZE.QuadPart / (1024 * 1024); // byte to MB
    if (VolSizeMB == TargetSizeMB) {
        return TRUE;;
    }
    
    /*
  extend size = X // extends volume by X. Doesnt resize, just adds X
  shrink desired = X // shrink volume to X, if X is bigger than current size, operation fails
  */
    if (VolSizeMB > TargetSizeMB) {
        // shrink volume
        sprintf(Buffer, "select volume %c\nshrink desired = %i\n", Letter, TargetSizeMB);
    }
    else {
        // extend volume
        ULONGLONG Diff = TargetSizeMB - VolSizeMB;
        sprintf(Buffer, "select volume %c\nextend size = %i\n", Letter, Diff);
    }
    
    //NarDumpToFile(const char *FileName, void* Data, int Size)
    if (NarDumpToFile(FNAME, Buffer, strlen(Buffer))) {
        char CMDBuffer[1024];
        sprintf(CMDBuffer, "diskpart \s %s", FNAME);
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
    
#if 0
    "select disk %i\ncreate partition primary size = %u\nassign letter = \"X\"\nformat fs = \"NTFS\" label = \"New Volume\" QUICK";
    
    // diskpart /s DiskPartFile to call DiskPartFile as script
    
    /*
  create partition primary size = X //
  assign letter = "X" // creates partition with given letter
  format fs = "NTFS" label = "New Volume" QUICK // label might change, dont know what actually it means
  */
#endif
    
    sprintf(Buffer, "select disk %i\ncreate partition primary size = %u\nassign letter = \"X\"\nformat fs = \"NTFS\" label = \"New Volume\" QUICK", Disk, size, Letter);
    char FileName[] = "NARDPSCRPT";
    
    if (NarDumpToFile(FileName, Buffer, strlen(Buffer))) {
        char CMDBuffer[1024];
        sprintf(CMDBuffer, "diskpart \s %s", FileName);
        system(CMDBuffer);
        // TODO(Batuhan): maybe check output
        Result = TRUE;
    }
    
    return Result;
}


// returns # of disks, returns 0 if information doesnt fit in array
int
NarGetDisks(disk_information* Result, int NElements) {

  int DiskCount = 0;
  char TextBuffer[1024];
  sprintf(TextBuffer, "list disk\n");
  char DPInputFName[] = "DPINPUT";
  char DPOutputFName[] = "DPOUTPUT";

  if (NarDumpToFile(DPInputFName, TextBuffer, strlen(TextBuffer))) {
    char CMDBuffer[1024];
    sprintf(CMDBuffer, "diskpart /s %s >%s", DPInputFName, DPOutputFName);
    system(CMDBuffer);
  }


  file_read File = NarReadFile(DPOutputFName);
  char* Buffer = (char*)File.Data;
  
  DWORD DriveLayoutSize = 1024 * 64;// 64KB 
  DRIVE_LAYOUT_INFORMATION_EX* DriveLayout = (DRIVE_LAYOUT_INFORMATION_EX*)malloc(DriveLayoutSize);

  //Find first occurance
  char Target[1024];
  int DiskIndex = 0;

  sprintf(Target, "Disk %i", DiskIndex);

  while (TRUE) {

    Buffer = strstr(Buffer, Target);
    if (Buffer != NULL) {

      if (DiskIndex == NElements) {
        DiskIndex = 0;
        break; //Exceeding array
      }

      Buffer = Buffer + strlen(Target); //bypass "Disk #" string

      Result[DiskIndex].ID = DiskIndex;

      int NumberLen = 1;
      char Temp[32];
      memset(Temp, ' ', 32);

      //TODO increment buffer by sizeof target
      while (!IsNumeric(*(++Buffer)) && *Buffer != '\0'); //Find size
      while (*(++Buffer) != ' ' && *Buffer != '\0') NumberLen++; //Find size's len in char

      strncpy(Temp, Buffer - NumberLen, NumberLen);
      Result[DiskIndex].SizeGB = atoi(Temp);
      
      while (*(++Buffer) == ' ' && *Buffer != '\0');// find size identifier letter

      if (*Buffer != 'G') Result[DiskIndex].SizeGB = 0; //If not GB, we are not interested with this disk

      // Find unallocated space
      NumberLen = 1;
      while (!IsNumeric(*(++Buffer)) && *Buffer != '\0');
      while (*(++Buffer) != ' ' && *Buffer != '\0') NumberLen++; //Find size's len in char

      strncpy(Temp, Buffer - NumberLen, NumberLen); //Copy number
      Result[DiskIndex].Unallocated = atoi(Temp);
      Buffer++;  // increment to unit identifier, KB or GB
      if (*Buffer != 'G') Result[DiskIndex].Unallocated = 0;

      {
        char PHNAME[] = "\\\\?\\PhysicalDrive%i";
        sprintf(PHNAME, "\\\\?\\PhysicalDrive%i", Result[DiskIndex].ID);
        
        HANDLE Disk = CreateFileA(PHNAME, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        if (Disk != INVALID_HANDLE_VALUE) {
          DWORD HELL;
          if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DriveLayout, DriveLayoutSize, &HELL, 0)) { // TODO check if we can pass NULL to byteswritten 
            if (DriveLayout->PartitionStyle == PARTITION_STYLE_MBR) {
              strcpy(Result[DiskIndex].Type, "MBR");
            }
            if (DriveLayout->PartitionStyle == PARTITION_STYLE_GPT) {
              strcpy(Result[DiskIndex].Type, "GPT");
            }
            if (DriveLayout->PartitionStyle == PARTITION_STYLE_RAW) {
              strcpy(Result[DiskIndex].Type, "RAW");
            }
            
          }
          else{
            printf("Can't get drive layout for disk %s\n", PHNAME);
          }
          CloseHandle(Disk);
        }
        else {
          printf("Can't open disk %s\n", PHNAME);
        }
        
      }

      DiskIndex++;
      sprintf(Target, "Disk %i", DiskIndex);

    }
    else {
      break;
    }

  }

  free(DriveLayout);
  free(File.Data);
  return DiskIndex;
}


//Returns # of volumes detected
int
GetVolumes(volume_information *Result, int NElements) {
  DWORD Drives = GetLogicalDrives();
  char VolumeString[] = " :\\";
  int VolumeIndex = 0;
  char WindowsDir[512];
  GetWindowsDirectoryA(WindowsDir, 512);
  for (int CurrentDriveIndex = 0; CurrentDriveIndex < 26; CurrentDriveIndex++) {
    DWORD Drives = GetLogicalDrives();
    if (VolumeIndex == NElements) {
      VolumeIndex = 0;
      break;
    }
    if (Drives & (1 << CurrentDriveIndex)){
      VolumeString[0] = 'A' + CurrentDriveIndex;
      char FSname[1024];
      ULARGE_INTEGER TotalSize = { 0 };

      GetVolumeInformationA(VolumeString, 0, 0, 0, 0, 0, FSname, 1024);
      GetDiskFreeSpaceExA(VolumeString, 0, &TotalSize, 0);
      strcpy(Result[VolumeIndex].FileSystem, FSname);

      Result[VolumeIndex].Letter = 'A' + CurrentDriveIndex;
      Result[VolumeIndex].SizeMB = TotalSize.QuadPart / (1024 * 1024);
      Result[VolumeIndex].FileSystem[strlen(FSname)] = '\0'; // Null termination
      Result[VolumeIndex].Bootable = (WindowsDir[0] == Result[VolumeIndex].Letter);
      VolumeIndex++;
    }
  }
  return VolumeIndex;

}



int main() {
    
    int DiskID, Size;
    char Type;
    /*
    DiskID = 3;
    Size = 650;
    Type = 'M';
    printf("%i %i %c", DiskID, Size, Type);
  
    if (Type == 'G' || Type == 'g') {
      CreateGPTPartition(DiskID, Size);
    }
    else if (Type == 'M' || Type == 'm') {
      CreateMBRPartition(DiskID, Size);
    }
    */
    volume_information* volumes = (volume_information*)malloc(sizeof(volume_information) * 26);
    int VCount = GetVolumes(volumes, 26);
    for (int VI = 0; VI < VCount; VI++) {
      printf("Letter:%c\nSize(MB):%u\nFileSystem:%s\nBootable:%i\n#########\n", volumes[VI].Letter, volumes[VI].SizeMB, volumes[VI].FileSystem, volumes[VI].Bootable);
    }

    disk_information* disks = (disk_information*)malloc(sizeof(disk_information) * 1024);
    int DiskCount = NarGetDisks(disks, 1024);
    printf("\n");
    for (int DI = 0; DI < DiskCount; DI++) {
      printf("ID:%i\nSize(GB):%u\nUnallocated(GB):%u\nType:%s\n##########\n", disks[DI].ID, disks[DI].SizeGB, disks[DI].Unallocated, disks[DI].Type);
    }

    for (int VI = 0; VI < VCount; VI++) {
      printf("%c\n",volumes[VI].Letter);
    }

    return 0;
    ULONGLONG TotalUsedSize = 0;
    
    BOOLEAN Result = FALSE;
    
    wchar_t VolPath[128];
    wchar_t Vol[] = L"X:\\";
    
    VOLUME_DISK_EXTENTS* Ext = NULL;
    
    DWORD BS = 1024 * 1024;
    DWORD T = 0;
    
    GetVolumeNameForVolumeMountPointW(Vol, VolPath, 128);
    VolPath[lstrlenW(VolPath) - 1] = L'\0'; //Remove trailing slash
    HANDLE Drive = CreateFileW(VolPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    
    if (Drive != INVALID_HANDLE_VALUE) {
        printf("Searching disk of volume %S\n", VolPath);
        
        Ext = (VOLUME_DISK_EXTENTS*)malloc(BS);
        DRIVE_LAYOUT_INFORMATION_EX* DL = (DRIVE_LAYOUT_INFORMATION_EX*)malloc(BS);
        
        HANDLE Disk = INVALID_HANDLE_VALUE;
        
        if (DeviceIoControl(Drive, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0, Ext, BS, &T, 0)) {
            for (int i = 0; i < Ext->NumberOfDiskExtents; i++) {
                
                wchar_t DiskPath[] = L"\\\\?\\PhysicalDrive3";
                //DiskPath[lstrlenW(DiskPath) - 1] = Ext->Extents[i].DiskNumber + '0';
                
                Disk = CreateFileW(DiskPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
                if (Disk != INVALID_HANDLE_VALUE) {
                    printf("Disk path  : %S\n", DiskPath);
                    
                    if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DL, BS, &T, 0)) {
                        
                        if (DL->PartitionStyle == PARTITION_STYLE_MBR) {
                            printf("Disk contains MBR partitions\n");
                            
                        }
                        if (DL->PartitionStyle == PARTITION_STYLE_GPT) {
                            printf("Disk contains GPT partitions\n");
                        }
                        if (DL->PartitionStyle == PARTITION_STYLE_RAW) {
                            printf("FATAL ERROR : Disk is RAW\n");
                            return FALSE;
                        }
                        
                        for (int k = 0; k < DL->PartitionCount; k++) {
                            PARTITION_INFORMATION_EX* PI = &DL->PartitionEntry[k];
                            
                            TotalUsedSize += PI->PartitionLength.QuadPart;
                            
                            printf("Partition number            : %i\n"
                                   "Partition offset at disk(MB): %I64d\n"
                                   "Partition length        (MB): %I64d\n", PI->PartitionNumber, PI->StartingOffset.QuadPart / (1024 * 1024), PI->PartitionLength.QuadPart / (1024 * 1024));
                            
                            if (PI->PartitionStyle == PARTITION_STYLE_MBR) {
                                printf("MBR partition\n");
                                printf("Bootable %i, ", PI->Mbr.BootIndicator);
                                printf("Recognized partitions %i\n", PI->Mbr.RecognizedPartition);
                                if (PI->Mbr.PartitionType & PARTITION_EXTENDED) {
                                    printf("\tEXTENDED PARTITION\n");
                                }
                                if (PI->Mbr.PartitionType & PARTITION_IFS) {
                                    printf("\tPARTITION IFS\n");
                                }
                                if (PI->Mbr.PartitionType & PARTITION_LDM) {
                                    printf("\tPARTITION LDM\n");
                                }
                                if (PI->Mbr.PartitionType & PARTITION_NTFT) {
                                    printf("\tNTFT\n");
                                }
                                
                            }
                            if (PI->PartitionStyle == PARTITION_STYLE_GPT) {
                                printf("GPT partition\n");
                                printf("GPT Name : %S\n", PI->Gpt.Name);
                                
                                if (PI->Gpt.Attributes != 0) {
                                    printf("Attributes :\n");
                                }
                                if (PI->Gpt.Attributes & GPT_ATTRIBUTE_PLATFORM_REQUIRED) {
                                    printf("\tPlatform required\n");
                                }
                                if (PI->Gpt.Attributes & GPT_BASIC_DATA_ATTRIBUTE_HIDDEN) {
                                    printf("\tData attribute hidden\n");
                                }
                                if (PI->Gpt.Attributes & GPT_BASIC_DATA_ATTRIBUTE_NO_DRIVE_LETTER) {
                                    printf("\tNo drive letter\n");
                                }
                                if (PI->Gpt.Attributes & GPT_BASIC_DATA_ATTRIBUTE_SHADOW_COPY) {
                                    printf("\tShadow copy\n");
                                }
                                
                            }
                            if (PI->PartitionStyle == PARTITION_STYLE_RAW) {
                                printf("Raw partition");
                            }
                            
                            printf("#########\n");
                        }
                        
                    }
                    else {
                        printf("DeviceIOControl get drive layout failed,");
                    }
                    
                    CloseHandle(Disk);
                    
                }
                else {
                    printf("Unable to open disk\n");
                }
                
            }
        }
        else {
            // TODO: Device get disk extents failed
            printf("DeviceIoControl, get disk extents failed\n");
            
        }
        
        
    }
    else {
        printf("Can't open volume\n");
    }
    
    printf("\nTotal used disk size = %I64d\n", TotalUsedSize);
    
    
    return 0;
}
