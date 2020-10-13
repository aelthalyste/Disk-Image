using DisckBackup.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.DiskManagement
{
    public interface IBackupService
    {
        List<DiskInfo> GetDiskList();
        List<RestoreTask> GetBackupFileList();
        List<BackupStorageInfo> GetBackupAreaInfoList();
        //Yüzde hesabı Read Write' a göre yapılacak
        //Okuma olmadığında pause olucak
        //TerminateBackup'a false verilince durdurulacak
        bool CreateIncrementalBackup(VolumeInfo volumeInfo);
        bool CreateDifferentialBackup(VolumeInfo volumeInfo);
        //Full için uç gelicek
        bool RestoreBackup(RestoreTask restoreTask, VolumeInfo volumeInfo);
        //uçlar belli değil
        bool RestoreFile(RestoreTask restoreTask, FileInfo fileInfo, string destination);
        bool RestoreFolder(RestoreTask restoreTask, FileInfo fileInfo, string destination);
        //...
        bool AddBackupArea(BackupStorageInfo backupArea);
        bool PauseBackup(VolumeInfo volumeInfo);
        bool CancelBackup(VolumeInfo volumeInfo);
        bool ResumeBackup(VolumeInfo volumeInfo);
        //File Backup Browse methodları eklenecek
    }
}

//Directory'deki backuplar için Batuhanın ucu var (CW_BackupsInDirectory)
//Systemdeki disklerin görüntülenmesi için Batuhanın ucu var(CW_DisksOnSystem)
//CW_GetVolumes'e pcName, ipAddres vs eklenecekmiş
//StreamInfo class işlemleri (ReadStream, SetupStream) ?? (status progressbarlar doldurulur)
//CW_RemoveFromTrack işlemi içine Disk harfini alıp trackten kaldırıyor (kast edilen track ne)
//CW_RestoreToFreshDisk ve CW_RestoreToVolume methodları ayrı restore işlemi yapan methodlar
//CW_InitTracker() driver okumasıyla ilgili 