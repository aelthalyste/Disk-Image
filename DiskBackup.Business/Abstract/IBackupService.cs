using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Abstract
{
    public interface IBackupService
    {
        List<DiskInformation> GetDiskList();
        List<BackupInfo> GetBackupFileList(List<BackupStorageInfo> backupStorageList);  //parametre olarak path alıyor -> dönüş değeri liste (CW_GetBackupsInDirectory)
        BackupInfo GetBackupFile(BackupInfo backupInfo); // istediğimiz backup bilgilerini almak için
        List<FilesInBackup> GetFileInfoList(); 
        List<Log> GetLogList(); //Böyle bir uç yapılacağı konuşuldu
        bool GetSelectedFileInfo(FilesInBackup filesInBackup);

        StatusInfo GetStatusInfo(); // Statu pencereleri için

        void PauseTask(TaskInfo taskInfo);
        void CancelTask(TaskInfo taskInfo);
        void ResumeTask(TaskInfo taskInfo);

        //INC ve DIFF TaskInfo'dan strObjeyi alarak çoklu seçim yapabilir
        bool CreateIncDiffBackup(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo); // Kod tekrarı olmasın diye diff ile ınc birleştirildi
        bool CreateFullBackup(TaskInfo taskInfo); //Bu daha hazır değil        

        //Parametreler bu methodun içinde RestoreTask oluşturacak
        bool RestoreBackupVolume(BackupInfo backupInfo, VolumeInfo volumeInfo); 
        bool RestoreBackupDisk(BackupInfo backupInfo, DiskInformation diskInformation);
        //Restore işleminde disk seçilirse CW_RestoreToFreshDisk, volume seçilirse CW_RestoreToVolume

        bool RestoreFilesInBackup(BackupInfo backupInfo, FilesInBackup fileInfo, string destination); //uçlar belli değil
        void PopDirectory(); //Üst dizine çıkma methodu

        bool InitTracker(); //CW_InitTracker driver okumayla ilgili bir method
        void InitFileExplorer(BackupInfo backupInfo); //CW_InitTracker file

    }
}