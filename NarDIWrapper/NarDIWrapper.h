#pragma once

#include <msclr/marshal.h>
#include "mspyLog.h"

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
        unsigned Size;
        wchar_t Type; // MBR, RAW, GPT
        int ID;
    };

    public ref class BackupMetadata {
    public:

        wchar_t Letter;
        int BackupType;
        int Version;
        int OSVolume; // boolean actually
        wchar_t DiskType;
        System::String^ WindowsName;
        /*
            ComputerName
            IpAdress
            UsedSize
            VolumeSize
        */
    };

    public ref class VolumeInformation {
    public:
        ULONGLONG Size; //in bytes!
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


    public ref class CSNarFileTime {
    public:
      
      CSNarFileTime(){

      }

        INT16 Year;
        INT16 Month;
        INT16 Day;
        INT16 Hour;
        INT16 Minute;
        INT16 Second;

    };

    public ref class CSNarFileEntry {
    public:
        CSNarFileEntry() {
        }

        INT16 IsDirectory;
        UINT64 Size;
        UINT64 ID;

        CSNarFileTime^ CreationTime;
        CSNarFileTime^ LastModifiedTime;
        System::String^ Name;
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
        bool CW_Init(wchar_t VolumeLetter, int Version, System::String^ RootDir);


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



    public ref class DiskTracker
    {
    public:
        DiskTracker();
        ~DiskTracker();

        List<VolumeInformation^>^ CW_GetVolumes();

        bool CW_InitTracker();

        bool CW_AddToTrack(wchar_t Letter, int Type);

        bool CW_RemoveFromTrack(wchar_t Letter);

        bool CW_SetupStream(wchar_t L, int BT, StreamInfo^ StrInf);

        INT32 CW_ReadStream(void* Data, wchar_t VolumeLetter, int Size);

        bool CW_TerminateBackup(bool Succeeded, wchar_t VolumeLetter);


        //terminate backuptan sonra cagir
        bool CW_SaveBootState();




        bool CW_RestoreToVolume(
            wchar_t TargetLetter,
            wchar_t SrcLetter,
            INT Version,
            bool ShouldFormat,
            System::String^ RootDir);

        bool CW_RestoreToFreshDisk(
          wchar_t TargetLetter, 
          wchar_t SrcLetter, 
          INT Version, 
          int DiskID, 
          System::String^ Rootdir);

        

        static List<BackupMetadata^>^ CW_GetBackupsInDirectory(System::String^ RootDir);
        static List<DiskInfo^>^ CW_GetDisksOnSystem();

        
    private:

        // Volume ID that it's stream requested store in wrapper, so requester doesnt have to pass letter or ID everytime it calls readstream or closestream.
        //StreamID is invalidated after CloseStream(), and refreshed every SetupStream() call

        LOG_CONTEXT* C;

    };




}
