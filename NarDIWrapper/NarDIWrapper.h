#pragma once

#include <msclr/marshal.h>
#include "mspyLog.h"


namespace NarDIWrapper {

  public ref class StreamInfo {
  public:
    System::Int32 ClusterSize; //Size of clusters, requester has to call readstream with multiples of this size
    System::Int32 ClusterCount; //In clusters
    System::String^ FileName;
    System::String^ MetadataFileName;
    System::String^ MFTFileName;
    System::String^ MFTMetadataName;
  };


  struct disk_information {
    ULONGLONG SizeGB; //In GB!
    ULONGLONG Unallocated; // IN GB!
    char Type[4]; // string, RAW,GPT,MBR, one byte for NULL termination
    int ID;
  };
  struct volume_information {
    ULONGLONG SizeMB; //in MB! 
    BOOLEAN Bootable; // Healthy && NTFS && !Boot
    char Letter;
    char FileSystem[6]; // FAT32, NTFS, FAT, 1 byte for NULL termination
  };

  public ref class DiskInfo {
  public:
    unsigned SizeGB;
    unsigned Unallocated;
    System::String^ Type; // MBR, RAW, GPT
    int ID;
  };


  public ref class VolumeInformation {
  public:
    unsigned SizeMB;
    bool Bootable;
    wchar_t Letter;
    System::String^ FileSystem;
  };


  public ref class DiskTracker
  {
  public:
    DiskTracker();
    ~DiskTracker();

    bool CW_InitTracker();

    bool CW_AddToTrack(wchar_t Letter, int Type);
    bool CW_RemoveFromTrack(wchar_t Letter);
    bool CW_SetupStream(wchar_t Letter, StreamInfo^ StrInf);
    bool CW_ReadStream(void* Data, int Size);
    bool CW_TerminateBackup(bool Succeeded);

    bool CW_RestoreToVolume(
      wchar_t TargetLetter,
      wchar_t SrcLetter,
      INT Version,
      bool ShouldFormat,
      System::String^ RootDir);

    bool CW_RestoreToFreshDisk(wchar_t TargetLetter, wchar_t SrcLetter, INT Version, int DiskID, System::String^ Rootdir);

  private:

    // Volume ID that it's stream requested store in wrapper, so requester doesnt have to pass letter or ID everytime it calls readstream or closestream.
    //StreamID is invalidated after CloseStream(), and refreshed every SetupStream() call

    int StreamID;
    LOG_CONTEXT* C;
    restore_inf* R;

  };




}
