#pragma once

#include <msclr/marshal.h>
#include "mspyLog.h"

using namespace System;
using namespace System::Text;
using namespace System::Collections::Generic;

namespace NarDIWrapper {

  public ref class StreamInfo {
  public:
    System::Int32 ClusterSize; //Size of clusters, requester has to call readstream with multiples of this size
    System::Int32 ClusterCount; //In clusters
    System::String^ FileName;
    System::String^ MetadataFileName;
  };


  public ref class DiskInfo {
  public:
    unsigned SizeGB;
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

  };

  public ref class VolumeInformation {
  public:
      ULONGLONG Size; //in bytes!
      BOOLEAN Bootable; // Healthy && NTFS && !Boot
      char Letter;
      INT8 DiskID;
      char DiskType;
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
    
    INT32 CW_ReadStream(void* Data, int Size);
    
    bool CW_TerminateBackup(bool Succeeded);

    bool CW_RestoreToVolume(
      wchar_t TargetLetter,
      wchar_t SrcLetter,
      INT Version,
      bool ShouldFormat,
      System::String^ RootDir);

    bool CW_RestoreToFreshDisk(wchar_t TargetLetter, wchar_t SrcLetter, INT Version, int DiskID, System::String^ Rootdir);

    List<BackupMetadata^>^ CW_GetBackupsInDirectory(const wchar_t *RootDir);
    List<DiskInfo^>^ CW_GetDisksOnSystem();

  private:

    // Volume ID that it's stream requested store in wrapper, so requester doesnt have to pass letter or ID everytime it calls readstream or closestream.
    //StreamID is invalidated after CloseStream(), and refreshed every SetupStream() call

    int StreamID;
    LOG_CONTEXT* C;
    restore_inf* R;

  };




}
