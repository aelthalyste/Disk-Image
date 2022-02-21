/* date = July 15th 2021 5:12 pm */
#pragma once 

#include "precompiled.h"
#include "narstring.hpp"
#include "nar_win32.hpp"
#include "platform_io.hpp"
#include "nar_compression.hpp"
#define MINISPY_NAME  L"MiniSpy"



#if _MANAGED
public
#endif

enum class BackupStream_Errors: int{
    Error_NoError,
    Error_Read,
    Error_SetFP,
    Error_SizeOvershoot,
    Error_Compression,
    Error_Count
};



struct restore_target {
    HANDLE Handle;// to file, or volume
    
    // valid only if target is to a volume or disk!
    int32_t DiskID;
    char TargetLetter;


    // valid only if target is to a file!
    UTF8 *FileName;


    uint64_t BytesWrittenSoFar;
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



struct backup_stream {
    
    data_array<nar_record> Records;
    int32_t RecIndex;
    int32_t ClusterIndex;
    HANDLE Handle;
    
    // If compression enabled, this value is equal to uncompressed size of the resultant compression job
    // otherwise it's equal to readstream's return value
    uint32_t BytesProcessed;
    uint64_t BytesReadOffset;
    
    bool ShouldCompress;
    
    // if set, forbids ReadStream to add more than one region per call. Useful when cloning a disk-volume
    bool RegionLock;
    
    int32_t               CompressionType;
    void*                 CompressionBuffer;
    size_t                BufferSize;
    ZSTD_CCtx*            CCtx;
    uint32_t              ClusterSize;
    BackupStream_Errors   Error;
    
    nar_record*           CompInf;
    size_t                CBII;
    size_t                MaxCBI;
    

    nar_backup_id         BackupID;
    char                  Letter;
    int32_t               Version;
    
    
    bool DidWePushTheBinaryIdentifier; 

    const char* GetErrorDescription(){
        
        static const struct {
            BackupStream_Errors val;
            const char* desc;
        }
        Table[] = {
            {
                BackupStream_Errors::Error_NoError,
                "No error occured during stream\n"
            },
            {
                BackupStream_Errors::Error_Read,
                "Error occured while reading shadow copy\n"
            },
            {
                BackupStream_Errors::Error_SetFP,
                "Error occured while setting volume file pointer\n"
            },
            {
                BackupStream_Errors::Error_SizeOvershoot,
                "Logical cluster exceeds volume size\n"
            },
            {
                BackupStream_Errors::Error_Compression,
                "Internal compression error occured\n"
            },
            {
                BackupStream_Errors::Error_Count,
                "Error_Count is not an error. This is placeholder\n"
            }
        };
        
        const int TableElCount  =sizeof(Table)/sizeof(Table[0]);
        static_assert(TableElCount - 1 == (int)BackupStream_Errors::Error_Count, "There must be same number of descriptions as error count\n");
        
        for(size_t i =0; i<TableElCount; i++){
            if(Table[i].val == this->Error){
                return Table[i].desc;
            }
        }
        
        return "Couldn't find error code in table, you must not be able to see this message\n";
    }
    
    
};


struct full_only_backup_ctx{
    backup_stream      Stream;
    process_listen_ctx PLCtx;
    nar_backup_id BackupID;
    char Letter;
    bool InitSuccess;
    int32_t VolumeTotalClusterCount;
};


struct volume_backup_inf {
    
    wchar_t Letter;
    int32_t IsOSVolume;
    int32_t INVALIDATEDENTRY; // If this flag is set, this entry is unusable. accessing its element wont give meaningful information to caller.
    
    BackupType BT = BackupType::Inc;
    
    
    int32_t Version;
    int32_t ClusterSize;    
    
    nar_backup_id BackupID;
    
    // HANDLE LogHandle; //Handle to file that is logging volume's changes.
    
    ////Incremental change count of the volume, this value will be reseted after every SUCCESSFUL backup operation
    // this value times sizeof(nar_record) indicates how much data appended since last backup, useful when doing incremental backups
    // we dont need that actually, possiblenewbackupregionoffsetmark - lastbackupoffset is equal to that thing
    //uint32_t IncRecordCount;  // IGNORED IF DIFF BACKUP
    
    // Indicates where last backup regions end in local metadata. bytes after that offset is non-backed up parts of the volume.
    // this value + increcordcount*sizeof(nar_record) is PossibleNewBackupRegionOffsetMark
    
    
    int64_t PossibleNewBackupRegionOffsetMark;
    
    /*
    Valid after diff-incremental setup. Stores all changes occured on disk, starting from latest incremental, or beginning if request was diff
    Diff between this and RecordsMem, RecordsMem is just temporary buffer that stores live changes on the disk, and will be flushed to file after it's available
    
    This structure contains information to track stream head. After every read, ClusterIndex MUST be incremented accordingly and if read operation exceeds that region, RecIndex must be incremented too.
    */
    
