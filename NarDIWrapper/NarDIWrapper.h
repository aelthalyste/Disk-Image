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
    };
    
    public ref class DiskTracker
    {
        public:
        DiskTracker();
        ~DiskTracker();
        
        bool CW_InitTracker();
        
        bool CW_AddToTrack(wchar_t Letter, int Type);
        bool CW_SetupStream(wchar_t Letter, StreamInfo^ StrInf);
        bool CW_ReadStream(void* Data, int Size);
        bool CW_TerminateBackup(bool Succeeded);
        
        bool CW_RestoreVolumeOffline(wchar_t TargetLetter,wchar_t SrcLetter,UINT32 ClusterSize,INT Version,INT Type);
        
        private:
        
        
        // Volume ID that it's stream requested store in wrapper, so requester doesnt have to pass letter or ID everytime it calls readstream or closestream.
        //StreamID is invalidated after CloseStream(), and refreshed every SetupStream() call
        
        int StreamID;
        
    };
    
    LOG_CONTEXT* C;
    restore_inf* R;

}
