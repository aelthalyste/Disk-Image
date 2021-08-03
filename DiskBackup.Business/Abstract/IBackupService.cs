using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.ServiceModel;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Abstract
{
    [ServiceContract]
    public interface IBackupService
    {
        [OperationContract]
        List<DiskInformation> GetDiskList();
        [OperationContract]
        List<BackupInfo> GetBackupFileList(List<BackupStorageInfo> backupStorageList);  //parametre olarak path alıyor -> dönüş değeri liste (CW_GetBackupsInDirectory)
        [OperationContract]
        BackupInfo GetBackupFile(BackupInfo backupInfo); // istediğimiz backup bilgilerini almak için
        [OperationContract]
        List<FilesInBackup> GetFileInfoList();
        [OperationContract]
        List<ActivityDownLog> GetDownLogList(); //Böyle bir uç yapılacağı konuşuldu
        [OperationContract]
        bool GetSelectedFileInfo(FilesInBackup filesInBackup);

        [OperationContract]
        void PauseTask(TaskInfo taskInfo);
        [OperationContract]
        void CancelTask(TaskInfo taskInfo);
        [OperationContract]
        void ResumeTask(TaskInfo taskInfo);

        //INC ve DIFF TaskInfo'dan strObjeyi alarak çoklu seçim yapabilir
        [OperationContract]
        int CreateIncDiffBackup(TaskInfo taskInfo);
        [OperationContract]
        bool CreateFullBackup(TaskInfo taskInfo); //Bu daha hazır değil        

        //Parametreler bu methodun içinde RestoreTask oluşturacak
        [OperationContract]
        byte RestoreBackupVolume(TaskInfo taskInfo);
        [OperationContract]
        byte RestoreBackupDisk(TaskInfo taskInfo);
        //Restore işleminde disk seçilirse CW_RestoreToFreshDisk, volume seçilirse CW_RestoreToVolume
        [OperationContract]
        bool CleanChain(char letter);
        [OperationContract]
        char AvailableVolumeLetter();
        [OperationContract]
        bool IsVolumeAvailable(char letter);

        [OperationContract]
        FileRestoreResult RestoreFilesInBackup(FilesInBackup file, string targetDirectory);
        [OperationContract]
        void PopDirectory(); //Üst dizine çıkma methodu
        [OperationContract]
        string GetCurrentDirectory(); // path yazdırma

        [OperationContract]
        bool InitTracker(); //CW_InitTracker driver okumayla ilgili bir method
        [OperationContract]
        void InitFileExplorer(BackupInfo backupInfo); //CW_InitTracker file
        [OperationContract]
        void FreeFileExplorer();

        [OperationContract]
        bool GetInitTracker();
        [OperationContract]
        bool GetRefreshIncDiffTaskFlag();
        [OperationContract]
        void RefreshIncDiffTaskFlag(bool value);
        [OperationContract]
        bool GetRefreshIncDiffLogFlag();
        [OperationContract]
        void RefreshIncDiffLogFlag(bool value);

        [OperationContract]
        byte BackupFileDelete(BackupInfo backupInfo);

    }
}