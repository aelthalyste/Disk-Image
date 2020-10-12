using DisckBackup.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.TaskManagement
{
    public interface ITaskService
    {
        bool AddRestoreTask(RestoreTask restoreTask);
        bool UpdateRestoreTask(RestoreTask restoreTask);
        bool AddBackupTask(BackupTask backupTask);
        bool RemoveBackupTask(BackupTask backupTask);
        bool PauseBackupTask(BackupTask backupTask);
        bool StopBackupTask(BackupTask backupTask);
        bool ResumeBackupTask(BackupTask backupTask);
        bool UpdateBackupTask(BackupTask backupTask);
        bool RestartFailedBackupTask(BackupTask backupTask);
        bool DisableBackupTask(BackupTask backupTask);
        bool EnableBackupTask(BackupTask backupTask);
        List<BackupTask> GetBackupTaskList();
        List<RestoreTask> GetRestoreTaskList();
        List<ActivityLog> GetActivityLogList();
    }
}
