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


-Driver kısmına çok girme, prensibini ve outputunu anlat.
-VSS ile olan tek bağlantımız diskin yedeklenme aşamasında var, onun dışında driverlara bağlı değiliz
-C# tarafındaki kod oldukça hafif, state machine gibi çalışıyor,içerisindeki bilgiler ile o anda hangi tür backup alacağına karar verip streami hazırlıyor.
-Stream için toplamda ne kadar veri aktarımı yapacağımızı biliyoruz, hedef veri bitene kadar okuma yapılacak, fonksiyon fail döndürmemeli, döndürürse memory hatası almışız demektir
-Diffbackup da alsak incremental da alsak, çıkardığımız loglar aynı olacak hep.
-Diff aldığımızda, son diff ile o an istenilen diff arasındaki incremental değişiklik metadatasını çıkartacağım. Tabi bu sadece son yedekleme arasındaki fark olduğu için eski bütün logları buluttan isteyeceğim, sonra bunları birleştirip streami sunacağım.  Verdiğim veri yine diff olacak
-Diskte saklanan veri, bölgelerin sıralı şekilde ard arda yerleştirilmesi ile oluşturulmuş bir binary data. Metadata olmadan diske nasıl geri yazacağımızı bilemeyiz.
-Incremental alırken, difften farklı olarak eski logları istemeyeceğim, direkt streami başlatacağım
-Dosyaların diskte kayıtlı olacak isimleri konusunda bir ortak nokta bulsak iyi olabilir. Şu anlık harf + versiyon olarak kodluyorum
-Metadatalar binary formatta, sadece bölgelerden oluşuyorlar. Metadataların bozulması durumunda restore yapılamaz
-Stream sağlarken, bölgelerin ayrı dosyalar olarak saklanması iyi olabilir.
-Restore işlemi için bütün versiyonları inceleyip çıkartılabilecek en küçük restore datası çıkartan bir algoritma tasarladım, buluttan kullanıcıya yollarken epey bir küçültme sağlayacaktır
-Fakat algoritma yollanan yedek dosyasının belirli parçalarını belirli uzunluklarda isteyecektir, fakat dosyalar bulutta sıkıştırıldıkları için seek edip istenilen bölgeyi bulamayız.
-Yapılabilecek bir çözüm var, bölgeleri yollarken unique bir ID ile kodlayıp her bir bölge için ayrı dosya yapıp onları ziplersek, istenilen bölgeyi yine çıkartabiliriz. Yine bölgenin seek edilerek okunabileceği ihtimali göz ardı edilmemelidir.
-Bu dosyayı diskte oluşturup kullanıcıya göndermek diskte ekstra yer kaplayacak ve işlem gücü harcayacağı için stream olarak direkt verilmesi daha iyi olabilir. Stream olarak verilmesi için seek ederek okuyup kullanıcıya gönderilebilinirse, kullanıcı tarafında metadata bilindiği için, düzgün sırayla yollayabilirsek restore yapabilirim.
-Eğer bu algoritmayı kullanamazsak, diff ve incremental için gerek offline gerek ise stream olarak restore yapmak basit bir işlem.




*/

#include "pch.h"

#include "NarDIWrapper.h"
#include <msclr\marshal.h>
#include <msclr\marshal_cppstd.h>

#include "mspyLog.h"
#include "mspyUser.cpp"
#include "mspyLog.cpp"

#include <msclr/marshal.h>
#include <Windows.h>
#include <stdio.h>


using namespace System;

#define CONVERT_TYPES(_in,_out) _out = msclr::interop::marshal_as<decltype(_out)>(_in);

void SystemStringToWCharPtr(System::String ^SystemStr, wchar_t *Destination) {
    
    pin_ptr<const wchar_t> wch = PtrToStringChars(SystemStr);
    size_t ConvertedChars = 0;
    size_t SizeInBytes = (SystemStr->Length + 1) * 2;
    
    memcpy(Destination, wch, SizeInBytes);

}


namespace NarDIWrapper {


    CSNarFileExplorer::CSNarFileExplorer(){
        memset(ctx, 0, sizeof(*ctx));
    }

    CSNarFileExplorer::~CSNarFileExplorer() {
        this->CW_Free();
    }
    
    bool CSNarFileExplorer::CW_Init(INT32 HandleOptions, wchar_t VolLetter, int Version, wchar_t *RootDir){
        ctx = (nar_backup_file_explorer_context*)malloc(sizeof(ctx));
        return NarInitFileExplorerContext(ctx, HandleOptions, VolLetter, Version, RootDir);
    }

