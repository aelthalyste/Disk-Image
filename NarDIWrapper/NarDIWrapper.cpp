/*
TOPLANTI için sorulacaklar

volume yedek bilgilerinin bir şekilde yedeklenmesi, mesela program crashlendi, bir şekilde volume çıkarıldı vs vs, volumein yedek bilgileri saklanıp tekrardan program açıldığında devam edilmeli mi

restore işlemi tamamen stream olarak mı yapılacak yoksa dosyanın diske inme ihtimali var mı? Dosya diske inecekse C tarafında kodlar yazılacak, eğer diske inmeden stream ile aktarılacaksa metadataya uygun olarak sırasıyla, sıra önemli, bölgeler aktarılmalı. byle olursa wrappera writestream yazılacak ve sadece veri alıp kendi içinde olayı çözecek

ekstra partition bilgileri nasıl saklanmalı, mesela C volumeunun bağlı olduğu 2 tane daha ek partition vs var. bunların bilgileri ve kendileri nasıl saklanmalı.

silme işlemleri, şu anlık diskteki silinmeleri kontrol etmiyoruz, yani birisi diskin bir yerine yazıp daha sonra orayı silse bile orayı da alıyoruz, fakat orası disk tarafından kullanılmıyor olabilir. bunun için bir kontrol mekanizması gerekir

çözüm 1;
üzerinde düşündüğüm yöntem şu, backup alırken, diskin o anda kullanılan kısımları da alınacak, eğer yedek aldığımız kısımlar diskte kullanılmıyor ise orayı almayacağız

metadatanın 4gbi geçmemesi gerekiyor bu yaklaşık olarak 2^29 tane girdi demek

mesaj loopunda, threadinde, bir sorun yaşanırsa nasıl ana uygulamaya bildirilecek, nasıl iletişim sağlanılmalı sonuçta poll edilmedikçe bu fark edilemeyebilir, interrupt tarzında bir yöntem elbet vardır

*/

#include "pch.h"

#include "NarDIWrapper.h"

#include <msclr/marshal.h>
#include <Windows.h>
#include <stdio.h>


using namespace System;

namespace NarDIWrapper {
    
    DiskTracker::DiskTracker() {
        C = (LOG_CONTEXT*)malloc(sizeof(LOG_CONTEXT));
        memset(C,0,sizeof(LOG_CONTEXT));
        C->Port = INVALID_HANDLE_VALUE;
        C->ShutDown = NULL;
        C->Thread = NULL;
        C->CleaningUp = FALSE;
        C->Volumes = { 0,0 };
        
        R = (restore_inf*)malloc(sizeof(restore_inf));
        memset(C,0,sizeof(restore_inf));
        
    }
    
    DiskTracker::~DiskTracker() {
        //Do deconstructor things
        delete R;
        delete C;
    }
    
    bool DiskTracker::CW_InitTracker() {
        
        if(SetupVSS()){
            return ConnectDriver(C);
        }
        return FALSE;
        
    }
    
    bool DiskTracker::CW_AddToTrack(wchar_t L, int Type) {
        return AddVolumeToTrack(C, L, (BackupType)Type);
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
    
    /*
Version: -1 to restore full backup otherwise version number to restore(version number=0 first inc-diff backup)
*/
    bool DiskTracker::CW_RestoreVolumeOffline(wchar_t TargetLetter,
                                              wchar_t SrcLetter,
                                              UINT32 ClusterSize,
                                              INT Version,
                                              BackupType Type
                                              ){
        
        R->TargetLetter = TargetLetter;
        R->SrcLetter = SrcLetter;
        R->ClusterSize = ClusterSize;
        R->Type = Type;
        
        R->ToFull = FALSE;
        R->Version = Version;
        if(Version < 0){
            R->ToFull = TRUE;
            R->Version = 0;
        }
        return OfflineRestore(R);
        
    }
    
    bool DiskTracker::CW_ReadStream(void* Data, int Size) {
        return ReadStream(&C->Volumes.Data[StreamID], Data, Size);
    }
    
    bool DiskTracker::CW_TerminateBackup(bool Succeeded) {
        return TerminateBackup(&C->Volumes.Data[StreamID], Succeeded);
    }
    
    
    // TODO(Batuhan): helper functions, like which volume we are streaming etc.
}