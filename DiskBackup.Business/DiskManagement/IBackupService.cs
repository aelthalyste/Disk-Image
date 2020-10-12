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

        bool CreateIncrementalBackup(VolumeInfo volumeInfo);
        bool CreateDifferentialBackup(VolumeInfo volumeInfo);
        bool RestoreBackup(RestoreTask restoreTask, VolumeInfo volumeInfo);
        bool RestoreFile(RestoreTask restoreTask, FileInfo fileInfo, string destination);
        bool RestoreFolder(RestoreTask restoreTask, FileInfo fileInfo, string destination);
        bool AddBackupArea(BackupStorageInfo backupArea);
        bool PauseBackup(VolumeInfo volumeInfo);
        bool CancelBackup(VolumeInfo volumeInfo);
        bool ResumeBackup(VolumeInfo volumeInfo);
    }
}
