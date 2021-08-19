/* date = July 15th 2021 0:12 pm */
#pragma once

#include "narstring.h"
#include "precompiled.h"
#include "compression.h"
#include "nar_win32.h"



#define MINISPY_NAME  L"MiniSpy"


struct volume_backup_inf;


struct DotNetStreamInf {
    int32_t ClusterSize; //Size of clusters, requester has to call readstream with multiples of this size
    int32_t ClusterCount; //In clusters
    std::wstring FileName;
    std::wstring MetadataFileName;
};




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
    
    void *CompressionBuffer;
    size_t BufferSize;
    ZSTD_CCtx* CCtx;
    uint32_t ClusterSize;
    BackupStream_Errors Error;
    
    nar_record *CompInf;
    size_t      CBII;
    size_t      MaxCBI;
    
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
    //UINT32 IncRecordCount;  // IGNORED IF DIFF BACKUP
    
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



static inline bool
CheckStreamCompletedSuccessfully(volume_backup_inf *V){
    if(V){
        return (V->Stream.Error == BackupStream_Errors::Error_NoError);
    }
    else{
        return false;
    }
}

int32_t
ConnectDriver(PLOG_CONTEXT Ctx);


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
int32_t
SetupStream(PLOG_CONTEXT C, wchar_t L, BackupType Type, DotNetStreamInf* SI, bool ShouldCompress);




void
TerminateFullOnlyStream(full_only_backup_ctx *Ctx, bool ShouldSaveMetadata = true);


int32_t
TerminateBackup(volume_backup_inf* V, int32_t Succeeded);




// Assumes CallerBufferSize >= NAR_COMPRESSION_FRAME_SIZE
uint32_t
ReadStream(backup_stream* Stream, void* CallerBuffer, unsigned int CallerBufferSize);




/*
Attachs filter
SetActive: default value is TRUE
*/
inline int32_t
AttachVolume(char Letter);


int32_t
DetachVolume(volume_backup_inf* VolInf);


int32_t
RemoveVolumeFromTrack(PLOG_CONTEXT C, wchar_t L);



/*
 Just removes volume entry from kernel memory, does not unattaches it.
*/
inline int32_t
NarRemoveVolumeFromKernelList(wchar_t Letter, HANDLE CommPort);





int32_t
GetVolumesOnTrack(PLOG_CONTEXT C, volume_information* Out, unsigned int BufferSize, int* OutCount);

INT32
GetVolumeID(PLOG_CONTEXT C, wchar_t Letter);

/*
This operation just adds volume to list, does not starts to filter it,
until it's fullbackup is requested. After fullbackup, call AttachVolume to start filtering
*/
int32_t
AddVolumeToTrack(PLOG_CONTEXT Context, wchar_t Letter, BackupType Type);


data_array<nar_record>
GetMFTandINDXLCN(char VolumeLetter, HANDLE VolumeHandle);


VSS_ID
GetShadowPath(std::wstring Drive, CComPtr<IVssBackupComponents>& ptr, wchar_t* OutShadowPath, size_t MaxOutCh);


inline int32_t
InitVolumeInf(volume_backup_inf* VolInf, wchar_t Letter, BackupType Type);


int32_t
CopyData(HANDLE S, HANDLE D, ULONGLONG Len);


int32_t
SaveMetadata(char Letter, int Version, int ClusterSize, BackupType BT, data_array<nar_record> BackupRegions, nar_backup_id BackupID, bool IsCompressed, HANDLE VSSHandle, nar_record *, size_t);



unsigned char
NarGetVolumeDiskID(char Letter);




inline int32_t
NarIsOSVolume(char Letter) {
    char windir[256];
    GetWindowsDirectoryA(windir, 256);
    return windir[0] == Letter;
}



/*
Its not best way to initialize a struct
*/
LOG_CONTEXT*
NarLoadBootState();



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
NarSaveBootState(LOG_CONTEXT* CTX);


data_array<volume_information>
NarGetVolumes();


int32_t
NarEditTaskNameAndDescription(const wchar_t* FileName, const wchar_t* TaskName, const wchar_t* TaskDescription);



inline nar_backup_id
NarGenerateBackupID(char Letter);


int32_t
NarGetBackupsInDirectory(const wchar_t* Directory, backup_metadata* B, int BufferSize, int* FoundCount);


inline int32_t
NarFileNameExtensionCheck(const wchar_t *Path, const wchar_t *Extension);

inline void
MergeRegions(data_array<nar_record>* R);

inline void
MergeRegionsWithoutRealloc(data_array<nar_record>* R);



inline bool
IsRegionsCollide(nar_record R1, nar_record R2);


int
CompareNarRecords(const void* v1, const void* v2);


inline void
NarGetProductName(char* OutName);

