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

        void PauseTask(TaskInfo taskInfo);
        void CancelTask(TaskInfo taskInfo);
        void ResumeTask(TaskInfo taskInfo);

        //INC ve DIFF TaskInfo'dan strObjeyi alarak çoklu seçim yapabilir
        byte CreateIncDiffBackup(TaskInfo taskInfo);
        bool CreateFullBackup(TaskInfo taskInfo); //Bu daha hazır değil        

        //Parametreler bu methodun içinde RestoreTask oluşturacak
        bool RestoreBackupVolume(BackupInfo backupInfo, char volumeLetter); 
        bool RestoreBackupDisk(BackupInfo backupInfo, DiskInformation diskInformation);
        //Restore işleminde disk seçilirse CW_RestoreToFreshDisk, volume seçilirse CW_RestoreToVolume

        void RestoreFilesInBackup(int fileId, string backupDirectory, string targetDirectory);
        void PopDirectory(); //Üst dizine çıkma methodu
        string GetCurrentDirectory(); // path yazdırma

        bool InitTracker(); //CW_InitTracker driver okumayla ilgili bir method
        void InitFileExplorer(BackupInfo backupInfo); //CW_InitTracker file
        void FreeFileExplorer();

    }
}