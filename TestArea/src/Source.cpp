﻿#include <Windows.h>
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




#include <Windows.h>
#include <rpcdcep.h>
#include <rpcdce.h>

inline void
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
NarCreateGPTBootPartition(int DiskID, int VolumeSizeMB, int EFISizeMB, int RecoverySizeMB, char Letter) {
  //TODO win10 MSR partition created by default, take it as parameter and format accordingly.

  char Buffer[2096];
  memset(Buffer, 0, 2096);

  sprintf(Buffer, ""
    "select disk 1\n"
    "clean\n"
    "convert gpt\n" // format disk as gpt
    "select partition 1\n"
    "delete partition override\n"
    "create partition primary size = 529\n"
    "set id=\"de94bba4-06d1-4d40-a16a-bfd50179d6ac\"\n"
    "gpt attributes=0x8000000000000001\n"
    "format fs = ntfs quick label = \"Recovery tools\"\n"
    "create partition efi size = 99\n"
    "format quick fs=fat32 label = \"System\"\n"
    "create partition msr size = 16\n" // win10
    "create partition primary\n"
    "assign letter = V\n"
    "format fs = \"ntfs\" quick\n"
    "exit\n", DiskID, EFISizeMB, VolumeSizeMB, Letter);


  char InputFN[] = "NARDPINPUT";
  if (NarDumpToFile(InputFN, Buffer, strlen(Buffer))) {
    sprintf(Buffer, "diskpart /s %s", InputFN);
    printf(Buffer);
    system(Buffer);
    return TRUE;
  }

  return FALSE;

}

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
          else {
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
GetVolumes(volume_information* Result, int NElements) {
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
    if (Drives & (1 << CurrentDriveIndex)) {
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


enum class BackupType : short {
  Type1,
  Type2,
  Type3
};


#pragma pack(push ,1) // force 1 byte alignment
struct backup_metadata {

  union {
    BOOL IsOSVolume; // int, 4 bytes
    struct {
      BOOLEAN IsGPT; // MBR if false
      BOOLEAN SYSTEM; // EFI for GPT
      BOOLEAN RESTORE;
      BOOLEAN MSR;
    };
  }; // 4byte


  int Version; // -1 for full backup
  int ClusterSize; // 4096 default
  char Letter;


  BOOLEAN Error; //whole error flags can fit here
  BackupType BackupType; // diff or inc


  struct {
    //Standart for all backup types
    ULONGLONG RegionMetadata;
    ULONGLONG Region;

    //For non-full backups
    ULONGLONG MFTMetadata;
    ULONGLONG MFT;

    //For volumes contains an operating system
    ULONGLONG SYSTEM;
    ULONGLONG RESTORE;
    ULONGLONG MSR; // 16MB or zero (0)
  }Size;


  enum class Flags : char {
    NARF_Metadata = 1 << 0,
    NARF_MFTMetadata = 1 << 1, //Ignored in fullbackup;
    //Actual data flags
    NARF_Regions = 1 << 2,
    NARF_MFT = 1 << 3,
    // if OS volume
    NARF_SYSTEM = 1 << 4,
    NARF_RESTORE = 1 << 5,
    NARF_MSR = 1 << 6
  };

};
#pragma pack(pop)

#pragma pack(push,1)
#include <stdint.h>
/*
0 (0x00) 	8 bytes 	Signature ("EFI PART", 45h 46h 49h 20h 50h 41h 52h 54h or 0x5452415020494645ULL[a] on little-endian machines)
8 (0x08) 	4 bytes 	Revision (for GPT version 1.0 (through at least UEFI version 2.7 (May 2017)), the value is 00h 00h 01h 00h)
12 (0x0C) 	4 bytes 	Header size in little endian (in bytes, usually 5Ch 00h 00h 00h or 92 bytes)
16 (0x10) 	4 bytes 	CRC32 of header (offset +0 up to header size) in little endian, with this field zeroed during calculation
20 (0x14) 	4 bytes 	Reserved; must be zero
24 (0x18) 	8 bytes 	Current LBA (location of this header copy)
32 (0x20) 	8 bytes 	Backup LBA (location of the other header copy)
40 (0x28) 	8 bytes 	First usable LBA for partitions (primary partition table last LBA + 1)
48 (0x30) 	8 bytes 	Last usable LBA (secondary partition table first LBA − 1)
56 (0x38) 	16 bytes 	Disk GUID in mixed endian[6]
72 (0x48) 	8 bytes 	Starting LBA of array of partition entries (always 2 in primary copy)
80 (0x50) 	4 bytes 	Number of partition entries in array
84 (0x54) 	4 bytes 	Size of a single partition entry (usually 80h or 128)
88 (0x58) 	4 bytes 	CRC32 of partition entries array in little endian
92 (0x5C) 	* 	Reserved; must be zeroes for the rest of the block (420 bytes for a sector size of 512 bytes; but can be more with larger sector sizes)
*/

//LBA size = 512 byte;
#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct _GUID {
  unsigned long  Data1;
  unsigned short Data2;
  unsigned short Data3;
  unsigned char  Data4[8];
} GUID;
#endif

typedef uint64_t BYTE_8;
typedef uint32_t BYTE_4;
typedef uint16_t BYTE_2;
typedef uint8_t  BYTE;

// https://www.thomas-krenn.com/en/wiki/CHS_and_LBA_Hard_Disk_Addresses

//all in little-endian format
// _byteswap_uint64
// _byteswap_ulong
// _byteswap_short

struct NAR_MBR_PARTITION {
  BYTE BootIndicator; // 0x80 bootable;
  BYTE StartingChs[3];
  BYTE OSType;
  BYTE EndingChs[3];
  BYTE_4 StartingLBA;
  BYTE_4 SizeInLBA;
};

struct NAR_LEGACY_MBR_HEADER {
  BYTE BootCode[424];
  BYTE Unknown1[16];
  BYTE_4 UniqueDiskSignature; //Written by OS, not by UEFI firmware;
  BYTE_2 Unknown2;
  NAR_MBR_PARTITION Partitions[4];
  BYTE_2 Signature;
};

// https://en.wikipedia.org/wiki/GUID_Partition_Table
// All in little endian


struct NAR_GPT_HEADER {
  //LBA 1
  BYTE_8 Signature;
  BYTE_4 Revision;
  BYTE_4 HeaderSize;
  BYTE_4 CRC32;
  BYTE_4 Reserved;
  BYTE_8 CurrentLBA;
  BYTE_8 BackupLBA;
  BYTE_8 FirstUsableLBA; // should be equal to 1MB in gpt, 2048th sector
  BYTE_8 LastUsableLBA;
  GUID GUID;
  BYTE_8 StartingLba;
  BYTE_4 EntryCount;
  BYTE_4 EntrySize;
  BYTE_4 EntryTableCRC32;
  BYTE Reserved2[420];
  //LBA 2
};

/*
0 (0x00) 	16 bytes 	Partition type GUID (mixed endian[6])
16 (0x10) 	16 bytes 	Unique partition GUID (mixed endian)
32 (0x20) 	8 bytes 	First LBA (little endian)
40 (0x28) 	8 bytes 	Last LBA (inclusive, usually odd)
48 (0x30) 	8 bytes 	Attribute flags (e.g. bit 60 denotes read-only)
56 (0x38) 	72 bytes 	Partition name (36 UTF-16LE code units)
*/
struct NAR_GPT_ENTRY {
  GUID PartitionGUID;
  GUID UniqueGUID;
  BYTE_8 FirstLBA;
  BYTE_8 LastLBA;
  BYTE_8 AttributeFlags;
  BYTE PartitionName[72]; // 36 UTF16LE code units, not ASCII
};

#pragma pack(pop)


static void nlog(const char* str, ...) {

  HWND notepad, edit;
  va_list ap;
  char buf[256];

  va_start(ap, str);
  vsprintf(buf, str, ap);
  va_end(ap);

  strcat(buf, "\r\n");
  notepad = FindWindow(NULL, "Untitled - Notepad");
  edit = FindWindowEx(notepad, NULL, "EDIT", NULL);
  SendMessage(edit, EM_REPLACESEL, TRUE, (LPARAM)buf);

}

BOOLEAN
InitGPTPartition(int DiskID) {
  char Buffer[1024];
  sprintf(Buffer, ""
    "select disk %i\n"
    "clean\n"
    "convert gpt\n"
    "select partition 1\n"
    "delete partition override\n"
    "exit\n", DiskID);

  char INPUTFNAME[] = "DPINPUT";
  if (NarDumpToFile(INPUTFNAME, Buffer, strlen(Buffer))) {
    memset(Buffer, ' ', 1024);
    sprintf(Buffer, "diskpart /s %s", INPUTFNAME);
    system(Buffer);
    return TRUE;
  }

  return FALSE;
}

BOOLEAN
CreateAndMountSystemPartition(int DiskID, char Letter, unsigned SizeMB) {
  char Buffer[1024];
  sprintf(Buffer, ""
    "select disk %i\n"
    "create partition efi size = %u\n" // size in MB
    "format fs = \"ntfs\" quick\n"
    "assign letter = %c\n"
    "exit\n", DiskID, SizeMB, Letter);

  char INPUTFNAME[] = "DPINPUT";
  if (NarDumpToFile(INPUTFNAME, Buffer, strlen(Buffer))) {
    memset(Buffer, ' ', 1024);
    sprintf(Buffer, "diskpart /s %s", INPUTFNAME);
    system(Buffer);
    return TRUE;
  }

  return FALSE;
}

BOOLEAN
CreateAndMountRecoveryPartition(int DiskID, char Letter, unsigned SizeMB) {
  char Buffer[1024];
  sprintf(Buffer, ""
    "select disk %i\n"
    "create partition primary size = %u\n" // size in MB
    "set id=\"de94bba4-06d1-4d40-a16a-bfd50179d6ac\"\n"
    "gpt attributes=0x8000000000000001\n"
    "format fs = \"fat32\" quick\n"
    "assign letter = %c\n"
    "exit\n", DiskID, SizeMB, Letter);

  char INPUTFNAME[] = "DPINPUT";
  if (NarDumpToFile(INPUTFNAME, Buffer, strlen(Buffer))) {
    memset(Buffer, ' ', 1024);
    sprintf(Buffer, "diskpart /s %s", INPUTFNAME);
    system(Buffer);
    return TRUE;
  }

  return FALSE;
}

BOOLEAN
CreateAndMountMSRPartition(int DiskID, unsigned SizeMB) {
  char Buffer[1024];
  sprintf(Buffer, ""
    "select disk %i\n"
    "create partition msr size = %u\n" // size in MB
    "exit\n", DiskID, SizeMB);

  char INPUTFNAME[] = "DPINPUT";
  if (NarDumpToFile(INPUTFNAME, Buffer, strlen(Buffer))) {
    memset(Buffer, ' ', 1024);
    sprintf(Buffer, "diskpart /s %s", INPUTFNAME);
    system(Buffer);
    return TRUE;
  }

  return FALSE;
}

BOOLEAN
RemoveLetter(int DiskID, unsigned PartitionID, char Letter) {
  char Buffer[1024];
  sprintf(Buffer, ""
    "select disk = %u\n"
    "select partition  = %u\n"
    "remove letter = %c\n"
    "exit\n", DiskID, PartitionID, Letter);

  char INPUTFNAME[] = "DPINPUT";
  if (NarDumpToFile(INPUTFNAME, Buffer, strlen(Buffer))) {
    memset(Buffer, ' ', 1024);
    sprintf(Buffer, "diskpart /s %s", INPUTFNAME);
    system(Buffer);
    return TRUE;
  }

  return FALSE;
}

// NOTE(Batuhan): create partition with given size
BOOLEAN
NarCreatePrimaryPartition(int DiskID, char Letter, unsigned SizeMB) {
  char Buffer[1024];
  sprintf(Buffer, ""
    "select disk %i\n"
    "create partition primary size  = %u\n" // size in MB
    "format fs = ntfs quick\n"
    "assign letter = %c\n"
    "exit\n", DiskID, SizeMB, Letter);

  char INPUTFNAME[] = "DPINPUT";
  if (NarDumpToFile(INPUTFNAME, Buffer, strlen(Buffer))) {
    memset(Buffer, ' ', 1024);
    sprintf(Buffer, "diskpart /s %s", INPUTFNAME);
    system(Buffer);
    return TRUE;
  }

  return FALSE;
}

// NOTE(Batuhan): new partition will use all unallocated space left
BOOLEAN
NarCreatePrimaryPartition(int DiskID, char Letter) {
  char Buffer[1024];
  sprintf(Buffer, ""
    "select disk %i\n"
    "create partition primary\n" // size in MB
    "format fs = ntfs quick\n"
    "assign letter = %c\n"
    "exit\n", DiskID, Letter);

  char INPUTFNAME[] = "DPINPUT";
  if (NarDumpToFile(INPUTFNAME, Buffer, strlen(Buffer))) {
    memset(Buffer, ' ', 1024);
    sprintf(Buffer, "diskpart /s %s", INPUTFNAME);
    system(Buffer);
    return TRUE;
  }

  return FALSE;
}

int main() {
  InitGPTPartition(1);
  CreateAndMountRecoveryPartition(1, 'V', 529);
  RemoveLetter(1,1,'V');

  return 0;
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

  //volume_information* volumes = (volume_information*)malloc(sizeof(volume_information) * 26);
  //int VCount = GetVolumes(volumes, 26);
  //for (int VI = 0; VI < VCount; VI++) {
  //  printf("Letter:%c\nSize(MB):%u\nFileSystem:%s\nBootable:%i\n#########\n", volumes[VI].Letter, volumes[VI].SizeMB, volumes[VI].FileSystem, volumes[VI].Bootable);
  //}
  //
  //disk_information* disks = (disk_information*)malloc(sizeof(disk_information) * 1024);
  //int DiskCount = NarGetDisks(disks, 1024);
  //printf("\n");
  //for (int DI = 0; DI < DiskCount; DI++) {
  //  printf("ID:%i\nSize(GB):%u\nUnallocated(GB):%u\nType:%s\n##########\n", disks[DI].ID, disks[DI].SizeGB, disks[DI].Unallocated, disks[DI].Type);
  //}
  //
  //for (int VI = 0; VI < VCount; VI++) {
  //  printf("%c\n", volumes[VI].Letter);
  //}
  //
  //printf("\n\n\n");

  ULONGLONG TotalUsedSize = 0;

  BOOLEAN Result = FALSE;


  VOLUME_DISK_EXTENTS* Ext = NULL;
  GUID GEFI = { 0 };
  StrToGUID("{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}", &GEFI);

  DWORD BS = 1024 * 1024 * 16;
  DWORD T = 0;

  DRIVE_LAYOUT_INFORMATION_EX* DL = (DRIVE_LAYOUT_INFORMATION_EX*)malloc(BS);

  wchar_t DiskPath[] = L"\\\\?\\PhysicalDrive2";


  HANDLE Disk = CreateFileW(DiskPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
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
          if (PI->Mbr.PartitionType & PARTIITON_OS_DATA) {
            printf("PARTITION OS DATA\n");
          }
          if (PI->Mbr.PartitionType & PARTITION_SYSTEM) {
            printf("SYSTEM PARTITION\n");
          }

        }
        if (PI->PartitionStyle == PARTITION_STYLE_GPT) {
          printf("GPT partition\n");
          printf("GPT Name : %S\n", PI->Gpt.Name);

          printf("GEFI partition detected\n");
          LARGE_INTEGER T;
          SetFilePointerEx(Disk, PI->StartingOffset, &T, 0);
          const unsigned buffersize = 512;
          char Buffer[buffersize];
          sprintf(Buffer, "TestInput");

          DWORD Test;
          if (WriteFile(Disk, Buffer, buffersize, &Test, 0)) {
            printf("Write op successfull\n");
          }
          if (Test != buffersize) {
            printf("Cant write to physical disk\n");
          }


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


  -

    printf("\nTotal used disk size = %I64d\n", TotalUsedSize);


  return 0;
}
