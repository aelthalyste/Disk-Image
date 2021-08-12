/* date = July 15th 2021 5:12 pm */
#pragma once 


#include "platform_io.h"


enum ProcessCommandType{
    ProcessCommandType_GetVSSPath,
    ProcessCommandType_TerminateVSS,
    ProcessCommandType_TerminateProcess
};


struct process_listen_ctx{
    char *ReadBuffer;
    char *WriteBuffer;
    uint16_t BufferSize;
    
    OVERLAPPED ReadOverlapped;
    OVERLAPPED WriteOverlapped;
    
    PROCESS_INFORMATION PInfo;
    
    HANDLE PipeHandle;
};


struct disk_information {
    uint64_t Size; //In bytes!
    uint64_t UnallocatedGB; // IN GB!
    char Type; // first character of {RAW,GPT,MBR}
    int ID;
};



/*
example output of diskpart list volume command
Volume ###  Ltr  Label        Fs     Type        Size     Status     Info
  ----------  ---  -----------  -----  ----------  -------  ---------  --------
  Volume 0     N                NTFS   Simple      5000 MB  Healthy
  Volume 1     D   HDD          NTFS   Simple       926 GB  Healthy    Pagefile
  Volume 2     C                NTFS   Partition    230 GB  Healthy    Boot
  Volume 3                      FAT32  Partition    100 MB  Healthy    System
*/
struct volume_information {
    uint64_t TotalSize;
    uint64_t FreeSize;
    int32_t Bootable; // Healthy && NTFS && !Boot
    char Letter;
    unsigned char DiskID;
    char DiskType;
    wchar_t VolumeName[MAX_PATH + 1];
};

struct disk_information_ex{
    uint64_t TotalSize;
    uint64_t UnusedSize;
    
    uint8_t DiskID;
    uint8_t VolumeCount;
    char DiskType;
    
    volume_information *Volumes;
};

struct nar_partition_info{
    uint64_t PartitionInfo;
    uint8_t DiskID;
    char Letter;
    char DiskType;
};


BOOLEAN
NarRemoveLetter(char Letter);

BOOLEAN
NarFormatVolume(char Letter);

void
NarRepairBoot(char OSVolumeLetter, char BootPartitionLetter);

BOOLEAN
NarSetVolumeSize(char Letter, int TargetSizeMB);

BOOLEAN
CreatePartition(int Disk, char Letter, unsigned size);


BOOLEAN
NarCreateCleanGPTBootablePartition(int DiskID, int VolumeSizeMB, int EFISizeMB, int RecoverySizeMB, char VolumeLetter, char BootVolumeLetter);

BOOLEAN
NarCreateCleanGPTPartition(int DiskID, int VolumeSizeMB, char Letter);


BOOLEAN
NarCreateCleanMBRPartition(int DiskID, char VolumeLetter, int VolumeSize);


BOOLEAN
NarCreateCleanMBRBootPartition(int DiskID, char VolumeLetter, int VolumeSizeMB, int SystemPartitionSizeMB, int RecoveryPartitionSizeMB,
                               char BootPartitionLetter);

ULONGLONG
NarGetVolumeTotalSize(char Letter);

ULONGLONG
NarGetVolumeUsedSize(char Letter);

int
NarGetVolumeDiskType(char Letter);

unsigned char
NarGetVolumeDiskID(char Letter);


/*
Expects Letter to be uppercase
*/
BOOLEAN
NarIsVolumeAvailable(char Letter);


/*
    Returns first available volume letter that is not used by system
*/
char
NarGetAvailableVolumeLetter();


// returns # of disks, returns 0 if information doesnt fit in array
data_array<disk_information>
NarGetDisks();


/*
Unlike generatemetadata, binary functions, this one generates absolute path of the log file. Which is 
under windows folder
C:\Windows\Log....
*/
std::wstring
GenerateLogFilePath(char Letter);


void
StrToGUID(const char* guid, GUID* G);

uint64_t
NarGetDiskTotalSize(int DiskID);


/*
For some reason, kernel and user GUID is not compatible for one character, which is just question mark (?)
. to be kernel compatible, one must call this function to communicate with kernel, otherwise kernel cant distinguish
given GUID and will return error.

VolumeGUID MUST have size of 98 bytes, (49 unicode char)
*/
BOOLEAN
NarGetVolumeGUIDKernelCompatible(wchar_t Letter, wchar_t *VolumeGUID);

wchar_t*
NarUTF8ToWCHAR(NarUTF8 s, nar_arena *Arena);

NarUTF8
NarWCHARToUTF8(wchar_t *Str, nar_arena *Arena);


uint64_t
NarReadBackup(nar_file_view *Backup, nar_file_view *Metadata, 
              uint64_t AbsoluteClusterOffset, uint64_t ReadSizeInCluster, 
              void *Output, uint64_t OutputMaxSize,
              void *ZSTDBuffer, size_t ZSTDBufferSize);



HANDLE 
NarCreateVSSPipe(uint32_t BufferSize, uint64_t Seed, char *Name, size_t MaxNameCb);

process_listen_ctx
NarSetupVSSListen(nar_backup_id ID);

void
NarFreeProcessListen(process_listen_ctx *Ctx);

bool
NarGetVSSPath(process_listen_ctx *Ctx, wchar_t *Out);

void 
NarTerminateVSS(process_listen_ctx *Ctx, uint8_t Success);

BOOLEAN
SetupVSS();


//Returns # of volumes detected
data_array<volume_information>
NarGetVolumes();



disk_information_ex*
NarGetPartitions(nar_arena *Arena, size_t* OutCount);


void
GUIDToStr(char *Out, GUID G);

char
NarGetVolumeLetterFromGUID(GUID G);