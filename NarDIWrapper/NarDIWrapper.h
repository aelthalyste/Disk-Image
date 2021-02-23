#pragma once

#include <msclr/marshal.h>

using namespace System;
using namespace System::Text;
using namespace System::Collections::Generic;

namespace NarDIWrapper {
    
    public ref class StreamInfo {
        public:
        System::UInt32 ClusterSize; //Size of clusters, requester has to call readstream with multiples of this size
        System::UInt32 ClusterCount;
        System::String^ FileName;
        System::String^ MetadataFileName;
        System::UInt64 CopySize;
    };
    
    public ref class DiskInfo {
        public:
        UINT64 Size;
        wchar_t Type; // MBR, RAW, GPT
        int ID;
    };
    
    public ref class CSNarFileTime {
        public:
        
        CSNarFileTime(){
            
        }
        
        CSNarFileTime(WORD pYear, WORD pMonth, WORD pDay, WORD pHour, WORD pMinute, WORD pSecond){
            Year = pYear;
            Month = pMonth;
            Day = pDay;
            Hour = pHour;
            Minute = pMinute;
            Second = pSecond;
        }
        
        WORD Year;
        WORD Month;
        WORD Day;
        WORD Hour;
        WORD Minute;
        WORD Second;
        
    };
    
    public ref class BackupMetadata {
        
        public:
        BackupMetadata(){
            BackupID = new nar_backup_id;
        }
        ~BackupMetadata(){
            delete BackupID;
        }
        
        bool IsSameChainID(BackupMetadata^ bm){
            return memcmp(bm->BackupID, BackupID, sizeof(*BackupID));
        }
        
        wchar_t Letter;
        int BackupType;
        int Version;
        int OSVolume;
        
        CSNarFileTime^ BackupDate;
        // TODO(Batuhan): 
        UINT64 VolumeTotalSize;
        UINT64 VolumeUsedSize;
        UINT64 BytesNeedToCopy; // just for this version, not cumilative
        UINT64 MaxWriteOffset;  // last write offset that is going to be made
        
        wchar_t DiskType;
        System::String^ Fullpath; // fullpath of the backup
        System::String^ Metadataname; // 
        System::String^ WindowsName;
        System::String^ TaskName;
        System::String^ TaskDescription;
        System::String^ ComputerName;
        System::String^ IpAdress;
        
        
        nar_backup_id *BackupID;
        
        /*
                            ComputerName
                            IpAdress
                            UsedSize
                            VolumeSize
                        */
    };
    
    public ref class VolumeInformation {
        public:
        ULONGLONG TotalSize; //in bytes!
        ULONGLONG FreeSize;
        System::String^ VolumeName;
        BOOLEAN Bootable; // Healthy && NTFS && !Boot
        char Letter;
        INT8 DiskID;
        char DiskType;
    };
    
    
    // could be either file or dir
    struct NarFileEntry {
        
        BOOLEAN Flags;
        
        UINT64 MFTFileIndex;
        UINT64 Size; // file size in bytes
        
        UINT64 CreationTime;
        UINT64 LastModifiedTime;
        
        wchar_t Name[MAX_PATH + 1]; // max path + 1 for null termination
    };
    
    
    public ref class CSNarFileEntry {
        public:
        CSNarFileEntry() {
        }
        
        bool IsDirectory;
        UINT64 Size;
        UINT64 ID;
        
        CSNarFileTime^ CreationTime;
        CSNarFileTime^ LastModifiedTime;
        System::String^ Name;
    };
    
    
    
    
    public ref class CSLog{
        public:
        System::String^ LogStr;
        CSNarFileTime^ Time;
    };
    
    
    public ref class CSNarFileExplorer {
        
        nar_backup_file_explorer_context *ctx;
        
        public:
        
        ~CSNarFileExplorer();
        CSNarFileExplorer();
        /*

          C++ Side = INT32 HandleOptions, char Letter, int Version, wchar_t *RootDir
          TODO(Batuhan): HandleOptions parameter will be removed in next builds, its just here for debugging purposes, just like in C++ side.

        */
        //NOTE im not sure about if RootDir is going to be converted to string for managed code
        bool CW_Init(System::String^ SysRootDir, System::String^ SysMetadataName);
        
        
        List<CSNarFileEntry^>^ CW_GetFilesInCurrentDirectory();
        
        // Entry should be directory, otherwise function doesnt do anything and returns false
        bool CW_SelectDirectory(UINT64 ID);
        
        // Pops directory stack by one, which is equal to "up to" button in file explorer
        void CW_PopDirectory();
        
        // deconstructor calls this, but in managed code object disposing may be delayed by GC. if caller want to release memory early, it can do by this utility function. 
        void CW_Free();
        
        void CW_RestoreFile(INT64 ID, System::String^ BackupDirectory, System::String^ TargetDir);
        
        System::String^ CW_GetCurrentDirectoryString();
        
    };
    
    
    public ref class ExistingBackupInfo{
        public:
        bool Exists;
        wchar_t Letter;
        int Version;
    };
    
    
    
    
    public ref class DiskTracker
    {
        public:
        DiskTracker();
        ~DiskTracker();
        
        static List<VolumeInformation^>^ CW_GetVolumes();
        
        bool CW_InitTracker();
        
        bool CW_AddToTrack(wchar_t Letter, int Type);
        
        bool CW_RemoveFromTrack(wchar_t Letter);
        
        bool CW_SetupStream(wchar_t L, int BT, StreamInfo^ StrInf);
        
        INT32 CW_ReadStream(void* Data, wchar_t VolumeLetter, int Size);
        
        bool CW_TerminateBackup(bool Succeeded, wchar_t VolumeLetter);
        
        
        
        static bool CW_RestoreToVolume(wchar_t TargetLetter, BackupMetadata^ BM, bool ShouldFormat, System::String^ RootDir);
        static bool CW_RestoreToFreshDisk(wchar_t TargetLetter, BackupMetadata^ BM, int DiskID,  System::String^ Rootdir, bool FormatBoot, bool OverWriteDiskType, wchar_t OverWritedTargetDiskType);
        
        static wchar_t CW_GetFirstAvailableVolumeLetter();
        static List<BackupMetadata^>^ CW_GetBackupsInDirectory(System::String^ RootDir);
        static List<DiskInfo^>^ CW_GetDisksOnSystem();
        static BOOLEAN CW_MetadataEditTaskandDescriptionField(System::String^ MetadataFileName, System::String^ TaskName, System::String^ TaskDescription);
        static bool CW_IsVolumeAvailable(wchar_t Letter);
        
        static List<CSLog^>^ CW_GetLogs();
        static void CW_GenerateLogs();
        
        private:
        
        // Volume ID that it's stream requested store in wrapper, so requester doesnt have to pass letter or ID everytime it calls readstream or closestream.
        //StreamID is invalidated after CloseStream(), and refreshed every SetupStream() call
        
        LOG_CONTEXT* C;
        
    };
    
    
    
    
}