    List<CSNarFileEntry^>^ CSNarFileExplorer::CW_GetFilesInCurrentDirectory(){

        List<CSNarFileEntry^>^ Result = gcnew List<CSNarFileEntry^>;     
        CSNarFileEntry^ Entry = gcnew CSNarFileEntry;
        
        for(int i = 0; i<ctx->EList.EntryCount; i++){
           
            Entry->Size = ctx->EList.Entries[i].Size;
            Entry->ID = i;
            Entry->Name = gcnew System::String(ctx->EList.Entries[i].Name);
    
            // TODO name, creation time, lastmodified time, isdirectory flags.

            Result->Add(Entry);

        }


    }
    
    bool CSNarFileExplorer::CW_SelectDirectory(CSNarFileEntry^ Entry){
        
        if(Entry->IsDirectory){
            NarFileExplorerPushDirectory(ctx, Entry->ID);
            return TRUE;
        }

        return FALSE;

    }

    void CSNarFileExplorer::CW_PopDirectory(){
        NarPopDirectoryStack(ctx);
    }

    void CSNarFileExplorer::CW_Free(){
        if (ctx) {
            NarReleaseFileExplorerContext(ctx);
            free(ctx);
            ctx = 0;
        }
    }















    DiskTracker::DiskTracker() {

        C = NarLoadBootState();
        // when loading, always check for old states, if one is not presented, create new one from scratch
        if (C == NULL) {
            printf("Coulndt load from boot file, initializing new CONTEXT\n");
            // couldnt find old state
            C = (LOG_CONTEXT*)malloc(sizeof(LOG_CONTEXT));
            memset(C, 0, sizeof(LOG_CONTEXT));
        }
        else {
            // found old state
            printf("Succ loaded boot state from file\n");
        }

        C->Port = INVALID_HANDLE_VALUE;
        C->ShutDown = NULL;
        C->Thread = NULL;
        C->CleaningUp = FALSE;
        C->Volumes = { 0,0 };

        
        
    }
    
    DiskTracker::~DiskTracker() {
        //Do deconstructor things
        
        free(C->Volumes.Data);
        C->CleaningUp = TRUE;
        WaitForSingleObject(C->ShutDown, INFINITE);
        CloseHandle(C->Thread);
        CloseHandle(C->ShutDown);
        
        delete C;
    }
    
   
    
    bool DiskTracker::CW_InitTracker() {
        return ConnectDriver(C) && SetupVSS();
    }
    
    bool DiskTracker::CW_AddToTrack(wchar_t L, int Type) {
        return AddVolumeToTrack(C, L, (BackupType)Type);
    }
    
    bool DiskTracker::CW_RemoveFromTrack(wchar_t Letter) {
        return RemoveVolumeFromTrack(C, Letter);
    }
    
    bool DiskTracker::CW_SetupStream(wchar_t L, int BT, StreamInfo^ StrInf) {
        
        DotNetStreamInf SI = { 0 };
        if (SetupStream(C, L, (BackupType)BT, &SI)) {
            
            StrInf->ClusterCount = SI.ClusterCount;
            StrInf->ClusterSize = SI.ClusterSize;
            StrInf->FileName = gcnew String(SI.FileName.c_str());
            StrInf->MetadataFileName = gcnew String(SI.MetadataFileName.c_str());
            
            return true;
        }
        
        return false;
    }
    
    /*
  Version: -1 to restore full backup otherwise version number to restore(version number=0 first inc-diff backup)
  */
    bool DiskTracker::CW_RestoreToVolume(
                                         wchar_t TargetLetter,
                                         wchar_t SrcLetter,
                                         INT Version,
                                         bool ShouldFormat,
                                         System::String^ RootDir
                                         ) {
    
        restore_inf R;

        R.TargetLetter = TargetLetter;
        R.SrcLetter = SrcLetter;
        R.Version = Version;

        if (Version < 0) {
            R.Version = NAR_FULLBACKUP_VERSION;
        }
        
        R.RootDir = msclr::interop::marshal_as<std::wstring>(RootDir);
        
        
        return OfflineRestoreToVolume(&R, ShouldFormat);
        
    }
    
    bool DiskTracker::CW_SaveBootState() {
        return NarSaveBootState(C);
    }

