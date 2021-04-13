#pragma once

#include <msclr/marshal.h>

using namespace System;
using namespace System::Text;
using namespace System::Collections::Generic;



namespace NarDIWrapper {
    
    void
        SystemStringToWCharPtr(System::String^ SystemStr, wchar_t* Destination) {
        
        pin_ptr<const wchar_t> wch = PtrToStringChars(SystemStr);
        size_t ConvertedChars = 0;
        size_t SizeInBytes = (SystemStr->Length + 1) * 2;
        memcpy(Destination, wch, SizeInBytes);
        
    }
    
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
        
        bool IsSameChainID(unsigned long long AnotherID){
            return (BackupID->Q == AnotherID);
        }
        
        wchar_t Letter;
        int BackupType;
        int Version;
        int OSVolume;
        
        CSNarFileTime^ BackupDate;
        // TODO(Batuhan): 
        size_t VolumeTotalSize;
        size_t VolumeUsedSize;
        size_t BytesNeedToCopy; // just for this version, not cumilative
        size_t MaxWriteOffset;  // last write offset that is going to be made
        
        size_t EFIPartSize;
        size_t SystemPartSize;
        
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
    
    
    public ref class BackupReadResult{
        public:
        BackupReadResult(){
            
        }
        
        uint32_t WriteSize = 0;
        uint32_t DecompressedSize = 0;
        BackupStream_Errors Error = BackupStream_Errors::Error_NoError;
    };
    
    
    public ref class RestoreStream {
        
        public:
        
        RestoreStream(BackupMetadata^ arg_BM, System::String^ arg_RootDir) {
            Stream      = 0;
            MemLen      = Megabyte(512);
            Mem         = VirtualAlloc(0, MemLen, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            
            BM          = gcnew BackupMetadata;
            BM          = arg_BM;
            
            RootDir     = gcnew System::String(L"");
            RootDir     = arg_RootDir;
            
        }
        
        ~RestoreStream() {
            TerminateRestore();
        }
        
        bool SetDiskRestore(int DiskID, wchar_t Letter, bool arg_RepairBoot, bool OverrideDiskType, wchar_t AlternativeDiskType) {
            
            char DiskType = (char)BM->DiskType;
            if (OverrideDiskType)
                DiskType = (char)AlternativeDiskType;
            
            RepairBoot = arg_RepairBoot;
            
            int VolSizeMB = BM->VolumeTotalSize/(1024ull* 1024ull) + 1;
            int SysPartitionMB = BM->EFIPartSize / (1024ull * 1024ull);
            int RecPartitionMB = 0;
            
            BootLetter = 0;
            {
                DWORD Drives = GetLogicalDrives();
                
                for (int CurrentDriveIndex = 0; CurrentDriveIndex < 26; CurrentDriveIndex++) {
                    if (Drives & (1 << CurrentDriveIndex) || ('A' + (char)CurrentDriveIndex) == (char)Letter) {
                        continue;
                    }
                    else {
                        BootLetter = ('A' + (char)CurrentDriveIndex);
                        break;
                    }
                }
            }
            
            if (BootLetter != 0) {
                if (DiskType == NAR_DISKTYPE_GPT) {
                    if (BM->OSVolume) {
                        NarCreateCleanGPTBootablePartition(DiskID, VolSizeMB, SysPartitionMB, RecPartitionMB, Letter, BootLetter);
                    }
                    else {
                        NarCreateCleanGPTPartition(DiskID, VolSizeMB, Letter);
                    }
                }
                if (DiskType == NAR_DISKTYPE_MBR) {
                    if (BM->OSVolume) {
                        NarCreateCleanMBRBootPartition(DiskID, Letter, VolSizeMB, SysPartitionMB, RecPartitionMB, BootLetter);
                    }
                    else {
                        NarCreateCleanMBRPartition(DiskID, Letter, VolSizeMB);
                    }
                }
                
            }
            else {
                return false;
            }
            
            
            IsDiskRestore = true;
            return true;
            
        }
        
        
        bool SetupStream(wchar_t VolumeLetter) {
            
            Stream = NULL;
            
            nar_arena Arena = ArenaInit(Mem, MemLen);
            TargetLetter = VolumeLetter;
            
            if (NarSetVolumeSize((char)VolumeLetter, BM->VolumeTotalSize)) {
                NarFormatVolume((char)VolumeLetter);
            }
            else {
                
            }
            
            restore_target *Target = InitVolumeTarget(VolumeLetter, &Arena);
            if (Target) {
                wchar_t bf[512];
                
                memset(bf, 0, sizeof(bf));
                SystemStringToWCharPtr(RootDir, &bf[0]);
                std::wstring RootDir(bf);
                
                memset(bf, 0, sizeof(bf));
                SystemStringToWCharPtr(BM->Metadataname, bf);
                std::wstring mdname(bf);
                
                std::wstring MetadataPath = RootDir + mdname;
                
                printf("WRAPPER: RootDir path : %S\n", RootDir.c_str());
                printf("WRAPPER: Metadata path : %S\n", MetadataPath.c_str());
                
                Stream = InitFileRestoreStream(MetadataPath, Target, &Arena, Megabyte(16));
                BytesNeedToCopy = Stream->BytesToBeCopied;
            }
            
            return !(Stream == NULL);
        }
        
        
        void TerminateRestore() {
            printf("Bytes copied %I64u, total was %I64u, ratio is %%(%.4f)\n", InternalBytesCopied, BytesNeedToCopy, (double)BytesNeedToCopy/(double)InternalBytesCopied);
            if (Mem) {
                FreeRestoreStream(Stream);
                if (IsDiskRestore) {
                    if (RepairBoot && BM->OSVolume) {
                        NarRepairBoot(TargetLetter, BootLetter);
                    }
                    NarRemoveLetter(BootLetter);
                }
                VirtualFree(Mem, MemLen, MEM_RELEASE);
            }
            Stream = 0;
            Mem = 0;
        }
        
        
        
        /* 
            returns how many bytes processed. 0 means either end of stream or something bad happened. check internal errors via CheckStreamStatus.
        */
        
        size_t CW_AdvanceStream() {
            size_t Result = 0;
            if(Stream){
                Result      = AdvanceStream(Stream);
                StreamError = Stream->Error;
                SrcError    = Stream->SrcError;
            }
            InternalBytesCopied += Result;
            return Result;
        }
        
        // returns false if internal error flags are set
        bool CheckStreamStatus() {
            bool Result = false;
            if(Stream){
                Result = (Stream->Error == RestoreStream_Errors::Error_NoError);
            }
            return Result;
        }
        
        RestoreSource_Errors SrcError    = RestoreSource_Errors::Error_NoError;
        RestoreStream_Errors StreamError = RestoreStream_Errors::Error_NoError;
        
        size_t BytesNeedToCopy;
        BackupMetadata^ BM;
        System::String^ RootDir;
        
        private:
        
        size_t InternalBytesCopied = 0;
        
        void            *Mem;
        size_t          MemLen;
        restore_stream* Stream;
        
        bool IsDiskRestore;
        char BootLetter;
        char TargetLetter;
        bool RepairBoot = true;
        
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
        
        bool CW_SetupStream(wchar_t L, int BT, StreamInfo^ StrInf, bool ShouldCompress);
        
        BackupReadResult^ CW_ReadStream(void* Data, wchar_t VolumeLetter, int Size);
        bool CW_CheckStreamStatus(wchar_t Letter);
        
        bool CW_TerminateBackup(bool Succeeded, wchar_t VolumeLetter);
        
        unsigned long long CW_IsVolumeExists(wchar_t Letter);
        
        static wchar_t CW_GetFirstAvailableVolumeLetter();
        static List<BackupMetadata^>^ CW_GetBackupsInDirectory(System::String^ RootDir);
        static List<DiskInfo^>^ CW_GetDisksOnSystem();
        static BOOLEAN CW_MetadataEditTaskandDescriptionField(System::String^ MetadataFileName, System::String^ TaskName, System::String^ TaskDescription);
        static bool CW_IsVolumeAvailable(wchar_t Letter);
        static int CW_HintBufferSize();
        
        static List<CSLog^>^ CW_GetLogs();
        
        private:
        
        static bool msInit    = false;
        static LOG_CONTEXT* C = nullptr;
        
        
        
    };
    
    
    
    
}
