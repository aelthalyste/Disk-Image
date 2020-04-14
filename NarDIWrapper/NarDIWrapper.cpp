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

#include "mspyUser.cpp"
#include "mspyLog.cpp"

#include <msclr/marshal.h>
#include <Windows.h>
#include <stdio.h>


using namespace System;

#define CONVERT_TYPES(_in,_out) _out = msclr::interop::marshal_as<decltype(_out)>(_in);

namespace NarDIWrapper {

  DiskTracker::DiskTracker() {
    C = (LOG_CONTEXT*)malloc(sizeof(LOG_CONTEXT));
    memset(C, 0, sizeof(LOG_CONTEXT));
    C->Port = INVALID_HANDLE_VALUE;
    C->ShutDown = NULL;
    C->Thread = NULL;
    C->CleaningUp = FALSE;
    C->Volumes = { 0,0 };

    R = (restore_inf*)malloc(sizeof(restore_inf));
    memset(C, 0, sizeof(restore_inf));

  }

  DiskTracker::~DiskTracker() {
    //Do deconstructor things
    delete R;
    delete C;
  }

  bool DiskTracker::CW_InitTracker() {

    if (SetupVSS()) {
      return ConnectDriver(C);
    }
    return FALSE;

  }

  bool DiskTracker::CW_AddToTrack(wchar_t L, int Type) {
    return AddVolumeToTrack(C, L, (BackupType)Type);
  }

  bool DiskTracker::CW_SetupStream(wchar_t L, StreamInfo^ StrInf) {
    StreamInf SI = { 0 };
    if (SetupStream(C, L, &SI)) {
      
      StrInf->ClusterCount = SI.ClusterCount;
      StrInf->ClusterSize = SI.ClusterSize;
      StrInf->FileName = gcnew String(SI.FileName.c_str());
      StrInf->MetadataFileName = gcnew String(SI.MetadataFileName.c_str());
      
      int ID = GetVolumeID(C, L);

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
    INT Type
  ) {

    R->TargetLetter = TargetLetter;
    R->SrcLetter = SrcLetter;
    R->ClusterSize = ClusterSize;
    R->Type = (BackupType)Type;

    R->ToFull = FALSE;
    R->Version = Version;
    if (Version < 0) {
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