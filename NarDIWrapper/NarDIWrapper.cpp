#include "pch.h"

#include "NarDIWrapper.h"

#include <msclr/marshal.h>
#include <Windows.h>
#include <stdio.h>


using namespace System;

namespace NarDIWrapper {
  
  DiskTracker::DiskTracker() {
    C = new LOG_CONTEXT;
    C->Port = INVALID_HANDLE_VALUE;
    C->ShutDown = NULL;
    C->Thread = NULL;
    C->CleaningUp = FALSE;
    C->Volumes = { 0,0 };
  }

  DiskTracker::~DiskTracker() {
    //Do deconstructor things

    delete C;
  }

  bool DiskTracker::CW_SetupVSS() {
    return SetupVSS();
  }

  bool DiskTracker::CW_SetupStream(wchar_t L, StreamInfo^ StrInf) {
    if (SetupStream(C, L)) {
      int ID = GetVolumeID(C, L);
      StrInf->ClusterSize = C->Volumes.Data[ID].ClusterSize;
      StrInf->RegionCount = C->Volumes.Data[ID].Stream.Records.Count;
      StrInf->TotalSize = 0;

      for (int i = 0; i < C->Volumes.Data[ID].Stream.Records.Count; i++) {
        StrInf->TotalSize += C->Volumes.Data[ID].Stream.Records.Data[i].Len;
      }
      return true;
    }

    return false;

  }

  bool DiskTracker::CW_ReadStream(void* Data, int Size) {
    return ReadStream(&C->Volumes.Data[StreamID], Data, Size);
  }

  void DiskTracker::CW_CloseStream()
  {
    throw gcnew System::NotImplementedException();
  }

  bool NarDIWrapper::DiskTracker::CW_AddToTrack(wchar_t L, int Type) {
    return AddVolumeToTrack(C, L, (BackupType)Type);
  }

  bool DiskTracker::CW_TerminateBackup(bool Succeeded) {
    return TerminateBackup(&C->Volumes.Data[StreamID], Succeeded);
  }

}