    bool DiskTracker::CW_RestoreToFreshDisk(wchar_t TargetLetter, wchar_t SrcLetter, INT Version, int DiskID, System::String^ RootDir) {
        
        R.TargetLetter = TargetLetter;
        R.SrcLetter = SrcLetter;
        R.Version = Version;
        R.RootDir = msclr::interop::marshal_as<std::wstring>(RootDir);

        if (Version < 0) {
            R.Version = NAR_FULLBACKUP_VERSION;
        }
            
        return OfflineRestoreCleanDisk(&R, DiskID);
    }
    
    INT32 DiskTracker::CW_ReadStream(void* Data, wchar_t VolumeLetter, int Size) {
        
        int VolID = GetVolumeID(C, VolumeLetter);
        if(VolID != NAR_INVALID_VOLUME_TRACK_ID){
            return ReadStream(&C->Volumes.Data[VolID], Data, Size);
        }
        // couldnt find volume in the Context, which shoudlnt happen at all
        return 0;
    }
    
    bool DiskTracker::CW_TerminateBackup(bool Succeeded, wchar_t VolumeLetter) {
        
        INT32 VolID = GetVolumeID(C, VolumeLetter);
        if(VolID != NAR_INVALID_VOLUME_TRACK_ID){
            return TerminateBackup(&C->Volumes.Data[VolID], Succeeded);
        }
        // couldnt find volume in the Context, which shoudlnt happen at all
        return 0;   

    }
    
    
    List<VolumeInformation^>^ DiskTracker::CW_GetVolumes() {
        
        data_array<volume_information> V = NarGetVolumes();
        
        List<VolumeInformation^>^ Result = gcnew  List<VolumeInformation^>;
        
        for (int i = 0; i < V.Count; i++) {
            VolumeInformation^ BI = gcnew VolumeInformation;
            BI->Size = V.Data[i].Size;
            BI->Bootable = V.Data[i].Bootable;
            BI->DiskID = V.Data[i].DiskID;
            BI->DiskType = V.Data[i].DiskType;
            BI->Letter = V.Data[i].Letter;
            Result->Add(BI);
        }
        
        FreeDataArray(&V);
        return Result;
    }
    
    List<DiskInfo^>^ DiskTracker::CW_GetDisksOnSystem() {
        
        data_array<disk_information> CResult = NarGetDisks();
        if (CResult.Data == NULL || CResult.Count == 0) {
            if (CResult.Count == 0) printf("Found 0 disks on the system\n");
            if (CResult.Data == 0) printf("GetDisksOnSystem returned NULL\n");

            return nullptr;
        }
        
        List<DiskInfo^>^ Result = gcnew List<DiskInfo^>;
        printf("Found %i disks on the system\n", CResult.Count);

        for (int i = 0; i < CResult.Count; i++) {
        
            DiskInfo^ temp = gcnew DiskInfo;
            temp->ID = CResult.Data[i].ID;
            temp->Size = CResult.Data[i].Size;
            temp->Type = CResult.Data[i].Type;
            
            Result->Add(temp);
        }

        FreeDataArray(&CResult);

        return Result;
        
        
    }

    List<BackupMetadata^>^ DiskTracker::CW_GetBackupsInDirectory(System::String^ SystemStrRootDir) {
        
        wchar_t RootDir[256];
        SystemStringToWCharPtr(SystemStrRootDir, RootDir);

        List<BackupMetadata^>^ ResultList = gcnew List<BackupMetadata^>;
        int MaxMetadataCount = 128;
        int Found = 0;
        backup_metadata* BMList = (backup_metadata*)malloc(sizeof(backup_metadata) * MaxMetadataCount);
        
        BOOLEAN bResult = NarGetBackupsInDirectory(RootDir, BMList, MaxMetadataCount, &Found);
        if (bResult && Found <= MaxMetadataCount) {
            
            printf("FOUND %i", Found);

            for (int i = 0; i < Found; i++) {
                
                BackupMetadata^ BMet = gcnew BackupMetadata;
                BMet->Letter = BMList[i].Letter;
                BMet->BackupType = (int)BMList[i].BT;
                BMet->DiskType = BMList[i].DiskType;
                BMet->OSVolume = BMList[i].IsOSVolume;
                BMet->Version = BMList[i].Version;
                ResultList->Add(BMet);

            }

            
        }
        else {

            if (bResult == FALSE) {
                printf("NarGetBackupsInDirectory returned FALSE\n");
            }
            if (Found > MaxMetadataCount) {
                printf("Found metadata count exceeds maxmetdatacount\n");
            }

        }


        free(BMList);

        return ResultList;
    }
    
    // TODO(Batuhan): helper functions, like which volume we are streaming etc.
}