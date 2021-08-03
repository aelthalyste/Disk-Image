/* date = July 15th 2021 5:12 pm */

#pragma once 

#include "mspyLog.h"
#include "nar.h"


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


inline BOOLEAN
NarRemoveLetter(char Letter);

inline BOOLEAN
NarFormatVolume(char Letter);

inline void
NarRepairBoot(char OSVolumeLetter, char BootPartitionLetter);

BOOLEAN
NarSetVolumeSize(char Letter, int TargetSizeMB);

BOOLEAN
CreatePartition(int Disk, char Letter, unsigned size);


inline BOOLEAN
NarCreateCleanGPTBootablePartition(int DiskID, int VolumeSizeMB, int EFISizeMB, int RecoverySizeMB, char VolumeLetter, char BootVolumeLetter);

inline BOOLEAN
NarCreateCleanGPTPartition(int DiskID, int VolumeSizeMB, char Letter);


BOOLEAN
NarCreateCleanMBRPartition(int DiskID, char VolumeLetter, int VolumeSize);


inline BOOLEAN
NarCreateCleanMBRBootPartition(int DiskID, char VolumeLetter, int VolumeSizeMB, int SystemPartitionSizeMB, int RecoveryPartitionSizeMB,
                               char BootPartitionLetter);

ULONGLONG
NarGetVolumeTotalSize(char Letter);

ULONGLONG
NarGetVolumeUsedSize(char Letter);

inline int
NarGetVolumeDiskType(char Letter);

inline unsigned char
NarGetVolumeDiskID(char Letter);


/*
Expects Letter to be uppercase
*/
inline BOOLEAN
NarIsVolumeAvailable(char Letter);


/*
    Returns first available volume letter that is not used by system
*/
inline char
NarGetAvailableVolumeLetter();


// returns # of disks, returns 0 if information doesnt fit in array
data_array<disk_information>
NarGetDisks();


/*
Unlike generatemetadata, binary functions, this one generates absolute path of the log file. Which is 
under windows folder
C:\Windows\Log....
*/
inline std::wstring
GenerateLogFilePath(char Letter);


inline void
StrToGUID(const char* guid, GUID* G);

ULONGLONG
NarGetDiskTotalSize(int DiskID);


/*
For some reason, kernel and user GUID is not compatible for one character, which is just question mark (?)
. to be kernel compatible, one must call this function to communicate with kernel, otherwise kernel cant distinguish
given GUID and will return error.

VolumeGUID MUST have size of 98 bytes, (49 unicode char)
*/
BOOLEAN
NarGetVolumeGUIDKernelCompatible(wchar_t Letter, wchar_t *VolumeGUID);

inline wchar_t*
NarUTF8ToWCHAR(NarUTF8 s, nar_arena *Arena);

inline NarUTF8
NarWCHARToUTF8(wchar_t *Str, nar_arena *Arena);


uint64_t
NarReadBackup(nar_file_view *Backup, nar_file_view *Metadata, 
              uint64_t AbsoluteClusterOffset, uint64_t ReadSizeInCluster, 
              void *Output, uint64_t OutputMaxSize,
              void *ZSTDBuffer, size_t ZSTDBufferSize);

<<<<<<< HEAD
=======



HANDLE 
NarCreateVSSPipe(uint32_t BufferSize, uint64_t Seed, char *Name, size_t MaxNameCb);

inline process_listen_ctx
NarSetupVSSListen(nar_backup_id ID);

inline void
NarFreeProcessListen(process_listen_ctx *Ctx);

inline bool
NarGetVSSPath(process_listen_ctx *Ctx, wchar_t *Out);

inline void 
NarTerminateVSS(process_listen_ctx *Ctx, uint8_t Success);
>>>>>>> sideup
