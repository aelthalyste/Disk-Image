#include <stdio.h>
#include <Windows.h>

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

  BOOLEAN Result = FALSE;

  wchar_t VolPath[128];
  wchar_t Vol[] = L"C:\\";
  Vol[0] = 'G';

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

        wchar_t DiskPath[] = L"\\\\?\\PhysicalDriveX";
        DiskPath[lstrlenW(DiskPath) - 1] = Ext->Extents[i].DiskNumber + '0';

        Disk = CreateFileW(DiskPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        if (Disk != INVALID_HANDLE_VALUE) {
          printf("Disk path  : %S\n", DiskPath);

          if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DL, BS, &T, 0)) {

            if (DL->PartitionStyle == PARTITION_STYLE_MBR) {
              printf("MBR drive partition\n");
              DWORD BSize = 512;
              DWORD BR = 0;
              void* Buffer = malloc(BSize);

              {
                SetFilePointer(Disk, 0, 0, 0);

                if (ReadFile(Disk, Buffer, BSize, &BR, 0) && BR == 512) {
                  Buffer = (void*)((char*)Buffer + 446);

                  for (int i = 0; i < 4; i++) {
                    BYTE* Partition = (BYTE*)Buffer + i * 16;
                    printf("####\n");
                    printf("Raw partition info: ");
                    for (int j = 0; j < 16; j++) {
                      printf("%0X ", *(Partition + j));
                    }
                    printf("\n");

                    unsigned int BootIndicator = 0;
                    unsigned int StartCHS = 0;
                    unsigned int EndCHS = 0;
                    unsigned int FileSystem = 0;
                    unsigned int StartSector = 0;
                    unsigned int EndSector = 0;

                    BootIndicator = *Partition;

                    StartCHS = StartCHS | *(Partition + 3);
                    StartCHS = StartCHS | ((*(Partition + 2)) << 8);
                    StartCHS = StartCHS | ((*(Partition + 1)) << 16);

                    FileSystem = *(Partition + 4);

                    EndCHS = EndCHS | *(Partition + 7);
                    EndCHS = EndCHS | ((*(Partition + 6)) << 8);
                    EndCHS = EndCHS | ((*(Partition + 5)) << 16);


                    BYTE Temp[4];
                    Temp[0] = *(Partition + 11);
                    Temp[1] = *(Partition + 10);
                    Temp[2] = *(Partition + 9);
                    Temp[3] = *(Partition + 8);
                    printf("%0X ", Temp[0]);
                    printf("%0X ", Temp[1]);
                    printf("%0X ", Temp[2]);
                    printf("%0X ", Temp[3]);

                    StartSector = *((int*)Temp);

                    auto Swap = [](char* v1, char* v2) { char Temp = *v1; *v1 = *v2; *v2 = Temp; };

                    StartSector = 0;

                    StartSector = StartSector | (*(Partition + 11));
                    StartSector = StartSector | (*(Partition + 10) << 8);
                    StartSector = StartSector | (*(Partition + 9) << 16);
                    StartSector = StartSector | (*(Partition + 8) << 24);

                    //StartSector = StartSector |  *(Partition + 11);
                    //StartSector = StartSector | ((*(Partition + 10)) <<  8);
                    //StartSector = StartSector | ((*(Partition +  9)) << 16);
                    //StartSector = StartSector | ((*(Partition +  8)) << 24);

                    EndSector = *(int*)(Partition + 12);
                    EndSector = ((EndSector << 8) & 0xFF00FF00) | ((EndSector >> 8) & 0xFF00FF);
                    EndSector = (EndSector << 16) | (EndSector >> 16);

                    //EndSector = EndSector |  *(Partition + 15);
                    //EndSector = EndSector | ((*(Partition + 14)) <<  8);
                    //EndSector = EndSector | ((*(Partition + 13)) << 16);
                    //EndSector = EndSector | ((*(Partition + 12)) << 24);


                    if (FileSystem == 0x07) {
                      printf("NTFS partition\n");
                    }
                    printf("\n"
                      "Bootable    : %i\n"
                      "StartCHS    : %i\n"
                      "EndCHS      : %i\n"
                      "StartSector : %i\n"
                      "EndSector   : %i\n", BootIndicator, StartCHS, EndCHS, StartSector, EndSector);
                    printf("####\n");



                  }

                }
                else {
                  printf("Cant read file, %i\n", GetLastError());
                }
              }


              //Read first 512 bytes(first sector)


            }
            if (DL->PartitionStyle == PARTITION_STYLE_GPT) {
              printf("GPT drive partition\n");
              //for now we wont be dealing with gpt drives
              Result = TRUE;
            }
            if (DL->PartitionStyle == PARTITION_STYLE_RAW) {
              printf("Raw drive partition\n");
              Result = TRUE;
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



  return 0;
}