    union {
        struct{
            int64_t BackupStartOffset;
        }DiffLogMark;
        
        struct{
            int64_t LastBackupRegionOffset;
        }IncLogMark;
    };
    
    int32_t VolumeTotalClusterCount;
    
    backup_stream Stream;
    
    process_listen_ctx PLCtx;
};


struct LOG_CONTEXT {
    void* Port = INVALID_HANDLE_VALUE;
    data_array<volume_backup_inf> Volumes = {};
};
typedef LOG_CONTEXT* PLOG_CONTEXT;


enum ProcessCommandType{
    ProcessCommandType_GetVSSPath,
    ProcessCommandType_TerminateVSS,
    ProcessCommandType_TerminateProcess
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
    uint64_t DiskID;
    char DiskType;
    wchar_t VolumeName[MAX_PATH + 1];
};

struct disk_information_ex{
    uint64_t TotalSize;
    uint64_t UnusedSize;
    
    uint64_t DiskID;
    uint64_t VolumeCount;
    char DiskType;
    
    volume_information *Volumes;
};

struct nar_partition_info{
    uint64_t PartitionInfo;
    uint64_t DiskID;
    char Letter;
    char DiskType;
};






struct volume_backup_inf;


struct DotNetStreamInf {
    int32_t ClusterSize; //Size of clusters, requester has to call readstream with multiples of this size
    int32_t ClusterCount; //In clusters
    std::string FileName;
    std::string BinaryExtension;
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

int BG_API NarGetVolumeDiskType(char Letter);

unsigned char
NarGetVolumeDiskID(char Letter);


/*
Expects Letter to be uppercase
*/
BOOLEAN BG_API NarIsVolumeAvailable(char Letter);


/*
    Returns first available volume letter that is not used by system
*/
char BG_API NarGetAvailableVolumeLetter();


// returns # of disks, returns 0 if information doesnt fit in array
data_array<disk_information> BG_API NarGetDisks();


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
data_array<volume_information> BG_API NarGetVolumes();



disk_information_ex*
NarGetPartitions(nar_arena *Arena, size_t* OutCount);


void
GUIDToStr(char *Out, GUID G);

char 
NarGetVolumeLetterFromGUID(GUID G);

char*
NarConsumeNextToken(char *Input, char *Out, size_t MaxBf, char* End);

char* 
NarConsumeNextLine(char *Input, char* Out, size_t MaxBf, char* InpEnd);


uint32_t
NarGetDiskListFromDiskPart();


static inline bool
CheckStreamCompletedSuccessfully(volume_backup_inf *V){
    if(V){
        return (V->Stream.Error == BackupStream_Errors::Error_NoError);
    }
    else{
        return false;
    }
}

int32_t BG_API ConnectDriver(PLOG_CONTEXT Ctx);


int32_t
SetIncRecords(HANDLE CommPort, volume_backup_inf* V);



ULONGLONG
NarGetLogFileSizeFromKernel(HANDLE CommPort, char Letter);


int32_t
SetDiffRecords(HANDLE CommPort ,volume_backup_inf* V);



int32_t
SetFullRecords(volume_backup_inf* V);



/*
  From given volume handle, extracts region information from volume
  Handle can be VSS handle or normal volume handle. VSS is preferred since volume information might change over time
  returns NULL if any error occurs
*/
nar_record*
GetVolumeRegionsFromBitmap(HANDLE VolumeHandle, uint32_t* OutRecordCount);



/*
  From given volume handle, extracts region information from volume
  Handle can be VSS handle or normal volume handle. VSS is preferred since volume information might change over time
  returns NULL if any error occurs
*/
nar_record*
OLD_GetVolumeRegionsFromBitmap(HANDLE VolumeHandle, uint32_t* OutRecordCount);

full_only_backup_ctx
SetupFullOnlyStream(wchar_t Letter, DotNetStreamInf *SI, bool ShouldCompress, bool RegionLock = false);

/*
*/
int32_t BG_API SetupStream(PLOG_CONTEXT C, wchar_t L, BackupType Type, uint64_t *BytesToTransferOut, char *BinaryExtensionOut, int32_t CompressionType);




void
TerminateFullOnlyStream(full_only_backup_ctx *Ctx, bool ShouldSaveMetadata, wchar_t *MetadataPath);


int32_t BG_API TerminateBackup(volume_backup_inf* V, int32_t Succeeded, const char *DirectoryToEmitMetadata, char *OutputMetadataName);




// Assumes CallerBufferSize >= NAR_COMPRESSION_FRAME_SIZE
uint32_t BG_API ReadStream(backup_stream* Stream, void* CallerBuffer, unsigned int CallerBufferSize);




/*
Attachs filter
SetActive: default value is TRUE
*/
inline int32_t
AttachVolume(char Letter);


int32_t
DetachVolume(volume_backup_inf* VolInf);


int32_t BG_API RemoveVolumeFromTrack(PLOG_CONTEXT C, wchar_t L);



/*
 Just removes volume entry from kernel memory, does not unattaches it.
*/
inline int32_t NarRemoveVolumeFromKernelList(wchar_t Letter, HANDLE CommPort);





int32_t GetVolumesOnTrack(PLOG_CONTEXT C, volume_information* Out, unsigned int BufferSize, int* OutCount);

int32_t BG_API GetVolumeID(PLOG_CONTEXT C, wchar_t Letter);

/*
This operation just adds volume to list, does not starts to filter it,
until it's fullbackup is requested. After fullbackup, call AttachVolume to start filtering
*/
int32_t BG_API AddVolumeToTrack(PLOG_CONTEXT Context, wchar_t Letter, BackupType Type);


data_array<nar_record>
GetMFTandINDXLCN(char VolumeLetter, HANDLE VolumeHandle);


VSS_ID
GetShadowPath(std::wstring Drive, CComPtr<IVssBackupComponents>& ptr, wchar_t* OutShadowPath, size_t MaxOutCh);


inline int32_t
InitVolumeInf(volume_backup_inf* VolInf, wchar_t Letter, BackupType Type);


int32_t
CopyData(HANDLE S, HANDLE D, ULONGLONG Len);


bool
SaveMetadata(char Letter, int Version, int ClusterSize, BackupType BT, data_array<nar_record> BackupRegions, nar_backup_id BackupID, bool IsCompressed, int32_t CompressionType, nar_record *CompInfoArray, size_t CompInfoCOunt, char *OutputFileName);



unsigned char
NarGetVolumeDiskID(char Letter);




static inline int32_t
NarIsOSVolume(char Letter) {
    char windir[256];
    GetWindowsDirectoryA(windir, 256);
    return windir[0] == Letter;
}



/*
Its not best way to initialize a struct
*/
BG_API LOG_CONTEXT* NarLoadBootState();



/*
    Saves the current program state into a file, so next time computer boots driver can recognize it 
    and setup itself accordingly.

    saved states:
        - Letter (user-kernel)
        - Version (user)
        - BackupType (user)
        - LastBackupRegionOffset (user)
*/

int32_t BG_API NarSaveBootState(LOG_CONTEXT* CTX);


data_array<volume_information>
NarGetVolumes();


int32_t BG_API NarEditTaskNameAndDescription(const wchar_t* FileName, const wchar_t* TaskName, const wchar_t* TaskDescription);



inline nar_backup_id
NarGenerateBackupID(char Letter);



inline int32_t
NarFileNameExtensionCheck(const wchar_t *Path, const wchar_t *Extension);

inline void
MergeRegions(data_array<nar_record>* R);

inline void
MergeRegionsWithoutRealloc(data_array<nar_record>* R);



inline bool
IsRegionsCollide(nar_record R1, nar_record R2);



inline void
NarGetProductName(char* OutName);


BOOLEAN
NarSetFilePointer(HANDLE File, ULONGLONG V);


bool
NarParseDataRun(void* DatarunStart, nar_record *OutRegions, uint32_t MaxRegionLen, uint32_t *OutRegionsFound, bool BitmapCompatibleInsert);

bool
NarParseIndexAllocationAttribute(void *IndexAttribute, nar_record *OutRegions, uint32_t MaxRegionLen, uint32_t *OutRegionsFound, bool BitmapCompatibleInsert);


uint32_t
NarGetFileID(void* FileRecord);


inline int32_t
NarGetVolumeClusterSize(char Letter);

/*
CAUTION: This function does NOT lookup attributes from ATTRIBUTE LIST, so if attribute is not resident in original file entry, function wont return it

// NOTE(Batuhan): Function early terminates in attribute iteration loop if it finds attribute with higher ID than given AttributeID parameter

For given MFT FileEntry, returns address AttributeID in given FileRecord. Caller can use return value to directly access given AttributeID 
Function returns NULL if attribute is not present 
*/
inline void*
NarFindFileAttributeFromFileRecord(void *FileRecord, int32_t AttributeID);


bool
NarGetMFTRegionsFromBootSector(HANDLE Volume, 
                               nar_record* Out, 
                               uint32_t* OutLen, 
                               uint32_t Capacity);
                               

bool BG_API NarPrepareRestoreTargetVolume(restore_target *TargetOut, const UTF8 *MetadataPath, char Letter);
bool BG_API NarPrepareRestoreTargetWithNewDisk(restore_target *TargetOut, const UTF8 *MetadataPath, int32_t Letter);
bool BG_API NarFeedRestoreTarget(restore_target *Target, const void *Buffer, int32_t BufferSize);
void BG_API NarFreeRestoreTarget(restore_target *Target);